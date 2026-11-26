/*
 * prophet5.h - Prophet-5 style 5-voice polysynth: two oscillators per voice (A saw/pulse with
 * pulse width, B saw/triangle/pulse with sync and a low-frequency mode), poly-mod from the
 * filter envelope and oscillator B, noise, 4-pole low-pass, two ADSR envelopes, LFO with
 * mod-wheel routing and analogue-style drift. Original presets; no unison or glide yet.
 */
#ifndef PROPHET5_H
#define PROPHET5_H
#include "synthcore.h"

enum { R_ASAW, R_APULSE, R_PW, R_BSAW, R_BTRI, R_BPULSE, R_BFREQ, R_BFINE, R_BLOW, R_BKBD, R_SYNC,
       R_PMFENV, R_PMOSCB, R_PMFREQ, R_PMPW, R_PMFILT, R_MIXA, R_MIXB, R_NOISE,
       R_CUTOFF, R_RES, R_ENVAMT, R_KBD, R_FA, R_FD, R_FS, R_FR, R_AA, R_AD, R_AS, R_AR,
       R_LFORATE, R_LFOSHAPE, R_LFOFREQ, R_LFOPW, R_LFOFILT, R_DRIFT, R_VEL, R_VOL, R_COUNT };

static const sk_param p5_params[R_COUNT] = {
    {"asaw", 1, 2}, {"apulse", 0, 2}, {"pw", 0.5f, 0}, {"bsaw", 0, 2}, {"btri", 0, 2}, {"bpulse", 0, 2},
    {"bfreq", 0.5f, 0}, {"bfine", 0.5f, 0}, {"blow", 0, 2}, {"bkbd", 1, 2}, {"sync", 0, 2},
    {"pmfenv", 0.0f, 0}, {"pmoscb", 0.0f, 0}, {"pmfreq", 0, 2}, {"pmpw", 0, 2}, {"pmfilt", 0, 2},
    {"mixa", 0.8f, 0}, {"mixb", 0.0f, 0}, {"noise", 0.0f, 0},
    {"cutoff", 0.5f, 0}, {"res", 0.15f, 0}, {"envamt", 0.3f, 0}, {"kbd", 1, 3},
    {"fatt", 0.0f, 0}, {"fdec", 0.4f, 0}, {"fsus", 0.3f, 0}, {"frel", 0.3f, 0},
    {"aatt", 0.0f, 0}, {"adec", 0.4f, 0}, {"asus", 0.7f, 0}, {"arel", 0.3f, 0},
    {"lforate", 0.3f, 0}, {"lfoshape", 0, 3}, {"lfofreq", 0.0f, 0}, {"lfopw", 0.0f, 0}, {"lfofilt", 0.0f, 0},
    {"drift", 0.3f, 0}, {"vel", 0.4f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def p5_presets[] = {
    {"Warm Brass",    "asaw=1 bsaw=1 mixa=0.7 mixb=0.7 bfine=0.54 cutoff=0.3 res=0.15 envamt=0.5 fatt=0.18 fdec=0.45 fsus=0.45 aatt=0.1 asus=0.85 arel=0.25"},
    {"Poly Strings",  "asaw=1 apulse=1 pw=0.4 bsaw=1 mixa=0.6 mixb=0.6 bfine=0.55 cutoff=0.5 res=0.1 envamt=0.1 aatt=0.5 adec=0.5 asus=0.9 arel=0.7 lfoshape=0 lforate=0.2 lfopw=0.25 drift=0.5"},
    {"Soft Pad",      "asaw=1 bsaw=1 mixa=0.6 mixb=0.6 bfine=0.57 cutoff=0.32 res=0.2 envamt=0.3 fatt=0.6 fdec=0.8 fsus=0.5 frel=0.8 aatt=0.65 asus=0.9 arel=0.9 drift=0.6"},
    {"Sync Lead",     "asaw=1 bsaw=1 sync=1 mixa=0.8 mixb=0.0 bfreq=0.8 cutoff=0.6 res=0.2 envamt=0.2 pmfenv=0.5 pmfreq=1 fdec=0.35 fsus=0.2 asus=0.85 arel=0.2"},
    {"Poly Bass",     "asaw=1 bpulse=1 mixa=0.8 mixb=0.5 bfreq=0.29 cutoff=0.22 res=0.3 envamt=0.5 fdec=0.3 fsus=0 aatt=0 adec=0.3 asus=0.5 arel=0.12 kbd=2"},
    {"Fm Bell",       "btri=1 mixa=0.9 mixb=0 asaw=1 bfreq=0.62 pmoscb=0.35 pmfreq=1 cutoff=0.7 res=0.1 envamt=0.1 aatt=0 adec=0.55 asus=0 arel=0.6 bkbd=1"},
    {"Pwm Keys",      "apulse=1 asaw=0 pw=0.5 mixa=0.8 lforate=0.25 lfoshape=0 lfopw=0.5 cutoff=0.42 res=0.2 envamt=0.4 fdec=0.35 fsus=0.25 adec=0.4 asus=0.5 arel=0.25"},
    {"Resonant Sweep","asaw=1 bsaw=1 mixa=0.7 mixb=0.6 bfine=0.52 cutoff=0.12 res=0.7 envamt=0.8 fatt=0.5 fdec=0.8 fsus=0.3 frel=0.7 aatt=0.3 asus=0.8 arel=0.7"},
    {"Pluck",         "asaw=1 mixa=0.8 cutoff=0.2 res=0.3 envamt=0.7 fdec=0.2 fsus=0 adec=0.3 asus=0 arel=0.25 kbd=2"},
    {"Choir",         "apulse=1 asaw=0 pw=0.3 bpulse=1 mixa=0.6 mixb=0.6 bfine=0.56 noise=0.04 cutoff=0.36 res=0.5 envamt=0.2 aatt=0.5 asus=0.9 arel=0.7 drift=0.6"},
    {"Mod Wheel Lead","asaw=1 bsaw=1 mixa=0.7 mixb=0.6 bfine=0.53 cutoff=0.55 res=0.2 envamt=0.2 lforate=0.45 lfoshape=0 lfofreq=0.15 lfofilt=0.25 asus=0.9 arel=0.3"},
    {"Hollow Reed",   "apulse=1 asaw=0 pw=0.15 mixa=0.9 cutoff=0.38 res=0.15 envamt=0.2 aatt=0.06 asus=0.85 arel=0.25"},
    {"Noise Wash",    "mixa=0 noise=0.9 asaw=0 cutoff=0.4 res=0.6 envamt=0.5 fatt=0.7 fdec=0.8 fsus=0.4 frel=0.8 aatt=0.6 asus=0.9 arel=0.8"},
    {"Detuned Saws",  "asaw=1 bsaw=1 mixa=0.7 mixb=0.7 bfine=0.6 cutoff=0.5 res=0.1 envamt=0.2 aatt=0.02 asus=0.9 arel=0.3 drift=0.8"},
    {"Zap",           "asaw=1 bsaw=1 mixa=0.7 mixb=0.5 pmfenv=0.8 pmfreq=1 cutoff=0.7 res=0.4 envamt=0 fdec=0.15 fsus=0 adec=0.2 asus=0 arel=0.2"},
    {"Soft Bell Pad", "btri=1 asaw=0 apulse=0 mixa=0 mixb=1 bkbd=1 pmoscb=0 cutoff=0.45 res=0.1 envamt=0.2 aatt=0.4 adec=0.7 asus=0.6 arel=0.9 asaw=1 mixa=0.4"},
};

typedef struct {
    sk_env fe, ae; sk_ladder lad; sk_rng rng;
    float pa, pb, lph, vel; int note;
    float drift, drift_t; int dcount;
} p5_voice;

static void p5_vinit(void *vv, float sr)
{
    p5_voice *v = (p5_voice *)vv; (void)sr;
    memset(v, 0, sizeof *v);
    sk_env_init(&v->fe); sk_env_init(&v->ae); sk_ladder_init(&v->lad); sk_rng_seed(&v->rng, 8675309u);
}
static void p5_von(void *vv, const float *p, int note, float vel, int first, float sr)
{
    p5_voice *v = (p5_voice *)vv; (void)p; (void)first; (void)sr;
    sk_rng_seed(&v->rng, (uint32_t)(note * 2246822519u) + (uint32_t)(v->dcount * 3266489917u) + 1u);
    v->note = note; v->vel = vel; v->lph = 0.0f; v->pa = 0.0f; v->pb = 0.0f;
    sk_env_on(&v->fe, 0); sk_env_on(&v->ae, 0);
}
static void p5_voff(void *vv, const float *p) { p5_voice *v = (p5_voice *)vv; (void)p; sk_env_off(&v->fe); sk_env_off(&v->ae); }

static int p5_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    p5_voice *v = (p5_voice *)vv; float sr = g->sr;
    int asaw = SK_SW(p, R_ASAW, 2), apul = SK_SW(p, R_APULSE, 2), bsaw = SK_SW(p, R_BSAW, 2), btri = SK_SW(p, R_BTRI, 2), bpul = SK_SW(p, R_BPULSE, 2);
    int blow = SK_SW(p, R_BLOW, 2), bkbd = SK_SW(p, R_BKBD, 2), sync = SK_SW(p, R_SYNC, 2);
    int pmfreq = SK_SW(p, R_PMFREQ, 2), pmpw = SK_SW(p, R_PMPW, 2), pmfilt = SK_SW(p, R_PMFILT, 2);
    int shape = SK_SW(p, R_LFOSHAPE, 3), kbdm = SK_SW(p, R_KBD, 3);
    sk_env_set(&v->fe, 0.0015f * powf(2000.0f, p[R_FA]), 0.004f * powf(1500.0f, p[R_FD]), p[R_FS], 0.004f * powf(1500.0f, p[R_FR]), sr);
    sk_env_set(&v->ae, 0.0015f * powf(2000.0f, p[R_AA]), 0.004f * powf(1500.0f, p[R_AD]), p[R_AS], 0.004f * powf(1500.0f, p[R_AR]), sr);
    float lforate = sk_expmap(p[R_LFORATE], 0.1f, 25.0f);
    float modw = g->mod;
    float cutbase = sk_expmap(p[R_CUTOFF], 25.0f, 16000.0f);
    float envoct = p[R_ENVAMT] * 6.0f, kb = (kbdm == 0 ? 0.0f : kbdm == 1 ? 0.5f : 1.0f) * ((float)(v->note - 60) / 12.0f);
    float bsemi = (p[R_BFREQ] - 0.5f) * 24.0f + (p[R_BFINE] - 0.5f) * 1.0f;
    float mixa = p[R_MIXA] * 0.35f, mixb = p[R_MIXB] * 0.35f, mixn = p[R_NOISE] * 0.3f;
    float velamt = p[R_VEL], vol = p[R_VOL] * 1.6f, dr = p[R_DRIFT] * 0.12f;
    float lfopitch = p[R_LFOFREQ] * p[R_LFOFREQ] * 4.0f * (0.2f + modw), lfopw = p[R_LFOPW] * 0.4f, lfofilt = p[R_LFOFILT] * 3.0f * (0.2f + modw);
    for (int i = 0; i < n; i++) {
        float fe = sk_env_tick(&v->fe), ae = sk_env_tick(&v->ae);
        if (--v->dcount <= 0) { v->dcount = 2048; v->drift_t = sk_rng_bi(&v->rng) * dr; }
        v->drift += (v->drift_t - v->drift) * 0.002f;
        v->lph += lforate / sr; if (v->lph >= 1.0f) v->lph -= 1.0f;
        float l = shape == 0 ? (v->lph < 0.5f ? 4.0f * v->lph - 1.0f : 3.0f - 4.0f * v->lph) : shape == 1 ? 1.0f - 2.0f * v->lph : (v->lph < 0.5f ? 1.0f : -1.0f);
        /* oscillator B first: it is the poly-mod source and the sync master */
        float fb = blow ? sk_expmap(sk_clampf(p[R_BFREQ], 0, 1), 0.1f, 30.0f)
                        : sk_mtof((bkbd ? (float)v->note : 60.0f) + bsemi + g->bend + v->drift + l * lfopitch);
        float dtb = fb / sr; if (dtb > 0.45f) dtb = 0.45f;
        v->pb += dtb; int wrapped = 0; if (v->pb >= 1.0f) { v->pb -= 1.0f; wrapped = 1; }
        float sb = 0.0f;
        if (bsaw) sb += sk_saw(v->pb, dtb);
        if (btri) sb += v->pb < 0.5f ? 4.0f * v->pb - 1.0f : 3.0f - 4.0f * v->pb;
        if (bpul) sb += sk_pulse(v->pb, dtb, 0.5f);
        float pm = p[R_PMFENV] * fe + p[R_PMOSCB] * sb;
        float semis = (float)v->note + g->bend + v->drift + l * lfopitch + (pmfreq ? pm * 12.0f : 0.0f);
        float fa = sk_mtof(semis), dta = fa / sr; if (dta > 0.45f) dta = 0.45f;
        v->pa += dta; if (v->pa >= 1.0f) v->pa -= 1.0f;
        if (sync && wrapped) v->pa = 0.0f;
        float pw = sk_clampf(0.05f + 0.9f * p[R_PW] + lfopw * l + (pmpw ? 0.4f * pm : 0.0f), 0.04f, 0.96f);
        float sa = 0.0f;
        if (asaw) sa += sk_saw(v->pa, dta);
        if (apul) sa += sk_pulse(v->pa, dta, pw);
        float s = sa * mixa + sb * mixb + mixn * sk_rng_bi(&v->rng);
        float fc = cutbase * exp2f(envoct * fe + kb + lfofilt * l + (pmfilt ? pm * 4.0f : 0.0f) + g->aftertouch);
        sk_ladder_set(&v->lad, fc, p[R_RES] * 0.95f, sr, 1);
        float y = sk_ladder_tick(&v->lad, s * 2.0f, 1.3f);
        out[i] += y * ae * (1.0f - velamt * (1.0f - v->vel)) * vol;
    }
    return v->ae.st != SK_ENV_IDLE;
}

static const sk_def p5_def = {
    "prophet5", R_COUNT, p5_params, (int)(sizeof p5_presets / sizeof p5_presets[0]), p5_presets,
    5, 0, -1, sizeof(p5_voice), 0,
    p5_vinit, p5_von, p5_voff, p5_vrender, 0, 0
};
#endif
