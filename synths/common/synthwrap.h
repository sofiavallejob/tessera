/*
 * synthwrap.h - generic Pd class around a sk_def.
 *
 * A synth source file only does:
 *     #include "sh101.h"            // defines  static const sk_def sh101_def
 *     SK_PD_CLASS(sh101, "sh101~", sh101_def)
 *
 * Messages:  note <n> <vel 0..127>   bend <-1..1>   mod <0..1>   aftertouch <0..1>
 *            sustain <0/1>   panic   set <param> <value>   params
 *            preset <n>  next  prev  mutate <0..1>  A  B  morph <0..1>
 *            store <name>  save <file>  load <file>   bendrange <semitones>
 *            data <0..1>  datamap <param> <amount>  datamap clear   samples <folder>  sample <slot> <file>
 * Arguments: [name] [voices]. The object binds to <name> (default: class name without ~),
 *            so a message box can say "; sh101 note 60 100". Every parameter is bound to
 *            <name>-<param> (float in) and pushed to <name>-<param>-r, so sliders need no wires.
 *            <name>-name and <name>-index receive the preset name and number.
 * Outlets:   signal left, signal right, preset name (symbol), info (list)
 */
#ifndef SYNTHWRAP_H
#define SYNTHWRAP_H

#include "m_pd.h"
#include "synthcore.h"

struct _skpd;
typedef struct _skproxy { t_pd pd; struct _skpd *owner; int idx; } t_skproxy;   /* one per parameter, bound to <prefix>-<param> */

typedef struct _skpd {
    t_object x_obj;
    sk_engine *e;
    const sk_def *d;
    t_outlet *o_l, *o_r, *o_name, *o_info;
    t_canvas *cv;
    float bendrange;
    t_symbol *prefix;
    t_skproxy *proxy[SK_MAXP + 1];                          /* last one: <prefix>-morph */
    int pushing;
} t_skpd;

static t_class *skpd_class, *skproxy_class;
static const sk_def *skpd_def_g;
static const char *skpd_name_g;

/* GUI sync: sliders listen on <prefix>-<param>-r, readouts on <prefix>-name and <prefix>-index */
static void skpd_push_one(t_skpd *x, int i)
{
    char nm[96]; t_pd *th;
    snprintf(nm, sizeof nm, "%s-%s-r", x->prefix->s_name, x->d->params[i].name);
    th = gensym(nm)->s_thing;
    if (th) { x->pushing++; pd_float(th, sk_to_user(&x->d->params[i], x->e->p[i])); x->pushing--; }
}
static void skpd_push(t_skpd *x)
{
    char nm[96]; t_pd *th;
    for (int i = 0; i < x->d->nparams; i++) skpd_push_one(x, i);
    snprintf(nm, sizeof nm, "%s-name", x->prefix->s_name);
    th = gensym(nm)->s_thing;
    if (th) pd_symbol(th, gensym(x->e->cur >= 0 ? sk_preset_name(x->e, x->e->cur) : "(edited)"));
    snprintf(nm, sizeof nm, "%s-index", x->prefix->s_name);
    th = gensym(nm)->s_thing;
    if (th) pd_float(th, (float)x->e->cur);
}

static void skproxy_float(t_skproxy *p, t_floatarg f)
{
    t_skpd *x = p->owner;
    if (x->pushing) return;                 /* echo from our own push */
    if (p->idx == SK_MAXP) { sk_morph(x->e, f); x->e->cur = -1; skpd_push(x); return; }
    x->e->p[p->idx] = sk_from_user(&x->d->params[p->idx], f);
    x->e->cur = -1;
}

static void skpd_report(t_skpd *x)
{
    skpd_push(x);
    t_atom a[3];
    int cur = x->e->cur;
    outlet_symbol(x->o_name, gensym(cur >= 0 ? sk_preset_name(x->e, cur) : "(edited)"));
    SETSYMBOL(a, gensym("preset"));
    SETFLOAT(a + 1, (float)cur);
    SETFLOAT(a + 2, (float)sk_total_presets(x->e));
    outlet_list(x->o_info, &s_list, 3, a);
}

