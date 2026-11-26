/*
 * Exercises dx7~.c without a running Pd: a minimal stand-in for the parts of the
 * Pd API the external uses. Build with ASan/UBSan:
 *   gcc -g -fsanitize=address,undefined -std=gnu99 -I<pd>/src -I.. pdstub_test.c -lm -o pdstub_test
 */
#include "m_pd.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---- stand-in Pd runtime ---- */
#undef class_addlist
t_symbol s_pointer, s_float = { "float", 0, 0 }, s_symbol = { "symbol", 0, 0 }, s_bang = { "bang", 0, 0 },
         s_list = { "list", 0, 0 }, s_anything = { "anything", 0, 0 }, s_signal = { "signal", 0, 0 },
         s__N, s__X, s_x, s_y;

static t_symbol *symtab[512]; static int nsym;
t_symbol *gensym(const char *s)
{
    int i;
    for (i = 0; i < nsym; i++) if (!strcmp(symtab[i]->s_name, s)) return symtab[i];
    symtab[nsym] = (t_symbol *)calloc(1, sizeof(t_symbol));
    symtab[nsym]->s_name = strdup(s);
    return symtab[nsym++];
}

struct meth { char sel[24]; t_method fn; int n; int types[8]; };
struct _class { char name[24]; t_newmethod newm; t_method freem; size_t size; struct meth m[64]; int nm; t_method listm; t_method floatm; };
struct _outlet { int dummy; };
static int n_errors, n_info;

t_class *class_new(t_symbol *name, t_newmethod newmethod, t_method freemethod, size_t size, int flags, t_atomtype a1, ...)
{
    t_class *c = (t_class *)calloc(1, sizeof(t_class));
    (void)flags; (void)a1;
    strncpy(c->name, name->s_name, 23); c->newm = newmethod; c->freem = freemethod; c->size = size;
    return c;
}
void class_addmethod(t_class *c, t_method fn, t_symbol *sel, t_atomtype a1, ...)
{
    va_list ap; int t = (int)a1; struct meth *m = &c->m[c->nm++];
    strncpy(m->sel, sel->s_name, 23); m->fn = fn;
    va_start(ap, a1);
    while (t != 0 && m->n < 8) { m->types[m->n++] = t; t = va_arg(ap, int); }
    va_end(ap);
}
void class_addlist(t_class *c, t_method fn) { c->listm = fn; }
void class_doaddfloat(t_class *c, t_method fn) { c->floatm = fn; }
void pd_bind(t_pd *x, t_symbol *s) { s->s_thing = x; }
void pd_unbind(t_pd *x, t_symbol *s) { if (s->s_thing == x) s->s_thing = 0; }
void pd_free(t_pd *x) { free(x); }
static int n_sym_push; static char last_sym[64];
void pd_float(t_pd *x, t_float f) { t_class *c = *x; if (c->floatm) ((void (*)(void *, t_float))c->floatm)(x, f); }
void pd_symbol(t_pd *x, t_symbol *s) { (void)x; n_sym_push++; snprintf(last_sym, sizeof last_sym, "%s", s->s_name); }
#define class_addlist(x, y) class_addlist((x), (t_method)(y))
t_pd *pd_new(t_class *cls) { t_pd *p = (t_pd *)calloc(1, cls->size); *p = cls; return p; }
t_outlet *outlet_new(t_object *o, t_symbol *s) { (void)o; (void)s; return (t_outlet *)calloc(1, sizeof(t_outlet)); }
void outlet_anything(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)x; n_info++;
    printf("   info outlet: %s", s->s_name);
    if (argc && argv[0].a_type == A_SYMBOL) printf(" '%s'", argv[0].a_w.w_symbol->s_name);
    else if (argc) printf(" %g", argv[0].a_w.w_float);
    printf("\n");
}
void outlet_symbol(t_outlet *x, t_symbol *s) { (void)x; n_info++; printf("   name outlet: '%s'\n", s->s_name); }
t_float atom_getfloat(const t_atom *a) { return a->a_type == A_FLOAT ? a->a_w.w_float : 0; }
t_symbol *atom_getsymbol(const t_atom *a) { return a->a_type == A_SYMBOL ? a->a_w.w_symbol : &s_symbol; }
void *getbytes(size_t n) { return calloc(1, n ? n : 1); }
void freebytes(void *p, size_t n) { (void)n; free(p); }
void *resizebytes(void *p, size_t o, size_t n) { void *q = calloc(1, n); if (p) { memcpy(q, p, o < n ? o : n); free(p); } return q; }
void pd_error(const void *o, const char *fmt, ...)
{
    va_list ap; (void)o; n_errors++;
    printf("   PD ERROR: "); va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n");
}
void post(const char *fmt, ...) { va_list ap; printf("   post: "); va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); }
t_glist *canvas_getcurrent(void) { return NULL; }
int canvas_open(const t_canvas *x, const char *name, const char *ext, char *dir, char **nameres, unsigned int size, int bin)
{ (void)x; (void)name; (void)ext; (void)dir; (void)nameres; (void)size; (void)bin; return -1; }
int sys_close(int fd) { return close(fd); }
FILE *sys_fopen(const char *f, const char *m) { return fopen(f, m); }
t_symbol *canvas_getdir(const t_glist *x) { (void)x; return gensym("/tmp/dx7lib_test"); }
void atom_string(const t_atom *a, char *buf, unsigned int n)
{ if (a->a_type == A_SYMBOL) snprintf(buf, n, "%s", a->a_w.w_symbol->s_name); else snprintf(buf, n, "%g", a->a_w.w_float); }
t_float sys_getsr(void) { return 48000; }

