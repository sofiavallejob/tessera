/*
 * [tspec~] - spectral engine of tessera: freeze, smear, gate, cross-synthesis.
 * Normally used through the tessera abstractions ([tessera/freeze~ name] etc.),
 * but it can be used directly:  [tessera/tspec~ cross 2048 name]
 * Shares the tessera effect framework (fxkit.h): presets, A/B morph, mutate, data inlet.
 *
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#define FX_PD 1
#include "fxkit.h"
#include "tspec_core.h"

typedef struct { ts_core *c; float sr; int size; } tspec_state;
static int tspec_size_g = 2048;

static const fx_param tf_params[] = {
    {"freeze", 0, 1, 0, FX_NOMUT, 2, "off|on"},
    {"jitter", 0, 1, 0.15f, 0, 0, 0},
    {"shimmer", 0, 1, 0.1f, 0, 0, 0},
    {"mix", 0, 1, 1, FX_NOMUT, 0, 0},
};
static const fx_preset tf_presets[] = {
    {"Default", ""},
    {"Pure drone", "freeze=on jitter=0 shimmer=0 mix=1"},
    {"Airy hold", "freeze=on jitter=0.6 shimmer=0.2 mix=1"},
    {"Glitter", "freeze=on jitter=0.2 shimmer=0.8 mix=0.9"},
    {"Dust cloud", "freeze=on jitter=1 shimmer=0.5 mix=1"},
    {"Live blend", "freeze=off jitter=0.3 shimmer=0.3 mix=0.5"},
};
static const fx_param ts_params_smear[] = {
    {"time", 1, 4000, 600, FX_LOG, 0, 0},
    {"shape", 0, 1, 0.7f, 0, 0, 0},
    {"width", 1, 64, 5, 0, 0, 0},
    {"diffuse", 0, 1, 0, 0, 0, 0},
    {"mix", 0, 1, 1, FX_NOMUT, 0, 0},
};
static const fx_preset ts_presets_smear[] = {
    {"Default", ""},
    {"Soft wash", "time=600 shape=0.3 width=5 diffuse=0 mix=1"},
    {"Long tail", "time=3000 shape=0.9 width=3 diffuse=0.1 mix=1"},
    {"Blur", "time=200 shape=0.2 width=30 diffuse=0.3 mix=0.8"},
    {"Noise bed", "time=1500 shape=0.6 width=12 diffuse=1 mix=1"},
    {"Subtle", "time=120 shape=0.4 width=4 diffuse=0 mix=0.5"},
};
static const fx_param tg_params[] = {
    {"thresh", -80, 0, -30, 0, 0, 0},
    {"floor", -90, 0, -60, 0, 0, 0},
    {"attack", 1, 500, 10, FX_LOG, 0, 0},
    {"release", 1, 2000, 120, FX_LOG, 0, 0},
    {"invert", 0, 1, 0, FX_NOMUT, 2, "off|on"},
    {"mix", 0, 1, 1, FX_NOMUT, 0, 0},
};
static const fx_preset tg_presets[] = {
    {"Default", ""},
    {"Skeleton", "thresh=-20 floor=-70 attack=8 release=90 invert=off mix=1"},
    {"Whisper", "thresh=-12 floor=-80 attack=4 release=40 invert=off mix=1"},
    {"Hollow", "thresh=-25 floor=-40 attack=20 release=200 invert=on mix=1"},
    {"Slow bloom", "thresh=-35 floor=-60 attack=300 release=1200 invert=off mix=1"},
    {"Subtle", "thresh=-40 floor=-30 attack=10 release=120 invert=off mix=0.5"},
};
static const fx_param tc_params[] = {
    {"cmode", 0, 3, 0, 0, 4, "envelope|phase|magnitude|morph"},
    {"amount", 0, 1, 1, 0, 0, 0},
    {"width", 1, 64, 9, 0, 0, 0},
    {"mix", 0, 1, 1, FX_NOMUT, 0, 0},
};
static const fx_preset tc_presets[] = {
    {"Default", ""},
    {"Vocoder", "cmode=envelope amount=1 width=9 mix=1"},
    {"Coarse vocoder", "cmode=envelope amount=1 width=32 mix=1"},
    {"Phase borrow", "cmode=phase amount=1 width=9 mix=1"},
    {"Magnitude borrow", "cmode=magnitude amount=1 width=9 mix=1"},
    {"Half morph", "cmode=morph amount=0.5 width=9 mix=1"},
};

static int ts_cur_mode;
static void *tspec_create(float sr, const fx_def *d)
{
    tspec_state *s = (tspec_state *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->sr = sr; s->size = tspec_size_g;
    s->c = ts_new(s->size, sr);
    if (!s->c) { free(s); return 0; }
    ts_get_params(s->c)->mode = !strcmp(d->name, "freeze~") ? TS_FREEZE : !strcmp(d->name, "smear~") ? TS_SMEAR : !strcmp(d->name, "gate~") ? TS_GATE : TS_CROSS;
    return s;
}
static void tspec_destroy(void *u) { tspec_state *s = (tspec_state *)u; ts_free(s->c); free(s); }
static void tspec_process(void *u, const float *P, const float **in, float **out, int n)
{
    tspec_state *s = (tspec_state *)u;
    ts_params *q = ts_get_params(s->c);
    switch (q->mode) {
    case TS_FREEZE: q->freeze = (int)P[0]; q->jitter = P[1]; q->shimmer = P[2]; q->mix = P[3]; break;
    case TS_SMEAR: q->time_ms = P[0]; q->shape = P[1]; q->width = P[2]; q->diffuse = P[3]; q->mix = P[4]; break;
    case TS_GATE: q->thresh_db = P[0]; q->floor_db = P[1]; q->attack_ms = P[2]; q->release_ms = P[3]; q->invert = (int)P[4]; q->mix = P[5]; break;
    default: q->cmode = (int)P[0]; q->amount = P[1]; q->width = P[2]; q->mix = P[3]; break;
    }
    ts_process(s->c, in[0], q->mode == TS_CROSS ? in[1] : 0, out[0], n);
}
static int tspec_cmd(void *u, const char *name, const char *arg, const char *dir)
{
    tspec_state *s = (tspec_state *)u;
    (void)dir;
    if (!strcmp(name, "reset")) { ts_reset(s->c); return 0; }
    if (!strcmp(name, "latency")) { post("tspec~: latency %d samples (%.1f ms)", ts_latency(s->c), 1000.0 * ts_latency(s->c) / s->sr); return 0; }
    if (!strcmp(name, "size")) {
        int n = atoi(arg); if (n < 256) n = 256; if (n > 16384) n = 16384;
        ts_core *c = ts_new(n, s->sr);
        if (!c) return 1;
        *ts_get_params(c) = *ts_get_params(s->c);
        ts_free(s->c); s->c = c; s->size = n;
        return 0;
    }
    return 1;
}
#define TDEF(nm, mode, np, pp, nps, pps, nin) \
    static const fx_def nm = { mode, np, pp, nps, pps, nin, 1, 0, tspec_create, tspec_destroy, tspec_process, tspec_cmd, 0 };
TDEF(def_freeze, "freeze~", 4, tf_params, 5, tf_presets, 1)
TDEF(def_smear, "smear~", 5, ts_params_smear, 5, ts_presets_smear, 1)
TDEF(def_gate, "gate~", 6, tg_params, 5, tg_presets, 1)
TDEF(def_cross, "cross~", 4, tc_params, 5, tc_presets, 2)

static const fx_def *tspec_pick(int argc, t_atom *argv)
{
    const fx_def *d = &def_freeze;
    tspec_size_g = 2048;
    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_FLOAT) { int n = (int)atom_getfloat(argv + i); if (n >= 256) tspec_size_g = n > 16384 ? 16384 : n; }
        else {
            t_symbol *m = atom_getsymbol(argv + i);
            const fx_def *f = m == gensym("freeze") ? &def_freeze : m == gensym("smear") ? &def_smear : m == gensym("gate") ? &def_gate : m == gensym("cross") ? &def_cross : 0;
            if (f) { d = f; SETFLOAT(argv + i, -1); }
        }
    }
    (void)ts_cur_mode;
    return d;
}
FX_PD_CLASS("tspec~", tspec_pick, tspec_tilde_setup)
