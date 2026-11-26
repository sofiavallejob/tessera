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
struct _class { char name[24]; t_newmethod newm; t_method freem; size_t size; struct meth m[64]; int nm; t_method listm; };
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

/* ---- the code under test ---- */
#include "dx7~.c"

/* ---- test driver ---- */
static t_class *cls; static t_dx7_tilde *obj;

static void dispatch(const char *line)
{
    char buf[512]; char *tok[64]; int nt = 0, i, argc;
    t_atom av[64]; struct meth *m = NULL; t_symbol *sel;
    strncpy(buf, line, 511); buf[511] = 0;
    for (char *t = strtok(buf, " "); t && nt < 64; t = strtok(NULL, " ")) tok[nt++] = t;
    printf(">> %s\n", line);
    if (nt == 0) return;
    {
        char *end; strtod(tok[0], &end);
        if (*end == 0) {                      /* all-numeric -> list */
            for (i = 0; i < nt; i++) SETFLOAT(&av[i], (t_float)atof(tok[i]));
            ((void (*)(void *, t_symbol *, int, t_atom *))cls->listm)(obj, &s_list, nt, av);
            return;
        }
    }
    sel = gensym(tok[0]); argc = nt - 1;
    for (i = 0; i < argc; i++) {
        char *end; double v = strtod(tok[i + 1], &end);
        if (*end == 0) SETFLOAT(&av[i], (t_float)v); else SETSYMBOL(&av[i], gensym(tok[i + 1]));
    }
    for (i = 0; i < cls->nm; i++) if (!strcmp(cls->m[i].sel, tok[0])) { m = &cls->m[i]; break; }
    if (!m) { printf("   (no such method: %s)\n", tok[0]); n_errors++; return; }
    if (m->n && m->types[0] == A_GIMME) {
        ((void (*)(void *, t_symbol *, int, t_atom *))m->fn)(obj, sel, argc, av);
    } else if (m->n && m->types[0] == A_SYMBOL) {
        ((void (*)(void *, t_symbol *))m->fn)(obj, argc ? atom_getsymbol(&av[0]) : gensym(""));
    } else {
        t_float f[3] = { 0, 0, 0 };
        for (i = 0; i < m->n && i < 3 && i < argc; i++) f[i] = atom_getfloat(&av[i]);
        switch (m->n) {
        case 0: ((void (*)(void *))m->fn)(obj); break;
        case 1: ((void (*)(void *, t_float))m->fn)(obj, f[0]); break;
        case 2: ((void (*)(void *, t_float, t_float))m->fn)(obj, f[0], f[1]); break;
        default: ((void (*)(void *, t_float, t_float, t_float))m->fn)(obj, f[0], f[1], f[2]); break;
        }
    }
}

static float run_blocks(int blocks, int n)
{
    static t_sample vec[4096]; float pk = 0; int b, i;
    for (b = 0; b < blocks; b++) {
        g_args[0] = (t_int)g_perf; g_args[2] = (t_int)vec; g_args[3] = n;
        memset(vec, 0, sizeof(vec));
        g_perf(g_args);
        for (i = 0; i < n; i++) { if (!(vec[i] == vec[i])) return -1; if (fabsf((float)vec[i]) > pk) pk = fabsf((float)vec[i]); }
    }
    return pk;
}

static int check(int cond, const char *msg) { printf("%s %s\n", cond ? "ok:  " : "FAIL:", msg); return !cond; }

