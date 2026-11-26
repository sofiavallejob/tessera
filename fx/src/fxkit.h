/*
 * fxkit.h - shared effect framework of tessera. Header only.
 *
 * An effect source file defines one fx_def (parameters, presets, process function) and
 * writes   FX_PD_CLASS("grain~", grain_pick)   . Everything else is shared:
 *
 *   parameters in real units, bound to <name>-<param> (float in) and pushed to <name>-<param>-r,
 *   option text for stepped parameters pushed to <name>-<param>-t, preset name to <name>-name,
 *   preset number to <name>-index, morph position to <name>-morph.
 *   messages: <param> <value>   set <param> <value>   params   preset <n|name>   next   prev
 *             mutate [0..1]   A   B   morph <0..1>   store <name>   save <file>   load <file>
 *             data <0..1>   datamap <param> <amount>   datamap clear   bang (report)
 *
 * The engine part has no Pd dependency, so it can be tested offline. The Pd part is
 * compiled only when FX_PD is defined.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#ifndef FXKIT_H
#define FXKIT_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FX_MAXP 24
#define FX_USER_MAX 48
#define FX_NAME 28
#define FX_PI 3.14159265358979323846f

enum { FX_LIN = 0, FX_LOG = 1, FX_INT = 2, FX_NOMUT = 4 };

typedef struct { const char *name; float lo, hi, def; int flags; int steps; const char *opts; } fx_param;
typedef struct { const char *name; const char *vals; } fx_preset;

struct fx_def;
typedef struct fx_def {
    const char *name;
    int nparams; const fx_param *params;
    int npresets; const fx_preset *presets;
    int nin, nout, nctl;                                        /* signal in, signal out, extra control outlets */
    void *(*create)(float sr, const struct fx_def *d);
    void (*destroy)(void *u);
    void (*process)(void *u, const float *P, const float **in, float **out, int n);
    int  (*cmd)(void *u, const char *name, const char *arg, const char *dir);        /* 0 = handled */
    int  (*poll)(void *u, int k, float *v, int *n, const char **sel);                /* 1 = emit on control outlet k */
} fx_def;

typedef struct { char name[FX_NAME]; float v[FX_MAXP]; } fx_user;

typedef struct fx_engine {
    const fx_def *d;
    float sr;
    float p[FX_MAXP], A[FX_MAXP], B[FX_MAXP], eff[FX_MAXP];
    int cur;
    float data;
    int ndmap, dmap[4]; float damt[4];
    fx_user user[FX_USER_MAX]; int nuser;
    unsigned rng;
    void *u;
} fx_engine;

static inline float fx_clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float fx_rnd(fx_engine *e) { e->rng ^= e->rng << 13; e->rng ^= e->rng >> 17; e->rng ^= e->rng << 5; return (e->rng >> 8) * (1.0f / 16777216.0f); }

