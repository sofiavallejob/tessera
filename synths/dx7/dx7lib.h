/*
 * dx7lib.h -- patch library for dx7~ (header-only C99, no Pd dependency).
 *
 *   - SysEx writing: 32-voice bank and single voice messages
 *   - reading every message in a file (some dumps hold several banks)
 *   - a library of voices with duplicate / blank filtering, search, favourites,
 *     bank stepping, and a "USER" bank for voices stored from the synth
 *   - A/B morph and mutate on single voices
 */
#ifndef DX7LIB_H
#define DX7LIB_H

#include "dx7core.h"
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------------------- */
/* SysEx writing                                                             */
/* ------------------------------------------------------------------------- */

/* 155-byte VCED -> 128-byte packed voice (the inverse of dx7_unpack) */
static inline void dx7_pack(const uint8_t *v, uint8_t *b)
{
    int n;
    memset(b, 0, 128);
    for (n = 0; n < 6; n++) {
        const uint8_t *s = v + n * DX7_OP_SIZE;
        uint8_t *d = b + n * 17;
        memcpy(d, s, 11);
        d[11] = (uint8_t)((s[DX7_OP_LC] & 3) | ((s[DX7_OP_RC] & 3) << 2));
        d[12] = (uint8_t)((s[DX7_OP_RS] & 7) | ((s[DX7_OP_DETUNE] & 15) << 3));
        d[13] = (uint8_t)((s[DX7_OP_AMS] & 3) | ((s[DX7_OP_KVS] & 7) << 2));
        d[14] = s[DX7_OP_LEVEL];
        d[15] = (uint8_t)((s[DX7_OP_MODE] & 1) | ((s[DX7_OP_COARSE] & 31) << 1));
        d[16] = s[DX7_OP_FINE];
    }
    memcpy(b + 102, v + 126, 9);
    b[111] = (uint8_t)((v[DX7_P_FB] & 7) | ((v[DX7_P_OSCSYNC] & 1) << 3));
    memcpy(b + 112, v + DX7_P_LFO_SPEED, 4);
    b[116] = (uint8_t)((v[DX7_P_LFO_SYNC] & 1) | ((v[DX7_P_LFO_WAVE] & 7) << 1) | ((v[DX7_P_LFO_PMS] & 7) << 4));
    memcpy(b + 117, v + DX7_P_TRANSPOSE, 11);
}

/* 32-voice bank message, 4104 bytes. n < 32 is padded with INIT VOICE. */
static inline void dx7_make_bank_sysex(uint8_t (*voices)[DX7_VCED], int n, uint8_t *out)
{
    static const uint8_t hdr[6] = { 0xF0, 0x43, 0x00, 0x09, 0x20, 0x00 };
    uint8_t init[DX7_VCED];
    int i, sum = 0;
    dx7_patch_init(init);
    memcpy(out, hdr, 6);
    for (i = 0; i < 32; i++) dx7_pack(i < n ? voices[i] : init, out + 6 + i * 128);
    for (i = 0; i < 4096; i++) sum += out[6 + i];
    out[6 + 4096] = (uint8_t)((-sum) & 0x7F);
    out[6 + 4097] = 0xF7;
}

/* single-voice message (VCED), 163 bytes */
static inline void dx7_make_voice_sysex(const uint8_t *v, uint8_t *out)
{
    int i, sum = 0;
    out[0] = 0xF0; out[1] = 0x43; out[2] = 0x00; out[3] = 0x00; out[4] = 0x01; out[5] = 0x1B;
    memcpy(out + 6, v, DX7_VCED);
    for (i = 0; i < DX7_VCED; i++) sum += v[i];
    out[6 + DX7_VCED] = (uint8_t)((-sum) & 0x7F);
    out[7 + DX7_VCED] = 0xF7;
}

#define DX7_VOICE_SYSEX_LEN (8 + DX7_VCED)
#define DX7_BANK_SYSEX_LEN  4104