static void *skpd_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    t_skpd *x = (t_skpd *)pd_new(skpd_class);
    int nv = 0;
    char pre[48];
    snprintf(pre, sizeof pre, "%s", skpd_name_g);
    { char *tl = strchr(pre, '~'); if (tl) *tl = 0; }
    x->prefix = gensym(pre);
    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_FLOAT) nv = (int)atom_getfloat(argv + i);
        else if (argv[i].a_type == A_SYMBOL) x->prefix = atom_getsymbol(argv + i);
    }
    if (nv > 64) nv = 64;
    x->d = skpd_def_g;
    x->e = sk_new(x->d, sys_getsr() > 0 ? (float)sys_getsr() : 44100.0f, nv);
    if (!x->e) return 0;
    x->cv = canvas_getcurrent();
    x->bendrange = 2.0f;
    x->o_l = outlet_new(&x->x_obj, &s_signal);
    x->o_r = outlet_new(&x->x_obj, &s_signal);
    x->o_name = outlet_new(&x->x_obj, &s_symbol);
    x->o_info = outlet_new(&x->x_obj, &s_list);
    pd_bind(&x->x_obj.ob_pd, x->prefix);                   /* "; <prefix> note 60 100" from any message box */
    for (int i = 0; i < x->d->nparams; i++) {
        char nm[96];
        t_skproxy *pr = (t_skproxy *)pd_new(skproxy_class);
        pr->owner = x; pr->idx = i; x->proxy[i] = pr;
        snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, x->d->params[i].name);
        pd_bind(&pr->pd, gensym(nm));
    }
    { char nm[96]; t_skproxy *pr = (t_skproxy *)pd_new(skproxy_class);
      pr->owner = x; pr->idx = SK_MAXP; x->proxy[SK_MAXP] = pr;
      snprintf(nm, sizeof nm, "%s-morph", x->prefix->s_name); pd_bind(&pr->pd, gensym(nm)); }
    if (x->d->npresets > 0) sk_load_preset(x->e, 0);
    return x;
}
static void skpd_free(t_skpd *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->prefix);
    for (int i = 0; i <= SK_MAXP; i++) if (x->proxy[i]) {
        char nm[96];
        if (i == SK_MAXP) snprintf(nm, sizeof nm, "%s-morph", x->prefix->s_name);
        else snprintf(nm, sizeof nm, "%s-%s", x->prefix->s_name, x->d->params[i].name);
        pd_unbind(&x->proxy[i]->pd, gensym(nm)); pd_free(&x->proxy[i]->pd);
    }
    sk_free(x->e);
}

static t_int *skpd_perform(t_int *w)
{
    t_skpd *x = (t_skpd *)w[1];
    t_sample *L = (t_sample *)w[2], *R = (t_sample *)w[3];
    int n = (int)w[4];
    sk_render(x->e, L, R, n);
    return w + 5;
}
static void skpd_dsp(t_skpd *x, t_signal **sp)
{
    float sr = sp[0]->s_sr;
    if (sr > 0 && fabsf(sr - x->e->sr) > 0.5f) {         /* rebuild voices, keep state */
        sk_engine *n = sk_new(x->d, sr, x->e->nv);
        if (n) {
            memcpy(n->p, x->e->p, sizeof n->p); memcpy(n->ps, x->e->p, sizeof n->p);
            memcpy(n->A, x->e->A, sizeof n->A); memcpy(n->B, x->e->B, sizeof n->B);
            memcpy(n->user, x->e->user, sizeof n->user); n->nuser = x->e->nuser; n->cur = x->e->cur;
            n->ndmap = x->e->ndmap; memcpy(n->dmap, x->e->dmap, sizeof n->dmap); memcpy(n->damt, x->e->damt, sizeof n->damt); sk_set_data(n, x->e->data);
            if (n->shared && x->d->user_free) x->d->user_free(n->shared);      /* keep the old shared data (loaded samples) */
            n->shared = x->e->shared; n->g.user = n->shared; x->e->shared = 0;
            sk_free(x->e); x->e = n;
        }
    }
    dsp_add(skpd_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_n);
}