static inline float fx_norm(const fx_param *q, float v)
{
    if (q->steps >= 2) return (v + 0.5f) / q->steps;
    if (q->flags & FX_LOG) return fx_clamp(logf(fx_clamp(v, q->lo, q->hi) / q->lo) / logf(q->hi / q->lo), 0, 1);
    return fx_clamp((v - q->lo) / (q->hi - q->lo), 0, 1);
}
static inline float fx_denorm(const fx_param *q, float n)
{
    if (q->steps >= 2) { int k = (int)floorf(n * q->steps); return (float)(k < 0 ? 0 : (k >= q->steps ? q->steps - 1 : k)); }
    n = fx_clamp(n, 0, 1);
    float v = (q->flags & FX_LOG) ? q->lo * powf(q->hi / q->lo, n) : q->lo + n * (q->hi - q->lo);
    return (q->flags & FX_INT) ? floorf(v + 0.5f) : v;
}
static inline float fx_fix(const fx_param *q, float v)
{
    if (q->steps >= 2) return fx_clamp(floorf(v + 0.5f), 0, (float)(q->steps - 1));
    v = fx_clamp(v, q->lo, q->hi);
    return (q->flags & FX_INT) ? floorf(v + 0.5f) : v;
}
static int fx_find(const fx_def *d, const char *name)
{
    for (int i = 0; i < d->nparams; i++) if (!strcmp(d->params[i].name, name)) return i;
    return -1;
}
/* option text for a stepped parameter: copies the k-th "a|b|c" item */
static const char *fx_opt(const fx_param *q, int k, char *buf, int n)
{
    const char *s = q->opts; int i = 0;
    buf[0] = 0;
    if (!s) { snprintf(buf, n, "%d", k); return buf; }
    while (*s && i < k) { if (*s == '|') i++; s++; }
    int j = 0;
    while (*s && *s != '|' && j < n - 1) buf[j++] = *s++;
    buf[j] = 0;
    return buf;
}
static int fx_opt_index(const fx_param *q, const char *txt)
{
    char b[48];
    for (int k = 0; k < q->steps; k++) { fx_opt(q, k, b, sizeof b); if (!strcasecmp(b, txt)) return k; }
    return -1;
}
static void fx_defaults(fx_engine *e)
{
    for (int i = 0; i < e->d->nparams; i++) e->p[i] = fx_fix(&e->d->params[i], e->d->params[i].def);
}
static fx_engine *fx_new(const fx_def *d, float sr)
{
    fx_engine *e = (fx_engine *)calloc(1, sizeof *e);
    if (!e) return 0;
    e->d = d; e->sr = sr > 0 ? sr : 44100.0f; e->cur = -1; e->data = 0.5f; e->rng = 2463534242u;
    fx_defaults(e);
    if (d->create) { e->u = d->create(e->sr, d); if (!e->u) { free(e); return 0; } }
    memcpy(e->A, e->p, sizeof e->p); memcpy(e->B, e->p, sizeof e->p);
    return e;
}
static void fx_free(fx_engine *e) { if (!e) return; if (e->d->destroy && e->u) e->d->destroy(e->u); free(e); }

