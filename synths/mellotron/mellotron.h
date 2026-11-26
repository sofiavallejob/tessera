/*
 * mellotron.h - Mellotron style tape keyboard. Each key plays a tape: your own WAV files are mapped to keys
 * (samples <folder> or sample <note> <file>) and play from the start when a key goes down. The tape runs for
 * a limited time (up to 8 seconds like the real machine) and then stops until you release the key. Without
 * samples it plays a built-in synthetic stand-in (flute, strings, choir) so the object always makes sound.
 * Wow, flutter, drift, tape hiss, tone loss and a pitch control. 16 voices. No samples are shipped.
 */
#ifndef MELLOTRON_H
#define MELLOTRON_H
#include "synthcore.h"
#ifdef PD_MAJOR_VERSION
#define MEL_POST post
#else
#define MEL_POST(...) (fprintf(stderr, __VA_ARGS__), fputc(10, stderr))
#endif
#include <strings.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

enum { L_TAPE, L_LENGTH, L_LOOP, L_PITCH, L_WOW, L_FLUTTER, L_DRIFT, L_TONE, L_HISS, L_ATT, L_REL, L_VEL, L_VOL, L_COUNT };

static const sk_param mel_params[L_COUNT] = {
    {"tape", 0, 3}, {"length", 1.0f, 0}, {"loop", 0, 2}, {"pitch", 0.5f, 0}, {"wow", 0.3f, 0}, {"flutter", 0.25f, 0}, {"drift", 0.2f, 0},
    {"tone", 0.6f, 0}, {"hiss", 0.15f, 0}, {"att", 0.05f, 0}, {"rel", 0.15f, 0}, {"vel", 0.0f, 0}, {"vol", 0.7f, 0},
};
static const sk_preset_def mel_presets[] = {
    {"Flute Tape",    "tape=0 length=1 wow=0.3 flutter=0.25 drift=0.2 tone=0.6 hiss=0.15 att=0.05 rel=0.15"},
    {"String Tape",   "tape=1 length=1 wow=0.35 flutter=0.2 drift=0.25 tone=0.5 hiss=0.2 att=0.12 rel=0.2"},
    {"Choir Tape",    "tape=2 length=1 wow=0.3 flutter=0.2 drift=0.25 tone=0.55 hiss=0.2 att=0.1 rel=0.25"},
    {"Worn Tape",     "tape=1 length=1 wow=0.8 flutter=0.6 drift=0.6 tone=0.3 hiss=0.5 att=0.08 rel=0.2"},
    {"Clean Tape",    "tape=0 length=1 wow=0.05 flutter=0.05 drift=0.05 tone=0.9 hiss=0.0 att=0.03 rel=0.1"},
    {"Short Tape",    "tape=2 length=0.1 wow=0.3 flutter=0.2 drift=0.2 tone=0.5 hiss=0.2 att=0.05 rel=0.12"},
    {"Looping Pad",   "tape=1 loop=1 length=0.4 wow=0.4 flutter=0.2 drift=0.3 tone=0.5 hiss=0.15 att=0.3 rel=0.5"},
    {"Seasick Choir", "tape=2 wow=1 flutter=0.4 drift=0.8 tone=0.45 hiss=0.3 att=0.2 rel=0.4 pitch=0.45"},
};

typedef struct { float *d; int len; float sr; } mel_slot;
typedef struct { mel_slot s[128]; int n; } mel_bank;

static void *mel_user_new(float sr) { (void)sr; return calloc(1, sizeof(mel_bank)); }
static void mel_clear(mel_bank *b) { for (int i = 0; i < 128; i++) { free(b->s[i].d); b->s[i].d = 0; b->s[i].len = 0; } b->n = 0; }
static void mel_user_free(void *u) { if (u) { mel_clear((mel_bank *)u); free(u); } }

