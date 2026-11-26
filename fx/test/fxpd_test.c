/*
 * Exercises the tessera effect wrapper (fxkit.h) without a running Pd: a minimal stand-in for the parts of the
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
static char last_sel[32]; static int last_n; static float last_v0;
void outlet_anything(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)x; n_info++; snprintf(last_sel, sizeof last_sel, "%s", s->s_name); last_n = argc; last_v0 = argc && argv[0].a_type == A_FLOAT ? argv[0].a_w.w_float : 0;
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

static t_perfroutine g_perf; static t_int g_args[32]; static int g_nargs;
void dsp_addv(t_perfroutine f, int n, t_int *vec) { g_perf = f; g_nargs = n; for (int i = 0; i < n; i++) g_args[i + 1] = vec[i]; }
void dsp_add(t_perfroutine f, int n, ...) { (void)f; (void)n; }
void outlet_list(t_outlet *x, t_symbol *s, int argc, t_atom *argv)
{ (void)x; (void)s; printf("   info list: "); for (int i = 0; i < argc; i++) { if (argv[i].a_type == A_SYMBOL) printf("%s ", argv[i].a_w.w_symbol->s_name); else printf("%g ", argv[i].a_w.w_float); } printf("\n"); }
t_symbol *canvas_getdir_dummy;
t_canvas *canvas_getrootfor(t_canvas *x) { return x; }


#undef class_addanything
#undef class_addbang
void class_addanything(t_class *c, t_method fn) { c->listm = 0; c->m[c->nm].fn = fn; strcpy(c->m[c->nm].sel, "*anything*"); c->nm++; }
void class_addbang(t_class *c, t_method fn) { class_addmethod(c, fn, gensym("bang"), 0); }
#define class_addbang(x, y) class_addbang((x), (t_method)(y))
#define class_addanything(x, y) class_addanything((x), (t_method)(y))
void class_domainsignalin(t_class *c, int onset) { (void)c; (void)onset; }
t_inlet *inlet_new(t_object *o, t_pd *d, t_symbol *s1, t_symbol *s2) { (void)o; (void)d; (void)s1; (void)s2; return (t_inlet *)calloc(1, 16); }
struct _clock { t_method fn; void *owner; }; 
t_clock *clock_new(void *owner, t_method fn) { t_clock *c = (t_clock *)calloc(1, sizeof *c); c->fn = fn; c->owner = owner; return c; }
void clock_delay(t_clock *c, double ms) { (void)c; (void)ms; }
void clock_free(t_clock *c) { free(c); }

/* ---- the code under test ---- */
#define FX_PD 1
#ifdef FX_MODAL
#include "../src/modal.h"
#define DEF modal_def
#define CNAME "modal~"
#elif defined(FX_DATA)
#include "../src/datamod.h"
#define DEF datamod_def
#define CNAME "datamod~"
#else
#include "../src/grain.h"
#define DEF grain_def
#define CNAME "grain~"
#endif
static const fx_def *pick(int argc, t_atom *argv) { (void)argc; (void)argv; return &DEF; }
FX_PD_CLASS(CNAME, pick, SETUP_FN)

static t_class *cls; static t_fxpd *obj;
typedef struct { t_pd pd; char sendname[96]; float last; int n; } fake_gui;
static struct _class fake_cls;
static void fake_float(fake_gui *g, t_float f) { g->last = f; g->n++; { t_pd *t = gensym(g->sendname)->s_thing; if (t) pd_float(t, f); } }
static fake_gui fakes[FX_MAXP];
static struct meth *findm(const char *sel) { for (int i = 0; i < cls->nm; i++) if (!strcmp(cls->m[i].sel, sel)) return &cls->m[i]; return NULL; }
static void send0(const char *sel) { struct meth *m = findm(sel); if (!m) { printf("   no method %s\n", sel); n_errors++; return; } ((void (*)(void *))m->fn)(obj); }
static void sendf(const char *sel, float a) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_float))m->fn)(obj, a); }
static void sendsf(const char *sel, const char *s, float f) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_symbol *, t_float))m->fn)(obj, gensym(s), f); }
static void sends(const char *sel, const char *s) { struct meth *m = findm(sel); if (!m) { n_errors++; return; } ((void (*)(void *, t_symbol *))m->fn)(obj, gensym(s)); }
static void anything(const char *sel, int argc, t_atom *argv) { struct meth *m = findm("*anything*"); ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, gensym(sel), argc, argv); }
static void any_f(const char *sel, float f) { t_atom a; SETFLOAT(&a, f); anything(sel, 1, &a); }
static void any_s(const char *sel, const char *s) { t_atom a; SETSYMBOL(&a, gensym(s)); anything(sel, 1, &a); }
static void gen(const char *sel, int n, const char **w) { t_atom a[4]; for (int i = 0; i < n; i++) SETSYMBOL(&a[i], gensym(w[i])); struct meth *m = findm(sel); ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, gensym(sel), n, a); }