/* ------------------------------------------------------------------------- */
/* Reading every message of a file                                           */
/* ------------------------------------------------------------------------- */

/* Walks all F0..F7 messages; header-less data is handed to dx7_parse_sysex as is.
 * Returns the number of voices written (at most maxn). */
static inline int dx7_parse_all(const uint8_t *b, size_t len, uint8_t (*out)[DX7_VCED], int maxn)
{
    size_t i = 0;
    int n = 0;
    if (len == 0) return 0;
    if (b[0] != 0xF0) return dx7_parse_sysex(b, len, out, maxn);
    while (i < len && n < maxn) {
        size_t j;
        if (b[i] != 0xF0) { i++; continue; }
        j = i + 1;
        while (j < len && b[j] != 0xF7) j++;
        if (j >= len) j = len - 1;                 /* unterminated: take the rest */
        n += dx7_parse_sysex(b + i, j - i + 1, out + n, maxn - n);
        i = j + 1;
    }
    return n;
}

/* ------------------------------------------------------------------------- */
/* Voice utilities                                                           */
/* ------------------------------------------------------------------------- */

/* hash over the 145 parameter bytes (name excluded) */
static inline uint64_t dx7_voice_hash(const uint8_t *v)
{
    uint64_t h = 1469598103934665603ULL;
    int i;
    for (i = 0; i < DX7_P_NAME; i++) { h ^= v[i]; h *= 1099511628211ULL; }
    return h;
}

/* no operator is audible (all output levels 0) */
static inline int dx7_voice_silent(const uint8_t *v)
{
    int n;
    for (n = 0; n < 6; n++) if (v[n * DX7_OP_SIZE + DX7_OP_LEVEL] > 0) return 0;
    return 1;
}

/* the factory "init voice": one plain sine, nothing else */
static inline int dx7_voice_is_init(const uint8_t *v)
{
    uint8_t init[DX7_VCED];
    dx7_patch_init(init);
    return dx7_voice_hash(v) == dx7_voice_hash(init);
}

static inline void dx7_voice_name(const uint8_t *v, char *out11)
{
    int i = 9;
    memcpy(out11, v + DX7_P_NAME, 10);
    out11[10] = 0;
    while (i >= 0 && out11[i] == ' ') out11[i--] = 0;
}

/*
 * Morph: continuous parameters are interpolated, switches (algorithm, ratios,
 * curves, waves, ...) flip at t = 0.5. Levels interpolate in DX7 level units,
 * which are logarithmic, so the blend is a crossfade in dB.
 */
static inline int dx7_is_continuous(int idx)
{
    if (idx >= 126 && idx <= 133) return 1;                 /* pitch EG */
    if (idx == DX7_P_FB) return 1;
    if (idx >= DX7_P_LFO_SPEED && idx <= DX7_P_LFO_AMD) return 1;
    if (idx < 126) {
        int o = idx % DX7_OP_SIZE;
        return o <= 10 || o == DX7_OP_LEVEL || o == DX7_OP_FINE || o == DX7_OP_DETUNE;
    }
    return 0;
}

static inline void dx7_morph(const uint8_t *a, const uint8_t *b, float t, uint8_t *out)
{
    int i;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    for (i = 0; i < DX7_VCED; i++) {
        if (i < DX7_P_NAME && dx7_is_continuous(i))
            out[i] = (uint8_t)floorf((1.f - t) * a[i] + t * b[i] + 0.5f);
        else
            out[i] = t < 0.5f ? a[i] : b[i];
    }
    dx7_patch_sanitize(out);
}

