/*
 * alphajuno.h - Alpha Juno style 6-voice synth: one DCO per voice (saw, pulse,
 * sub, noise), switched high-pass, 24 dB low-pass that does not self-oscillate,
 * one envelope routed to pitch, filter and amplifier, one LFO, stereo chorus
 * with programmable rate. Original presets. The hardware's 4-stage T/L envelope
 * is simplified to ADSR here.
 */
#ifndef ALPHAJUNO_H
#define ALPHAJUNO_H
#include "synthcore.h"

enum { J_RANGE, J_SAW, J_PULSE, J_SUB, J_NOISE, J_PW, J_PWMODE, J_PWAMT,
       J_LFORATE, J_LFODELAY, J_LFOWAVE, J_LFOPITCH, J_LFOFILT,
       J_HPF, J_CUTOFF, J_RES, J_ENVFILT, J_ENVPITCH, J_KBD,
       J_ATT, J_DEC, J_SUS, J_REL, J_VCA, J_VEL, J_DRIVE, J_CHORUS, J_CHRATE, J_VOL, J_COUNT };

static const sk_param aj_params[J_COUNT] = {
    {"range", 1, 3}, {"saw", 0.0f, 0}, {"pulse", 0.0f, 0}, {"sub", 0.0f, 0}, {"noise", 0.0f, 0},
    {"pw", 0.5f, 0}, {"pwmode", 0, 3}, {"pwamt", 0.0f, 0},
    {"lforate", 0.3f, 0}, {"lfodelay", 0.0f, 0}, {"lfowave", 0, 3}, {"lfopitch", 0.0f, 0}, {"lfofilt", 0.0f, 0},
    {"hpf", 0, 4}, {"cutoff", 0.6f, 0}, {"res", 0.2f, 0}, {"envfilt", 0.6f, 0}, {"envpitch", 0.5f, 0}, {"kbd", 0.5f, 0},
    {"att", 0.0f, 0}, {"dec", 0.4f, 0}, {"sus", 0.6f, 0}, {"rel", 0.3f, 0}, {"vca", 1, 2}, {"vel", 0.4f, 0}, {"drive", 0.2f, 0},
    {"chorus", 1, 3}, {"chrate", 0.35f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def aj_presets[] = {
    {"Warm Brass",    "saw=1 sub=0.3 range=1 cutoff=0.3 res=0.15 envfilt=0.75 att=0.12 dec=0.5 sus=0.7 rel=0.25 hpf=0 chorus=1"},
    {"PWM Strings",   "pulse=1 sub=0.2 pwmode=1 pwamt=0.55 lforate=0.22 range=1 cutoff=0.55 res=0.1 envfilt=0.52 att=0.55 dec=0.6 sus=0.85 rel=0.7 hpf=1 chorus=2 chrate=0.3"},
    {"Glass Pad",     "saw=0.6 pulse=0.5 pwmode=1 pwamt=0.3 range=1 cutoff=0.65 res=0.3 envfilt=0.6 att=0.7 dec=0.7 sus=0.8 rel=0.9 hpf=2 chorus=2 chrate=0.45"},
    {"Bass One",      "saw=1 sub=0.7 range=0 cutoff=0.22 res=0.3 envfilt=0.7 att=0 dec=0.3 sus=0.15 rel=0.12 hpf=0 chorus=0 kbd=0.6 drive=0.4 vol=1"},
    {"Sub Floor",     "pulse=0.6 sub=1 pw=0.5 range=0 cutoff=0.25 res=0.1 envfilt=0.6 dec=0.4 sus=0.5 rel=0.15 chorus=0"},
    {"Poly Lead",     "saw=1 pulse=0.4 range=2 cutoff=0.62 res=0.25 envfilt=0.6 att=0.01 dec=0.5 sus=0.75 rel=0.25 lfopitch=0.06 lfodelay=0.5 lforate=0.4 chorus=1"},
    {"Soft Pluck",    "pulse=1 pw=0.35 range=1 cutoff=0.25 res=0.3 envfilt=0.8 dec=0.25 sus=0 rel=0.25 kbd=0.8 chorus=1 vol=1"},
    {"Sweep Pad",     "saw=1 range=1 cutoff=0.2 res=0.5 envfilt=0.85 att=0.8 dec=0.9 sus=0.6 rel=1.0 hpf=0 chorus=2 chrate=0.2"},
    {"Brass Stab",    "saw=1 pulse=0.3 range=1 cutoff=0.28 res=0.2 envfilt=0.85 att=0.04 dec=0.3 sus=0.3 rel=0.2 chorus=1 drive=0.4 vol=1"},
    {"Vox Pad",       "pulse=0.8 noise=0.05 pw=0.3 range=1 cutoff=0.38 res=0.55 envfilt=0.55 att=0.5 dec=0.5 sus=0.9 rel=0.7 hpf=1 chorus=2"},
    {"Clav Funk",     "pulse=1 pw=0.2 range=1 cutoff=0.45 res=0.4 envfilt=0.7 dec=0.18 sus=0 rel=0.12 kbd=0.9 chorus=0 hpf=1 vol=1 drive=0.5"},
    {"Air",           "noise=0.8 saw=0.4 range=2 cutoff=0.6 res=0.5 envfilt=0.55 att=0.6 sus=0.9 rel=0.8 hpf=2 chorus=2 chrate=0.25 vol=1"},
    {"Pulse Keys",    "pulse=1 sub=0.4 pwmode=2 pwamt=0.35 range=1 cutoff=0.45 res=0.2 envfilt=0.7 dec=0.35 sus=0.4 rel=0.2 chorus=1"},
    {"Dream Chorus",  "saw=0.8 sub=0.3 range=1 cutoff=0.5 res=0.15 envfilt=0.55 att=0.3 dec=0.5 sus=0.9 rel=0.6 chorus=2 chrate=0.55"},
    {"Saw Stack",     "saw=1 sub=0.5 pulse=0.5 range=1 cutoff=0.55 res=0.2 envfilt=0.6 att=0.02 sus=0.8 rel=0.3 chorus=1 drive=0.5"},
    {"Harp",          "pulse=0.7 saw=0.3 pw=0.4 range=2 cutoff=0.4 res=0.25 envfilt=0.75 dec=0.5 sus=0 rel=0.6 kbd=0.9 chorus=2 chrate=0.4 vol=1"},
};

typedef struct {
    sk_env env; sk_ladder lad; sk_svf hp; sk_lfo lfo; sk_rng rng;
    float ph, subph, vel; int note; float lfo_age;
} aj_voice;

static void aj_vinit(void *vv, float sr)
{
    aj_voice *v = (aj_voice *)vv; (void)sr;
    memset(v, 0, sizeof *v);
    sk_env_init(&v->env); sk_ladder_init(&v->lad); sk_svf_init(&v->hp);
    sk_lfo_init(&v->lfo, 99u); sk_rng_seed(&v->rng, 31337u);
}
static void aj_von(void *vv, const float *p, int note, float vel, int retrig, float sr)
{
    aj_voice *v = (aj_voice *)vv; (void)p; (void)retrig; (void)sr;
    uint32_t seed = (uint32_t)(note * 2654435761u) + 1u;
    sk_rng_seed(&v->rng, seed);
    v->note = note; v->vel = vel; v->lfo_age = 0.0f; v->ph = 0.0f; v->subph = 0.0f;
    v->lfo.ph = 0.0f;
    sk_env_on(&v->env, 0);
}
static void aj_voff(void *vv, const float *p) { (void)p; sk_env_off(&((aj_voice *)vv)->env); }

static int aj_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    aj_voice *v = (aj_voice *)vv; float sr = g->sr;
    int range = SK_SW(p, J_RANGE, 3), pwmode = SK_SW(p, J_PWMODE, 3), lfowave = SK_SW(p, J_LFOWAVE, 3);
    int hpfs = SK_SW(p, J_HPF, 4), vca = SK_SW(p, J_VCA, 2);
    static const float hpf_hz[4] = { 0.0f, 150.0f, 400.0f, 1000.0f };
    float oct = range == 0 ? -12.0f : range == 1 ? 0.0f : 12.0f;
    sk_env_set(&v->env, 0.0015f * powf(2000.0f, p[J_ATT]), 0.004f * powf(1500.0f, p[J_DEC]), p[J_SUS], 0.004f * powf(1500.0f, p[J_REL]), sr);
    float lforate = sk_expmap(p[J_LFORATE], 0.1f, 25.0f);
    float lfo_st = p[J_LFOPITCH] * p[J_LFOPITCH] * 5.0f + g->mod * 0.5f;
    float lfodelay = p[J_LFODELAY] * 2.5f;
    float cutbase = sk_expmap(p[J_CUTOFF], 30.0f, 14000.0f);
    float envf = (p[J_ENVFILT] - 0.5f) * 2.0f * 6.0f, envp = (p[J_ENVPITCH] - 0.5f) * 2.0f * 12.0f;
    float lfoct = p[J_LFOFILT] * 3.0f;
    float mix_saw = p[J_SAW] * 0.3f, mix_pul = p[J_PULSE] * 0.3f, mix_sub = p[J_SUB] * 0.3f, mix_noi = p[J_NOISE] * 0.25f;
    float drive = 1.0f + p[J_DRIVE] * 2.0f, vol = p[J_VOL] * 1.0f, velamt = p[J_VEL];
    float kb = p[J_KBD] * 2.0f * ((float)(v->note - 60) / 12.0f);
    if (hpfs) sk_svf_set(&v->hp, hpf_hz[hpfs], 0.7f, sr);
    for (int i = 0; i < n; i++) {
        float e = sk_env_tick(&v->env);
        v->lfo_age += 1.0f / sr;
        float fade = lfodelay > 0.001f ? sk_clampf(v->lfo_age / lfodelay, 0.0f, 1.0f) : 1.0f;
        float l = sk_lfo_tick(&v->lfo, lfowave, lforate, sr) * fade;
        float f = sk_mtof((float)v->note + oct + g->bend + l * lfo_st + e * envp);
        float dt = f / sr; if (dt > 0.45f) dt = 0.45f;
        v->ph += dt; if (v->ph >= 1.0f) v->ph -= 1.0f;
        v->subph += dt * 0.5f; if (v->subph >= 1.0f) v->subph -= 1.0f;
        float pw = 0.05f + 0.9f * p[J_PW];
        if (pwmode == 1) pw += 0.4f * p[J_PWAMT] * l; else if (pwmode == 2) pw += 0.4f * p[J_PWAMT] * e;
        pw = sk_clampf(pw, 0.04f, 0.96f);
        float s = 0.0f;
        if (mix_saw > 0.0f) s += mix_saw * sk_saw(v->ph, dt);
        if (mix_pul > 0.0f) s += mix_pul * sk_pulse(v->ph, dt, pw);
        if (mix_sub > 0.0f) s += mix_sub * sk_pulse(v->subph, dt * 0.5f, 0.5f);
        if (mix_noi > 0.0f) s += mix_noi * sk_rng_bi(&v->rng);
        if (hpfs) { float lo, bp, hp; sk_svf_tick(&v->hp, s, &lo, &bp, &hp); s = hp; }
        float fc = cutbase * exp2f(envf * e + lfoct * l + kb + g->aftertouch);
        sk_ladder_set(&v->lad, fc, p[J_RES], sr, 0);
        float y = sk_ladder_tick(&v->lad, s * 2.0f, drive);
        float amp = (vca ? e : (v->env.st == SK_ENV_REL ? e : 1.0f)) * (1.0f - velamt * (1.0f - v->vel));
        out[i] += y * amp * vol;
    }
    return v->env.st != SK_ENV_IDLE;
}

static void aj_finit(void *fx, float sr) { (void)sr; sk_chorus_init((sk_chorus *)fx); }
static void aj_frun(void *fx, const float *p, const float *in, float *L, float *R, int n, float sr)
{
    int ch = SK_SW(p, J_CHORUS, 3);
    if (ch == 0) { for (int i = 0; i < n; i++) L[i] = R[i] = in[i]; return; }
    float rate = sk_expmap(p[J_CHRATE], 0.2f, 6.0f);
    sk_chorus_run((sk_chorus *)fx, in, L, R, n, sr, rate, ch == 1 ? 0.6f : 1.0f, ch == 1 ? 0.6f : 0.8f);
}

static const sk_def aj_def = {
    "alphajuno", J_COUNT, aj_params, (int)(sizeof aj_presets / sizeof aj_presets[0]), aj_presets,
    6, 0, -1, sizeof(aj_voice), sizeof(sk_chorus),
    aj_vinit, aj_von, aj_voff, aj_vrender, aj_finit, aj_frun
};
#endif