static t_perfroutine g_perf; static t_int g_args[8]; static int g_nargs;
void dsp_add(t_perfroutine f, int n, ...)
{
    va_list ap; int i; g_perf = f; g_nargs = n;
    va_start(ap, n); for (i = 0; i < n; i++) g_args[i + 1] = va_arg(ap, t_int); va_end(ap);
}

void outlet_list(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{ (void)x; (void)s; printf("   info list: "); for (int i = 0; i < argc; i++) { if (argv[i].a_type == A_SYMBOL) printf("%s ", argv[i].a_w.w_symbol->s_name); else printf("%g ", argv[i].a_w.w_float); } printf("\n"); }
t_symbol *canvas_getdir_dummy;

/* ---- the code under test ---- */
#ifdef SYNTH_TX
#include "tx81z.h"
#include "synthwrap.h"
SK_PD_CLASS(tx81z, "tx81z~", tx_def)
#define SETUP tx81z_tilde_setup
#elif defined(SYNTH_MS)
#include "ms20.h"
#include "synthwrap.h"
SK_PD_CLASS(ms20, "ms20~", ms_def)
#define SETUP ms20_tilde_setup
#elif defined(SYNTH_CZ)
#include "cz101.h"
#include "synthwrap.h"
SK_PD_CLASS(cz101, "cz101~", cz_def)
#define SETUP cz101_tilde_setup
#elif defined(SYNTH_MEL)
#include "mellotron.h"
#include "synthwrap.h"
SK_PD_CLASS(mellotron, "mellotron~", mel_def)
#define SETUP mellotron_tilde_setup
#elif defined(SYNTH_P5)
#include "prophet5.h"
#include "synthwrap.h"
SK_PD_CLASS(prophet5, "prophet5~", p5_def)
#define SETUP prophet5_tilde_setup
#elif defined(SYNTH_MM)
#include "minimoog.h"
#include "synthwrap.h"
SK_PD_CLASS(minimoog, "minimoog~", mm_def)
#define SETUP minimoog_tilde_setup
#elif defined(SYNTH_B3)
#include "b3.h"
#include "synthwrap.h"
SK_PD_CLASS(b3, "b3~", b3_def)
#define SETUP b3_tilde_setup
#elif defined(SYNTH_AJ)
#include "alphajuno.h"
#include "synthwrap.h"
SK_PD_CLASS(alphajuno, "alphajuno~", aj_def)
#define SETUP alphajuno_tilde_setup
#else
#include "sh101.h"
#include "synthwrap.h"
SK_PD_CLASS(sh101, "sh101~", sh101_def)
#define SETUP sh101_tilde_setup
#endif

static t_class *cls; static t_skpd *obj;
/* fake iemgui sliders: record the pushed value, then echo it to the send symbol like a real slider does */
typedef struct { t_pd pd; char sendname[96]; float last; int n; } fake_gui;
static struct _class fake_cls;
static void fake_float(fake_gui *g, t_float f) { g->last = f; g->n++; { t_pd *t = gensym(g->sendname)->s_thing; if (t) pd_float(t, f); } }
static fake_gui fakes[SK_MAXP];
static struct meth *findm(const char *sel) { for (int i = 0; i < cls->nm; i++) if (!strcmp(cls->m[i].sel, sel)) return &cls->m[i]; return NULL; }
static void send0(const char *sel) { struct meth *m = findm(sel); if (!m) { printf("   no method %s\n", sel); n_errors++; return; } ((void (*)(void *))m->fn)(obj); }
static void sendf(const char *sel, float a) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_float))m->fn)(obj, a); }
static void sendff(const char *sel, float a, float b) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_float, t_float))m->fn)(obj, a, b); }
static void sendsf(const char *sel, const char *s, float f) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_symbol *, t_float))m->fn)(obj, gensym(s), f); }
static void sends(const char *sel, const char *s) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_symbol *))m->fn)(obj, gensym(s)); }
static void sendg(const char *sel, const char *w1, const char *w2)
{ struct meth *m = findm(sel); t_atom a[2]; int n = 0; if (!m) { n_errors++; return; } SETSYMBOL(&a[n], gensym(w1)); n++; if (w2) { SETSYMBOL(&a[n], gensym(w2)); n++; }
  ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, gensym(sel), n, a); }