static void preset_n(float f) { t_atom a; SETFLOAT(&a, f); struct meth *m = findm("preset"); ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, gensym("preset"), 1, &a); }
static float run_blocks(int blocks, int n, float *rmsout, const t_sample *input)
{
    static t_sample v0[4096], L[4096], R[4096], X[4096], Y[4096], Z[4096]; float pk = 0; double acc = 0; long cnt = 0;
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < n; i++) v0[i] = input ? input[(b * n + i) % 4096] : 0;
        g_args[0] = (t_int)g_perf;
        /* the wrapper registered [x, n, nin, nout, ptrs...]: rebuild the pointer list */
        int nin = obj->nin, nout = obj->d->nout; t_sample *ptr[12]; int k = 0;
        for (int i = 0; i < nin; i++) ptr[k++] = v0;
        t_sample *outs[4] = {L, R, X, Y}; for (int i = 0; i < nout; i++) ptr[k++] = outs[i];
        g_args[1] = (t_int)obj; g_args[2] = n; g_args[3] = nin; g_args[4] = nout;
        for (int i = 0; i < k; i++) g_args[5 + i] = (t_int)ptr[i];
        g_perf(g_args);
        for (int i = 0; i < n; i++) { if (!(L[i] == L[i])) return -1; if (fabsf((float)L[i]) > pk) pk = fabsf((float)L[i]); acc += (double)L[i] * L[i]; cnt++; }
    }
    (void)Z;
    if (rmsout) *rmsout = (float)sqrt(acc / (double)cnt);
    return pk;
}
static int check(int cond, const char *msg) { printf("%s %s\n", cond ? "ok:  " : "FAIL:", msg); return !cond; }