static int fx_set_value(fx_engine *e, int i, float v) { e->p[i] = fx_fix(&e->d->params[i], v); e->cur = -1; return i; }
static int fx_set(fx_engine *e, const char *name, float v)
{
    int i = fx_find(e->d, name);
    if (i < 0) return -1;
    return fx_set_value(e, i, v);
}
/* "key=value key=value" (values may be option names) applied over the defaults */
static void fx_apply_string(fx_engine *e, const char *s)
{
    char key[40], val[48];
    fx_defaults(e);
    while (*s) {
        while (*s == ' ') s++;
        int k = 0, v = 0;
        while (*s && *s != '=' && *s != ' ' && k < 39) key[k++] = *s++;
        key[k] = 0;
        if (*s == '=') s++;
        while (*s && *s != ' ' && v < 47) val[v++] = *s++;
        val[v] = 0;
        if (!k) break;
        int i = fx_find(e->d, key);
        if (i < 0) { fprintf(stderr, "fxkit: preset key '%s' unknown in %s\n", key, e->d->name); continue; }
        const fx_param *q = &e->d->params[i];
        float x;
        if (q->steps >= 2 && fx_opt_index(q, val) >= 0) x = (float)fx_opt_index(q, val); else x = (float)atof(val);
        e->p[i] = fx_fix(q, x);
    }
}
static int fx_total(const fx_engine *e) { return e->d->npresets + e->nuser; }
static const char *fx_preset_name(const fx_engine *e, int i)
{
    if (i < e->d->npresets) return e->d->presets[i].name;
    return e->user[i - e->d->npresets].name;
}
static void fx_load_preset(fx_engine *e, int i)
{
    int n = fx_total(e);
    if (n <= 0) return;
    if (i < 0) i = n - 1;
    if (i >= n) i = 0;
    if (i < e->d->npresets) fx_apply_string(e, e->d->presets[i].vals);
    else memcpy(e->p, e->user[i - e->d->npresets].v, sizeof e->p);
    e->cur = i;
}
static int fx_find_preset(const fx_engine *e, const char *name)
{
    for (int i = 0; i < fx_total(e); i++) if (!strcasecmp(fx_preset_name(e, i), name)) return i;
    return -1;
}
static void fx_morph(fx_engine *e, float m)
{
    m = fx_clamp(m, 0, 1);
    for (int i = 0; i < e->d->nparams; i++) {
        const fx_param *q = &e->d->params[i];
        if (q->steps >= 2) e->p[i] = m < 0.5f ? e->A[i] : e->B[i];
        else e->p[i] = fx_fix(q, fx_denorm(q, fx_norm(q, e->A[i]) * (1 - m) + fx_norm(q, e->B[i]) * m));
    }
}
static float fx_gauss(fx_engine *e) { return (fx_rnd(e) + fx_rnd(e) + fx_rnd(e) + fx_rnd(e) - 2.0f) * 1.7320508f; }
static void fx_mutate(fx_engine *e, float amt)
{
    amt = fx_clamp(amt, 0, 1);
    for (int i = 0; i < e->d->nparams; i++) {
        const fx_param *q = &e->d->params[i];
        if (q->flags & FX_NOMUT) continue;
        if (q->steps >= 2) {
            if (fx_rnd(e) < amt * 0.5f) {
                if (q->steps > 16) e->p[i] = fx_fix(q, e->p[i] + floorf(fx_gauss(e) * amt * q->steps * 0.15f));
                else e->p[i] = (float)(int)(fx_rnd(e) * q->steps);
            }
        } else e->p[i] = fx_fix(q, fx_denorm(q, fx_norm(q, e->p[i]) + fx_gauss(e) * amt * 0.3f));
    }
    e->cur = -1;
}
static int fx_data_map(fx_engine *e, const char *name, float amt)
{
    int i = fx_find(e->d, name);
    if (i < 0 || e->d->params[i].steps >= 2) return -1;
    for (int k = 0; k < e->ndmap; k++) if (e->dmap[k] == i) {
        if (amt == 0) { for (int j = k; j < e->ndmap - 1; j++) { e->dmap[j] = e->dmap[j + 1]; e->damt[j] = e->damt[j + 1]; } e->ndmap--; }
        else e->damt[k] = amt;
        return i;
    }
    if (amt == 0) return i;
    if (e->ndmap >= 4) return -2;
    e->dmap[e->ndmap] = i; e->damt[e->ndmap] = amt; e->ndmap++;
    return i;
}
static void fx_process(fx_engine *e, const float **in, float **out, int n)
{
    memcpy(e->eff, e->p, sizeof e->p);
    for (int k = 0; k < e->ndmap; k++) {
        const fx_param *q = &e->d->params[e->dmap[k]];
        e->eff[e->dmap[k]] = fx_denorm(q, fx_norm(q, e->p[e->dmap[k]]) + e->damt[k] * (e->data * 2 - 1));
    }
    e->d->process(e->u, e->eff, in, out, n);
}
static int fx_store(fx_engine *e, const char *name)
{
    int k;
    for (k = 0; k < e->nuser; k++) if (!strcasecmp(e->user[k].name, name)) break;
    if (k == e->nuser) { if (e->nuser >= FX_USER_MAX) return -1; e->nuser++; }
    snprintf(e->user[k].name, FX_NAME, "%s", name);
    memcpy(e->user[k].v, e->p, sizeof e->p);
    e->cur = e->d->npresets + k;
    return k;
}
static int fx_save_user(fx_engine *e, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "tessera %s 1\n", e->d->name);
    for (int k = 0; k < e->nuser; k++) {
        fprintf(f, "%s;", e->user[k].name);
        for (int i = 0; i < e->d->nparams; i++) fprintf(f, " %.6g", e->user[k].v[i]);
        fprintf(f, "\n");
    }
    fclose(f);
    return e->nuser;
}
static int fx_load_user(fx_engine *e, const char *path)
{
    char line[1024];
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    e->nuser = 0;
    if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }
    while (fgets(line, sizeof line, f) && e->nuser < FX_USER_MAX) {
        char *semi = strchr(line, ';');
        if (!semi) continue;
        *semi = 0;
        fx_user *u = &e->user[e->nuser];
        snprintf(u->name, FX_NAME, "%.*s", FX_NAME - 1, line);
        char *s = semi + 1;
        memcpy(u->v, e->p, sizeof e->p);
        for (int i = 0; i < e->d->nparams; i++) {
            char *end; float x = strtof(s, &end);
            if (end == s) break;
            u->v[i] = fx_fix(&e->d->params[i], x); s = end;
        }
        e->nuser++;
    }
    fclose(f);
    return e->nuser;
}