static float run_blocks(int blocks, int n, float *rmsout)
{
    static t_sample L[4096], R[4096]; float pk = 0; double acc = 0; long cnt = 0;
    for (int b = 0; b < blocks; b++) {
        g_args[0] = (t_int)g_perf; g_args[2] = (t_int)L; g_args[3] = (t_int)R; g_args[4] = n;
        g_perf(g_args);
        for (int i = 0; i < n; i++) { if (!(L[i] == L[i]) || !(R[i] == R[i])) return -1; if (fabsf((float)L[i]) > pk) pk = fabsf((float)L[i]); acc += (double)L[i] * L[i]; cnt++; }
    }
    if (rmsout) *rmsout = (float)sqrt(acc / (double)cnt);
    return pk;
}
static int check(int cond, const char *msg) { printf("%s %s\n", cond ? "ok:  " : "FAIL:", msg); return !cond; }

int main(void)
{
    int fails = 0; float pk, rms;
    static t_sample v0[4096], v1[4096]; t_signal s0, s1;
    SETUP(); cls = skpd_class;
    obj = (t_skpd *)((void *(*)(t_symbol *, int, t_atom *))cls->newm)(gensym("x"), 0, NULL);
    fake_cls.floatm = (t_method)fake_float;
    { static t_pd namefake; char nm[96]; namefake = &fake_cls; snprintf(nm, sizeof nm, "%s-name", obj->prefix->s_name); gensym(nm)->s_thing = &namefake; }
    for (int i = 0; i < obj->d->nparams; i++) {
        char nm[96];
        fakes[i].pd = &fake_cls;
        snprintf(fakes[i].sendname, sizeof fakes[i].sendname, "%s-%s", obj->prefix->s_name, obj->d->params[i].name);
        snprintf(nm, sizeof nm, "%s-%s-r", obj->prefix->s_name, obj->d->params[i].name);
        gensym(nm)->s_thing = &fakes[i].pd;
    }
    fails += check(obj != NULL, "constructor returns an object");
    memset(&s0, 0, sizeof s0); memset(&s1, 0, sizeof s1);
    s0.s_n = s1.s_n = 64; s0.s_vec = v0; s1.s_vec = v1; s0.s_sr = s1.s_sr = 44100;   /* different from the constructor sample rate */
    { t_signal *sp[2] = { &s0, &s1 }; ((void (*)(void *, t_signal **))findm("dsp")->fn)(obj, sp); }
    fails += check(g_perf != NULL && g_nargs == 4, "dsp registers a perform routine (rebuilds engine at 44100)");
    fails += check(fabsf(obj->e->sr - 44100.0f) < 1 && obj->e->cur == 0, "sample-rate change keeps state");
    pk = run_blocks(10, 64, NULL); fails += check(pk == 0.f, "silence before any note");

    sendff("note", 48, 100); sendff("note", 55, 100); sendff("note", 64, 100);
    pk = run_blocks(300, 64, &rms); printf("   peak %.3f rms %.3f\n", pk, rms);
    fails += check(pk > 0.02f && pk < 2.5f, "notes make bounded sound");
    sendf("bend", 1.0f); sendf("mod", 1.0f); sendf("aftertouch", 0.5f); sendf("sustain", 1);
    pk = run_blocks(50, 64, NULL); fails += check(pk >= 0, "bend, mod, aftertouch, sustain keep output finite");
    sendff("note", 48, 0); sendff("note", 55, 0); sendff("note", 64, 0); sendf("sustain", 0);
    run_blocks(1500, 64, NULL); pk = run_blocks(100, 64, &rms); fails += check(rms < 0.001f, "voices release");
    { int act = 0; for (int i = 0; i < obj->e->nv; i++) act += obj->e->slot[i].active; printf("   active slots %d rms %.4f\n", act, rms); }

    printf("-- parameters and presets\n");
    obj->e->cur = 3;
    sendsf("set", obj->d->params[0].name, 0.9f); fails += check(fabsf(obj->e->p[0] - 0.9f) < 1e-4f || obj->d->params[0].steps >= 2, "set known parameter");
    { int before = n_errors; sendsf("set", "nonsense", 0.5f); fails += check(n_errors == before + 1, "unknown parameter reports an error"); }
    send0("params");
    sendf("preset", 2); fails += check(obj->e->cur == 2, "preset 2");
    send0("next"); fails += check(obj->e->cur == 3, "next");
    send0("prev"); send0("prev"); fails += check(obj->e->cur == 1, "prev");
    sendf("preset", -1); fails += check(obj->e->cur == obj->d->npresets - 1, "preset wraps below zero");
    printf("-- GUI sync\n");
    sendf("preset", 4);
    { int ok = 1, nz = 0; for (int i = 0; i < obj->d->nparams; i++) { if (fabsf(fakes[i].last - sk_to_user(&obj->d->params[i], obj->e->p[i])) > 1e-5f) ok = 0; nz += fakes[i].n > 0; }
      fails += check(ok && nz == obj->d->nparams, "preset load pushes every parameter to its slider"); }
    fails += check(obj->e->cur == 4, "slider echo during a push does not mark the patch edited");
    fails += check(!strcmp(last_sym, obj->d->presets[4].name), "preset name pushed to the name readout");
    { t_pd *t = gensym(fakes[0].sendname)->s_thing; fails += check(t != NULL, "parameter proxy is bound"); if (t) { float v = obj->d->params[0].steps >= 2 ? 1.0f : 0.37f; pd_float(t, v); fails += check(obj->e->cur == -1 && fabsf(sk_to_user(&obj->d->params[0], obj->e->p[0]) - v) < 0.02f, "slider move edits the parameter"); } }
    fails += check(gensym(obj->d->name)->s_thing == &obj->x_obj.ob_pd, "object bound to its prefix");
    sendf("preset", 0); send0("A"); sendf("preset", 3); send0("B"); sendf("morph", 0.5f); fails += check(obj->e->cur == -1, "morph marks patch edited");
    sendf("mutate", 0.4f);
    for (int i = 0; i < obj->d->nparams; i++) if (obj->e->p[i] < 0 || obj->e->p[i] > 1) { fails += check(0, "param out of range after mutate"); break; }
    sendg("store", "MY", "SOUND"); fails += check(obj->e->nuser == 1 && !strcmp(obj->e->user[0].name, "MY SOUND"), "store user preset with spaced name");
    sends("save", "/tmp/skpd_user.bank"); sends("load", "/tmp/skpd_user.bank"); fails += check(obj->e->nuser == 2, "save then load appends user presets");
    { int before = n_errors; sends("load", "/nonexistent/x.bank"); fails += check(n_errors == before + 1, "load of missing file reports an error"); }
    sendf("preset", 0); sendff("note", 60, 90); pk = run_blocks(100, 64, NULL); fails += check(pk > 0, "plays after all that");
    send0("panic"); pk = run_blocks(200, 64, &rms); printf("   after panic rms %.6f peak %.6f\n", rms, pk); fails += check(rms < 0.001f, "panic silences");
    ((void (*)(void *))cls->freem)(obj);
    printf("\n%s (%d failures; %d Pd errors, 2 are deliberate)\n", fails ? "FAILED" : "passed", fails, n_errors);
    return fails != 0;
}
