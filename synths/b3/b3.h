/*
 * b3.h - tonewheel organ in the style of a B-3: nine drawbars, key click,
 * single-trigger percussion, scanner vibrato / chorus, overdrive and a rotary
 * speaker (horn and drum with inertia). Equal-tempered additive sines, not a
 * tonewheel-by-tonewheel model.
 */
#ifndef B3_H
#define B3_H
#include "synthcore.h"

enum { B_D16, B_D513, B_D8, B_D4, B_D223, B_D2, B_D135, B_D113, B_D1,
       B_PERC, B_PHARM, B_PDEC, B_PVOL, B_CLICK, B_VIB, B_LESLIE, B_DRIVE, B_VOL, B_COUNT };

static const sk_param b3_params[B_COUNT] = {
    {"d16", 0, 0}, {"d513", 0, 0}, {"d8", 1.0f, 0}, {"d4", 0, 0}, {"d223", 0, 0}, {"d2", 0, 0}, {"d135", 0, 0}, {"d113", 0, 0}, {"d1", 0, 0},
    {"perc", 0, 2}, {"pharm", 1, 2}, {"pdec", 0, 2}, {"pvol", 1, 2}, {"click", 0.3f, 0},
    {"vib", 0, 7}, {"leslie", 1, 3}, {"drive", 0.25f, 0}, {"vol", 0.7f, 0},
};

static const sk_preset_def b3_presets[] = {
    {"Jazz 888",      "d16=1 d513=1 d8=1 perc=1 pharm=1 pdec=0 pvol=0 vib=4 leslie=1"},
    {"Full Organ",    "d16=1 d513=1 d8=1 d4=1 d223=1 d2=1 d135=1 d113=1 d1=1 vib=1 leslie=2 drive=0.4 vol=0.45"},
    {"Gospel",        "d16=1 d513=0.75 d8=1 d135=0.5 d113=0.75 d1=1 vib=5 leslie=2 drive=0.5 vol=0.5"},
    {"Rock",          "d16=1 d513=1 d8=1 d4=1 vib=0 leslie=2 drive=0.7 vol=0.55"},
    {"Mellow Flute",  "d8=1 d223=0.375 vib=2 leslie=1 drive=0.1 click=0.15"},
    {"Church",        "d16=0.75 d8=1 d4=0.5 d2=0.25 vib=0 leslie=0 drive=0.05 click=0.1"},
    {"Pad Organ",     "d8=1 d4=1 d223=0.6 d2=0.4 d135=0.25 vib=6 leslie=1 vol=0.55"},
    {"Percussive",    "d16=1 d513=1 d8=1 perc=1 pharm=1 pdec=0 pvol=1 vib=0 leslie=2 drive=0.35 vol=0.5"},
    {"Ballad",        "d16=0.75 d8=1 vib=3 leslie=1 click=0.1 drive=0.15"},
    {"Reed Buzz",     "d8=1 d4=0.75 d223=1 d2=0.75 d135=0.5 d113=0.5 vib=0 leslie=2 drive=0.6 vol=0.5"},
    {"Soft Pad 4",    "d16=0.5 d8=0.75 d4=0.5 vib=5 leslie=1 click=0.05 drive=0.1"},
    {"Third Perc",    "d8=1 d4=0.25 perc=1 pharm=1 pdec=1 pvol=1 vib=4 leslie=2 drive=0.3"},
    {"Dry Organ",     "d16=1 d513=1 d8=1 d4=0.5 vib=0 leslie=0 drive=0.2"},
    {"Thin Lead",     "d8=1 d4=1 d2=0.75 d1=0.5 perc=1 pharm=0 pdec=0 pvol=0 vib=0 leslie=1"},
};

#define B3_TAB 2048
static float b3_tab[B3_TAB + 1];
static int b3_tab_ready;
static void b3_mktab(void)
{
    if (b3_tab_ready) return;
    for (int i = 0; i <= B3_TAB; i++) b3_tab[i] = sinf(2.0f * SK_PI * (float)i / (float)B3_TAB);
    b3_tab_ready = 1;
}
static inline float b3_sin(float ph)
{
    float x = ph * (float)B3_TAB; int i = (int)x; float f = x - (float)i;
    return b3_tab[i] + (b3_tab[i + 1] - b3_tab[i]) * f;
}