int main(void)
{
    int fails = 0, i; float pk; t_atom a[1];
    t_signal sig; static t_sample dummy[4096];
    const char *syx = "/home/claude/ref/msfa/app/src/main/res/raw/rom1a.syx";
    char line[600];

    dx7_tilde_setup();
    for (i = 0; i < nsym; i++) if (!strcmp(symtab[i]->s_name, "dx7~")) break;
    cls = NULL;
    /* the class pointer was returned to dx7_tilde_class */
    cls = dx7_tilde_class;
    (void)a;
    obj = (t_dx7_tilde *)((void *(*)(t_floatarg))cls->newm)(8);
    fails += check(obj != NULL, "constructor returns an object");

    memset(&sig, 0, sizeof(sig)); sig.s_n = 64; sig.s_vec = dummy; sig.s_sr = 48000;
    {
        t_signal *sp[1]; sp[0] = &sig;
        for (i = 0; i < cls->nm; i++) if (!strcmp(cls->m[i].sel, "dsp")) ((void (*)(void *, t_signal **))cls->m[i].fn)(obj, sp);
    }
    fails += check(g_perf != NULL && g_nargs == 3, "dsp method registers a perform routine");

    printf("-- silence before any note\n");
    pk = run_blocks(10, 64); fails += check(pk == 0.f, "no output with no notes");

    printf("-- load bank\n");
    snprintf(line, sizeof line, "load %s", syx); dispatch(line);
    fails += check(obj->x_nbank == 32, "bank has 32 voices");
    dispatch("program 10");
    fails += check(!strncmp((char *)obj->x_syn->patch + DX7_P_NAME, "E.PIANO 1", 9), "program 10 is E.PIANO 1");
    snprintf(line, sizeof line, "load %s 7", syx); dispatch(line);
    fails += check(obj->x_program == 7, "load with program number selects it");

    printf("-- play\n");
    dispatch("program 10");
    dispatch("60 100");
    pk = run_blocks(200, 64); printf("   peak = %.4f\n", pk); fails += check(pk > 0.05f && pk < 1.0f, "note produces bounded output");
    dispatch("note 60 0"); dispatch("noteoff 64");
    pk = run_blocks(2000, 64);
    { int act = 0; for (i = 0; i < obj->x_syn->nvoices; i++) act += obj->x_syn->v[i].active; fails += check(act == 0, "voices released after note off"); }
    (void)pk;

    printf("-- editing messages\n");
    dispatch("init"); dispatch("set algorithm 5"); dispatch("set transpose -12"); dispatch("set feedback 6");
    dispatch("op 1 level 99"); dispatch("op 2 coarse 14"); dispatch("op 2 level 70"); dispatch("op 2 detune 9");
    dispatch("op 3 mode 1"); dispatch("op 6 kvs 7"); dispatch("set lfospeed 40"); dispatch("set lfowave 4"); dispatch("set pegl1 60");
    dispatch("param 134 3"); dispatch("name HELLO"); dispatch("opswitch 2 0"); dispatch("opswitch 2 1"); dispatch("wave 1 3");
    fails += check(obj->x_syn->patch[DX7_P_ALGO] == 3 && obj->x_syn->patch[DX7_P_TRANSPOSE] == 12, "param/transpose stored correctly");
    fails += check(!strncmp((char *)obj->x_syn->patch + DX7_P_NAME, "HELLO     ", 10), "name set");
    dispatch("print");
    dispatch("bend 0.5"); dispatch("bendrange 12"); dispatch("modwheel 0.5"); dispatch("modassign 1 1"); dispatch("sustain 1");
    dispatch("tune 10"); dispatch("gain 0.5"); dispatch("dac vintage"); dispatch("dac modern");
    dispatch("64 90"); dispatch("67 90"); pk = run_blocks(200, 64);
    fails += check(pk > 0.0f, "edited patch plays");
    dispatch("sustain 0"); dispatch("panic");
    pk = run_blocks(4, 64); fails += check(pk == 0.f, "panic silences everything");

    printf("-- sysex message (whole bank as a list)\n");
    {
        FILE *f = fopen(syx, "rb"); unsigned char d[5000]; size_t n = fread(d, 1, 5000, f); fclose(f);
        t_atom *big = (t_atom *)calloc(n, sizeof(t_atom)); size_t k;
        for (k = 0; k < n; k++) SETFLOAT(&big[k], d[k]);
        obj->x_nbank = 0;
        d7t_sysex(obj, gensym("sysex"), (int)n, big);
        fails += check(obj->x_nbank == 32, "sysex list loads a 32-voice bank");
        free(big);
    }


    printf("-- library: scan, navigate, filter, favourites, save, reload\n");
    {
        uint8_t vv[40][DX7_VCED], msg[DX7_BANK_SYSEX_LEN]; uint32_t seed = 777; int k, j; FILE *f;
        mkdir("/tmp/dx7lib_test", 0755); mkdir("/tmp/dx7lib_test/banks", 0755); mkdir("/tmp/dx7lib_test/banks/sub", 0755);
        for (k = 0; k < 40; k++) {
            for (j = 0; j < DX7_VCED; j++) vv[k][j] = (uint8_t)(dx7_rand(&seed) & 0xFF);
            dx7_patch_sanitize(vv[k]); vv[k][DX7_OP_LEVEL] = 90;
            memcpy(vv[k] + DX7_P_NAME, k % 2 ? "ORGAN     " : "BELL      ", 10);
        }
        dx7_make_bank_sysex(vv, 32, msg);       f = fopen("/tmp/dx7lib_test/banks/one.syx", "wb"); fwrite(msg, 1, sizeof msg, f); fclose(f);
        dx7_make_bank_sysex(vv + 8, 32, msg);   f = fopen("/tmp/dx7lib_test/banks/sub/two.SYX", "wb"); fwrite(msg, 1, sizeof msg, f); fclose(f);
        f = fopen("/tmp/dx7lib_test/banks/notes.txt", "wb"); fputs("not a bank", f); fclose(f);
    }
    dispatch("lib scan banks");
    fails += check(obj->x_lib.n == 40 && obj->x_lib.nbanks == 2 && obj->x_lib.skipped_dup == 24, "scan reads sub-folders, ignores other files, skips duplicates (40 unique voices, 2 banks)");
    fails += check(obj->x_cur == 0 && obj->x_lib.pos == 0, "first voice selected after scan");
    dispatch("lib next"); dispatch("lib next");
    fails += check(obj->x_lib.pos == 2 && obj->x_cur == 2, "lib next");
    dispatch("lib prev"); dispatch("lib prev"); dispatch("lib prev");
    fails += check(obj->x_lib.pos == 39, "lib prev wraps around");
    dispatch("lib goto 5"); fails += check(obj->x_lib.pos == 4, "lib goto");
    dispatch("lib random"); fails += check(obj->x_cur >= 0, "lib random");
    dispatch("lib search organ"); fails += check(obj->x_lib.nview == 20, "search filters by name");
    dispatch("lib search nothingmatches"); fails += check(obj->x_lib.nview == 0, "search with no match leaves an empty view");
    dispatch("lib search clear"); fails += check(obj->x_lib.nview == 40, "search clear");
    dispatch("lib bank next"); fails += check(obj->x_lib.nview == obj->x_lib.bank_count[obj->x_lib.f_bank], "lib bank next filters to one bank");
    dispatch("lib bank all"); fails += check(obj->x_lib.nview == 40, "lib bank all");
    dispatch("lib first"); dispatch("lib fav"); fails += check(obj->x_lib.e[0].fav == 1, "lib fav marks the voice");
    dispatch("lib fav"); fails += check(obj->x_lib.e[0].fav == 0, "lib fav toggles");
    dispatch("lib fav"); dispatch("lib next"); dispatch("lib fav");
    dispatch("lib favs on"); fails += check(obj->x_lib.nview == 2, "favourites filter");
    dispatch("lib favs off");
    dispatch("lib algo 3"); fails += check(obj->x_lib.nview == 0 || obj->x_lib.e[obj->x_lib.view[0]].v[DX7_P_ALGO] == 2, "algorithm filter");
    dispatch("lib reset"); fails += check(obj->x_lib.nview == 40, "lib reset clears the filters");
    dispatch("lib goto 3"); dispatch("op 1 level 55"); dispatch("lib store MYPATCH");
    fails += check(obj->x_lib.n == 41 && obj->x_lib.e[40].fav && obj->x_lib.e[40].v[DX7_OP_LEVEL + 5 * 21] == 55 && !strncmp((char *)obj->x_lib.e[40].v + DX7_P_NAME, "MYPATCH   ", 10), "lib store keeps the edited voice under a name");
    dispatch("dx7sound");   /* unknown message is only reported */
    dispatch("64 100"); pk = run_blocks(50, 64); fails += check(pk > 0.f, "library voice plays");
    dispatch("lib A"); dispatch("lib next"); dispatch("lib B");
    dispatch("morph 0"); fails += check(!memcmp(obj->x_syn->patch, obj->x_ab[0], DX7_VCED), "morph 0 = A");
    dispatch("morph 1"); fails += check(!memcmp(obj->x_syn->patch, obj->x_ab[1], DX7_VCED), "morph 1 = B");
    dispatch("morph 0.5"); dispatch("mutate 0.4");
    pk = run_blocks(10, 64); fails += check(pk >= 0.f, "morph and mutate leave a playable voice");
    dispatch("savefavs favs.syx");
    { struct stat st; fails += check(stat("/tmp/dx7lib_test/favs.syx", &st) == 0 && st.st_size == DX7_BANK_SYSEX_LEN, "savefavs writes a 4104-byte bank"); }
    dispatch("savevoice one.syx");
    { struct stat st; fails += check(stat("/tmp/dx7lib_test/one.syx", &st) == 0 && st.st_size == DX7_VOICE_SYSEX_LEN, "savevoice writes a single-voice message"); }
    dispatch("savebank bank32.syx");
    dispatch("lib clear"); fails += check(obj->x_lib.n == 0, "lib clear empties the library");
    dispatch("favload favs.syx");
    fails += check(obj->x_lib.n == dx7_lib_count_favs(&obj->x_lib) && obj->x_lib.n == 3, "favload brings the 3 saved favourites back (into USER)");
    dispatch("load /tmp/dx7lib_test/bank32.syx"); fails += check(obj->x_nbank == 32 && obj->x_program == 0, "a saved 32-voice bank loads again");
    dispatch("lib scan /nonexistent/dir"); dispatch("lib bogus"); dispatch("morph 0.3");

    printf("-- error handling (each line must report an error and not crash)\n");
    n_errors = 0;
    dispatch("op 7 level 5"); dispatch("op 1 bogus 5"); dispatch("set foo 1"); dispatch("load /nonexistent/file.syx");
    dispatch("program 99"); dispatch("sysex 1 2 3"); dispatch("opswitch 9 1"); dispatch("wave 0 1"); dispatch("param 999 1");
    fails += check(n_errors == 9, "9 bad messages -> 9 errors");

    printf("-- free\n");
    ((void (*)(void *))cls->freem)(obj);
    fails += check(1, "free ran");

    printf("\n%s\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return fails ? 1 : 0;
}