static inline uint32_t dx7_rand(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

static inline float dx7_rand_pm(uint32_t *s) { return (float)(dx7_rand(s) & 0xFFFFFF) / 8388608.f - 1.f; }

/* random variation of a voice: keeps the algorithm and the frequency ratios */
static inline void dx7_mutate(uint8_t *v, float amount, uint32_t *seed)
{
    int n, i;
    if (amount < 0.f) amount = 0.f;
    if (amount > 1.f) amount = 1.f;
    for (n = 0; n < 6; n++) {
        uint8_t *o = v + n * DX7_OP_SIZE;
        if (o[DX7_OP_LEVEL] == 0) continue;
        for (i = 0; i < 4; i++) {
            int r = o[i] + (int)floorf(dx7_rand_pm(seed) * amount * 18.f + 0.5f);
            o[i] = (uint8_t)(r < 0 ? 0 : (r > 99 ? 99 : r));
        }
        for (i = 4; i < 7; i++) {
            int l = o[i] + (int)floorf(dx7_rand_pm(seed) * amount * 22.f + 0.5f);
            o[i] = (uint8_t)(l < 0 ? 0 : (l > 99 ? 99 : l));
        }
        {
            int lv = o[DX7_OP_LEVEL] + (int)floorf(dx7_rand_pm(seed) * amount * 16.f + 0.5f);
            o[DX7_OP_LEVEL] = (uint8_t)(lv < 1 ? 1 : (lv > 99 ? 99 : lv));
        }
        {
            int d = o[DX7_OP_DETUNE] + (int)floorf(dx7_rand_pm(seed) * amount * 4.f + 0.5f);
            o[DX7_OP_DETUNE] = (uint8_t)(d < 0 ? 0 : (d > 14 ? 14 : d));
        }
    }
    {
        int fb = v[DX7_P_FB] + (int)floorf(dx7_rand_pm(seed) * amount * 3.f + 0.5f);
        v[DX7_P_FB] = (uint8_t)(fb < 0 ? 0 : (fb > 7 ? 7 : fb));
    }
    dx7_patch_sanitize(v);
}

/* ------------------------------------------------------------------------- */
/* Library                                                                   */
/* ------------------------------------------------------------------------- */

#define DX7_LIB_MAX      200000
#define DX7_LIB_BANKNAME 32

typedef struct {
    uint8_t  v[DX7_VCED];
    uint8_t  fav;
    uint8_t  slot;            /* 0-based position inside its bank */
    uint16_t bank;
} dx7_entry;

typedef struct {
    dx7_entry *e;
    int        n, cap;
    char     (*bank)[DX7_LIB_BANKNAME];
    int       *bank_count;    /* entries per bank (after filtering duplicates) */
    int        nbanks, bankcap;
    uint64_t  *ht;            /* open-addressing hash set of voice hashes (0 = empty) */
    size_t     htcap, htn;
    int        user_bank;     /* index of the USER bank, -1 if not created */

    /* view = entries that pass the filters, in library order */
    int       *view;
    int        nview;
    int        pos;           /* position inside the view, -1 = nothing selected */
    char       f_text[32];
    int        f_favs, f_algo, f_bank;   /* f_algo 0 = any (1..32), f_bank -1 = all */

    int        skipped_dup, skipped_blank;
} dx7_lib;

static inline void dx7_lib_init(dx7_lib *L)
{
    memset(L, 0, sizeof(*L));
    L->user_bank = -1;
    L->pos = -1;
    L->f_bank = -1;
}

static inline void dx7_lib_free(dx7_lib *L)
{
    free(L->e); free(L->bank); free(L->bank_count); free(L->ht); free(L->view);
    dx7_lib_init(L);
}

/* forget everything (favourites too) */
static inline void dx7_lib_clear(dx7_lib *L)
{
    dx7_lib_free(L);
}

static inline int dx7_lib_grow_entries(dx7_lib *L)
{
    if (L->n < L->cap) return 1;
    {
        int nc = L->cap ? L->cap * 2 : 1024;
        dx7_entry *ne;
        if (nc > DX7_LIB_MAX) nc = DX7_LIB_MAX;
        if (nc <= L->cap) return 0;
        ne = (dx7_entry *)realloc(L->e, (size_t)nc * sizeof(dx7_entry));
        if (!ne) return 0;
        L->e = ne;
        L->cap = nc;
    }
    return 1;
}

static inline int dx7_lib_add_bank_name(dx7_lib *L, const char *name)
{
    if (L->nbanks >= 65535) return -1;
    if (L->nbanks >= L->bankcap) {
        int nc = L->bankcap ? L->bankcap * 2 : 64;
        char (*nb)[DX7_LIB_BANKNAME] = (char (*)[DX7_LIB_BANKNAME])realloc(L->bank, (size_t)nc * DX7_LIB_BANKNAME);
        int *cnt = (int *)realloc(L->bank_count, (size_t)nc * sizeof(int));
        if (!nb || !cnt) return -1;
        L->bank = nb; L->bank_count = cnt; L->bankcap = nc;
    }
    snprintf(L->bank[L->nbanks], DX7_LIB_BANKNAME, "%.*s", DX7_LIB_BANKNAME - 1, name);
    L->bank_count[L->nbanks] = 0;
    return L->nbanks++;
}

static inline int dx7_lib_ht_insert(dx7_lib *L, uint64_t h)
{
    size_t i;
    if (h == 0) h = 1;
    if ((L->htn + 1) * 2 > L->htcap) {
        size_t nc = L->htcap ? L->htcap * 2 : 4096, k;
        uint64_t *nt = (uint64_t *)calloc(nc, sizeof(uint64_t));
        if (!nt) return -1;
        for (k = 0; k < L->htcap; k++) {
            uint64_t v = L->ht[k];
            if (v) { size_t j = (size_t)(v * 0x9E3779B97F4A7C15ULL >> 20) & (nc - 1); while (nt[j]) j = (j + 1) & (nc - 1); nt[j] = v; }
        }
        free(L->ht);
        L->ht = nt; L->htcap = nc;
    }
    i = (size_t)(h * 0x9E3779B97F4A7C15ULL >> 20) & (L->htcap - 1);
    while (L->ht[i]) { if (L->ht[i] == h) return 0; i = (i + 1) & (L->htcap - 1); }
    L->ht[i] = h; L->htn++;
    return 1;        /* 1 = new, 0 = already there, -1 = out of memory */
}

/* add the voices of one file as a bank; returns how many were kept (-1 on memory error) */
static inline int dx7_lib_add_bank(dx7_lib *L, const char *bankname, uint8_t (*voices)[DX7_VCED], int nv, int dedupe)
{
    int b, i, kept = 0;
    if (nv <= 0) return 0;
    b = dx7_lib_add_bank_name(L, bankname);
    if (b < 0) return -1;
    for (i = 0; i < nv; i++) {
        uint64_t h = dx7_voice_hash(voices[i]);
        dx7_entry *en;
        int ins;
        if (dx7_voice_silent(voices[i]) || dx7_voice_is_init(voices[i])) { L->skipped_blank++; continue; }
        ins = dx7_lib_ht_insert(L, h);
        if (ins < 0) return kept;
        if (dedupe && ins == 0) { L->skipped_dup++; continue; }
        if (!dx7_lib_grow_entries(L)) return kept;
        en = &L->e[L->n];
        memcpy(en->v, voices[i], DX7_VCED);
        en->fav = 0;
        en->slot = (uint8_t)i;
        en->bank = (uint16_t)b;
        L->n++;
        L->bank_count[b]++;
        kept++;
    }
    if (kept == 0) L->nbanks--;           /* nothing worth keeping: drop the empty bank */
    return kept;
}

static inline void dx7_lib_entry_name(const dx7_lib *L, int idx, char *out11)
{
    dx7_voice_name(L->e[idx].v, out11);
}

static inline int dx7_lib_icontains(const char *hay, const char *needle)
{
    size_t nl = strlen(needle), hl = strlen(hay), i, j;
    if (nl == 0) return 1;
    if (nl > hl) return 0;
    for (i = 0; i + nl <= hl; i++) {
        for (j = 0; j < nl; j++)
            if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j])) break;
        if (j == nl) return 1;
    }
    return 0;
}