typedef struct {
    sk_env env; float ph[9]; float pph; int note; float perc_env, click_env; sk_rng rng; float vel;
} b3_voice;

static void b3_vinit(void *vv, float sr)
{
    b3_voice *v = (b3_voice *)vv; (void)sr;
    b3_mktab(); memset(v, 0, sizeof *v); sk_env_init(&v->env); sk_rng_seed(&v->rng, 5150u);
}
static void b3_von(void *vv, const float *p, int note, float vel, int first, float sr)
{
    b3_voice *v = (b3_voice *)vv; (void)p; (void)sr;
    v->note = note; v->vel = vel;
    v->perc_env = first ? 1.0f : 0.0f;     /* percussion fires on the first key only, as on the real organ */
    v->click_env = 1.0f; v->pph = 0.0f;
    sk_env_on(&v->env, 0);
}
static void b3_voff(void *vv, const float *p) { (void)p; sk_env_off(&((b3_voice *)vv)->env); }

static int b3_vrender(void *vv, const float *p, const sk_glob *g, float *out, int n)
{
    static const float mult[9] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
    b3_voice *v = (b3_voice *)vv; float sr = g->sr;
    int perc = SK_SW(p, B_PERC, 2), pharm = SK_SW(p, B_PHARM, 2), pdec = SK_SW(p, B_PDEC, 2), pvol = SK_SW(p, B_PVOL, 2);
    sk_env_set(&v->env, 0.002f, 1.0f, 1.0f, 0.012f, sr);
    float f0 = sk_mtof((float)v->note + g->bend);
    float amps[9], dt[9], norm = 0.0f;
    for (int h = 0; h < 9; h++) {
        amps[h] = p[B_D16 + h]; dt[h] = f0 * mult[h] / sr;
        if (dt[h] > 0.45f) amps[h] = 0.0f;
    }
    if (perc) amps[8] = 0.0f;                               /* percussion cuts the 1' drawbar */
    for (int h = 0; h < 9; h++) norm += amps[h];
    float pgain = perc ? (pvol ? 1.0f : 0.5f) : 0.0f;
    float pdt = f0 * (pharm ? 3.0f : 4.0f) / sr;           /* 0 = 2nd harmonic (4'), 1 = 3rd (2 2/3') */
    if (pdt > 0.45f) pgain = 0.0f;
    float pcoef = 1.0f - expf(-6.9f / (sr * (pdec ? 0.6f : 0.2f)));
    float click = p[B_CLICK] * 0.3f;
    float ccoef = 1.0f - expf(-1.0f / (sr * 0.003f));
    float scale = 0.18f / (norm > 1.0f ? sqrtf(norm) : 1.0f);
    for (int i = 0; i < n; i++) {
        float e = sk_env_tick(&v->env), s = 0.0f;
        for (int h = 0; h < 9; h++) {
            if (amps[h] <= 0.0f) continue;
            v->ph[h] += dt[h]; if (v->ph[h] >= 1.0f) v->ph[h] -= 1.0f;
            s += amps[h] * b3_sin(v->ph[h]);
        }
        s *= scale;
        if (pgain > 0.0f && v->perc_env > 1e-4f) {
            v->pph += pdt; if (v->pph >= 1.0f) v->pph -= 1.0f;
            s += 0.6f * pgain * v->perc_env * b3_sin(v->pph);
            v->perc_env -= v->perc_env * pcoef;
        }
        if (click > 0.0f && v->click_env > 1e-3f) { s += click * v->click_env * sk_rng_bi(&v->rng) * 0.3f; v->click_env -= v->click_env * ccoef; }
        out[i] += s * e;
    }
    return v->env.st != SK_ENV_IDLE;
}

/* ---- scanner vibrato / chorus, overdrive and rotary speaker ---- */
#define B3_DL 1024
typedef struct {
    float vib[B3_DL], horn[B3_DL], drum[B3_DL];
    int w; float vph, hph, dph, hs, ds; sk_svf xo;
} b3_fx;

