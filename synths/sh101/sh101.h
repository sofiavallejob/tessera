/*
 * sh101.h - SH-101 style monophonic synth (one VCO with saw, pulse and sub,
 * noise, self-oscillating 24 dB low-pass, one ADSR, one LFO, portamento).
 * Original presets; this is a model of the architecture, not a copy of any ROM.
 */
#ifndef SH101_H
#define SH101_H
#include "synthcore.h"

enum { P_SAW, P_PULSE, P_SUB, P_SUBMODE, P_NOISE, P_PW, P_PWMODE, P_PWAMT,
       P_LFORATE, P_LFOWAVE, P_LFOPITCH, P_LFOFILT, P_CUTOFF, P_RES, P_ENVAMT, P_KBD,
       P_ATT, P_DEC, P_SUS, P_REL, P_VCA, P_GLIDE, P_LEGATO, P_RANGE, P_VEL, P_DRIVE, P_VOL, P_COUNT };

static const sk_param sh101_params[P_COUNT] = {
    {"saw", 0.0f, 0}, {"pulse", 0.0f, 0}, {"sub", 0.0f, 0}, {"submode", 0, 3}, {"noise", 0.0f, 0},
    {"pw", 0.5f, 0}, {"pwmode", 0, 3}, {"pwamt", 0.0f, 0},
    {"lforate", 0.3f, 0}, {"lfowave", 0, 3}, {"lfopitch", 0.0f, 0}, {"lfofilt", 0.0f, 0},
    {"cutoff", 0.5f, 0}, {"res", 0.2f, 0}, {"envamt", 0.3f, 0}, {"kbd", 0.5f, 0},
    {"att", 0.0f, 0}, {"dec", 0.4f, 0}, {"sus", 0.5f, 0}, {"rel", 0.2f, 0},
    {"vca", 1, 2}, {"glide", 0.0f, 0}, {"legato", 0, 2}, {"range", 1, 4}, {"vel", 0.3f, 0}, {"drive", 0.3f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def sh101_presets[] = {
    {"Classic Bass",   "saw=1 sub=0.6 submode=0 cutoff=0.28 res=0.35 envamt=0.45 dec=0.35 sus=0.1 rel=0.15 range=0 kbd=0.6 drive=0.4"},
    {"Square Sub",     "pulse=0.8 sub=0.9 submode=1 cutoff=0.35 res=0.2 envamt=0.3 dec=0.4 sus=0.3 rel=0.2 range=0 pw=0.5"},
    {"Squelch",        "saw=1 cutoff=0.18 res=0.8 envamt=0.65 dec=0.3 sus=0 rel=0.1 range=1 glide=0.25 legato=1 drive=0.6"},
    {"PWM Lead",       "pulse=1 sub=0.3 pwmode=1 pwamt=0.6 lforate=0.25 cutoff=0.55 res=0.2 envamt=0.2 att=0.05 sus=0.8 rel=0.25 range=2 lfopitch=0.05 glide=0.1 legato=1"},
    {"Fat Saw Lead",   "saw=1 pulse=0.5 sub=0.4 submode=2 cutoff=0.6 res=0.25 envamt=0.25 att=0.02 dec=0.5 sus=0.7 rel=0.3 range=2 glide=0.15 legato=1 drive=0.5"},
    {"Zap",            "saw=1 pulse=0.4 cutoff=0.9 res=0.5 envamt=0.0 dec=0.18 sus=0 rel=0.1 range=2 lfowave=0 lfopitch=0.9 lforate=0.9 vca=1"},
    {"Pluck",          "pulse=1 pw=0.35 cutoff=0.22 res=0.3 envamt=0.55 dec=0.22 sus=0 rel=0.2 range=2 kbd=0.8"},
    {"Resonant Sweep", "saw=0.7 sub=0.5 cutoff=0.1 res=0.9 envamt=0.8 att=0.35 dec=0.9 sus=0.4 rel=0.6 range=1"},
    {"Laser Siren",    "pulse=1 cutoff=0.7 res=0.3 lforate=0.55 lfowave=1 lfopitch=0.6 sus=1 rel=0.1 range=2 vca=0"},
    {"Noise Hit",      "noise=1 cutoff=0.6 res=0.5 envamt=0.4 dec=0.28 sus=0 rel=0.2 vca=1 kbd=0.2"},
    {"Wind",           "noise=1 cutoff=0.3 res=0.7 lforate=0.12 lfowave=2 lfofilt=0.5 att=0.7 sus=1 rel=0.8 vca=1 kbd=0.1"},
    {"Hollow Reed",    "pulse=1 pw=0.15 sub=0.2 cutoff=0.4 res=0.15 envamt=0.2 att=0.08 sus=0.8 rel=0.2 range=1 lfopitch=0.03 lforate=0.35"},
    {"Wobble",         "saw=1 sub=0.7 cutoff=0.3 res=0.55 lforate=0.4 lfowave=0 lfofilt=0.7 envamt=0.1 sus=1 rel=0.15 range=0 vca=0"},
    {"Blip",           "pulse=1 pw=0.25 cutoff=0.45 res=0.4 envamt=0.4 dec=0.12 sus=0 rel=0.08 range=2"},
    {"Rez Pad",        "saw=1 pulse=0.6 pwmode=1 pwamt=0.3 cutoff=0.35 res=0.4 envamt=0.2 att=0.5 dec=0.8 sus=0.8 rel=0.9 range=1"},
    {"Self Osc Sine",  "cutoff=0.4 res=1 envamt=0 kbd=1 sus=1 rel=0.2 noise=0.02 range=1 vca=0 drive=0"},
};

typedef struct {
    sk_env env, gate;
    sk_ladder lad; sk_lfo lfo; sk_rng rng;
    float ph, subph, pitch, target; int note; int playing;
    float vel;
} sh101_voice;

static void sh101_vinit(void *vv, float sr)
{
    sh101_voice *v = (sh101_voice *)vv; (void)sr;
    memset(v, 0, sizeof *v);
    sk_env_init(&v->env); sk_env_init(&v->gate); sk_ladder_init(&v->lad);
    sk_lfo_init(&v->lfo, 777u); sk_rng_seed(&v->rng, 4242u);
}
static void sh101_von(void *vv, const float *p, int note, float vel, int retrig, float sr)
{
    sh101_voice *v = (sh101_voice *)vv; (void)sr; (void)p;
    v->target = (float)note;
    v->note = note; v->vel = vel;
    if (!v->playing) { v->pitch = v->target; v->ph = 0.0f; v->subph = 0.0f; }
    v->playing = 1;
    if (retrig) { sk_env_on(&v->env, 0); sk_env_on(&v->gate, 0); }
    else if (v->env.st == SK_ENV_IDLE) { sk_env_on(&v->env, 0); sk_env_on(&v->gate, 0); }
}
static void sh101_voff(void *vv, const float *p)
{
    sh101_voice *v = (sh101_voice *)vv; (void)p;
    sk_env_off(&v->env); sk_env_off(&v->gate);
}
static int sh101_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    sh101_voice *v = (sh101_voice *)vv;
    float sr = g->sr;
    int range = SK_SW(p, P_RANGE, 4), submode = SK_SW(p, P_SUBMODE, 3), pwmode = SK_SW(p, P_PWMODE, 3);
    int lfowave = SK_SW(p, P_LFOWAVE, 3), vca = SK_SW(p, P_VCA, 2);
    float oct = range == 0 ? -12.0f : range == 1 ? 0.0f : range == 2 ? 12.0f : 24.0f;
    sk_env_set(&v->env, 0.0015f * powf(2000.0f, p[P_ATT]), 0.004f * powf(1500.0f, p[P_DEC]), p[P_SUS], 0.004f * powf(1500.0f, p[P_REL]), sr);
    sk_env_set(&v->gate, 0.001f, 0.1f, 1.0f, 0.008f, sr);
    float glide = p[P_GLIDE] < 0.01f ? 1.0f : 1.0f - expf(-1.0f / (sr * (0.005f + 1.5f * p[P_GLIDE] * p[P_GLIDE])));
    float lforate = sk_expmap(p[P_LFORATE], 0.1f, 30.0f);
    float lfo_st = p[P_LFOPITCH] * p[P_LFOPITCH] * 7.0f + g->mod * 0.6f;
    float cutbase = sk_expmap(p[P_CUTOFF], 25.0f, 16000.0f);
    float envoct = p[P_ENVAMT] * 6.0f, lfoct = p[P_LFOFILT] * 3.0f;
    float res = p[P_RES], drive = 1.0f + p[P_DRIVE] * 3.0f;
    float mix_saw = p[P_SAW] * 0.5f, mix_pul = p[P_PULSE] * 0.5f, mix_sub = p[P_SUB] * 0.5f, mix_noi = p[P_NOISE] * 0.45f;
    float velamt = p[P_VEL], vol = p[P_VOL] * 3.0f;
    int sel = vca ? 1 : 0;
    for (int i = 0; i < n; i++) {
        float e = sk_env_tick(&v->env), ge = sk_env_tick(&v->gate);
        float l = sk_lfo_tick(&v->lfo, lfowave, lforate, sr);
        v->pitch += (v->target - v->pitch) * glide;
        float semis = v->pitch + oct + g->bend + l * lfo_st;
        float f = sk_mtof(semis);
        float dt = f / sr; if (dt > 0.45f) dt = 0.45f;
        v->ph += dt; if (v->ph >= 1.0f) v->ph -= 1.0f;
        float sdt = submode == 0 ? dt * 0.5f : dt * 0.25f;
        v->subph += sdt; if (v->subph >= 1.0f) v->subph -= 1.0f;
        float pw = 0.05f + 0.9f * p[P_PW];
        if (pwmode == 1) pw += 0.4f * p[P_PWAMT] * l; else if (pwmode == 2) pw += 0.4f * p[P_PWAMT] * e;
        pw = sk_clampf(pw, 0.04f, 0.96f);
        float s = 0.0f;
        if (mix_saw > 0.0f) s += mix_saw * sk_saw(v->ph, dt);
        if (mix_pul > 0.0f) s += mix_pul * sk_pulse(v->ph, dt, pw);
        if (mix_sub > 0.0f) s += mix_sub * sk_pulse(v->subph, sdt, submode == 2 ? 0.25f : 0.5f);
        if (mix_noi > 0.0f) s += mix_noi * sk_rng_bi(&v->rng);
        float kb = (p[P_KBD] * 2.0f) * ((float)(v->note - 60) / 12.0f);   /* kbd 0.5 = full tracking */
        float fc = cutbase * exp2f(envoct * e + lfoct * l + kb + g->aftertouch);
        sk_ladder_set(&v->lad, fc, res, sr, 1);
        float y = sk_ladder_tick(&v->lad, s * 2.0f, drive);
        float amp = (sel ? e : ge) * (1.0f - velamt * (1.0f - v->vel));
        out[i] += y * amp * vol;
    }
    return (sel ? v->env.st : v->gate.st) != SK_ENV_IDLE || (sel ? v->gate.st : v->env.st) == SK_ENV_ATT;
}

static const sk_def sh101_def = {
    "sh101", P_COUNT, sh101_params, (int)(sizeof sh101_presets / sizeof sh101_presets[0]), sh101_presets,
    1, 1, P_LEGATO, sizeof(sh101_voice), 0,
    sh101_vinit, sh101_von, sh101_voff, sh101_vrender, 0, 0
};
#endif
