/*
 * synthkit.h - shared DSP primitives for the Pd synth collection.
 * Header-only, no Pd dependency, so every piece can be tested offline.
 *
 *   sk_blep / sk_osc : PolyBLEP saw and pulse
 *   sk_env           : exponential ADSR
 *   sk_ladder        : zero-delay-feedback 4-pole low-pass with tanh drive
 *   sk_svf           : TPT state variable filter (high-pass, band-pass, low-pass)
 *   sk_lfo           : triangle / square / sample-and-hold
 *   sk_chorus        : two-voice stereo chorus with adjustable rate
 *   sk_rng           : tiny xorshift generator
 */
#ifndef SYNTHKIT_H
#define SYNTHKIT_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef SK_PI
#define SK_PI 3.14159265358979323846f
#endif

static inline float sk_clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float sk_mtof(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }
static inline float sk_lerp(float a, float b, float t) { return a + (b - a) * t; }
/* map 0..1 onto lo..hi exponentially */
static inline float sk_expmap(float x, float lo, float hi) { return lo * powf(hi / lo, sk_clampf(x, 0.0f, 1.0f)); }
/* fast bounded soft clip, close to tanh */
static inline float sk_tanh(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float sk_flush(float x) { return (x > -1e-20f && x < 1e-20f) ? 0.0f : x; }

/* ---------------- random ---------------- */
typedef struct { uint32_t s; } sk_rng;
static inline void sk_rng_seed(sk_rng *r, uint32_t s) { r->s = s ? s : 0x9e3779b9u; }
static inline uint32_t sk_rng_u32(sk_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return r->s = x;
}
/* -1..1 */
static inline float sk_rng_bi(sk_rng *r) { return (float)(int32_t)sk_rng_u32(r) * (1.0f / 2147483648.0f); }
/* 0..1 */
static inline float sk_rng_uni(sk_rng *r) { return (float)(sk_rng_u32(r) >> 8) * (1.0f / 16777216.0f); }

/* ---------------- PolyBLEP oscillator ---------------- */
static inline float sk_blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float sk_saw(float ph, float dt) { return 2.0f * ph - 1.0f - sk_blep(ph, dt); }
static inline float sk_pulse(float ph, float dt, float pw)
{
    float v = ph < pw ? 1.0f : -1.0f;
    float ph2 = ph - pw; if (ph2 < 0.0f) ph2 += 1.0f;
    return v + sk_blep(ph, dt) - sk_blep(ph2, dt);
}
static inline float sk_wrap1(float ph) { return ph - floorf(ph); }

/* ---------------- ADSR ---------------- */
enum { SK_ENV_IDLE, SK_ENV_ATT, SK_ENV_DEC, SK_ENV_SUS, SK_ENV_REL };
typedef struct { float lvl; int st; float a, d, s, r; } sk_env;  /* a d r = per-sample coefficients */

static inline float sk_coef(float seconds, float sr)
{
    if (seconds < 1e-4f) seconds = 1e-4f;
    return 1.0f - expf(-6.9f / (seconds * sr));  /* reaches 99.9% in `seconds` */
}
static inline void sk_env_init(sk_env *e) { e->lvl = 0.0f; e->st = SK_ENV_IDLE; e->a = e->d = e->r = 0.01f; e->s = 0.5f; }
static inline void sk_env_set(sk_env *e, float att, float dec, float sus, float rel, float sr)
{
    e->a = sk_coef(att, sr); e->d = sk_coef(dec, sr); e->s = sk_clampf(sus, 0.0f, 1.0f); e->r = sk_coef(rel, sr);
}
static inline void sk_env_on(sk_env *e, int retrig_from_zero)
{
    if (retrig_from_zero) e->lvl = 0.0f;
    e->st = SK_ENV_ATT;
}
static inline void sk_env_off(sk_env *e) { if (e->st != SK_ENV_IDLE) e->st = SK_ENV_REL; }
static inline float sk_env_tick(sk_env *e)
{
    switch (e->st) {
    case SK_ENV_ATT:
        e->lvl += (1.25f - e->lvl) * e->a;
        if (e->lvl >= 1.0f) { e->lvl = 1.0f; e->st = SK_ENV_DEC; }
        break;
    case SK_ENV_DEC:
        e->lvl += (e->s - 0.002f - e->lvl) * e->d;
        if (e->lvl <= e->s + 1e-4f) { e->lvl = e->s; e->st = SK_ENV_SUS; }
        break;
    case SK_ENV_SUS: e->lvl = e->s; break;
    case SK_ENV_REL:
        e->lvl += (-0.002f - e->lvl) * e->r;
        if (e->lvl <= 1e-4f) { e->lvl = 0.0f; e->st = SK_ENV_IDLE; }
        break;
    default: e->lvl = 0.0f; break;
    }
    return e->lvl;
}

/* ---------------- 4-pole ladder (ZDF) ---------------- */
typedef struct { float s[4]; float g, G, k, gain; } sk_ladder;
static inline void sk_ladder_init(sk_ladder *f) { memset(f, 0, sizeof *f); f->G = 0.1f; }
/* fc in Hz, res 0..1 (1 = self-oscillation if selfosc, else stays just below) */
static inline void sk_ladder_set(sk_ladder *f, float fc, float res, float sr, int selfosc)
{
    fc = sk_clampf(fc, 10.0f, sr * 0.45f);
    f->g = tanf(SK_PI * fc / sr);
    f->G = f->g / (1.0f + f->g);
    f->k = res * (selfosc ? 4.5f : 3.4f);
    float G2 = f->G * f->G;
    f->gain = 1.0f / (1.0f + f->k * G2 * G2);
}
static inline float sk_ladder_tick(sk_ladder *f, float x, float drive)
{
    float G = f->G, ig = 1.0f - G;
    float S = G * G * G * f->s[0] * ig + G * G * f->s[1] * ig + G * f->s[2] * ig + f->s[3] * ig;
    float u = (x - f->k * S) * f->gain;
    u = sk_tanh(u * drive) / (drive > 0.01f ? drive : 1.0f);
    float y = u;
    for (int i = 0; i < 4; i++) {
        float v = (y - f->s[i]) * G;
        y = v + f->s[i];
        f->s[i] = sk_flush(y + v);
    }
    return y;
}

/* ---------------- TPT state variable filter ---------------- */
typedef struct { float s1, s2, g, R, h; } sk_svf;
static inline void sk_svf_init(sk_svf *f) { memset(f, 0, sizeof *f); f->R = 1.0f; }
static inline void sk_svf_set(sk_svf *f, float fc, float q, float sr)
{
    fc = sk_clampf(fc, 5.0f, sr * 0.45f);
    f->g = tanf(SK_PI * fc / sr);
    f->R = 1.0f / (2.0f * (q < 0.5f ? 0.5f : q));
    f->h = 1.0f / (1.0f + 2.0f * f->R * f->g + f->g * f->g);
}
/* out[0]=low out[1]=band out[2]=high */
static inline void sk_svf_tick(sk_svf *f, float x, float *lo, float *bp, float *hp)
{
    float h = (x - (2.0f * f->R + f->g) * f->s1 - f->s2) * f->h;
    float b = f->g * h + f->s1;
    f->s1 = sk_flush(f->g * h + b);
    float l = f->g * b + f->s2;
    f->s2 = sk_flush(f->g * b + l);
    *lo = l; *bp = b; *hp = h;
}

/* ---------------- LFO ---------------- */
enum { SK_LFO_TRI, SK_LFO_SQR, SK_LFO_SH };
typedef struct { float ph; float held; sk_rng r; } sk_lfo;
static inline void sk_lfo_init(sk_lfo *l, uint32_t seed) { l->ph = 0.0f; l->held = 0.0f; sk_rng_seed(&l->r, seed); }
static inline float sk_lfo_tick(sk_lfo *l, int wave, float hz, float sr)
{
    l->ph += hz / sr;
    if (l->ph >= 1.0f) { l->ph -= 1.0f; l->held = sk_rng_bi(&l->r); }
    switch (wave) {
    case SK_LFO_SQR: return l->ph < 0.5f ? 1.0f : -1.0f;
    case SK_LFO_SH: return l->held;
    default: return l->ph < 0.5f ? 4.0f * l->ph - 1.0f : 3.0f - 4.0f * l->ph;
    }
}

/* ---------------- stereo chorus ---------------- */
#define SK_CH_LEN 2048
typedef struct { float buf[SK_CH_LEN]; int w; float ph; } sk_chorus;
static inline void sk_chorus_init(sk_chorus *c) { memset(c, 0, sizeof *c); }
/* mix 0..1, rate Hz, depth 0..1 */
static inline void sk_chorus_run(sk_chorus *c, const float *in, float *L, float *R, int n, float sr,
                                 float rate, float depth, float mix)
{
    float base = 0.0065f * sr, dev = 0.0025f * sr * depth;
    for (int i = 0; i < n; i++) {
        c->buf[c->w] = in[i];
        c->ph += rate / sr; if (c->ph >= 1.0f) c->ph -= 1.0f;
        float m = c->ph < 0.5f ? 4.0f * c->ph - 1.0f : 3.0f - 4.0f * c->ph;  /* triangle */
        float dl = base + dev * m, dr = base - dev * m;
        float out2[2];
        for (int ch = 0; ch < 2; ch++) {
            float d = ch ? dr : dl;
            float rp = (float)c->w - d; if (rp < 0.0f) rp += SK_CH_LEN;
            int i0 = (int)rp; float fr = rp - (float)i0; int i1 = (i0 + 1) % SK_CH_LEN;
            out2[ch] = c->buf[i0] * (1.0f - fr) + c->buf[i1] * fr;
        }
        L[i] = in[i] * (1.0f - 0.5f * mix) + out2[0] * mix;
        R[i] = in[i] * (1.0f - 0.5f * mix) + out2[1] * mix;
        c->w = (c->w + 1) % SK_CH_LEN;
    }
}

#endif
