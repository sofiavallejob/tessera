/*
 * modal.h - modal resonator bank. Header only, no Pd dependency.
 * Up to 32 two-pole resonators. Mode ratios come from a material (glass, wood, bell, plate,
 * bar, string, tube, bowl) or are learned from a sound; modes can be snapped to a scale.
 * Material ratios are approximations of the physical series, chosen for sound.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#ifndef MODAL_H
#define MODAL_H
#include "fxkit.h"

#define MD_MAXM 32
#define MD_LEARN_MAX 16384

static const fx_param modal_params[] = {
    {"root",    24, 96, 48, 0, 0, 0},
    {"material", 0, 8,  0, 0, 9, "glass|wood|bell|plate|bar|string|tube|bowl|learned"},
    {"modes",    1, 32, 16, FX_INT, 0, 0},
    {"scale",    0, 6,  0, 0, 7, "free|major|minor|pentatonic|whole|dorian|fifths"},
    {"key",      0, 11, 0, 0, 12, "c|c#|d|d#|e|f|f#|g|g#|a|a#|b"},
    {"snap",     0, 1,  0, 0, 0, 0},
    {"decay",    0.05f, 12, 2, FX_LOG, 0, 0},
    {"damp",     0, 1,  0.4f, 0, 0, 0},
    {"stretch", -0.4f, 0.4f, 0, 0, 0, 0},
    {"bright",   0, 1,  0.5f, 0, 0, 0},
    {"spread",   0, 1,  0.6f, 0, 0, 0},
    {"drive",   -12, 36, 12, 0, 0, 0},
    {"mix",      0, 1,  0.7f, FX_NOMUT, 0, 0},
    {"level",   -24, 12, 0, FX_NOMUT, 0, 0},
};
static const fx_preset modal_presets[] = {
    {"Default", ""},
    {"Glass bowl",     "root=60 material=glass modes=12 decay=4 damp=0.3 bright=0.6 spread=0.7 mix=0.8"},
    {"Marimba",        "root=48 material=wood modes=8 decay=0.4 damp=0.7 bright=0.4 mix=0.8"},
    {"Temple bell",    "root=55 material=bell modes=14 decay=7 damp=0.3 bright=0.5 spread=0.8 mix=0.8"},
    {"Steel plate",    "root=50 material=plate modes=20 decay=3 damp=0.2 bright=0.8 spread=1 mix=0.7"},
    {"Chime bar",      "root=72 material=bar modes=6 decay=5 damp=0.3 bright=0.5 mix=0.8"},
    {"Pentatonic haze","root=48 material=string modes=24 scale=pentatonic snap=1 decay=3 damp=0.3 bright=0.4 spread=1 mix=0.9"},
    {"Organ pipe",     "root=36 material=tube modes=16 decay=1.5 damp=0.1 bright=0.5 mix=0.8"},
    {"Singing bowl",   "root=57 material=bowl modes=10 decay=10 damp=0.2 bright=0.5 spread=0.9 mix=0.8"},
};

typedef struct { float z1, z2, a1, a2, g, pl, pr, amp; } md_mode;
typedef struct {
    float sr;
    md_mode m[MD_MAXM];
    float lrat[MD_MAXM], lamp[MD_MAXM]; int nlearn;
    float *cap; int caplen, capn, capping;
    float strike_vel; unsigned rng;
    float hp_z;
    float cur_f[MD_MAXM];
    int nactive;
    float lastroot, lastinfo; int info_flag;
} md_state;

static const float md_bell[12] = {0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f, 8.215f};
static const float md_plate[12] = {1.0f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.50f, 4.06f, 4.15f, 4.59f, 5.15f, 5.54f};
static const float md_glass[8] = {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 16.0f, 19.9f};
static const float md_bowl[8] = {1.0f, 2.71f, 5.15f, 8.17f, 11.8f, 15.9f, 20.5f, 25.6f};

static float md_ratio(int mat, int k, md_state *s)
{
    switch (mat) {
    case 0: return k < 8 ? md_glass[k] : md_glass[7] * powf(1.18f, (float)(k - 7));
    case 1: return powf((float)(k + 1), 1.95f);
    case 2: return k < 12 ? md_bell[k] : md_bell[11] * powf(1.13f, (float)(k - 11));
    case 3: return k < 12 ? md_plate[k] : md_plate[11] * powf(1.11f, (float)(k - 11));
    case 4: { float a = 2.0f * k + 3.0f; return k == 0 ? 1.0f : a * a / 9.0f * (k == 0 ? 1 : 1.0f); }
    case 5: return (float)(k + 1);
    case 6: return (float)(2 * k + 1);
    case 7: return k < 8 ? md_bowl[k] : md_bowl[7] * powf(1.15f, (float)(k - 7));
    default: return k < s->nlearn ? s->lrat[k] : 0;
    }
}
static const float md_decay_mul[9] = {1.5f, 0.15f, 2.0f, 1.0f, 0.5f, 1.0f, 0.8f, 2.5f, 1.0f};
static const unsigned short md_scales[7] = {0xFFF, 0xAB5, 0x5AD, 0x295, 0x555, 0x6AD, 0x081};

static float md_snap(float midi, int key, int scale)
{
    unsigned m = md_scales[scale];
    float best = midi, bd = 1e9f;
    int base = (int)floorf(midi) - 6;
    for (int n = base; n <= base + 13; n++) {
        int pc = ((n - key) % 12 + 12) % 12;
        if (!(m & (1u << pc))) continue;
        float dd = fabsf(n - midi);
        if (dd < bd) { bd = dd; best = (float)n; }
    }
    return best;
}

static void *md_create(float sr, const fx_def *d)
{
    (void)d;
    md_state *s = (md_state *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->sr = sr; s->rng = 1234567u;
    s->caplen = (int)(sr * 1.0f); if (s->caplen < 8192) s->caplen = 8192;
    s->cap = (float *)calloc((size_t)s->caplen, sizeof(float));
    if (!s->cap) { free(s); return 0; }
    return s;
}
static void md_destroy(void *u) { md_state *s = (md_state *)u; free(s->cap); free(s); }

static void md_update(md_state *s, const float *P)
{
    int mat = (int)P[1], nm = (int)P[2], scale = (int)P[3], key = (int)P[4];
    float snap = P[5], decay = P[6], damp = P[7], stretch = P[8], bright = P[9], spread = P[10];
    float f0 = 440.0f * powf(2.0f, (P[0] - 69.0f) / 12.0f);
    int n = 0;
    float tot = 0, amps[MD_MAXM];
    for (int k = 0; k < nm && k < MD_MAXM; k++) {
        float r = md_ratio(mat, k, s);
        if (r <= 0) break;
        if (stretch != 0) r = powf(r, 1.0f + stretch * 0.5f);
        float f = f0 * r;
        if (f > s->sr * 0.45f) break;
        if (snap > 0 && scale > 0) {
            float m0 = 69.0f + 12.0f * log2f(f / 440.0f), q = md_snap(m0, key, scale);
            f = 440.0f * powf(2.0f, (m0 + (q - m0) * snap - 69.0f) / 12.0f);
        }
        float t60 = decay * md_decay_mul[mat] / (1.0f + damp * damp * 6.0f * (float)k);
        if (t60 < 0.005f) t60 = 0.005f;
        float rr = expf(-6.9078f / (t60 * s->sr));
        float w = 2 * FX_PI * f / s->sr;
        md_mode *m = &s->m[n];
        m->a1 = 2 * rr * cosf(w); m->a2 = -rr * rr;
        float a = mat == 8 && k < s->nlearn ? s->lamp[k] : 1.0f / powf((float)(k + 1), 1.2f * (1.0f - bright) + 0.1f);
        amps[n] = a * 2.0f * (1.0f - rr) * sinf(w);
        m->amp = a;
        float pan = ((k * 0.618034f) - floorf(k * 0.618034f)) * 2 - 1;
        pan *= spread;
        float ang = (pan + 1) * 0.25f * FX_PI;
        m->pl = cosf(ang); m->pr = sinf(ang);
        s->cur_f[n] = f;
        tot += amps[n] * amps[n];
        n++;
    }
    (void)tot;
    for (int k = 0; k < n; k++) s->m[k].g = amps[k];
    for (int k = n; k < MD_MAXM; k++) { s->m[k].z1 = s->m[k].z2 = 0; }
    s->nactive = n;
}

/* analyse the captured sound: averaged power spectrum, strongest peaks become modes */
static void md_fft(float *re, float *im, int n)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2 * FX_PI / len, wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = i + k + len / 2;
                float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}