#ifdef FX_PD
/* ======================================================================== Pd class */
#include "m_pd.h"
#include "g_canvas.h"

typedef char fx_needs_32bit_float[sizeof(t_sample) == sizeof(float) ? 1 : -1];

struct _fxpd;
typedef struct _fxproxy { t_pd pd; struct _fxpd *owner; int idx; } t_fxproxy;
#define FX_MORPH_IDX FX_MAXP

typedef struct _fxpd {
    t_object x_obj;
    t_float x_f;
    fx_engine *e;
    const fx_def *d;
    t_canvas *cv;
    t_symbol *prefix;
    t_outlet *o_ctl[4], *o_name, *o_info;
    t_fxproxy *proxy[FX_MAXP + 1];
    int pushing, nin;
    t_clock *clk;
    float morphpos;
} t_fxpd;

typedef const fx_def *(*fx_pick_fn)(int argc, t_atom *argv);
static t_class *fxpd_class, *fxproxy_class;
static fx_pick_fn fxpd_pick_g;
static const char *fxpd_cname_g;

static void fxpd_snd_f(t_fxpd *x, const char *suffix, float f)
{
    char nm[128];
    snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, suffix);
    t_pd *th = gensym(nm)->s_thing;
    if (th) { x->pushing++; pd_float(th, f); x->pushing--; }
}
static void fxpd_snd_s(t_fxpd *x, const char *suffix, const char *s)
{
    char nm[128];
    snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, suffix);
    t_pd *th = gensym(nm)->s_thing;
    if (th) { x->pushing++; pd_symbol(th, gensym(s)); x->pushing--; }
}
static void fxpd_push_text(t_fxpd *x, int i)
{
    const fx_param *q = &x->d->params[i];
    if (q->steps < 2) return;
    char nm[64], b[48];
    snprintf(nm, sizeof nm, "%s-t", q->name);
    fx_opt(q, (int)x->e->p[i], b, sizeof b);
    fxpd_snd_s(x, nm, b);
}
static void fxpd_push_one(t_fxpd *x, int i)
{
    char nm[64];
    snprintf(nm, sizeof nm, "%s-r", x->d->params[i].name);
    fxpd_snd_f(x, nm, x->e->p[i]);
    fxpd_push_text(x, i);
}
static void fxpd_push(t_fxpd *x)
{
    for (int i = 0; i < x->d->nparams; i++) fxpd_push_one(x, i);
    fxpd_snd_s(x, "name", x->e->cur >= 0 ? fx_preset_name(x->e, x->e->cur) : "(edited)");
    fxpd_snd_f(x, "index", (float)x->e->cur);
}
static void fxpd_report(t_fxpd *x)
{
    t_atom a[3];
    fxpd_push(x);
    outlet_symbol(x->o_name, gensym(x->e->cur >= 0 ? fx_preset_name(x->e, x->e->cur) : "(edited)"));
    SETSYMBOL(a, gensym("preset")); SETFLOAT(a + 1, (float)x->e->cur); SETFLOAT(a + 2, (float)fx_total(x->e));
    outlet_list(x->o_info, &s_list, 3, a);
}
static void fxproxy_float(t_fxproxy *p, t_floatarg f)
{
    t_fxpd *x = p->owner;
    if (x->pushing) return;
    if (p->idx == FX_MORPH_IDX) { fx_morph(x->e, f); x->e->cur = -1; x->morphpos = f; fxpd_push(x); return; }
    fx_set_value(x->e, p->idx, f);
    fxpd_push_text(x, p->idx);
    fxpd_snd_s(x, "name", "(edited)");
}

