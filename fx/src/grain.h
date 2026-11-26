/*
 * grain.h - pitch-aware granulator. Header only, no Pd dependency.
 * A live buffer is read by overlapping grains. The pitch of the input is tracked (YIN style),
 * grains can be cut to whole periods (pitch synchronous), retuned toward the nearest scale
 * note, played backwards with a probability, and the read position can be scrubbed by data.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#ifndef GRAIN_H
#define GRAIN_H
#include "fxkit.h"

#define GR_MAXG 64
static const fx_param grain_params[] = {
    {"size",    5, 500, 80, FX_LOG, 0, 0},
    {"density", 1, 200, 20, FX_LOG, 0, 0},
    {"scrub",   0, 1,   0.1f, 0, 0, 0},
    {"spray",   0, 500, 20, 0, 0, 0},
    {"pitch",  -24, 24, 0, 0, 0, 0},
    {"tune",    0, 1,   0, 0, 0, 0},
    {"key",     0, 11,  0, 0, 12, "c|c#|d|d#|e|f|f#|g|g#|a|a#|b"},
    {"scale",   0, 6,   1, 0, 7, "chromatic|major|minor|pentatonic|whole|dorian|fifths"},
    {"scatter", 0, 12,  0, 0, 0, 0},
    {"reverse", 0, 1,   0, 0, 0, 0},
    {"sync",    0, 1,   0, 0, 2, "off|on"},
    {"shape",   0, 1,   0.2f, 0, 0, 0},
    {"spread",  0, 1,   0.5f, 0, 0, 0},
    {"jitter",  0, 1,   0.5f, 0, 0, 0},
    {"freeze",  0, 1,   0, FX_NOMUT, 2, "off|on"},
    {"mix",     0, 1,   0.8f, FX_NOMUT, 0, 0},
    {"gain",   -24, 12, 0, FX_NOMUT, 0, 0},
};
static const fx_preset grain_presets[] = {
    {"Default", ""},
    {"Cloud",         "size=90 density=30 scrub=0.15 spray=60 spread=0.8 mix=0.8"},
    {"Pitch snap",    "size=60 density=40 tune=1 scale=major sync=on spread=0.4 mix=1"},
    {"Frozen glass",  "size=140 density=24 scrub=0.02 spray=15 pitch=12 shape=0.6 freeze=on mix=1"},
    {"Reverse wash",  "size=200 density=14 scrub=0.4 spray=120 reverse=0.8 spread=1 mix=0.9"},
    {"Octave swarm",  "size=45 density=70 pitch=-12 scatter=0.3 spread=1 jitter=0.9 mix=0.8"},
    {"Choir sync",    "size=70 density=60 tune=1 scale=pentatonic sync=on scatter=0.15 spread=0.9 mix=1"},
    {"Dust",          "size=12 density=120 scrub=0.5 spray=400 pitch=7 scatter=3 spread=1 mix=0.7"},
    {"Slow scan",     "size=120 density=18 scrub=0.3 spray=5 shape=0.5 mix=1"},
};

typedef struct { int on, len, age, rev; double pos; float rate, pl, pr, amp, alpha; } gr_grain;
typedef struct {
    float sr;
    float *buf; int N, mask; long wpos;
    gr_grain g[GR_MAXG];
    double next;                       /* samples until next grain */
    unsigned rng;
    float dec_buf[512];
    int dec, hop;
    float f0, conf; int voiced; int hold;
    float since, lastsent; int sendflag; float last_rate;
} gr_state;

static float gr_rand(gr_state *s) { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; return (s->rng >> 8) * (1.0f / 16777216.0f); }

static void *gr_create(float sr, const fx_def *d)
{
    (void)d;
    gr_state *s = (gr_state *)calloc(1, sizeof *s);
    int n = 1024;
    if (!s) return 0;
    while (n < (int)(sr * 4.0f)) n <<= 1;
    s->buf = (float *)calloc((size_t)n, sizeof(float));
    if (!s->buf) { free(s); return 0; }
    s->sr = sr; s->N = n; s->mask = n - 1; s->rng = 88172645u; s->next = 0;
    s->dec = (int)(sr / 11025.0f + 0.5f); if (s->dec < 1) s->dec = 1; if (s->dec > 8) s->dec = 8;
    return s;
}
static void gr_destroy(void *u) { gr_state *s = (gr_state *)u; free(s->buf); free(s); }