static unsigned mel_rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }
static unsigned mel_rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
/* mono float samples from a RIFF WAV (8/16/24/32-bit PCM or 32-bit float, any channel count). Returns frames or -1 */
static int mel_load_wav(const char *path, float **out, float *sr_out)
{
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 44 || sz > 400L * 1024 * 1024) { fclose(f); return -1; }
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { free(buf); return -1; }
    int tag = 0, ch = 0, bits = 0; unsigned rate = 0; const unsigned char *data = 0; unsigned dlen = 0;
    long pos = 12;
    while (pos + 8 <= sz) {
        unsigned cl = mel_rd32(buf + pos + 4); const unsigned char *c = buf + pos + 8;
        if (!memcmp(buf + pos, "fmt ", 4) && pos + 8 + 16 <= sz) {
            tag = (int)mel_rd16(c); ch = (int)mel_rd16(c + 2); rate = mel_rd32(c + 4); bits = (int)mel_rd16(c + 14);
            if (tag == 0xFFFE && cl >= 26) tag = (int)mel_rd16(c + 24);
        } else if (!memcmp(buf + pos, "data", 4)) { data = c; dlen = cl; if ((long)(pos + 8 + dlen) > sz) dlen = (unsigned)(sz - pos - 8); break; }
        pos += 8 + (long)cl + (cl & 1);
    }
    int bytes = bits / 8;
    if (!data || ch < 1 || ch > 16 || rate < 1000 || (tag != 1 && tag != 3) || bytes < 1 || bytes > 4 || (tag == 3 && bits != 32)) { free(buf); return -1; }
    int frames = (int)(dlen / (unsigned)(bytes * ch));
    if (frames < 16) { free(buf); return -1; }
    float *o = (float *)malloc(sizeof(float) * (size_t)frames);
    if (!o) { free(buf); return -1; }
    for (int i = 0; i < frames; i++) {
        float acc = 0.0f;
        for (int c2 = 0; c2 < ch; c2++) {
            const unsigned char *q = data + ((size_t)i * ch + c2) * bytes; float v;
            if (tag == 3) { float fv; memcpy(&fv, q, 4); v = fv; }
            else if (bytes == 1) v = ((int)q[0] - 128) / 128.0f;
            else if (bytes == 2) v = (short)mel_rd16(q) / 32768.0f;
            else if (bytes == 3) { int x = (int)(q[0] << 8 | q[1] << 16 | (unsigned)q[2] << 24) >> 8; v = (float)x / 8388608.0f; }
            else v = (float)((int)mel_rd32(q)) / 2147483648.0f;
            acc += v;
        }
        o[i] = acc / (float)ch;
    }
    free(buf); *out = o; *sr_out = (float)rate; return frames;
}
static void mel_path(const char *dir, const char *in, char *out, size_t n)
{
    if (in[0] == '/') snprintf(out, n, "%s", in);
    else if (in[0] == '~') snprintf(out, n, "%s%s", getenv("HOME") ? getenv("HOME") : "", in + 1);
    else snprintf(out, n, "%s/%s", dir, in);
}
static int mel_add(mel_bank *b, int note, const char *path)
{
    float *d; float sr; int n = mel_load_wav(path, &d, &sr);
    if (n < 0) { MEL_POST("mellotron: cannot read wav '%s'", path); return -1; }
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    free(b->s[note].d); b->s[note].d = d; b->s[note].len = n; b->s[note].sr = sr;
    int c = 0; for (int i = 0; i < 128; i++) if (b->s[i].d) c++; b->n = c;
    return 0;
}
static int mel_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int mel_cmd(void *u, const char *name, const char *arg, const char *dir, float sr)
{
    mel_bank *b = (mel_bank *)u; char path[1024]; (void)sr;
    if (!b) return -1;
    if (!strcmp(name, "sample")) {                       /* sample <note> <file> */
        int note = atoi(arg); const char *sp = strchr(arg, ' ');
        if (!sp) return -1;
        while (*sp == ' ') sp++;
        mel_path(dir, sp, path, sizeof path);
        return mel_add(b, note, path);
    }
    if (!strcmp(arg, "clear")) { mel_clear(b); return 0; }
    mel_path(dir, arg, path, sizeof path);
    DIR *dp = opendir(path); if (!dp) { MEL_POST("mellotron: cannot open folder '%s'", path); return -1; }
    char *names[256]; int nn = 0; struct dirent *de;
    while ((de = readdir(dp)) && nn < 256) {
        size_t l = strlen(de->d_name);
        if (l > 4 && de->d_name[0] != '.' && !strcasecmp(de->d_name + l - 4, ".wav")) names[nn++] = strdup(de->d_name);
    }
    closedir(dp);
    qsort(names, (size_t)nn, sizeof(char *), mel_cmp);
    mel_clear(b);
    int next = 48, ok = 0;
    for (int i = 0; i < nn; i++) {
        char full[1400]; snprintf(full, sizeof full, "%s/%s", path, names[i]);
        int note = -1; const char *s = names[i];
        if (s[0] >= '0' && s[0] <= '9') { note = atoi(s); }
        if (nn == 1 && note < 0) note = 60;      /* one file without a number: it is middle C and every key transposes it */
        if (note < 0 || note > 127) note = next;
        next = note + 1;
        if (mel_add(b, note, full) == 0) ok++;
        free(names[i]);
    }
    MEL_POST("mellotron: loaded %d wav files from %s", ok, path);
    return ok > 0 ? 0 : -1;
}