static void fxpd_tick(t_fxpd *x)
{
    float v[16]; int n; const char *sel;
    t_atom a[16];
    for (int k = 0; k < x->d->nctl && x->d->poll; k++) {
        n = 0; sel = "";
        while (x->d->poll(x->e->u, k, v, &n, &sel) && n > 0) {
            for (int j = 0; j < n && j < 16; j++) SETFLOAT(a + j, v[j]);
            if (sel[0]) outlet_anything(x->o_ctl[k], gensym(sel), n, a);
            else outlet_list(x->o_ctl[k], &s_list, n, a);
            n = 0;
            break;
        }
    }
    clock_delay(x->clk, 15);
}

static t_int *fxpd_perform(t_int *w)
{
    t_fxpd *x = (t_fxpd *)w[1];
    int n = (int)w[2], nin = (int)w[3], nout = (int)w[4];
    const float *in[4]; float *out[8];
    for (int i = 0; i < nin; i++) in[i] = (const float *)w[5 + i];
    for (int i = 0; i < nout; i++) out[i] = (float *)w[5 + nin + i];
    fx_process(x->e, in, out, n);
    return w + 5 + nin + nout;
}
static void fxpd_dsp(t_fxpd *x, t_signal **sp)
{
    t_int v[16];
    int nin = x->nin, nout = x->d->nout;
    float sr = sp[0]->s_sr;
    if (sr > 0 && fabsf(sr - x->e->sr) > 0.5f) {                 /* new sample rate: rebuild the effect state, keep settings */
        fx_engine *n = fx_new(x->d, sr);
        if (n) {
            memcpy(n->p, x->e->p, sizeof n->p); memcpy(n->A, x->e->A, sizeof n->A); memcpy(n->B, x->e->B, sizeof n->B);
            memcpy(n->user, x->e->user, sizeof n->user); n->nuser = x->e->nuser; n->cur = x->e->cur;
            n->ndmap = x->e->ndmap; memcpy(n->dmap, x->e->dmap, sizeof n->dmap); memcpy(n->damt, x->e->damt, sizeof n->damt);
            n->data = x->e->data;
            fx_free(x->e); x->e = n;
        }
    }
    v[0] = (t_int)x; v[1] = (t_int)sp[0]->s_n; v[2] = nin; v[3] = nout;
    for (int i = 0; i < nin + nout; i++) v[4 + i] = (t_int)sp[i]->s_vec;
    dsp_addv(fxpd_perform, 4 + nin + nout, v);
}

