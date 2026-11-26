/*
 * ms20.h - MS-20 style monosynth: two oscillators (triangle saw pulse noise and saw square triangle plus ring
 * modulation), a high-pass and a low-pass filter that can scream (each with its own peak and overdrive),
 * two envelope generators and a modulation generator. The patch panel is a matrix of bipolar amounts:
 * every modulation source (MG and EG2) can go to pitch, pulse width, low-pass and high-pass cutoff.
 * 0.5 means no modulation. Original presets. Mono with glide and legato.
 */
#ifndef MS20_H
#define MS20_H
#include "synthcore.h"

enum { S_RANGE1, S_WAVE1, S_PW1, S_RANGE2, S_WAVE2, S_TUNE2, S_LVL1, S_LVL2, S_RING, S_NOISE,
       S_HPCUT, S_HPPEAK, S_LPCUT, S_LPPEAK, S_DRIVE, S_SERIAL,
       S_MGRATE, S_MGWAVE, S_MG2PITCH, S_MG2LP, S_MG2HP, S_MG2PW,
       S_AA, S_AD, S_AS, S_AR, S_EA, S_ED, S_ES, S_ER, S_EG2PITCH, S_EG2LP, S_EG2HP,
       S_KBD, S_GLIDE, S_LEGATO, S_VEL, S_VOL, S_COUNT };