/* YIN style pitch estimate from the last 512*dec samples */
static void gr_detect(gr_state *s)
{
    enum { M = 512, W = 256 };
    int dec = s->dec, lagmax = (int)(s->sr / dec / 70.0f), lagmin = (int)(s->sr / dec / 1200.0f);
    float d[260], cm[260];
    double e = 0;
    if (lagmax > M - W - 1) lagmax = M - W - 1;
    if (lagmin < 2) lagmin = 2;
    for (int j = 0; j < M; j++) {
        float a = 0;
        for (int k = 0; k < dec; k++) a += s->buf[(s->wpos - (long)(M - j) * dec + k) & s->mask];
        s->dec_buf[j] = a / dec;
    }
    for (int j = M - W; j < M; j++) e += (double)s->dec_buf[j] * s->dec_buf[j];
    if (sqrt(e / W) < 0.004) { s->conf = 0; if (++s->hold > 6) s->voiced = 0; return; }
    for (int t = 1; t <= lagmax; t++) {
        float acc = 0;
        for (int j = 0; j < W; j++) { float q = s->dec_buf[j] - s->dec_buf[j + t]; acc += q * q; }
        d[t] = acc;
    }
    float run = 0; cm[0] = 1;
    for (int t = 1; t <= lagmax; t++) { run += d[t]; cm[t] = run > 1e-12f ? d[t] * t / run : 1.0f; }
    int best = -1;
    for (int t = lagmin; t <= lagmax; t++) if (cm[t] < 0.15f) { best = t; while (best + 1 <= lagmax && cm[best + 1] < cm[best]) best++; break; }
    if (best < 0) {
        float m = 2; for (int t = lagmin; t <= lagmax; t++) if (cm[t] < m) { m = cm[t]; best = t; }
        if (m > 0.35f) { s->conf = 0; if (++s->hold > 6) s->voiced = 0; return; }
    }
    float tau = (float)best;
    if (best > 1 && best < lagmax) {
        float a = cm[best - 1], b = cm[best], c = cm[best + 1], den = a - 2 * b + c;
        if (fabsf(den) > 1e-9f) tau += 0.5f * (a - c) / den;
    }
    float f = s->sr / (tau * dec);
    s->conf = 1.0f - cm[best]; s->hold = 0;
    if (s->voiced && fabsf(f / s->f0 - 1) < 0.06f) s->f0 = 0.6f * s->f0 + 0.4f * f; else s->f0 = f;
    s->voiced = 1;
}

static const unsigned short gr_scales[7] = {0xFFF, 0xAB5, 0x5AD, 0x295, 0x555, 0x6AD, 0x081};