static void *fxpd_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    const fx_def *d = fxpd_pick_g(argc, argv);
    if (!d) return 0;
    t_fxpd *x = (t_fxpd *)pd_new(fxpd_class);
    char pre[48];
    snprintf(pre, sizeof pre, "%s", fxpd_cname_g);
    { char *tl = strchr(pre, '~'); if (tl) *tl = 0; }
    x->prefix = gensym(pre);
    for (int i = 0; i < argc; i++) if (argv[i].a_type == A_SYMBOL) {
        t_symbol *a = atom_getsymbol(argv + i);
        x->prefix = a;           /* the pick function turns the words it consumed into floats, so a remaining symbol is the name */
    }
    x->d = d;
    x->nin = d->nin > 1 ? d->nin : 1;
    x->e = fx_new(d, sys_getsr() > 0 ? (float)sys_getsr() : 44100.0f);
    if (!x->e) return 0;
    x->cv = canvas_getcurrent();
    for (int i = 1; i < x->nin; i++) inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for (int i = 0; i < d->nout; i++) outlet_new(&x->x_obj, &s_signal);
    for (int k = 0; k < d->nctl && k < 4; k++) x->o_ctl[k] = outlet_new(&x->x_obj, 0);
    x->o_name = outlet_new(&x->x_obj, &s_symbol);
    x->o_info = outlet_new(&x->x_obj, &s_list);
    pd_bind(&x->x_obj.ob_pd, x->prefix);
    for (int i = 0; i <= FX_MAXP; i++) x->proxy[i] = 0;
    for (int i = 0; i < d->nparams; i++) {
        char nm[128];
        t_fxproxy *pr = (t_fxproxy *)pd_new(fxproxy_class);
        pr->owner = x; pr->idx = i; x->proxy[i] = pr;
        snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, d->params[i].name);
        pd_bind(&pr->pd, gensym(nm));
    }
    { char nm[128]; t_fxproxy *pr = (t_fxproxy *)pd_new(fxproxy_class);
      pr->owner = x; pr->idx = FX_MORPH_IDX; x->proxy[FX_MORPH_IDX] = pr;
      snprintf(nm, sizeof nm, "%s-morph", x->prefix->s_name); pd_bind(&pr->pd, gensym(nm)); }
    if (d->npresets > 0) fx_load_preset(x->e, 0);
    memcpy(x->e->A, x->e->p, sizeof x->e->p);
    if (d->npresets > 1) { fx_load_preset(x->e, 1); memcpy(x->e->B, x->e->p, sizeof x->e->p); fx_load_preset(x->e, 0); }
    x->clk = clock_new(x, (t_method)fxpd_tick);
    if (d->nctl > 0) clock_delay(x->clk, 15);
    return x;
}
static void fxpd_free(t_fxpd *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->prefix);
    for (int i = 0; i <= FX_MAXP; i++) if (x->proxy[i]) {
        char nm[128];
        if (i == FX_MORPH_IDX) snprintf(nm, sizeof nm, "%s-morph", x->prefix->s_name);
        else snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, x->d->params[i].name);
        pd_unbind(&x->proxy[i]->pd, gensym(nm)); pd_free(&x->proxy[i]->pd);
    }
    clock_free(x->clk);
    fx_free(x->e);
}


/* the folder of the patch the effect is used in. When the effect sits inside one of the tessera abstractions, that is the patch that holds the abstraction */
static const char *fxpd_patchdir(t_fxpd *x)
{
    static const char *mine[] = {"freeze~", "smear~", "gate~", "cross~", "grain~", "modal~", "datamod~", 0};
    t_canvas *c;
    if (!x->cv) return ".";
    c = canvas_getrootfor(x->cv);
    for (int guard = 0; guard < 8 && c && c->gl_owner; guard++) {
        int ours = 0;
        for (int i = 0; mine[i]; i++) if (!strncmp(c->gl_name->s_name, mine[i], strlen(mine[i]))) ours = 1;
        if (!ours) break;
        c = canvas_getrootfor(c->gl_owner);
    }
    return canvas_getdir(c)->s_name;
}

static void fxpd_atoms_to_string(int argc, t_atom *argv, char *out, size_t n)
{
    char buf[256];
    out[0] = 0;
    for (int i = 0; i < argc; i++) {
        atom_string(argv + i, buf, sizeof buf);
        if (strlen(out) + strlen(buf) + 2 < n) { if (i) strcat(out, " "); strcat(out, buf); }
    }
}
static void fxpd_path(t_fxpd *x, const char *in, char *out, size_t n)
{
    if (in[0] == '/') snprintf(out, n, "%s", in);
    else if (in[0] == '~') snprintf(out, n, "%s%s", getenv("HOME") ? getenv("HOME") : "", in + 1);
    else snprintf(out, n, "%s/%s", fxpd_patchdir(x), in);
}