/* rebuild the view; tries to keep the same entry selected (cur = entry index or -1) */
static inline void dx7_lib_rebuild(dx7_lib *L, int cur)
{
    int i, newpos = -1;
    int *nv;
    free(L->view);
    L->view = NULL; L->nview = 0; L->pos = -1;
    if (L->n == 0) return;
    nv = (int *)malloc((size_t)L->n * sizeof(int));
    if (!nv) return;
    for (i = 0; i < L->n; i++) {
        const dx7_entry *en = &L->e[i];
        if (L->f_favs && !en->fav) continue;
        if (L->f_algo && en->v[DX7_P_ALGO] + 1 != L->f_algo) continue;
        if (L->f_bank >= 0 && en->bank != L->f_bank) continue;
        if (L->f_text[0]) {
            char nm[11];
            dx7_voice_name(en->v, nm);
            if (!dx7_lib_icontains(nm, L->f_text) && !dx7_lib_icontains(L->bank[en->bank], L->f_text)) continue;
        }
        if (i == cur) newpos = L->nview;
        nv[L->nview++] = i;
    }
    L->view = nv;
    L->pos = L->nview ? (newpos >= 0 ? newpos : 0) : -1;
}

/* entry index at the current position, -1 if the view is empty */
static inline int dx7_lib_current(const dx7_lib *L)
{
    return (L->pos >= 0 && L->pos < L->nview) ? L->view[L->pos] : -1;
}