typedef struct {
    sk_rng rng; sk_svf f1, f2; float env, ph, ph2, ph3, pos, tape_t, lp, wowph, flph, drift, driftt, vel, hiss_lp;
    int note, gate, slot, active, count;
} mel_voice;

static void mel_vinit(void *vv, float sr) { mel_voice *v = (mel_voice *)vv; (void)sr; memset(v, 0, sizeof *v); sk_rng_seed(&v->rng, 400u); sk_svf_init(&v->f1); sk_svf_init(&v->f2); }
static void mel_von(void *vv, const float *p, int note, float vel, int first, float sr)
{
    mel_voice *v = (mel_voice *)vv; (void)p; (void)first; (void)sr;
    sk_rng_seed(&v->rng, (uint32_t)(note * 2654435761u) + (uint32_t)(++v->count * 40503u));
    v->note = note; v->vel = vel; v->gate = 1; v->pos = 0.0f; v->tape_t = 0.0f; v->env = 0.0f; v->active = 1;
    v->ph = v->ph2 = v->ph3 = 0.0f; v->wowph = sk_rng_uni(&v->rng); v->flph = sk_rng_uni(&v->rng);
    sk_svf_init(&v->f1); sk_svf_init(&v->f2); v->lp = 0.0f;
}
static void mel_voff(void *vv, const float *p) { mel_voice *v = (mel_voice *)vv; (void)p; v->gate = 0; }

