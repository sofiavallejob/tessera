/*
 * minimoog.h - Minimoog Model D style monosynth: three oscillators (six waveforms, six ranges,
 * osc 2 and 3 tuning, osc 3 keyboard control and low mode, hard sync), mixer with white or pink
 * noise, 4-pole ladder with emphasis and contour, filter and loudness ADS envelopes with a
 * decay switch, glide, legato, LFO, overload drive. Original presets.
 */
#ifndef MINIMOOG_H
#define MINIMOOG_H
#include "synthcore.h"

enum { M_R1, M_W1, M_R2, M_W2, M_T2, M_R3, M_W3, M_T3, M_K3, M_SYNC, M_MIX1, M_MIX2, M_MIX3, M_NOISE, M_NTYPE,
       M_CUTOFF, M_RES, M_ENVAMT, M_KBD, M_FA, M_FD, M_FS, M_AA, M_AD, M_AS, M_DECAY, M_GLIDE, M_LEGATO,
       M_LFORATE, M_LFOWAVE, M_LFOPITCH, M_LFOFILT, M_VEL, M_DRIVE, M_VOL, M_COUNT };

static const sk_param mm_params[M_COUNT] = {
    {"range1", 3, 6}, {"wave1", 2, 6}, {"range2", 3, 6}, {"wave2", 2, 6}, {"tune2", 0.5f, 0},
    {"range3", 3, 6}, {"wave3", 2, 6}, {"tune3", 0.5f, 0}, {"kbd3", 1, 2}, {"sync", 0, 2},
    {"mix1", 0.8f, 0}, {"mix2", 0.0f, 0}, {"mix3", 0.0f, 0}, {"noise", 0.0f, 0}, {"noisetype", 0, 2},
    {"cutoff", 0.5f, 0}, {"res", 0.2f, 0}, {"envamt", 0.3f, 0}, {"kbd", 1, 3},
    {"fatt", 0.0f, 0}, {"fdec", 0.4f, 0}, {"fsus", 0.3f, 0},
    {"aatt", 0.0f, 0}, {"adec", 0.4f, 0}, {"asus", 0.8f, 0}, {"decay", 1, 2},
    {"glide", 0.0f, 0}, {"legato", 0, 2},
    {"lforate", 0.3f, 0}, {"lfowave", 0, 2}, {"lfopitch", 0.0f, 0}, {"lfofilt", 0.0f, 0},
    {"vel", 0.3f, 0}, {"drive", 0.3f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def mm_presets[] = {
    {"Fat Bass",       "range1=2 wave1=2 range2=2 wave2=2 tune2=0.52 range3=1 wave3=0 mix1=0.8 mix2=0.7 mix3=0.6 cutoff=0.25 res=0.3 envamt=0.5 fdec=0.3 fsus=0.1 adec=0.3 asus=0.7 drive=0.5"},
    {"Classic Lead",   "range1=3 wave1=2 range2=3 wave2=2 tune2=0.53 mix1=0.8 mix2=0.7 cutoff=0.55 res=0.25 envamt=0.3 fdec=0.4 fsus=0.5 asus=0.9 glide=0.2 legato=1 lfopitch=0.08 lforate=0.4"},
    {"Sync Screamer",  "range1=3 wave1=2 range2=4 wave2=2 tune2=0.7 sync=1 mix1=0.0 mix2=0.9 cutoff=0.6 res=0.35 envamt=0.4 fdec=0.35 fsus=0.3 asus=0.85 glide=0.15 legato=1"},
    {"Pulse Bass",     "range1=2 wave1=4 range2=2 wave2=5 tune2=0.5 mix1=0.8 mix2=0.5 cutoff=0.3 res=0.4 envamt=0.55 fdec=0.25 fsus=0 adec=0.25 asus=0.5 drive=0.4"},
    {"Hollow Flute",   "range1=3 wave1=0 range2=4 wave2=0 tune2=0.5 mix1=0.8 mix2=0.2 noise=0.03 cutoff=0.45 res=0.1 envamt=0.15 aatt=0.08 asus=0.9 lforate=0.35 lfopitch=0.05"},
    {"Brass Section",  "range1=3 wave1=2 range2=3 wave2=2 tune2=0.54 range3=3 wave3=2 tune3=0.46 kbd3=1 mix1=0.6 mix2=0.6 mix3=0.6 cutoff=0.35 res=0.15 envamt=0.55 fatt=0.15 fdec=0.4 fsus=0.5 aatt=0.06 asus=0.9"},
    {"Squelch",        "range1=2 wave1=2 mix1=0.9 cutoff=0.18 res=0.75 envamt=0.7 fdec=0.28 fsus=0 adec=0.3 asus=0.4 glide=0.2 legato=1 drive=0.6"},
    {"Zap Tom",        "range1=2 wave1=0 range2=2 wave2=0 tune2=0.5 mix1=0.9 mix2=0.0 cutoff=0.4 res=0.3 envamt=0.6 fdec=0.2 fsus=0 adec=0.35 asus=0 decay=1"},
    {"Wobble",         "range1=2 wave1=2 mix1=0.9 cutoff=0.3 res=0.55 envamt=0.1 lforate=0.45 lfowave=0 lfofilt=0.7 asus=0.9 drive=0.5"},
    {"Vibrato Lead",   "range1=3 wave1=1 range2=3 wave2=1 tune2=0.52 mix1=0.8 mix2=0.6 cutoff=0.6 res=0.15 envamt=0.2 aatt=0.03 asus=0.9 lforate=0.5 lfopitch=0.12 glide=0.1 legato=1"},
    {"Wind",           "mix1=0 noise=0.9 noisetype=1 cutoff=0.4 res=0.6 envamt=0.5 fatt=0.7 fdec=0.8 fsus=0.4 aatt=0.6 asus=0.9 decay=1"},
    {"Octave Bass",    "range1=2 wave1=2 range2=3 wave2=2 mix1=0.9 mix2=0.5 cutoff=0.28 res=0.2 envamt=0.45 fdec=0.3 fsus=0.1 adec=0.35 asus=0.6"},
    {"Soft Triangle",  "range1=3 wave1=0 range2=3 wave2=0 tune2=0.505 mix1=0.8 mix2=0.6 cutoff=0.7 res=0.05 envamt=0.1 aatt=0.1 asus=0.9"},
    {"Pluck Lead",     "range1=3 wave1=2 range2=4 wave2=4 mix1=0.8 mix2=0.4 cutoff=0.3 res=0.3 envamt=0.7 fdec=0.2 fsus=0 adec=0.25 asus=0 glide=0.0"},
};

typedef struct {
    sk_env fe, ae; sk_ladder lad; sk_rng rng;
    float p1, p2, p3, lph, pitch, target, vel, pink; int note, playing;
} mm_voice;

static void mm_vinit(void *vv, float sr)
{
    mm_voice *v = (mm_voice *)vv; (void)sr; memset(v, 0, sizeof *v);
    sk_env_init(&v->fe); sk_env_init(&v->ae); sk_ladder_init(&v->lad); sk_rng_seed(&v->rng, 1970u);
}
static void mm_von(void *vv, const float *p, int note, float vel, int retrig, float sr)
{
    mm_voice *v = (mm_voice *)vv; (void)p; (void)sr;
    v->target = (float)note; v->note = note; v->vel = vel;
    if (!v->playing) { v->pitch = v->target; v->p1 = v->p2 = v->p3 = 0.0f; }
    v->playing = 1;
    if (retrig || v->ae.st == SK_ENV_IDLE) { sk_env_on(&v->fe, 0); sk_env_on(&v->ae, 0); }
}
static void mm_voff(void *vv, const float *p) { mm_voice *v = (mm_voice *)vv; (void)p; sk_env_off(&v->fe); sk_env_off(&v->ae); }

static const float mm_oct[6] = { 0.0f, -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };

static inline float mm_wave(int w, float ph, float dt)
{
    switch (w) {
    case 0: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case 1: { float t = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; return 0.5f * (t + sk_saw(ph, dt)); }
    case 2: return sk_saw(ph, dt);
    case 3: return -sk_saw(ph, dt);
    case 4: return sk_pulse(ph, dt, 0.5f);
    default: return sk_pulse(ph, dt, 0.25f);
    }
}

static int mm_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    mm_voice *v = (mm_voice *)vv; float sr = g->sr;
    int r1 = SK_SW(p, M_R1, 6), w1 = SK_SW(p, M_W1, 6), r2 = SK_SW(p, M_R2, 6), w2 = SK_SW(p, M_W2, 6);
    int r3 = SK_SW(p, M_R3, 6), w3 = SK_SW(p, M_W3, 6), k3 = SK_SW(p, M_K3, 2), sync = SK_SW(p, M_SYNC, 2);
    int nt = SK_SW(p, M_NTYPE, 2), lw = SK_SW(p, M_LFOWAVE, 2), kbdm = SK_SW(p, M_KBD, 3), dec = SK_SW(p, M_DECAY, 2);
    float rel = dec ? 0.004f * powf(1500.0f, p[M_AD]) : 0.01f;
    sk_env_set(&v->fe, 0.0015f * powf(2000.0f, p[M_FA]), 0.004f * powf(1500.0f, p[M_FD]), p[M_FS], dec ? 0.004f * powf(1500.0f, p[M_FD]) : 0.01f, sr);
    sk_env_set(&v->ae, 0.0015f * powf(2000.0f, p[M_AA]), 0.004f * powf(1500.0f, p[M_AD]), p[M_AS], rel, sr);
    float glide = p[M_GLIDE] < 0.01f ? 1.0f : 1.0f - expf(-1.0f / (sr * (0.005f + 1.5f * p[M_GLIDE] * p[M_GLIDE])));
    float lforate = sk_expmap(p[M_LFORATE], 0.1f, 25.0f), modw = g->mod;
    float lfop = p[M_LFOPITCH] * p[M_LFOPITCH] * 4.0f * (0.2f + modw), lfof = p[M_LFOFILT] * 3.0f * (0.2f + modw);
    float cutbase = sk_expmap(p[M_CUTOFF], 25.0f, 16000.0f), envoct = p[M_ENVAMT] * 6.0f;
    float kbfac = kbdm == 0 ? 0.0f : kbdm == 1 ? 0.5f : 1.0f;
    float t2 = (p[M_T2] - 0.5f) * 14.0f, t3 = (p[M_T3] - 0.5f) * 14.0f;
    float m1 = p[M_MIX1] * 0.3f, m2 = p[M_MIX2] * 0.3f, m3 = p[M_MIX3] * 0.3f, mn = p[M_NOISE] * 0.3f;
    float vol = p[M_VOL] * 1.7f, velamt = p[M_VEL], drv = 1.0f + p[M_DRIVE] * 2.5f;
    for (int i = 0; i < n; i++) {
        float fe = sk_env_tick(&v->fe), ae = sk_env_tick(&v->ae);
        v->pitch += (v->target - v->pitch) * glide;
        v->lph += lforate / sr; if (v->lph >= 1.0f) v->lph -= 1.0f;
        float l = lw == 0 ? (v->lph < 0.5f ? 4.0f * v->lph - 1.0f : 3.0f - 4.0f * v->lph) : (v->lph < 0.5f ? 1.0f : -1.0f);
        float base = v->pitch + g->bend + l * lfop;
        float f1 = sk_mtof(base + mm_oct[r1]); if (f1 > 0.45f * sr) f1 = 0.45f * sr; if (r1 == 0) f1 = 2.0f;
        float f2 = sk_mtof(base + mm_oct[r2] + t2); if (f2 > 0.45f * sr) f2 = 0.45f * sr; if (r2 == 0) f2 = 2.0f * exp2f(t2 / 12.0f);
        float b3 = k3 ? base : 60.0f;
        float f3 = sk_mtof(b3 + mm_oct[r3] + t3); if (f3 > 0.45f * sr) f3 = 0.45f * sr; if (r3 == 0) f3 = 2.0f * exp2f(t3 / 12.0f);
        float d1 = f1 / sr, d2 = f2 / sr, d3 = f3 / sr;
        v->p1 += d1; int wr = 0; if (v->p1 >= 1.0f) { v->p1 -= 1.0f; wr = 1; }
        v->p2 += d2; if (v->p2 >= 1.0f) v->p2 -= 1.0f; if (sync && wr) v->p2 = 0.0f;
        v->p3 += d3; if (v->p3 >= 1.0f) v->p3 -= 1.0f;
        float w = sk_rng_bi(&v->rng);
        if (nt) { v->pink += (w - v->pink) * 0.12f; w = v->pink * 2.5f; }
        float s = mm_wave(w1, v->p1, d1) * m1 + mm_wave(w2, v->p2, d2) * m2 + mm_wave(w3, v->p3, d3) * m3 + w * mn;
        float kb = kbfac * (v->pitch - 60.0f) / 12.0f;
        float fc = cutbase * exp2f(envoct * fe + kb + lfof * l + g->aftertouch);
        sk_ladder_set(&v->lad, fc, p[M_RES] * 0.95f, sr, 1);
        float y = sk_ladder_tick(&v->lad, s * 2.0f, drv);
        out[i] += y * ae * (1.0f - velamt * (1.0f - v->vel)) * vol;
    }
    return v->ae.st != SK_ENV_IDLE;
}

static const sk_def mm_def = {
    "minimoog", M_COUNT, mm_params, (int)(sizeof mm_presets / sizeof mm_presets[0]), mm_presets,
    1, 1, M_LEGATO, sizeof(mm_voice), 0,
    mm_vinit, mm_von, mm_voff, mm_vrender, 0, 0
};
#endif