/* ---- messages ---- */
static void skpd_note(t_skpd *x, t_floatarg n, t_floatarg v)
{
    if (v <= 0) sk_note_off(x->e, (int)n);
    else sk_note_on(x->e, (int)n, v / 127.0f);
}
static void skpd_bend(t_skpd *x, t_floatarg f) { x->e->g.bend = sk_clampf(f, -1.0f, 1.0f) * x->bendrange; }
static void skpd_bendrange(t_skpd *x, t_floatarg f) { x->bendrange = sk_clampf(f, 0.0f, 24.0f); }
static void skpd_mod(t_skpd *x, t_floatarg f) { x->e->g.mod = sk_clampf(f, 0.0f, 1.0f); }
static void skpd_at(t_skpd *x, t_floatarg f) { x->e->g.aftertouch = sk_clampf(f, 0.0f, 1.0f); }
static void skpd_sustain(t_skpd *x, t_floatarg f) { sk_set_sustain(x->e, f >= 0.5f); }
static void skpd_panic(t_skpd *x) { sk_panic(x->e); }
static void skpd_set(t_skpd *x, t_symbol *s, t_floatarg f)
{
    if (sk_set_param(x->e, s->s_name, f) < 0) pd_error(x, "%s: no parameter '%s' (send 'params')", skpd_name_g, s->s_name);
    else { x->e->cur = -1; skpd_push_one(x, sk_find(x->d, s->s_name)); }
}
static void skpd_params(t_skpd *x)
{
    for (int i = 0; i < x->d->nparams; i++) {
        const sk_param *p = &x->d->params[i];
        if (p->steps >= 2) post("%s: %-10s switch 0..%d  now %d", skpd_name_g, p->name, p->steps - 1, (int)sk_to_user(p, x->e->p[i]));
        else post("%s: %-10s 0..1  now %.3f", skpd_name_g, p->name, x->e->p[i]);
    }
}
static void skpd_preset(t_skpd *x, t_floatarg f) { sk_load_preset(x->e, (int)f); skpd_report(x); }
static void skpd_next(t_skpd *x) { sk_load_preset(x->e, x->e->cur + 1); skpd_report(x); }
static void skpd_prev(t_skpd *x) { sk_load_preset(x->e, x->e->cur < 0 ? 0 : x->e->cur - 1); skpd_report(x); }
static void skpd_mutate(t_skpd *x, t_floatarg f) { sk_mutate(x->e, f); x->e->cur = -1; skpd_report(x); }
static void skpd_A(t_skpd *x) { memcpy(x->e->A, x->e->p, sizeof x->e->p); }
static void skpd_B(t_skpd *x) { memcpy(x->e->B, x->e->p, sizeof x->e->p); }
static void skpd_morph(t_skpd *x, t_floatarg f) { sk_morph(x->e, f); x->e->cur = -1; skpd_push(x); }
static void skpd_path(t_skpd *x, const char *in, char *out, size_t n);
static void skpd_data(t_skpd *x, t_floatarg f) { sk_set_data(x->e, f); }
static void skpd_datamap(t_skpd *x, t_symbol *s, t_floatarg f)
{
    if (s == gensym("clear")) { x->e->ndmap = 0; return; }
    if (sk_data_map(x->e, s->s_name, f) < 0) pd_error(x, "%s: datamap needs a continuous parameter name (send 'params')", skpd_name_g);
}
static void skpd_cmd(t_skpd *x, t_symbol *s, int argc, t_atom *argv)
{
    char arg[1024] = "", buf[256];
    for (int i = 0; i < argc; i++) { atom_string(argv + i, buf, sizeof buf); if (strlen(arg) + strlen(buf) + 2 < sizeof arg) { if (i) strcat(arg, " "); strcat(arg, buf); } }
    if (!x->d->cmd) { pd_error(x, "%s: no message '%s'", skpd_name_g, s->s_name); return; }
    int r = x->d->cmd(x->e->shared, s->s_name, arg, canvas_getdir(x->cv)->s_name, x->e->sr);
    if (r != 0) pd_error(x, "%s: %s '%s' failed (see the console above)", skpd_name_g, s->s_name, arg);
    else skpd_report(x);
}
static void skpd_store(t_skpd *x, t_symbol *s, int argc, t_atom *argv)
{
    char name[SK_NAME * 2] = "", buf[64];
    (void)s;
    for (int i = 0; i < argc; i++) {
        atom_string(argv + i, buf, sizeof buf);
        if (strlen(name) + strlen(buf) + 2 < sizeof name) { if (i) strcat(name, " "); strcat(name, buf); }
    }
    sk_store_user(x->e, name);
    skpd_report(x);
}
static void skpd_path(t_skpd *x, const char *in, char *out, size_t n);
static void skpd_path(t_skpd *x, const char *in, char *out, size_t n)
{
    if (in[0] == '/') snprintf(out, n, "%s", in);
    else if (in[0] == '~') snprintf(out, n, "%s%s", getenv("HOME") ? getenv("HOME") : "", in + 1);
    else snprintf(out, n, "%s/%s", canvas_getdir(x->cv)->s_name, in);
}
static void skpd_save(t_skpd *x, t_symbol *s)
{
    char p[1024]; skpd_path(x, s->s_name, p, sizeof p);
    int n = sk_save_user(x->e, p);
    if (n < 0) pd_error(x, "%s: cannot write '%s'", skpd_name_g, p); else post("%s: saved %d user presets to %s", skpd_name_g, n, p);
}
static void skpd_load(t_skpd *x, t_symbol *s)
{
    char p[1024]; skpd_path(x, s->s_name, p, sizeof p);
    int n = sk_load_user(x->e, p);
    if (n < 0) pd_error(x, "%s: cannot read '%s'", skpd_name_g, p);
    else { post("%s: loaded %d user presets from %s", skpd_name_g, n, p); skpd_report(x); }
}

