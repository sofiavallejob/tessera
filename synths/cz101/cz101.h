/*
 * cz101.h - CZ-101 style phase distortion synth. Two lines per voice, each a sine whose phase is bent
 * by a distortion amount (DCW) into saw, square, pulse, double sine, saw-pulse and three resonant
 * waveforms. An envelope per line group for DCW and amplitude plus a pitch envelope, ring or noise
 * modulation between the lines, vibrato with delay. 8 voices. Original presets.
 * The distortion curves are in the spirit of phase distortion and are not copied from the hardware.
 */
#ifndef CZ101_H
#define CZ101_H
#include "synthcore.h"

enum { Z_WAVE1, Z_WAVE2, Z_DCW1, Z_DCW2, Z_DETUNE, Z_OCT2, Z_MOD, Z_MIX2, Z_DCWKBD,
       Z_WA, Z_WD, Z_WS, Z_WR, Z_AA, Z_AD, Z_AS, Z_AR, Z_PAMT, Z_PDEC, Z_VRATE, Z_VDEPTH, Z_VDELAY, Z_VEL, Z_VOL, Z_COUNT };

static const sk_param cz_params[Z_COUNT] = {
    {"wave1", 0, 8}, {"wave2", 0, 8}, {"dcw1", 0.5f, 0}, {"dcw2", 0.5f, 0}, {"detune", 0.5f, 0}, {"oct2", 1, 3}, {"mod", 0, 3}, {"mix2", 0.0f, 0}, {"dcwkbd", 0.3f, 0},
    {"dcwatt", 0.0f, 0}, {"dcwdec", 0.4f, 0}, {"dcwsus", 0.4f, 0}, {"dcwrel", 0.3f, 0},
    {"att", 0.0f, 0}, {"dec", 0.4f, 0}, {"sus", 0.8f, 0}, {"rel", 0.25f, 0},
    {"pitchamt", 0.5f, 0}, {"pitchdec", 0.2f, 0}, {"vibrate", 0.4f, 0}, {"vibdepth", 0.0f, 0}, {"vibdelay", 0.3f, 0}, {"vel", 0.4f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def cz_presets[] = {
    {"Saw Brass",      "wave1=0 dcw1=0.75 dcwatt=0.15 dcwdec=0.45 dcwsus=0.6 att=0.05 sus=0.9 wave2=0 dcw2=0.75 detune=0.55 mix2=0.7"},
    {"Pink Pluck",     "wave1=0 dcw1=0.9 dcwdec=0.3 dcwsus=0 dec=0.35 sus=0 rel=0.25 wave2=0 dcw2=0.9 mix2=0.6 detune=0.53"},
    {"Cosmic Bass",    "wave1=1 dcw1=0.8 dcwdec=0.3 dcwsus=0.1 dec=0.3 sus=0.6 oct2=0 wave2=1 dcw2=0.7 mix2=0.7 detune=0.5 rel=0.15"},
    {"Resonance Lead", "wave1=5 dcw1=0.5 dcwatt=0.1 dcwdec=0.5 dcwsus=0.7 sus=0.9 vibdepth=0.3 vibdelay=0.4 wave2=5 dcw2=0.45 mix2=0.5 detune=0.54"},
    {"Square Keys",    "wave1=1 dcw1=0.7 dcwdec=0.45 dcwsus=0.2 dec=0.5 sus=0.3 wave2=0 dcw2=0.5 mix2=0.4 oct2=2 rel=0.3"},
    {"Double Bell",    "wave1=3 dcw1=0.8 dcwdec=0.75 dcwsus=0.1 dec=0.8 sus=0 rel=0.7 wave2=3 dcw2=0.6 oct2=2 mix2=0.4 detune=0.505"},
    {"Glass Reso",     "wave1=6 dcw1=0.7 dcwatt=0.2 dcwdec=0.6 dcwsus=0.3 dec=0.7 sus=0.4 rel=0.6 wave2=7 dcw2=0.6 mix2=0.5 oct2=2"},
    {"String Pad",     "wave1=0 dcw1=0.5 att=0.55 sus=0.9 rel=0.7 dcwatt=0.5 dcwsus=0.7 wave2=0 dcw2=0.5 detune=0.56 mix2=0.8 vibdepth=0.15 vibrate=0.3 vibdelay=0.6"},
    {"Hollow Pipe",    "wave1=2 dcw1=0.6 att=0.05 sus=0.9 wave2=2 dcw2=0.4 oct2=2 mix2=0.3 vibdepth=0.2 vibdelay=0.5"},
    {"Zapper",         "wave1=4 dcw1=0.8 dcwdec=0.2 dcwsus=0 pitchamt=0.9 pitchdec=0.25 dec=0.3 sus=0.1 wave2=4 dcw2=0.6 mix2=0.5"},
    {"Wood Block",     "wave1=1 dcw1=0.9 dcwdec=0.1 dcwsus=0 dec=0.15 sus=0 rel=0.1 pitchamt=0.58 pitchdec=0.05"},
    {"Ring Mod Metal", "wave1=0 dcw1=0.4 wave2=0 dcw2=0.4 oct2=2 mod=1 mix2=0.8 detune=0.6 dec=0.6 sus=0.2 rel=0.4 dcwdec=0.5 dcwsus=0.3"},
    {"Noise Edge",     "wave1=0 dcw1=0.6 mod=2 mix2=0.6 dcwdec=0.3 dcwsus=0.2 dec=0.4 sus=0.5 wave2=1 dcw2=0.7"},
    {"Soft Sine",      "wave1=0 dcw1=0.05 dcwsus=0.05 att=0.03 sus=0.9 wave2=0 dcw2=0.1 mix2=0.3 oct2=2"},
};

typedef struct { sk_env we, ae; sk_rng rng; float p1, p2, lph, ph_t, penv, vel, delayt; int note; } cz_voice;

static void cz_vinit(void *vv, float sr) { cz_voice *v = (cz_voice *)vv; (void)sr; memset(v, 0, sizeof *v); sk_env_init(&v->we); sk_env_init(&v->ae); sk_rng_seed(&v->rng, 101u); }
static void cz_von(void *vv, const float *p, int note, float vel, int first, float sr)
{
    cz_voice *v = (cz_voice *)vv; (void)p; (void)first; (void)sr;
    v->note = note; v->vel = vel; v->p1 = v->p2 = 0.0f; v->lph = 0.0f; v->penv = 1.0f; v->delayt = 0.0f;
    sk_env_on(&v->we, 0); sk_env_on(&v->ae, 0);
}
static void cz_voff(void *vv, const float *p) { cz_voice *v = (cz_voice *)vv; (void)p; sk_env_off(&v->we); sk_env_off(&v->ae); }

/* phase distortion: ph 0..1, d 0..1. Returns -1..1 */
static inline float cz_wave(int w, float ph, float d)
{
    const float TP = 6.2831853f;
    float k, t;
    switch (w) {
    case 0: k = 0.5f - 0.49f * d;                                                   /* saw */
        t = ph < k ? ph * (0.5f / k) : 0.5f + (ph - k) * (0.5f / (1.0f - k));
        return cosf(TP * t);
    case 1: k = 0.49f * d;                                                           /* square */
        t = ph < k ? 0.0f : ph < 0.5f ? (ph - k) / (0.5f - k) * 0.5f : ph < 0.5f + k ? 0.5f : 0.5f + (ph - 0.5f - k) / (0.5f - k) * 0.5f;
        return cosf(TP * t);
    case 2: k = 0.49f * d;                                                           /* pulse: short high part */
        { float a = k * 0.5f, hi = 0.5f - 0.45f * d;
          t = ph < a ? 0.0f : ph < hi ? (ph - a) / (hi - a) * 0.5f : ph < hi + a ? 0.5f : 0.5f + (ph - hi - a) / (1.0f - hi - a) * 0.5f;
          return cosf(TP * t); }
    case 3: return cosf(TP * ph) * (1.0f - d) + cosf(2.0f * TP * ph) * d;           /* double sine */
    case 4: return 0.5f * (cz_wave(0, ph, d) + cz_wave(1, ph, d));                   /* saw-pulse */
    case 5: return cosf(TP * ph * (1.0f + d * 7.0f)) * (1.0f - ph);                  /* resonance I: saw window */
    case 6: return cosf(TP * ph * (1.0f + d * 7.0f)) * (1.0f - fabsf(2.0f * ph - 1.0f)); /* resonance II: triangle window */
    default: return cosf(TP * ph * (1.0f + d * 7.0f)) * (ph < 0.5f ? 1.0f : 2.0f - 2.0f * ph);  /* resonance III: trapezoid */
    }
}

static int cz_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    cz_voice *v = (cz_voice *)vv; float sr = g->sr;
    int w1 = SK_SW(p, Z_WAVE1, 8), w2 = SK_SW(p, Z_WAVE2, 8), oct2 = SK_SW(p, Z_OCT2, 3), mod = SK_SW(p, Z_MOD, 3);
    sk_env_set(&v->we, 0.0015f * powf(2000.0f, p[Z_WA]), 0.004f * powf(1500.0f, p[Z_WD]), p[Z_WS], 0.004f * powf(1500.0f, p[Z_WR]), sr);
    sk_env_set(&v->ae, 0.0015f * powf(2000.0f, p[Z_AA]), 0.004f * powf(1500.0f, p[Z_AD]), p[Z_AS], 0.004f * powf(1500.0f, p[Z_AR]), sr);
    float kb = sk_clampf(1.0f - p[Z_DCWKBD] * 0.8f * ((float)(v->note - 60) / 60.0f), 0.1f, 1.5f);
    float pamt = (p[Z_PAMT] - 0.5f) * 24.0f, pdec = expf(-1.0f / (sr * (0.01f + 1.5f * p[Z_PDEC] * p[Z_PDEC])));
    float det = (p[Z_DETUNE] - 0.5f) * 1.0f, o2 = oct2 == 0 ? -12.0f : oct2 == 2 ? 12.0f : 0.0f;
    float vr = sk_expmap(p[Z_VRATE], 0.5f, 12.0f), vd = p[Z_VDEPTH] * p[Z_VDEPTH] * 0.6f * (1.0f + 2.0f * g->mod), vdel = 0.02f + 2.0f * p[Z_VDELAY] * p[Z_VDELAY];
    float vol = p[Z_VOL] * 1.5f, vs = 1.0f - p[Z_VEL] * (1.0f - v->vel), mix2 = p[Z_MIX2];
    for (int i = 0; i < n; i++) {
        float we = sk_env_tick(&v->we), ae = sk_env_tick(&v->ae);
        v->penv *= pdec; v->delayt += 1.0f / sr;
        v->lph += vr / sr; if (v->lph >= 1.0f) v->lph -= 1.0f;
        float vib = sinf(6.2831853f * v->lph) * vd * sk_clampf(v->delayt / vdel, 0.0f, 1.0f);
        float semi = (float)v->note + g->bend + vib + pamt * v->penv;
        float f1 = sk_mtof(semi), f2 = sk_mtof(semi + o2 + det);
        float d1 = sk_clampf(f1 / sr, 0.0f, 0.45f), d2 = sk_clampf(f2 / sr, 0.0f, 0.45f);
        float dc1 = sk_clampf(p[Z_DCW1] * kb * (0.1f + 0.9f * we), 0.0f, 1.0f), dc2 = sk_clampf(p[Z_DCW2] * kb * (0.1f + 0.9f * we), 0.0f, 1.0f);
        float a = 0.0f, b = 0.0f;
        for (int os = 0; os < 2; os++) {                              /* 2x oversampling: two phase steps averaged */
            v->p1 += d1 * 0.5f; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
            v->p2 += d2 * 0.5f; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
            a += cz_wave(w1, v->p1, dc1) * 0.5f; b += cz_wave(w2, v->p2, dc2) * 0.5f;
        }
        float s;
        if (mod == 1) s = a * b * (0.4f + 0.6f * mix2) * 1.4f;      /* ring: line 1 times line 2 */
        else if (mod == 2) s = a + b * mix2 + sk_rng_bi(&v->rng) * mix2 * 0.3f;
        else s = a + b * mix2;
        out[i] += s * 0.35f * ae * vs * vol;
    }
    return v->ae.st != SK_ENV_IDLE;
}

static const sk_def cz_def = {
    "cz101", Z_COUNT, cz_params, (int)(sizeof cz_presets / sizeof cz_presets[0]), cz_presets,
    8, 0, -1, sizeof(cz_voice), 0,
    cz_vinit, cz_von, cz_voff, cz_vrender, 0, 0
};
#endif