static int mel_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    mel_voice *v = (mel_voice *)vv; float sr = g->sr; mel_bank *bank = (mel_bank *)g->user;
    int tape = SK_SW(p, L_TAPE, 3), loop = SK_SW(p, L_LOOP, 2);
    float tlen = 1.0f + 7.0f * p[L_LENGTH];
    float att = 0.002f + 1.0f * p[L_ATT] * p[L_ATT], rel = 0.01f + 2.0f * p[L_REL] * p[L_REL];
    float ca = 1.0f / (att * sr), cr = 1.0f / (rel * sr);
    float wowd = p[L_WOW] * 0.012f, fld = p[L_FLUTTER] * 0.004f, drd = p[L_DRIFT] * 0.006f;
    float lpc = 1.0f - expf(-6.2831853f * sk_expmap(p[L_TONE], 1200.0f, 16000.0f) / sr);
    float hiss = p[L_HISS] * p[L_HISS] * 0.05f, vol = p[L_VOL] * 1.6f, velamt = p[L_VEL];
    float pitch = (p[L_PITCH] - 0.5f) * 4.0f;
    /* slot: nearest note that has a tape */
    mel_slot *sl = 0; int root = v->note;
    if (bank && bank->n > 0) {
        for (int d = 0; d < 128 && !sl; d++) {
            int a = v->note - d, b = v->note + d;
            if (a >= 0 && bank->s[a].d) { sl = &bank->s[a]; root = a; }
            else if (b < 128 && bank->s[b].d) { sl = &bank->s[b]; root = b; }
        }
    }
    for (int i = 0; i < n; i++) {
        if (v->gate) { v->env += ca; if (v->env > 1.0f) v->env = 1.0f; }
        else { v->env -= cr; if (v->env <= 0.0f) { v->env = 0.0f; v->active = 0; return 0; } }
        v->wowph += 0.55f / sr; if (v->wowph >= 1.0f) v->wowph -= 1.0f;
        v->flph += 6.8f / sr; if (v->flph >= 1.0f) v->flph -= 1.0f;
        if (--v->count <= 0 || v->count > 100000) { v->count = 4096; v->driftt = sk_rng_bi(&v->rng); }
        v->drift += (v->driftt - v->drift) * 0.0008f;
        float wm = 1.0f + wowd * sinf(6.2831853f * v->wowph) + fld * sinf(6.2831853f * v->flph) + drd * v->drift;
        float s = 0.0f;
        v->tape_t += 1.0f / sr;
        int tape_end = v->tape_t >= tlen;
        if (tape_end && loop) { v->tape_t -= tlen; v->pos = 0.0f; tape_end = 0; }
        if (!tape_end) {
            if (sl) {
                float rate = (sl->sr / sr) * exp2f(((float)v->note - (float)root + g->bend + pitch) / 12.0f) * wm;
                v->pos += rate;
                int ip = (int)v->pos;
                if (ip + 1 >= sl->len) { if (loop) { v->pos = 0.0f; ip = 0; } else ip = -1; }
                if (ip >= 0) { float fr = v->pos - (float)ip; s = sl->d[ip] * (1.0f - fr) + sl->d[ip + 1] * fr; }
            } else {
                float f = sk_mtof((float)v->note + g->bend + pitch) * wm, dt = f / sr;
                v->ph += dt; if (v->ph >= 1.0f) v->ph -= 1.0f;
                if (tape == 0) {                                                  /* flute */
                    float b = sk_rng_bi(&v->rng) * 0.25f * hiss * 20.0f;      /* breath noise follows the hiss control */
                    s = 0.55f * sinf(6.2831853f * v->ph) + 0.2f * sinf(12.566371f * v->ph) + 0.07f * sinf(18.849556f * v->ph) + b;
                } else if (tape == 1) {                                           /* strings */
                    v->ph2 += dt * 1.004f; if (v->ph2 >= 1.0f) v->ph2 -= 1.0f;
                    v->ph3 += dt * 0.996f; if (v->ph3 >= 1.0f) v->ph3 -= 1.0f;
                    s = 0.25f * (sk_saw(v->ph, dt) + sk_saw(v->ph2, dt) + sk_saw(v->ph3, dt));
                } else {                                                           /* choir: pulse through two formants */
                    float lo, bp, hp, lo2, bp2, hp2, x = sk_saw(v->ph, dt) * 0.6f;
                    sk_svf_set(&v->f1, 700.0f, 6.0f, sr); sk_svf_set(&v->f2, 1150.0f, 8.0f, sr);
                    sk_svf_tick(&v->f1, x, &lo, &bp, &hp); sk_svf_tick(&v->f2, x, &lo2, &bp2, &hp2);
                    s = (bp + 0.7f * bp2) * 0.9f;
                }
            }
        }
        v->lp += (s - v->lp) * lpc;
        float hs = sk_rng_bi(&v->rng) * hiss;
        v->hiss_lp += (hs - v->hiss_lp) * 0.3f;
        out[i] += (v->lp * 0.5f + v->hiss_lp) * v->env * (1.0f - velamt * (1.0f - v->vel)) * vol;
    }
    return v->active;
}

static const sk_def mel_def = {
    "mellotron", L_COUNT, mel_params, (int)(sizeof mel_presets / sizeof mel_presets[0]), mel_presets,
    16, 0, -1, sizeof(mel_voice), 0,
    mel_vinit, mel_von, mel_voff, mel_vrender, 0, 0,
    mel_user_new, mel_user_free, mel_cmd
};
#endif