static void fxpd_set_atom(t_fxpd *x, int i, t_atom *a)
{
    const fx_param *q = &x->d->params[i];
    float v;
    if (a->a_type == A_FLOAT) v = atom_getfloat(a);
    else {
        int k = q->steps >= 2 ? fx_opt_index(q, atom_getsymbol(a)->s_name) : -1;
        if (k < 0) { pd_error(x, "%s: '%s' is not a value of %s (send 'params')", x->d->name, atom_getsymbol(a)->s_name, q->name); return; }
        v = (float)k;
    }
    fx_set_value(x->e, i, v);
    fxpd_push_one(x, i);
    fxpd_snd_s(x, "name", "(edited)");
}
static void fxpd_anything(t_fxpd *x, t_symbol *s, int argc, t_atom *argv)
{
    int i = fx_find(x->d, s->s_name);
    if (i >= 0) { if (argc) fxpd_set_atom(x, i, argv); return; }
    if (x->d->cmd) {
        char arg[1024]; fxpd_atoms_to_string(argc, argv, arg, sizeof arg);
        if (x->d->cmd(x->e->u, s->s_name, arg, fxpd_patchdir(x)) == 0) { fxpd_report(x); return; }
        pd_error(x, "%s: '%s %s' failed (see the console)", x->d->name, s->s_name, arg);
        return;
    }
    pd_error(x, "%s: no parameter or message '%s' (send 'params')", x->d->name, s->s_name);
}
static void fxpd_set(t_fxpd *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc < 2 || argv[0].a_type != A_SYMBOL) { pd_error(x, "%s: set <param> <value>", x->d->name); return; }
    fxpd_anything(x, atom_getsymbol(argv), argc - 1, argv + 1);
}
static void fxpd_params(t_fxpd *x)
{
    char b[48];
    for (int i = 0; i < x->d->nparams; i++) {
        const fx_param *q = &x->d->params[i];
        if (q->steps >= 2) post("%s: %-10s %s  (now %s)", x->d->name, q->name, q->opts ? q->opts : "switch", fx_opt(q, (int)x->e->p[i], b, sizeof b));
        else post("%s: %-10s %g .. %g  now %g", x->d->name, q->name, q->lo, q->hi, x->e->p[i]);
    }
}
static void fxpd_preset(t_fxpd *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (!argc) return;
    if (argv[0].a_type == A_FLOAT) fx_load_preset(x->e, (int)atom_getfloat(argv));
    else {
        char nm[128]; fxpd_atoms_to_string(argc, argv, nm, sizeof nm);
        int i = fx_find_preset(x->e, nm);
        if (i < 0) { pd_error(x, "%s: no preset '%s'", x->d->name, nm); return; }
        fx_load_preset(x->e, i);
    }
    fxpd_report(x);
}
static void fxpd_next(t_fxpd *x) { fx_load_preset(x->e, x->e->cur + 1); fxpd_report(x); }
static void fxpd_prev(t_fxpd *x) { fx_load_preset(x->e, x->e->cur < 0 ? 0 : x->e->cur - 1); fxpd_report(x); }
static void fxpd_mutate(t_fxpd *x, t_floatarg f) { fx_mutate(x->e, f > 0 ? f : 0.4f); fxpd_report(x); }
static void fxpd_A(t_fxpd *x) { memcpy(x->e->A, x->e->p, sizeof x->e->p); }
static void fxpd_B(t_fxpd *x) { memcpy(x->e->B, x->e->p, sizeof x->e->p); }
static void fxpd_morph(t_fxpd *x, t_floatarg f) { fx_morph(x->e, f); x->e->cur = -1; fxpd_push(x); }
static void fxpd_data(t_fxpd *x, t_floatarg f) { x->e->data = fx_clamp(f, 0, 1); }
static void fxpd_datamap(t_fxpd *x, t_symbol *s, t_floatarg f)
{
    if (s == gensym("clear")) { x->e->ndmap = 0; return; }
    int r = fx_data_map(x->e, s->s_name, f);
    if (r == -1) pd_error(x, "%s: datamap needs a continuous parameter name (send 'params')", x->d->name);
    else if (r == -2) pd_error(x, "%s: at most 4 data mappings (use 'datamap clear')", x->d->name);
}
static void fxpd_store(t_fxpd *x, t_symbol *s, int argc, t_atom *argv)
{
    char nm[FX_NAME * 2];
    (void)s;
    fxpd_atoms_to_string(argc, argv, nm, sizeof nm);
    if (!nm[0]) snprintf(nm, sizeof nm, "user %d", x->e->nuser + 1);
    if (fx_store(x->e, nm) < 0) pd_error(x, "%s: user preset memory is full", x->d->name);
    fxpd_report(x);
}
static void fxpd_save(t_fxpd *x, t_symbol *s)
{
    char p[1024]; fxpd_path(x, s->s_name, p, sizeof p);
    int n = fx_save_user(x->e, p);
    if (n < 0) pd_error(x, "%s: cannot write '%s'", x->d->name, p); else post("%s: saved %d user presets to %s", x->d->name, n, p);
}
static void fxpd_load(t_fxpd *x, t_symbol *s)
{
    char p[1024]; fxpd_path(x, s->s_name, p, sizeof p);
    int n = fx_load_user(x->e, p);
    if (n < 0) pd_error(x, "%s: cannot read '%s'", x->d->name, p);
    else { post("%s: loaded %d user presets from %s", x->d->name, n, p); fxpd_report(x); }
}
static void fxpd_bang(t_fxpd *x) { fxpd_report(x); }