static inline float b3_tap(const float *b, int w, float d)
{
    float rp = (float)w - d; while (rp < 0.0f) rp += (float)B3_DL;
    int i0 = (int)rp; float fr = rp - (float)i0; int i1 = (i0 + 1) % B3_DL; if (i0 >= B3_DL) i0 -= B3_DL;
    return b[i0] * (1.0f - fr) + b[i1] * fr;
}
static void b3_finit(void *fx, float sr) { b3_fx *f = (b3_fx *)fx; (void)sr; memset(f, 0, sizeof *f); sk_svf_init(&f->xo); f->hs = 0.8f; f->ds = 0.66f; }
static void b3_frun(void *fx, const float *p, const float *in, float *L, float *R, int n, float sr)
{
    b3_fx *f = (b3_fx *)fx;
    int vib = SK_SW(p, B_VIB, 7), les = SK_SW(p, B_LESLIE, 3);
    static const float vdepth[4] = { 0.0f, 0.00022f, 0.00045f, 0.0008f };   /* seconds peak */
    int vmode = vib == 0 ? 0 : ((vib - 1) % 3) + 1, chorus = vib >= 4;
    float drive = 1.0f + p[B_DRIVE] * 5.0f, vol = p[B_VOL] * 1.3f;
    float htarget = les == 2 ? 6.6f : 0.8f, dtarget = les == 2 ? 5.7f : 0.66f;
    float hco = 1.0f - expf(-1.0f / (sr * 0.8f)), dco = 1.0f - expf(-1.0f / (sr * 2.5f));
    sk_svf_set(&f->xo, 800.0f, 0.7f, sr);
    for (int i = 0; i < n; i++) {
        float x = in[i];
        f->vib[f->w] = x;
        if (vmode) {
            f->vph += 6.8f / sr; if (f->vph >= 1.0f) f->vph -= 1.0f;
            float m = f->vph < 0.5f ? 4.0f * f->vph - 1.0f : 3.0f - 4.0f * f->vph;
            float d = (0.0015f + vdepth[vmode] * (1.0f + m)) * sr;
            float vv = b3_tap(f->vib, f->w, d);
            x = chorus ? 0.5f * (x + vv) : vv;
        }
        x = sk_tanh(x * drive) / sqrtf(drive) * vol;
        float l, r;
        if (les == 0) { l = r = x; }
        else {
            f->hs += (htarget - f->hs) * hco; f->ds += (dtarget - f->ds) * dco;
            f->hph += f->hs / sr; if (f->hph >= 1.0f) f->hph -= 1.0f;
            f->dph += f->ds / sr; if (f->dph >= 1.0f) f->dph -= 1.0f;
            float lo, bp, hi; sk_svf_tick(&f->xo, x, &lo, &bp, &hi);
            f->horn[f->w] = hi; f->drum[f->w] = lo;
            float ch = cosf(2.0f * SK_PI * f->hph), sh = sinf(2.0f * SK_PI * f->hph);
            float cd = cosf(2.0f * SK_PI * f->dph), sd = sinf(2.0f * SK_PI * f->dph);
            float hh = b3_tap(f->horn, f->w, (0.0012f + 0.0004f * ch) * sr);
            float dd = b3_tap(f->drum, f->w, (0.0014f + 0.0003f * cd) * sr);
            l = hh * (0.75f + 0.5f * ch) + dd * (0.85f + 0.3f * cd);
            r = hh * (0.75f + 0.5f * sh) + dd * (0.85f + 0.3f * sd);
        }
        L[i] = l; R[i] = r;
        f->w = (f->w + 1) % B3_DL;
    }
}

static const sk_def b3_def = {
    "b3", B_COUNT, b3_params, (int)(sizeof b3_presets / sizeof b3_presets[0]), b3_presets,
    32, 0, -1, sizeof(b3_voice), sizeof(b3_fx),
    b3_vinit, b3_von, b3_voff, b3_vrender, b3_finit, b3_frun
};
#endif