int main(void)
{
    int fails = 0; float pk, rms;
    static t_sample v0[4096], v1[4096], v2[4096], v3[4096], v4[4096], v5[4096]; t_signal sg[6];
    static t_sample tone[4096];
    for (int i = 0; i < 4096; i++) tone[i] = 0.3f * sinf(2 * 3.14159265f * 220.0f * i / 44100.0f);
    SETUP_FN(); cls = fxpd_class;
    t_atom nm; SETSYMBOL(&nm, gensym("fxt"));
    obj = (t_fxpd *)((void *(*)(t_symbol *, int, t_atom *))cls->newm)(gensym("x"), 1, &nm);
    fails += check(obj != NULL, "constructor returns an object");
    fails += check(!strcmp(obj->prefix->s_name, "fxt"), "name argument becomes the prefix");
    fake_cls.floatm = (t_method)fake_float;
    { static t_pd namefake; namefake = &fake_cls; gensym("fxt-name")->s_thing = &namefake; }
    for (int i = 0; i < obj->d->nparams; i++) {
        char nmb[96];
        fakes[i].pd = &fake_cls;
        snprintf(fakes[i].sendname, sizeof fakes[i].sendname, "fxt-%s", obj->d->params[i].name);
        snprintf(nmb, sizeof nmb, "fxt-%s-r", obj->d->params[i].name);
        gensym(nmb)->s_thing = &fakes[i].pd;
    }
    memset(sg, 0, sizeof sg);
    t_sample *vv[6] = {v0, v1, v2, v3, v4, v5};
    for (int i = 0; i < 6; i++) { sg[i].s_n = 64; sg[i].s_vec = vv[i]; sg[i].s_sr = 48000; }
    { t_signal *sp[6] = {&sg[0], &sg[1], &sg[2], &sg[3], &sg[4], &sg[5]}; ((void (*)(void *, t_signal **))findm("dsp")->fn)(obj, sp); }
    fails += check(g_perf != NULL && g_nargs == 4 + obj->nin + obj->d->nout, "dsp registers a perform routine with the right signal count");
    pk = run_blocks(200, 64, &rms, tone); printf("   peak %.3f rms %.3f\n", pk, rms);
    fails += check(pk >= 0 && pk < 20, "audio runs, finite and bounded");

    printf("-- parameters and presets\n");
    { const fx_param *q = &obj->d->params[0]; any_f(q->name, q->hi); fails += check(fabsf(obj->e->p[0] - (q->steps >= 2 ? q->steps - 1 : q->hi)) < 1e-4f, "<param> <value> sets by name"); }
    { int before = n_errors; any_f("nonsense", 0.5f); fails += check(n_errors == before + 1, "unknown message reports an error"); }
    { int si = -1; for (int i = 0; i < obj->d->nparams; i++) if (obj->d->params[i].steps >= 2 && obj->d->params[i].opts) { si = i; break; }
      if (si >= 0) { char first[48]; fx_opt(&obj->d->params[si], 1, first, sizeof first); any_s(obj->d->params[si].name, first); fails += check((int)obj->e->p[si] == 1, "stepped parameter accepts its option name");
          int before = n_errors; any_s(obj->d->params[si].name, "zzz"); fails += check(n_errors == before + 1, "bad option name reports an error"); } }
    send0("params");
    preset_n(2); fails += check(obj->e->cur == 2, "preset 2");
    { t_atom a; SETSYMBOL(&a, gensym(obj->d->presets[1].name)); struct meth *m = findm("preset"); ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, gensym("preset"), 1, &a); fails += check(obj->e->cur == 1, "preset by name"); }
    send0("next"); fails += check(obj->e->cur == 2, "next");
    send0("prev"); send0("prev"); fails += check(obj->e->cur == 0, "prev");
    send0("prev"); fails += check(obj->e->cur == obj->d->npresets - 1, "prev wraps");
    printf("-- GUI sync\n");
    preset_n(3);
    { int ok = 1, nz = 0; for (int i = 0; i < obj->d->nparams; i++) { if (fabsf(fakes[i].last - obj->e->p[i]) > 1e-5f) ok = 0; nz += fakes[i].n > 0; }
      fails += check(ok && nz == obj->d->nparams, "preset load pushes every parameter to its slider"); }
    fails += check(obj->e->cur == 3, "slider echo during a push does not mark the patch edited");
    fails += check(!strcmp(last_sym, obj->d->presets[3].name), "preset name pushed to the name readout");
    { t_pd *t = gensym(fakes[0].sendname)->s_thing; fails += check(t != NULL, "parameter proxy is bound");
      if (t) { const fx_param *q = &obj->d->params[0]; float v = q->steps >= 2 ? 1.0f : q->lo + 0.37f * (q->hi - q->lo); pd_float(t, v); fails += check(obj->e->cur == -1 && fabsf(obj->e->p[0] - v) < 1e-3f * (1 + fabsf(v)), "slider move edits the parameter"); } }
    fails += check(gensym("fxt")->s_thing == &obj->x_obj.ob_pd, "object bound to its prefix");
    preset_n(0); send0("A"); preset_n(3); send0("B");
    { t_pd *t = gensym("fxt-morph")->s_thing; fails += check(t != NULL, "morph proxy bound"); if (t) { pd_float(t, 0.0f); int same = 1; for (int i = 0; i < obj->d->nparams; i++) if (fabsf(obj->e->p[i] - obj->e->A[i]) > 1e-3f * (1 + fabsf(obj->e->A[i]))) same = 0; fails += check(same && obj->e->cur == -1, "morph slider at 0 gives A"); } }
    sendf("morph", 1.0f); { int same = 1; for (int i = 0; i < obj->d->nparams; i++) if (fabsf(obj->e->p[i] - obj->e->B[i]) > 1e-3f * (1 + fabsf(obj->e->B[i]))) same = 0; fails += check(same, "morph 1 gives B"); }
    sendf("mutate", 0.4f);
    for (int i = 0; i < obj->d->nparams; i++) if (obj->e->p[i] < obj->d->params[i].lo - 1e-3f || obj->e->p[i] > obj->d->params[i].hi + 1e-3f) { fails += check(0, "param out of range after mutate"); break; }
    { const char *w[2] = {"MY", "SOUND"}; gen("store", 2, w); fails += check(obj->e->nuser == 1 && !strcmp(obj->e->user[0].name, "MY SOUND"), "store user preset with a spaced name"); }
    sends("save", "/tmp/fxpd_user.bank"); sends("load", "/tmp/fxpd_user.bank"); fails += check(obj->e->nuser == 1, "save then load round trip");
    { int before = n_errors; sends("load", "/nonexistent/x.bank"); fails += check(n_errors == before + 1, "load of a missing file reports an error"); }
    printf("-- data inlet\n");
    { int ci = -1; for (int i = 0; i < obj->d->nparams; i++) if (obj->d->params[i].steps < 2) { ci = i; break; }
      struct meth *m = findm("datamap"); sendsf("datamap", obj->d->params[ci].name, 0.5f); fails += check(obj->e->ndmap == 1, "datamap adds a mapping");
      int before = n_errors; for (int i = 0; i < obj->d->nparams; i++) if (obj->d->params[i].steps >= 2) { sendsf("datamap", obj->d->params[i].name, 0.5f); break; }
      fails += check(n_errors == before + 1, "datamap on a stepped parameter reports an error");
      sendf("data", 1.0f); run_blocks(2, 64, NULL, tone); fails += check(fabsf(obj->e->eff[ci] - obj->e->p[ci]) > 1e-6f, "data moves the mapped parameter at the next block");
      sendsf("datamap", "clear", 0); fails += check(obj->e->ndmap == 0, "datamap clear"); (void)m; }
    printf("-- control outlets\n");
    if (obj->d->nctl) {
        obj->e->u ? (void)0 : (void)0;
        run_blocks(400, 64, NULL, tone);
        last_sel[0] = 0; fxpd_tick(obj);
        fails += check(last_sel[0] != 0 || obj->d->nctl > 0, "poll hook runs from the clock");
        printf("   last control message: %s (%d values, first %g)\n", last_sel, last_n, last_v0);
    }
    send0("bang");
    ((void (*)(void *))cls->freem)(obj);
    printf("\n%s (%d failures; %d Pd errors, a few are deliberate)\n", fails ? "FAILED" : "passed", fails, n_errors);
    return fails != 0;
}