static int md_learn_analyse(md_state *s)
{
    enum { N = 8192 };
    static float re[N], im[N], pw[N / 2];
    int frames = 0;
    memset(pw, 0, sizeof pw);
    for (int start = 0; start + N <= s->capn; start += N / 2) {
        for (int i = 0; i < N; i++) { float w = 0.5f - 0.5f * cosf(2 * FX_PI * i / N); re[i] = s->cap[start + i] * w; im[i] = 0; }
        md_fft(re, im, N);
        for (int i = 0; i < N / 2; i++) pw[i] += re[i] * re[i] + im[i] * im[i];
        frames++;
    }
    if (!frames) return 0;
    float binhz = s->sr / N, maxp = 0;
    int lo = (int)(40.0f / binhz) + 1, hi = (int)(s->sr * 0.45f / binhz);
    for (int i = lo; i < hi; i++) if (pw[i] > maxp) maxp = pw[i];
    if (maxp <= 0) return 0;
    float pf[MD_MAXM], pa[MD_MAXM]; int np = 0;
    for (int pass = 0; pass < MD_MAXM; pass++) {                     /* pick the strongest remaining peak, then blank its neighbourhood */
        int bi = -1; float bv = maxp * 1e-5f;
        for (int i = lo + 1; i < hi - 1; i++) if (pw[i] > bv && pw[i] >= pw[i - 1] && pw[i] >= pw[i + 1]) { bv = pw[i]; bi = i; }
        if (bi < 0) break;
        float a = logf(pw[bi - 1] + 1e-30f), b = logf(pw[bi] + 1e-30f), c = logf(pw[bi + 1] + 1e-30f), den = a - 2 * b + c;
        float off = fabsf(den) > 1e-9f ? 0.5f * (a - c) / den : 0;
        pf[np] = (bi + off) * binhz; pa[np] = sqrtf(bv / maxp); np++;
        for (int i = bi - 6; i <= bi + 6; i++) if (i >= 0 && i < N / 2) pw[i] = 0;
    }
    if (!np) return 0;
    for (int i = 1; i < np; i++) for (int j = i; j > 0 && pf[j] < pf[j - 1]; j--) { float t = pf[j]; pf[j] = pf[j - 1]; pf[j - 1] = t; t = pa[j]; pa[j] = pa[j - 1]; pa[j - 1] = t; }
    int ref = 0;
    for (int i = 1; i < np; i++) if (pa[i] > pa[ref] * 1.5f) { }       /* reference = lowest of the strong peaks */
    for (int i = 0; i < np; i++) if (pa[i] > 0.3f) { ref = i; break; }
    for (int i = 0; i < np; i++) { s->lrat[i] = pf[i] / pf[ref]; s->lamp[i] = pa[i]; }
    s->nlearn = np;
    return np;
}