static const sk_param ms_params[S_COUNT] = {
    {"range1", 2, 4}, {"wave1", 1, 4}, {"pw1", 0.5f, 0}, {"range2", 2, 4}, {"wave2", 1, 4}, {"tune2", 0.5f, 0},
    {"lvl1", 0.8f, 0}, {"lvl2", 0.0f, 0}, {"ring", 0.0f, 0}, {"noise", 0.0f, 0},
    {"hpcut", 0.1f, 0}, {"hppeak", 0.1f, 0}, {"lpcut", 0.6f, 0}, {"lppeak", 0.2f, 0}, {"drive", 0.3f, 0}, {"serial", 1, 2},
    {"mgrate", 0.3f, 0}, {"mgwave", 0, 4}, {"mg2pitch", 0.5f, 0}, {"mg2lp", 0.5f, 0}, {"mg2hp", 0.5f, 0}, {"mg2pw", 0.5f, 0},
    {"att", 0.0f, 0}, {"dec", 0.4f, 0}, {"sus", 0.8f, 0}, {"rel", 0.25f, 0},
    {"eatt", 0.0f, 0}, {"edec", 0.4f, 0}, {"esus", 0.3f, 0}, {"erel", 0.3f, 0},
    {"eg2pitch", 0.5f, 0}, {"eg2lp", 0.5f, 0}, {"eg2hp", 0.5f, 0},
    {"kbd", 0.5f, 0}, {"glide", 0.0f, 0}, {"legato", 0, 2}, {"vel", 0.3f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def ms_presets[] = {
    {"Screamer",      "range1=2 wave1=1 lvl1=0.8 lpcut=0.45 lppeak=0.9 hpcut=0.25 hppeak=0.5 drive=0.7 eg2lp=0.8 edec=0.35 esus=0.2 sus=0.9 glide=0.15 legato=1"},
    {"Ring Bell",     "range1=2 wave1=0 range2=2 wave2=2 tune2=0.58 lvl1=0.2 lvl2=0 ring=0.9 lpcut=0.7 lppeak=0.2 dec=0.6 sus=0 rel=0.5 eg2lp=0.6 edec=0.5 esus=0"},
    {"Wow Bass",      "range1=1 wave1=1 range2=1 wave2=1 tune2=0.505 lvl1=0.8 lvl2=0.6 lpcut=0.3 lppeak=0.55 drive=0.5 eg2lp=0.75 edec=0.3 esus=0 dec=0.35 sus=0.5"},
    {"Sync Alarm",    "range1=2 wave1=2 pw1=0.3 range2=2 wave2=0 lvl1=0.7 lvl2=0.5 tune2=0.65 lpcut=0.6 lppeak=0.5 mgrate=0.55 mgwave=2 mg2pitch=0.62 sus=0.9"},
    {"Filter Wah",    "range1=2 wave1=1 lvl1=0.8 hpcut=0.3 hppeak=0.7 lpcut=0.35 lppeak=0.6 mgrate=0.4 mgwave=0 mg2lp=0.85 mg2hp=0.7 sus=0.9 serial=0"},
    {"Pulse Lead",    "range1=2 wave1=2 pw1=0.35 lvl1=0.8 lpcut=0.55 lppeak=0.3 mgrate=0.35 mg2pw=0.7 sus=0.9 glide=0.12 legato=1 kbd=0.7"},
    {"Wind Noise",    "lvl1=0 noise=0.9 hpcut=0.3 hppeak=0.5 lpcut=0.4 lppeak=0.7 eg2lp=0.8 eatt=0.7 edec=0.8 esus=0.5 att=0.6 sus=0.9 rel=0.7"},
    {"Zap Pitch",     "range1=2 wave1=1 lvl1=0.8 eg2pitch=0.9 edec=0.2 esus=0 lpcut=0.6 lppeak=0.4 dec=0.25 sus=0.2"},
    {"Dual Saw",      "range1=2 wave1=1 range2=2 wave2=0 tune2=0.53 lvl1=0.7 lvl2=0.7 lpcut=0.5 lppeak=0.25 sus=0.9 drive=0.4"},
    {"Sub Growl",     "range1=0 wave1=2 pw1=0.4 range2=1 wave2=1 lvl1=0.7 lvl2=0.5 lpcut=0.25 lppeak=0.75 drive=0.8 mgrate=0.25 mg2lp=0.7 sus=0.9"},
    {"Self Sine",     "lvl1=0 lpcut=0.5 lppeak=1 noise=0.02 kbd=1 drive=0.2 sus=1 glide=0.1 legato=1"},
    {"Tri Flute",     "range1=2 wave1=0 lvl1=0.9 lpcut=0.7 lppeak=0.1 att=0.1 sus=0.9 mgrate=0.3 mg2pitch=0.54"},
};

typedef struct {
    sk_env ae, ee; sk_rng rng; float p1, p2, mgph, pitch, target, vel, mgh, s1[4], s2[4], pink;
    int note, playing;
} ms_voice;

static void ms_vinit(void *vv, float sr) { ms_voice *v = (ms_voice *)vv; (void)sr; memset(v, 0, sizeof *v); sk_env_init(&v->ae); sk_env_init(&v->ee); sk_rng_seed(&v->rng, 1978u); }
static void ms_von(void *vv, const float *p, int note, float vel, int retrig, float sr)
{
    ms_voice *v = (ms_voice *)vv; (void)p; (void)sr;
    v->target = (float)note; v->note = note; v->vel = vel;
    if (!v->playing) { v->pitch = v->target; v->p1 = v->p2 = 0.0f; }
    v->playing = 1;
    if (retrig || v->ae.st == SK_ENV_IDLE) { sk_env_on(&v->ae, 0); sk_env_on(&v->ee, 0); }
}
static void ms_voff(void *vv, const float *p) { ms_voice *v = (ms_voice *)vv; (void)p; sk_env_off(&v->ae); sk_env_off(&v->ee); }

static inline float ms_wave(int w, float ph, float dt, float pw)
{
    switch (w) {
    case 0: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
    case 1: return sk_saw(ph, dt);
    case 2: return sk_pulse(ph, dt, pw);
    default: return 0.0f;
    }
}
/* two-pole state variable filter with a soft-clipped band state: it screams instead of blowing up */
static inline float ms_filter(float *s, float x, float g, float R, float drive, int hp)
{
    float h = (x - (2.0f * R + g) * s[0] - s[1]) / (1.0f + 2.0f * R * g + g * g);
    float b = g * h + s[0];
    float bc = sk_tanh(b * drive) / drive;
    s[0] = sk_flush(g * h + bc);
    float l = g * bc + s[1];
    s[1] = sk_flush(g * bc + l);
    return hp ? h : l;
}

static int ms_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    ms_voice *v = (ms_voice *)vv; float sr = g->sr;
    static const float oct[4] = { -12.0f, 0.0f, 12.0f, 24.0f };
    int r1 = SK_SW(p, S_RANGE1, 4), w1 = SK_SW(p, S_WAVE1, 4), r2 = SK_SW(p, S_RANGE2, 4), w2 = SK_SW(p, S_WAVE2, 4);
    int mgw = SK_SW(p, S_MGWAVE, 4), serial = SK_SW(p, S_SERIAL, 2);
    sk_env_set(&v->ae, 0.0015f * powf(2000.0f, p[S_AA]), 0.004f * powf(1500.0f, p[S_AD]), p[S_AS], 0.004f * powf(1500.0f, p[S_AR]), sr);
    sk_env_set(&v->ee, 0.0015f * powf(2000.0f, p[S_EA]), 0.004f * powf(1500.0f, p[S_ED]), p[S_ES], 0.004f * powf(1500.0f, p[S_ER]), sr);
    float glide = p[S_GLIDE] < 0.01f ? 1.0f : 1.0f - expf(-1.0f / (sr * (0.005f + 1.5f * p[S_GLIDE] * p[S_GLIDE])));
    float mgr = sk_expmap(p[S_MGRATE], 0.1f, 40.0f);
    float mg2pitch = (p[S_MG2PITCH] - 0.5f) * 2.0f * 12.0f * (0.3f + g->mod), mg2lp = (p[S_MG2LP] - 0.5f) * 2.0f * 4.0f, mg2hp = (p[S_MG2HP] - 0.5f) * 2.0f * 4.0f, mg2pw = (p[S_MG2PW] - 0.5f) * 0.9f;
    float e2pitch = (p[S_EG2PITCH] - 0.5f) * 2.0f * 24.0f, e2lp = (p[S_EG2LP] - 0.5f) * 2.0f * 6.0f, e2hp = (p[S_EG2HP] - 0.5f) * 2.0f * 6.0f;
    float hpbase = sk_expmap(p[S_HPCUT], 20.0f, 9000.0f), lpbase = sk_expmap(p[S_LPCUT], 40.0f, 16000.0f);
    float hq = 1.0f - p[S_HPPEAK], lq = 1.0f - p[S_LPPEAK];   /* damping: slightly negative at the top so the filter oscillates by itself */
    float hpR = 0.9f * hq * hq - 0.015f * p[S_HPPEAK] * p[S_HPPEAK] * p[S_HPPEAK], lpR = 0.9f * lq * lq - 0.015f * p[S_LPPEAK] * p[S_LPPEAK] * p[S_LPPEAK];
    float drv = 1.0f + p[S_DRIVE] * 3.0f, vol = p[S_VOL] * 1.5f, velamt = p[S_VEL], tune2 = (p[S_TUNE2] - 0.5f) * 14.0f;
    float l1 = p[S_LVL1] * 0.35f, l2 = p[S_LVL2] * 0.35f, ring = p[S_RING] * 0.4f, nz = p[S_NOISE] * 0.35f;
    for (int i = 0; i < n; i++) {
        float ae = sk_env_tick(&v->ae), ee = sk_env_tick(&v->ee);
        v->pitch += (v->target - v->pitch) * glide;
        v->mgph += mgr / sr; if (v->mgph >= 1.0f) { v->mgph -= 1.0f; v->mgh = sk_rng_bi(&v->rng); }
        float mg = mgw == 0 ? (v->mgph < 0.5f ? 4.0f * v->mgph - 1.0f : 3.0f - 4.0f * v->mgph) : mgw == 1 ? 1.0f - 2.0f * v->mgph : mgw == 2 ? (v->mgph < 0.5f ? 1.0f : -1.0f) : v->mgh;
        float base = v->pitch + g->bend + mg * mg2pitch + ee * e2pitch;
        float f1 = sk_mtof(base + oct[r1]), f2 = sk_mtof(base + oct[r2] + tune2);
        float d1 = sk_clampf(f1 / sr, 0.0f, 0.45f), d2 = sk_clampf(f2 / sr, 0.0f, 0.45f);
        v->p1 += d1; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
        v->p2 += d2; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
        float pw = sk_clampf(0.05f + 0.9f * p[S_PW1] + mg * mg2pw, 0.05f, 0.95f);
        float noise = sk_rng_bi(&v->rng);
        float o1 = w1 == 3 ? noise : ms_wave(w1, v->p1, d1, pw);
        float o2 = w2 == 3 ? noise : (w2 == 2 ? ms_wave(0, v->p2, d2, 0.5f) : w2 == 1 ? sk_pulse(v->p2, d2, 0.5f) : sk_saw(v->p2, d2));
        float s = o1 * l1 + o2 * l2 + o1 * o2 * ring + noise * nz;
        float kb = p[S_KBD] * 2.0f * (v->pitch - 60.0f) / 12.0f;
        float fhp = hpbase * exp2f(ee * e2hp + mg * mg2hp + kb * 0.5f), flp = lpbase * exp2f(ee * e2lp + mg * mg2lp + kb + g->aftertouch);
        float ghp = tanf(SK_PI * sk_clampf(fhp, 10.0f, sr * 0.45f) / sr), glp = tanf(SK_PI * sk_clampf(flp, 10.0f, sr * 0.45f) / sr);
        float y;
        if (serial) { float a = ms_filter(v->s1, s * 2.0f, ghp, hpR, drv, 1); y = ms_filter(v->s2, a, glp, lpR, drv, 0); }
        else { float a = ms_filter(v->s1, s * 2.0f, ghp, hpR, drv, 1), b = ms_filter(v->s2, s * 2.0f, glp, lpR, drv, 0); y = 0.7f * (a + b); }
        out[i] += y * ae * (1.0f - velamt * (1.0f - v->vel)) * vol;
    }
    return v->ae.st != SK_ENV_IDLE;
}

static const sk_def ms_def = {
    "ms20", S_COUNT, ms_params, (int)(sizeof ms_presets / sizeof ms_presets[0]), ms_presets,
    1, 1, S_LEGATO, sizeof(ms_voice), 0,
    ms_vinit, ms_von, ms_voff, ms_vrender, 0, 0
};
#endif