static float gr_snap(float midi, int key, int scale)
{
    unsigned m = gr_scales[scale];
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

static void gr_spawn(gr_state *s, const float *P)
{
    gr_grain *g = 0;
    for (int i = 0; i < GR_MAXG; i++) if (!s->g[i].on) { g = &s->g[i]; break; }
    if (!g) return;
    float sr = s->sr, size = P[0], dens = P[1];
    int len = (int)(size * 0.001f * sr);
    float period = (s->voiced && s->f0 > 20) ? sr / s->f0 : 0;
    if (P[10] >= 0.5f && period > 0) { int np = (int)floorf(len / period + 0.5f); if (np < 1) np = 1; len = (int)(np * period + 0.5f); }
    if (len < 16) len = 16;
    float semi = P[4];
    if (P[8] > 0) semi += (gr_rand(s) + gr_rand(s) + gr_rand(s) - 1.5f) * 2.0f * P[8];
    if (P[5] > 0 && s->voiced && s->conf > 0.5f) {
        float m0 = 69.0f + 12.0f * log2f(s->f0 / 440.0f);
        float q = gr_snap(m0, (int)P[6], (int)P[7]);
        semi += (q - m0) * P[5];
    }
    float rate = powf(2.0f, semi / 12.0f);
    s->last_rate = rate;
    float span = (float)s->N - 2.0f * (float)len * (rate > 1 ? rate : 1) - 4096.0f;
    float off = P[2] * (span > 0 ? span : 0);
    off += gr_rand(s) * P[3] * 0.001f * sr;
    if (P[10] >= 0.5f && period > 0) off = floorf(off / period + 0.5f) * period;
    float minoff = (float)len * (rate > 1 ? rate : 1) + 8.0f;
    if (off < minoff) off = minoff;
    if (off > (float)s->N - 8.0f - (float)len * (rate > 1 ? rate : 1)) off = (float)s->N - 8.0f - (float)len * (rate > 1 ? rate : 1);
    g->rev = gr_rand(s) < P[9];
    double start = (double)s->wpos - off;
    g->pos = g->rev ? start + (double)len * rate : start;
    g->rate = rate; g->len = len; g->age = 0; g->on = 1;
    float pan = (gr_rand(s) * 2 - 1) * P[12];
    float ang = (pan + 1) * 0.25f * FX_PI;
    float ovl = dens * size * 0.001f; if (ovl < 1) ovl = 1;
    g->amp = 1.0f / sqrtf(ovl);
    g->pl = cosf(ang) * g->amp; g->pr = sinf(ang) * g->amp;
    g->alpha = 1.0f - 0.9f * P[11];
}

static inline float gr_win(float t, float alpha)
{
    float h = alpha * 0.5f;
    if (t < h) return 0.5f * (1 - cosf(2 * FX_PI * t / alpha));
    if (t > 1 - h) return 0.5f * (1 - cosf(2 * FX_PI * (1 - t) / alpha));
    return 1.0f;
}

static void gr_process(void *u, const float *P, const float **in, float **out, int n)
{
    gr_state *s = (gr_state *)u;
    const float *x = in[0]; float *L = out[0], *R = out[1];
    float mix = P[15], gain = powf(10.0f, P[16] / 20.0f);
    float base = s->sr / P[1], jit = P[13];
    int frozen = P[14] >= 0.5f;
    for (int i = 0; i < n; i++) {
        float xin = x[i], wl = 0, wr = 0;
        if (!frozen) s->buf[s->wpos & s->mask] = xin;
        if (!frozen) s->wpos++;
        if (!frozen && ++s->hop >= 256) { s->hop = 0; gr_detect(s); }
        s->next -= 1.0;
        while (s->next <= 0) { gr_spawn(s, P); s->next += base * (1.0f + jit * 0.9f * (gr_rand(s) * 2 - 1)); }
        for (int k = 0; k < GR_MAXG; k++) {
            gr_grain *g = &s->g[k];
            if (!g->on) continue;
            double p = g->pos; long ip = (long)floor(p); float fr = (float)(p - ip);
            float y0 = s->buf[(ip - 1) & s->mask], y1 = s->buf[ip & s->mask], y2 = s->buf[(ip + 1) & s->mask], y3 = s->buf[(ip + 2) & s->mask];
            float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2 * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            float v = ((c3 * fr + c2) * fr + c1) * fr + y1;
            v *= gr_win((float)g->age / g->len, g->alpha);
            wl += v * g->pl; wr += v * g->pr;
            g->pos += g->rev ? -g->rate : g->rate;
            if (++g->age >= g->len) g->on = 0;
        }
        L[i] = xin * (1 - mix) + wl * gain * mix;
        R[i] = xin * (1 - mix) + wr * gain * mix;
    }
    s->since += (float)n;
}

static int gr_poll(void *u, int k, float *v, int *n, const char **sel)
{
    gr_state *s = (gr_state *)u;
    (void)k;
    if (s->since < s->sr * 0.05f) return 0;
    s->since = 0;
    v[0] = s->voiced ? s->f0 : 0; v[1] = s->conf < 0 ? 0 : s->conf; *n = 2; *sel = "pitch";
    return 1;
}

static const fx_def grain_def = {
    "grain~", (int)(sizeof grain_params / sizeof *grain_params), grain_params,
    (int)(sizeof grain_presets / sizeof *grain_presets), grain_presets,
    1, 2, 1, gr_create, gr_destroy, gr_process, 0, gr_poll
};
#endif