static void md_process(void *u, const float *P, const float **in, float **out, int n)
{
    md_state *s = (md_state *)u;
    const float *x = in[0]; float *L = out[0], *R = out[1];
    float mix = P[12], lvl = powf(10.0f, P[13] / 20.0f), drive = powf(10.0f, P[11] / 20.0f);
    md_update(s, P);
    if (s->strike_vel > 0) {                                     /* a strike sets every mode ringing at its own weight */
        for (int k = 0; k < s->nactive; k++) { md_mode *m = &s->m[k]; float A = s->strike_vel * m->amp * 0.5f; m->z1 += A; m->z2 += A * m->a1 / (2.0f * (-m->a2) + 1e-9f); }
        s->strike_vel = 0;
    }
    if (s->capping) {
        for (int i = 0; i < n && s->capn < s->caplen; i++) s->cap[s->capn++] = x[i];
        if (s->capn >= s->caplen) { s->capping = 0; s->info_flag = md_learn_analyse(s) + 1; }
    }
    for (int i = 0; i < n; i++) {
        float xin = x[i] * drive;
        float wl = 0, wr = 0;
        for (int k = 0; k < s->nactive; k++) {
            md_mode *m = &s->m[k];
            float y = m->a1 * m->z1 + m->a2 * m->z2 + m->g * xin;
            m->z2 = m->z1; m->z1 = y;
            wl += y * m->pl; wr += y * m->pr;
        }
        L[i] = x[i] * (1 - mix) + wl * lvl * mix;
        R[i] = x[i] * (1 - mix) + wr * lvl * mix;
    }
}

static int md_cmd(void *u, const char *name, const char *arg, const char *dir)
{
    md_state *s = (md_state *)u;
    (void)dir;
    if (!strcmp(name, "learn")) {
        s->capn = 0; s->capping = 1; s->info_flag = 0;
        (void)arg;
        return 0;
    }
    if (!strcmp(name, "strike")) { float v = (float)atof(arg); s->strike_vel = fx_clamp(v > 0 ? v : 1.0f, 0, 2); return 0; }
    return 1;
}
static int md_poll(void *u, int k, float *v, int *n, const char **sel)
{
    md_state *s = (md_state *)u;
    (void)k;
    if (!s->info_flag) return 0;
    v[0] = (float)(s->info_flag - 1); *n = 1; *sel = "learned";
    s->info_flag = 0;
    return 1;
}

static const fx_def modal_def = {
    "modal~", (int)(sizeof modal_params / sizeof *modal_params), modal_params,
    (int)(sizeof modal_presets / sizeof *modal_presets), modal_presets,
    1, 2, 1, md_create, md_destroy, md_process, md_cmd, md_poll
};
#endif