static void fxpd_setup(const char *name, fx_pick_fn pick)
{
    fxpd_pick_g = pick; fxpd_cname_g = name;
    post("=^..^=  %s  version 1 (2026)  Sofia Vallejo Budziszewski  IEM KUG Graz Austria", name);
    { char cn[64]; snprintf(cn, sizeof cn, "fxpd_param_%s", name);
      fxproxy_class = class_new(gensym(cn), 0, 0, sizeof(t_fxproxy), CLASS_PD | CLASS_NOINLET, 0); }
    class_addfloat(fxproxy_class, (t_method)fxproxy_float);
    fxpd_class = class_new(gensym(name), (t_newmethod)fxpd_new, (t_method)fxpd_free, sizeof(t_fxpd), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(fxpd_class, t_fxpd, x_f);
    class_addmethod(fxpd_class, (t_method)fxpd_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_set, gensym("set"), A_GIMME, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_params, gensym("params"), 0);
    class_addmethod(fxpd_class, (t_method)fxpd_preset, gensym("preset"), A_GIMME, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_next, gensym("next"), 0);
    class_addmethod(fxpd_class, (t_method)fxpd_prev, gensym("prev"), 0);
    class_addmethod(fxpd_class, (t_method)fxpd_mutate, gensym("mutate"), A_DEFFLOAT, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_A, gensym("A"), 0);
    class_addmethod(fxpd_class, (t_method)fxpd_B, gensym("B"), 0);
    class_addmethod(fxpd_class, (t_method)fxpd_morph, gensym("morph"), A_FLOAT, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_store, gensym("store"), A_GIMME, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_data, gensym("data"), A_FLOAT, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_datamap, gensym("datamap"), A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_save, gensym("save"), A_SYMBOL, 0);
    class_addmethod(fxpd_class, (t_method)fxpd_load, gensym("load"), A_SYMBOL, 0);
    class_addbang(fxpd_class, (t_method)fxpd_bang);
    class_addanything(fxpd_class, (t_method)fxpd_anything);
}

#define FX_PD_CLASS(pdname, pickfn, setupname) \
    void setupname(void) { fxpd_setup(pdname, pickfn); }

#endif /* FX_PD */
#endif