static inline int dx7_lib_step(dx7_lib *L, int d)
{
    if (L->nview == 0) return -1;
    L->pos = ((L->pos < 0 ? 0 : L->pos) + d) % L->nview;
    if (L->pos < 0) L->pos += L->nview;
    return dx7_lib_current(L);
}

static inline int dx7_lib_goto(dx7_lib *L, int pos1)   /* 1-based */
{
    if (L->nview == 0) return -1;
    if (pos1 < 1) pos1 = 1;
    if (pos1 > L->nview) pos1 = L->nview;
    L->pos = pos1 - 1;
    return dx7_lib_current(L);
}

/* step to the next / previous bank that has entries; returns its index or -1 */
static inline int dx7_lib_bank_step(dx7_lib *L, int d)
{
    int b, k;
    if (L->nbanks == 0) return -1;
    b = L->f_bank;
    if (b < 0) {
        int cur = dx7_lib_current(L);
        b = cur >= 0 ? L->e[cur].bank : (d > 0 ? -1 : 0);
    }
    for (k = 0; k < L->nbanks; k++) {
        b = ((b + d) % L->nbanks + L->nbanks) % L->nbanks;
        if (L->bank_count[b] > 0) return b;
    }
    return -1;
}

/* USER bank for voices stored from the synth; returns the new entry index or -1 */
static inline int dx7_lib_store(dx7_lib *L, const uint8_t *vced)
{
    dx7_entry *en;
    if (L->user_bank < 0) {
        L->user_bank = dx7_lib_add_bank_name(L, "USER");
        if (L->user_bank < 0) return -1;
    }
    if (!dx7_lib_grow_entries(L)) return -1;
    en = &L->e[L->n];
    memcpy(en->v, vced, DX7_VCED);
    en->fav = 1;
    en->slot = (uint8_t)(L->bank_count[L->user_bank] & 255);
    en->bank = (uint16_t)L->user_bank;
    dx7_lib_ht_insert(L, dx7_voice_hash(vced));
    L->bank_count[L->user_bank]++;
    return L->n++;
}

static inline int dx7_lib_find_hash(const dx7_lib *L, uint64_t h)
{
    int i;
    for (i = 0; i < L->n; i++) if (dx7_voice_hash(L->e[i].v) == h) return i;
    return -1;
}

static inline int dx7_lib_count_favs(const dx7_lib *L)
{
    int i, c = 0;
    for (i = 0; i < L->n; i++) c += L->e[i].fav;
    return c;
}

#endif /* DX7LIB_H */