static void skpd_setup(const char *name, const sk_def *d)
{
    skpd_def_g = d; skpd_name_g = name;
    post("=^..^=  %s  version 1 (2026)  Sofia Vallejo Budziszewski  IEM KUG Graz Austria", name);
    { char cn[64]; snprintf(cn, sizeof cn, "skpd_param_%s", name);
      skproxy_class = class_new(gensym(cn), 0, 0, sizeof(t_skproxy), CLASS_PD | CLASS_NOINLET, 0); }
    class_addfloat(skproxy_class, (t_method)skproxy_float);
    skpd_class = class_new(gensym(name), (t_newmethod)skpd_new, (t_method)skpd_free, sizeof(t_skpd), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(skpd_class, (t_method)skpd_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(skpd_class, (t_method)skpd_note, gensym("note"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_bend, gensym("bend"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_bendrange, gensym("bendrange"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_mod, gensym("mod"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_at, gensym("aftertouch"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_sustain, gensym("sustain"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_panic, gensym("panic"), 0);
    class_addmethod(skpd_class, (t_method)skpd_set, gensym("set"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_params, gensym("params"), 0);
    class_addmethod(skpd_class, (t_method)skpd_preset, gensym("preset"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_next, gensym("next"), 0);
    class_addmethod(skpd_class, (t_method)skpd_prev, gensym("prev"), 0);
    class_addmethod(skpd_class, (t_method)skpd_mutate, gensym("mutate"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_A, gensym("A"), 0);
    class_addmethod(skpd_class, (t_method)skpd_B, gensym("B"), 0);
    class_addmethod(skpd_class, (t_method)skpd_morph, gensym("morph"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_store, gensym("store"), A_GIMME, 0);
    class_addmethod(skpd_class, (t_method)skpd_data, gensym("data"), A_FLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_datamap, gensym("datamap"), A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(skpd_class, (t_method)skpd_cmd, gensym("samples"), A_GIMME, 0);
    class_addmethod(skpd_class, (t_method)skpd_cmd, gensym("sample"), A_GIMME, 0);
    class_addmethod(skpd_class, (t_method)skpd_save, gensym("save"), A_SYMBOL, 0);
    class_addmethod(skpd_class, (t_method)skpd_load, gensym("load"), A_SYMBOL, 0);
}

#define SK_PD_CLASS(prefix, pdname, def) \
    void prefix##_tilde_setup(void) { skpd_setup(pdname, &(def)); }

#endif
