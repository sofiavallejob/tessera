/*
 * dx7~ -- polyphonic DX7-style 6-operator FM synthesizer for Pure Data.
 *
 * The DSP lives in dx7core.h (no Pd dependency). This file is only the Pd
 * wrapper: messages, SysEx / bank loading, and the DSP perform routine.
 *
 * See dx7~-help.pd for the message list.
 */
#include "m_pd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dx7lib.h"
#include <time.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

static t_class *dx7_tilde_class;

typedef struct _dx7_tilde {
    t_object   x_obj;
    t_outlet  *x_out_sig;
    t_outlet  *x_out_info;
    t_outlet  *x_out_name;
    t_outlet  *x_out_file;
    t_canvas  *x_canvas;
    dx7_synth *x_syn;
    uint8_t    x_bank[32][DX7_VCED];
    int        x_nbank;
    int        x_program;
    float     *x_tmp;
    int        x_tmpn;
    dx7_lib    x_lib;
    int        x_cur;                 /* library entry now sounding, -1 if none */
    uint8_t    x_ab[2][DX7_VCED];     /* morph slots A and B */
    int        x_has_ab[2];
    uint32_t   x_seed;
} t_dx7_tilde;

/* ------------------------------------------------------------------ */
/* parameter names                                                    */
/* ------------------------------------------------------------------ */

typedef struct { const char *name; int off; int max; } d7t_pname;

static const d7t_pname d7t_op_names[] = {
    { "r1", 0, 99 }, { "r2", 1, 99 }, { "r3", 2, 99 }, { "r4", 3, 99 },
    { "l1", 4, 99 }, { "l2", 5, 99 }, { "l3", 6, 99 }, { "l4", 7, 99 },
    { "bp", DX7_OP_BP, 99 }, { "ld", DX7_OP_LD, 99 }, { "rd", DX7_OP_RD, 99 },
    { "lc", DX7_OP_LC, 3 }, { "rc", DX7_OP_RC, 3 }, { "rs", DX7_OP_RS, 7 },
    { "ams", DX7_OP_AMS, 3 }, { "kvs", DX7_OP_KVS, 7 }, { "level", DX7_OP_LEVEL, 99 },
    { "mode", DX7_OP_MODE, 1 }, { "coarse", DX7_OP_COARSE, 31 },
    { "fine", DX7_OP_FINE, 99 }, { "detune", DX7_OP_DETUNE, 14 },
    { NULL, 0, 0 }
};

static const d7t_pname d7t_global_names[] = {
    { "pegr1", 126, 99 }, { "pegr2", 127, 99 }, { "pegr3", 128, 99 }, { "pegr4", 129, 99 },
    { "pegl1", 130, 99 }, { "pegl2", 131, 99 }, { "pegl3", 132, 99 }, { "pegl4", 133, 99 },
    { "algorithm", DX7_P_ALGO, 32 }, { "feedback", DX7_P_FB, 7 }, { "oscsync", DX7_P_OSCSYNC, 1 },
    { "lfospeed", DX7_P_LFO_SPEED, 99 }, { "lfodelay", DX7_P_LFO_DELAY, 99 },
    { "lfopmd", DX7_P_LFO_PMD, 99 }, { "lfoamd", DX7_P_LFO_AMD, 99 },
    { "lfosync", DX7_P_LFO_SYNC, 1 }, { "lfowave", DX7_P_LFO_WAVE, 5 },
    { "lfopms", DX7_P_LFO_PMS, 7 }, { "transpose", DX7_P_TRANSPOSE, 24 },
    { NULL, 0, 0 }
};

static const d7t_pname *d7t_find(const d7t_pname *tab, const char *name)
{
    for (; tab->name; tab++) if (!strcmp(tab->name, name)) return tab;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static void d7t_report(t_dx7_tilde *x)
{
    char nm[11];
    t_atom a[6];
    const uint8_t *p = x->x_syn->patch;
    int n;
    memcpy(nm, p + DX7_P_NAME, 10);
    nm[10] = 0;
    {   /* trim trailing blanks */
        int i = 9;
        while (i >= 0 && nm[i] == ' ') nm[i--] = 0;
    }
    /* own outlet: [route] would treat the first symbol of a name as a selector */
    outlet_symbol(x->x_out_name, gensym(nm));
    SETFLOAT(a, (t_float)x->x_program);
    outlet_anything(x->x_out_info, gensym("program"), 1, a);
    SETFLOAT(a, (t_float)(p[DX7_P_ALGO] + 1));
    outlet_anything(x->x_out_info, gensym("algo"), 1, a);
    SETFLOAT(a, (t_float)p[DX7_P_FB]);
    outlet_anything(x->x_out_info, gensym("fb"), 1, a);
    for (n = 1; n <= 6; n++) SETFLOAT(a + n - 1, (t_float)p[(6 - n) * DX7_OP_SIZE + DX7_OP_LEVEL]);
    outlet_anything(x->x_out_info, gensym("ops"), 6, a);
}

static void d7t_apply_program(t_dx7_tilde *x, int n)
{
    if (n < 0 || n >= x->x_nbank) {
        pd_error(x, "dx7~: program %d out of range (0..%d)", n, x->x_nbank - 1);
        return;
    }
    x->x_program = n;
    x->x_cur = -1;
    dx7_set_patch(x->x_syn, x->x_bank[n]);
    d7t_report(x);
}

static void d7t_take_voices(t_dx7_tilde *x, uint8_t (*voices)[DX7_VCED], int n)
{
    int i;
    for (i = 0; i < n && i < 32; i++) memcpy(x->x_bank[i], voices[i], DX7_VCED);
    x->x_nbank = n < 32 ? n : 32;
    {
        t_atom a[1];
        SETFLOAT(a, (t_float)x->x_nbank);
        outlet_anything(x->x_out_info, gensym("bank"), 1, a);
    }
    d7t_apply_program(x, 0);
}


/* ------------------------------------------------------------------ */
/* patch library, favourites, saving, morph                            */
/* ------------------------------------------------------------------ */

static void d7t_path(t_dx7_tilde *x, const char *name, char *out, size_t n)
{
    const char *home = getenv("HOME");
    if (name[0] == '/' || (name[0] && name[1] == ':')) snprintf(out, n, "%s", name);
    else if (name[0] == '~' && (name[1] == '/' || name[1] == 0) && home) snprintf(out, n, "%s%s", home, name + 1);
    else snprintf(out, n, "%s/%s", canvas_getdir(x->x_canvas)->s_name, name);
}

static void d7t_lib_report(t_dx7_tilde *x)
{
    dx7_lib *L = &x->x_lib;
    t_atom a[2];
    int cur = x->x_cur;
    SETFLOAT(a, (t_float)(L->nview ? L->pos + 1 : 0));
    SETFLOAT(a + 1, (t_float)L->nview);
    outlet_anything(x->x_out_info, gensym("lib"), 2, a);
    if (cur >= 0 && cur < L->n) {
        const dx7_entry *e = &L->e[cur];
        SETFLOAT(a, (t_float)(e->bank + 1));
        SETFLOAT(a + 1, (t_float)L->nbanks);
        outlet_anything(x->x_out_info, gensym("libbank"), 2, a);
        SETFLOAT(a, (t_float)(e->slot + 1));
        outlet_anything(x->x_out_info, gensym("slot"), 1, a);
        SETFLOAT(a, (t_float)e->fav);
        outlet_anything(x->x_out_info, gensym("fav"), 1, a);
        outlet_symbol(x->x_out_file, gensym(L->bank[e->bank]));
    } else {
        SETFLOAT(a, 0);
        outlet_anything(x->x_out_info, gensym("fav"), 1, a);
    }
}

static void d7t_lib_select(t_dx7_tilde *x, int idx)
{
    dx7_lib *L = &x->x_lib;
    if (idx < 0 || idx >= L->n) { d7t_lib_report(x); return; }
    x->x_cur = idx;
    dx7_set_patch(x->x_syn, L->e[idx].v);
    d7t_report(x);
    d7t_lib_report(x);
}

static int d7t_ext_syx(const char *n)
{
    size_t l = strlen(n);
    return l > 4 && n[l - 4] == '.' && tolower((unsigned char)n[l - 3]) == 's' &&
           tolower((unsigned char)n[l - 2]) == 'y' && tolower((unsigned char)n[l - 1]) == 'x';
}

static int d7t_scan_file(t_dx7_tilde *x, const char *path, const char *fname)
{
    FILE *f = sys_fopen(path, "rb");
    long len;
    uint8_t *buf;
    uint8_t (*voices)[DX7_VCED];
    char stem[DX7_LIB_BANKNAME];
    size_t sl;
    int maxn, n, kept = 0;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 100 || len > 4 * 1024 * 1024) { fclose(f); return 0; }
    buf = (uint8_t *)malloc((size_t)len);
    maxn = (int)(len / 128) + 2;
    voices = (uint8_t (*)[DX7_VCED])malloc((size_t)maxn * DX7_VCED);
    if (!buf || !voices) { fclose(f); free(buf); free(voices); return 0; }
    if ((long)fread(buf, 1, (size_t)len, f) == len) {
        n = dx7_parse_all(buf, (size_t)len, voices, maxn);
        snprintf(stem, sizeof(stem), "%s", fname);
        sl = strlen(stem);
        if (sl > 4) stem[sl - 4] = 0;
        if (n > 0) kept = dx7_lib_add_bank(&x->x_lib, stem, voices, n, 1);
    }
    fclose(f);
    free(buf); free(voices);
    return kept > 0 ? kept : 0;
}

#ifndef _WIN32
static int d7t_cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static void d7t_scan_dir(t_dx7_tilde *x, const char *dir, int depth, int *files)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    char **names = NULL;
    int nn = 0, cap = 0, i;
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (nn >= cap) {
            char **nb = (char **)realloc(names, (size_t)(cap ? cap * 2 : 64) * sizeof(char *));
            if (!nb) break;
            names = nb; cap = cap ? cap * 2 : 64;
        }
        names[nn] = strdup(de->d_name);
        if (names[nn]) nn++;
    }
    closedir(d);
    if (nn > 1) qsort(names, (size_t)nn, sizeof(char *), d7t_cmp_str);
    for (i = 0; i < nn && *files < 20000; i++) {
        char full[2 * MAXPDSTRING + 2];
        struct stat st;
        snprintf(full, sizeof(full), "%s/%s", dir, names[i]);
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (depth < 8) d7t_scan_dir(x, full, depth + 1, files); }
        else if (S_ISREG(st.st_mode) && d7t_ext_syx(names[i])) { (*files)++; d7t_scan_file(x, full, names[i]); }
    }
    for (i = 0; i < nn; i++) free(names[i]);
    free(names);
}
#endif

static void d7t_lib_scan(t_dx7_tilde *x, const char *name)
{
    char path[2 * MAXPDSTRING + 2];
    dx7_lib *L = &x->x_lib;
    int files = 0;
    t_atom a[4];
#ifndef _WIN32
    struct stat st;
#endif
    d7t_path(x, name, path, sizeof(path));
    L->skipped_dup = L->skipped_blank = 0;
#ifndef _WIN32
    if (stat(path, &st) != 0) { pd_error(x, "dx7~: can't find '%s'", name); return; }
    if (S_ISDIR(st.st_mode)) d7t_scan_dir(x, path, 0, &files);
    else { const char *b = strrchr(path, '/'); files = 1; d7t_scan_file(x, path, b ? b + 1 : path); }
#else
    files = 1; d7t_scan_file(x, path, path);
#endif
    if (L->n == 0) { pd_error(x, "dx7~: no DX7 voices found in '%s'", name); return; }
    dx7_lib_rebuild(L, -1);
    post("dx7~: library: %d voices in %d banks (%d duplicates and %d blank voices skipped)",
         L->n, L->nbanks, L->skipped_dup, L->skipped_blank);
    SETFLOAT(a, (t_float)L->n);
    SETFLOAT(a + 1, (t_float)L->nbanks);
    SETFLOAT(a + 2, (t_float)L->skipped_dup);
    SETFLOAT(a + 3, (t_float)L->skipped_blank);
    outlet_anything(x->x_out_info, gensym("scan"), 4, a);
    d7t_lib_select(x, dx7_lib_current(L));
}

static int d7t_write_bytes(t_dx7_tilde *x, const char *path, const uint8_t *d, size_t n)
{
    FILE *f = sys_fopen(path, "wb");
    if (!f) { pd_error(x, "dx7~: can't write '%s'", path); return 0; }
    if (fwrite(d, 1, n, f) != n) { fclose(f); pd_error(x, "dx7~: write error on '%s'", path); return 0; }
    fclose(f);
    return 1;
}

/* write voices as one or more 32-voice banks: name.syx, or name_1.syx, name_2.syx ... */
static void d7t_write_banks(t_dx7_tilde *x, const char *name, uint8_t (*v)[DX7_VCED], int n)
{
    char base[2 * MAXPDSTRING + 2], path[2 * MAXPDSTRING + 16];
    uint8_t msg[DX7_BANK_SYSEX_LEN];
    int i, files = (n + 31) / 32;
    size_t l;
    d7t_path(x, name, base, sizeof(base));
    l = strlen(base);
    if (l > 4 && d7t_ext_syx(base)) base[l - 4] = 0;
    for (i = 0; i < files; i++) {
        int cnt = n - i * 32 > 32 ? 32 : n - i * 32;
        if (files == 1) snprintf(path, sizeof(path), "%s.syx", base);
        else snprintf(path, sizeof(path), "%s_%d.syx", base, i + 1);
        dx7_make_bank_sysex(v + i * 32, cnt, msg);
        if (d7t_write_bytes(x, path, msg, sizeof(msg))) post("dx7~: saved %d voices to %s", cnt, path);
    }
}

static void d7t_savefavs(t_dx7_tilde *x, t_symbol *s)
{
    dx7_lib *L = &x->x_lib;
    int nf = dx7_lib_count_favs(L), i, k = 0;
    uint8_t (*v)[DX7_VCED];
    if (nf == 0) { pd_error(x, "dx7~: no favourites to save (use 'lib fav')"); return; }
    v = (uint8_t (*)[DX7_VCED])malloc((size_t)nf * DX7_VCED);
    if (!v) return;
    for (i = 0; i < L->n; i++) if (L->e[i].fav) memcpy(v[k++], L->e[i].v, DX7_VCED);
    d7t_write_banks(x, s->s_name, v, nf);
    free(v);
}

static void d7t_savebank(t_dx7_tilde *x, t_symbol *s)
{
    d7t_write_banks(x, s->s_name, x->x_bank, x->x_nbank);
}

static void d7t_savevoice(t_dx7_tilde *x, t_symbol *s)
{
    char path[2 * MAXPDSTRING + 2];
    uint8_t msg[DX7_VOICE_SYSEX_LEN];
    d7t_path(x, s->s_name, path, sizeof(path));
    dx7_make_voice_sysex(x->x_syn->patch, msg);
    if (d7t_write_bytes(x, path, msg, sizeof(msg))) post("dx7~: saved the current voice to %s", path);
}

/* favload <file>: mark the voices of a saved favourites bank as favourites again */
static void d7t_favload(t_dx7_tilde *x, t_symbol *s)
{
    char path[2 * MAXPDSTRING + 2];
    dx7_lib *L = &x->x_lib;
    FILE *f;
    long len;
    uint8_t *buf;
    uint8_t (*voices)[DX7_VCED];
    int n, i, marked = 0, added = 0, maxn;
    d7t_path(x, s->s_name, path, sizeof(path));
    f = sys_fopen(path, "rb");
    if (!f) { pd_error(x, "dx7~: can't open '%s'", s->s_name); return; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 100 || len > 4 * 1024 * 1024) { fclose(f); pd_error(x, "dx7~: unexpected size for '%s'", s->s_name); return; }
    buf = (uint8_t *)malloc((size_t)len);
    maxn = (int)(len / 128) + 2;
    voices = (uint8_t (*)[DX7_VCED])malloc((size_t)maxn * DX7_VCED);
    if (!buf || !voices || (long)fread(buf, 1, (size_t)len, f) != len) { fclose(f); free(buf); free(voices); return; }
    fclose(f);
    n = dx7_parse_all(buf, (size_t)len, voices, maxn);
    for (i = 0; i < n; i++) {
        int idx;
        if (dx7_voice_silent(voices[i]) || dx7_voice_is_init(voices[i])) continue;
        idx = dx7_lib_find_hash(L, dx7_voice_hash(voices[i]));
        if (idx >= 0) { L->e[idx].fav = 1; marked++; }
        else if (dx7_lib_store(L, voices[i]) >= 0) added++;
    }
    free(buf); free(voices);
    dx7_lib_rebuild(L, x->x_cur);
    post("dx7~: favourites: %d marked in the library, %d added to USER", marked, added);
    d7t_lib_report(x);
}

static void d7t_lib_store(t_dx7_tilde *x, int argc, t_atom *argv)
{
    dx7_lib *L = &x->x_lib;
    uint8_t v[DX7_VCED];
    int idx;
    memcpy(v, x->x_syn->patch, DX7_VCED);
    if (argc >= 1 && argv[0].a_type == A_SYMBOL) {
        const char *t = atom_getsymbol(argv)->s_name;
        int i, tl = (int)strlen(t);
        for (i = 0; i < 10; i++) v[DX7_P_NAME + i] = (uint8_t)(i < tl ? t[i] : ' ');
    }
    idx = dx7_lib_store(L, v);
    if (idx < 0) { pd_error(x, "dx7~: library is full"); return; }
    L->f_text[0] = 0; L->f_favs = 0; L->f_algo = 0; L->f_bank = -1;
    dx7_lib_rebuild(L, idx);
    x->x_cur = idx;
    d7t_report(x);
    d7t_lib_report(x);
}

static void d7t_lib_fav(t_dx7_tilde *x, int argc, t_atom *argv)
{
    dx7_lib *L = &x->x_lib;
    int cur = x->x_cur;
    if (cur < 0 || cur >= L->n) { d7t_lib_store(x, 0, NULL); return; }   /* a loose voice: keep it in USER */
    L->e[cur].fav = argc >= 1 ? (atom_getfloat(argv) != 0) : !L->e[cur].fav;
    if (L->f_favs) dx7_lib_rebuild(L, cur);
    d7t_lib_report(x);
}

static void d7t_lib_filters_changed(t_dx7_tilde *x)
{
    dx7_lib *L = &x->x_lib;
    dx7_lib_rebuild(L, -1);
    if (L->nview) d7t_lib_select(x, dx7_lib_current(L));
    else d7t_lib_report(x);
}

static void d7t_lib(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    dx7_lib *L = &x->x_lib;
    const char *cmd;
    (void)s;
    if (argc < 1 || argv[0].a_type != A_SYMBOL) { pd_error(x, "dx7~: usage: lib <scan|next|prev|random|goto|bank|search|favs|algo|fav|store|A|B|reset|clear> ..."); return; }
    cmd = atom_getsymbol(argv)->s_name;
    argc--; argv++;

    if (!strcmp(cmd, "scan")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) { pd_error(x, "dx7~: usage: lib scan <folder or file>"); return; }
        d7t_lib_scan(x, atom_getsymbol(argv)->s_name);
    } else if (!strcmp(cmd, "clear")) {
        dx7_lib_clear(L);
        x->x_cur = -1;
        d7t_lib_report(x);
    } else if (!strcmp(cmd, "next")) d7t_lib_select(x, dx7_lib_step(L, 1));
    else if (!strcmp(cmd, "prev")) d7t_lib_select(x, dx7_lib_step(L, -1));
    else if (!strcmp(cmd, "first")) d7t_lib_select(x, dx7_lib_goto(L, 1));
    else if (!strcmp(cmd, "last")) d7t_lib_select(x, dx7_lib_goto(L, L->nview));
    else if (!strcmp(cmd, "random")) {
        if (L->nview) d7t_lib_select(x, dx7_lib_goto(L, 1 + (int)(dx7_rand(&x->x_seed) % (uint32_t)L->nview)));
    } else if (!strcmp(cmd, "goto")) {
        if (argc >= 1) d7t_lib_select(x, dx7_lib_goto(L, (int)atom_getfloat(argv)));
    } else if (!strcmp(cmd, "bank")) {
        const char *w = (argc >= 1 && argv[0].a_type == A_SYMBOL) ? atom_getsymbol(argv)->s_name : "";
        if (!strcmp(w, "all")) L->f_bank = -1;
        else if (!strcmp(w, "next") || !strcmp(w, "prev")) {
            int b = dx7_lib_bank_step(L, w[0] == 'n' ? 1 : -1);
            if (b < 0) return;
            L->f_bank = b;
        } else if (argc >= 1 && argv[0].a_type == A_FLOAT) {
            int b = (int)atom_getfloat(argv) - 1;
            if (b < 0 || b >= L->nbanks) { pd_error(x, "dx7~: bank %d out of range (1..%d)", b + 1, L->nbanks); return; }
            L->f_bank = b;
        } else { pd_error(x, "dx7~: usage: lib bank <next|prev|all|n>"); return; }
        d7t_lib_filters_changed(x);
    } else if (!strcmp(cmd, "search")) {
        char buf[64];
        int i, l = 0;
        buf[0] = 0;
        for (i = 0; i < argc && l < 40; i++) {
            char tmp[32];
            atom_string(argv + i, tmp, sizeof(tmp));
            l += snprintf(buf + l, sizeof(buf) - (size_t)l, "%s%s", l ? " " : "", tmp);
        }
        if (!strcmp(buf, "clear")) buf[0] = 0;
        snprintf(L->f_text, sizeof(L->f_text), "%.31s", buf);
        d7t_lib_filters_changed(x);
    } else if (!strcmp(cmd, "favs")) {
        L->f_favs = argc >= 1 ? (argv[0].a_type == A_SYMBOL ? !strcmp(atom_getsymbol(argv)->s_name, "on") : atom_getfloat(argv) != 0) : !L->f_favs;
        d7t_lib_filters_changed(x);
    } else if (!strcmp(cmd, "algo")) {
        int a = argc >= 1 ? (int)atom_getfloat(argv) : 0;
        L->f_algo = a < 0 ? 0 : (a > 32 ? 32 : a);
        d7t_lib_filters_changed(x);
    } else if (!strcmp(cmd, "reset")) {
        L->f_text[0] = 0; L->f_favs = 0; L->f_algo = 0; L->f_bank = -1;
        d7t_lib_filters_changed(x);
    } else if (!strcmp(cmd, "fav")) d7t_lib_fav(x, argc, argv);
    else if (!strcmp(cmd, "store")) d7t_lib_store(x, argc, argv);
    else if (!strcmp(cmd, "A") || !strcmp(cmd, "B")) {
        int k = cmd[0] == 'B';
        memcpy(x->x_ab[k], x->x_syn->patch, DX7_VCED);
        x->x_has_ab[k] = 1;
        post("dx7~: morph slot %s = this voice", cmd);
    } else pd_error(x, "dx7~: unknown lib command '%s'", cmd);
}

static void d7t_morph(t_dx7_tilde *x, t_floatarg t)
{
    uint8_t v[DX7_VCED];
    if (!x->x_has_ab[0] || !x->x_has_ab[1]) { pd_error(x, "dx7~: set both morph slots first ('lib A' and 'lib B')"); return; }
    dx7_morph(x->x_ab[0], x->x_ab[1], t, v);
    x->x_cur = -1;
    dx7_set_patch(x->x_syn, v);
    d7t_report(x);
}

static void d7t_mutate(t_dx7_tilde *x, t_floatarg amount)
{
    uint8_t v[DX7_VCED];
    memcpy(v, x->x_syn->patch, DX7_VCED);
    dx7_mutate(v, amount, &x->x_seed);
    x->x_cur = -1;
    dx7_set_patch(x->x_syn, v);
    d7t_report(x);
}

/* ------------------------------------------------------------------ */
/* notes and performance controls                                     */
/* ------------------------------------------------------------------ */

static void d7t_note(t_dx7_tilde *x, t_floatarg n, t_floatarg v)
{
    if (v > 0) dx7_note_on(x->x_syn, (int)n, (int)v);
    else dx7_note_off(x->x_syn, (int)n);
}

static void d7t_noteoff(t_dx7_tilde *x, t_floatarg n) { dx7_note_off(x->x_syn, (int)n); }

static void d7t_list(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc >= 2) d7t_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) d7t_note(x, atom_getfloat(argv), 100);
}

static void d7t_bend(t_dx7_tilde *x, t_floatarg f)
{
    x->x_syn->bend = f < -1.f ? -1.f : (f > 1.f ? 1.f : f);
}

static void d7t_bendrange(t_dx7_tilde *x, t_floatarg f) { x->x_syn->bendrange = f < 0 ? 0 : (f > 48 ? 48 : f); }
static void d7t_modwheel(t_dx7_tilde *x, t_floatarg f) { x->x_syn->wheel = f < 0 ? 0 : (f > 1 ? 1 : f); }

static void d7t_modassign(t_dx7_tilde *x, t_floatarg p, t_floatarg a)
{
    x->x_syn->wheel_pitch = p != 0;
    x->x_syn->wheel_amp = a != 0;
}

static void d7t_sustain(t_dx7_tilde *x, t_floatarg f) { dx7_set_sustain(x->x_syn, f != 0); }
static void d7t_tune(t_dx7_tilde *x, t_floatarg cents) { x->x_syn->tune_cents = cents; }
static void d7t_gain(t_dx7_tilde *x, t_floatarg g) { x->x_syn->gain = g < 0 ? 0 : g; }
static void d7t_panic(t_dx7_tilde *x) { dx7_all_off(x->x_syn); x->x_syn->sustain = 0; }

static void d7t_dac(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc < 1) return;
    if (argv[0].a_type == A_SYMBOL) {
        const char *m = atom_getsymbol(argv)->s_name;
        if (!strcmp(m, "vintage")) x->x_syn->vintage = 1;
        else if (!strcmp(m, "modern")) x->x_syn->vintage = 0;
        else pd_error(x, "dx7~: dac mode must be 'vintage' or 'modern'");
    } else {
        x->x_syn->vintage = atom_getfloat(argv) != 0;
    }
}

/* ------------------------------------------------------------------ */
/* patch editing                                                      */
/* ------------------------------------------------------------------ */

static int d7t_opnum(t_dx7_tilde *x, t_atom *a)
{
    int n = (int)atom_getfloat(a);
    if (a->a_type != A_FLOAT || n < 1 || n > 6) {
        pd_error(x, "dx7~: operator number must be 1..6");
        return -1;
    }
    return n;
}

/* op <1-6> <name> <value> */
static void d7t_op(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    int n;
    const d7t_pname *pn;
    int v;
    (void)s;
    if (argc < 3 || argv[1].a_type != A_SYMBOL) { pd_error(x, "dx7~: usage: op <1-6> <name> <value>"); return; }
    if ((n = d7t_opnum(x, argv)) < 0) return;
    pn = d7t_find(d7t_op_names, atom_getsymbol(argv + 1)->s_name);
    if (!pn) { pd_error(x, "dx7~: unknown operator parameter '%s'", atom_getsymbol(argv + 1)->s_name); return; }
    v = (int)atom_getfloat(argv + 2);
    if (v < 0) v = 0;
    if (v > pn->max) v = pn->max;
    dx7_set_param(x->x_syn, (6 - n) * DX7_OP_SIZE + pn->off, v);
}

/* set <name> <value> */
static void d7t_set(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    const d7t_pname *pn;
    int v;
    (void)s;
    if (argc < 2 || argv[0].a_type != A_SYMBOL) { pd_error(x, "dx7~: usage: set <name> <value>"); return; }
    pn = d7t_find(d7t_global_names, atom_getsymbol(argv)->s_name);
    if (!pn) { pd_error(x, "dx7~: unknown parameter '%s'", atom_getsymbol(argv)->s_name); return; }
    v = (int)atom_getfloat(argv + 1);
    if (!strcmp(pn->name, "algorithm")) {
        if (v < 1) v = 1;
        if (v > 32) v = 32;
        v -= 1;
    } else if (!strcmp(pn->name, "transpose")) {
        if (v < -24) v = -24;
        if (v > 24) v = 24;
        v += 24;
    } else {
        if (v < 0) v = 0;
        if (v > pn->max) v = pn->max;
    }
    dx7_set_param(x->x_syn, pn->off, v);
}

/* param <vced index 0..154> <value>: raw access */
static void d7t_param(t_dx7_tilde *x, t_floatarg idx, t_floatarg v)
{
    if (idx < 0 || idx >= 145) { pd_error(x, "dx7~: parameter index must be 0..144"); return; }
    dx7_set_param(x->x_syn, (int)idx, (int)v);
}

static void d7t_name(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    char nm[11];
    int i, len;
    (void)s;
    memset(nm, ' ', 10); nm[10] = 0;
    if (argc >= 1 && argv[0].a_type == A_SYMBOL) {
        const char *t = atom_getsymbol(argv)->s_name;
        len = (int)strlen(t);
        for (i = 0; i < 10 && i < len; i++) nm[i] = t[i];
    }
    for (i = 0; i < 10; i++) dx7_set_param(x->x_syn, DX7_P_NAME + i, (unsigned char)nm[i]);
}

static void d7t_opswitch(t_dx7_tilde *x, t_floatarg n, t_floatarg on)
{
    int k = (int)n;
    if (k < 1 || k > 6) { pd_error(x, "dx7~: operator number must be 1..6"); return; }
    if (on != 0) x->x_syn->opmask |= (uint8_t)(1u << (k - 1));
    else x->x_syn->opmask &= (uint8_t)~(1u << (k - 1));
}

static void d7t_wave(t_dx7_tilde *x, t_floatarg n, t_floatarg w)
{
    int k = (int)n, wv = (int)w;
    if (k < 1 || k > 6) { pd_error(x, "dx7~: operator number must be 1..6"); return; }
    if (wv < 0) wv = 0;
    if (wv > 7) wv = 7;
    x->x_syn->wave[k - 1] = (uint8_t)wv;
}

static void d7t_init_voice(t_dx7_tilde *x)
{
    uint8_t p[DX7_VCED];
    dx7_patch_init(p);
    dx7_set_patch(x->x_syn, p);
    x->x_nbank = 1;
    memcpy(x->x_bank[0], p, DX7_VCED);
    x->x_program = 0;
    x->x_cur = -1;
    d7t_report(x);
}

/* ------------------------------------------------------------------ */
/* banks and SysEx                                                    */
/* ------------------------------------------------------------------ */

static void d7t_program(t_dx7_tilde *x, t_floatarg n) { d7t_apply_program(x, (int)n); }

/* sysex <bytes...>: a whole SysEx message (or raw dump) as a list of numbers */
static void d7t_sysex(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    static uint8_t voices[32][DX7_VCED];
    uint8_t *buf;
    int i, n;
    (void)s;
    if (argc < 1) return;
    buf = (uint8_t *)getbytes((size_t)argc);
    if (!buf) return;
    for (i = 0; i < argc; i++) {
        int b = (int)atom_getfloat(argv + i);
        buf[i] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
    }
    n = dx7_parse_sysex(buf, (size_t)argc, voices, 32);
    freebytes(buf, (size_t)argc);
    if (n < 1) { pd_error(x, "dx7~: sysex data not recognised (%d bytes)", argc); return; }
    d7t_take_voices(x, voices, n);
}

/* load <file> [program]: .syx bank (32 voices) or single voice, found relative to the patch */
static void d7t_load(t_dx7_tilde *x, t_symbol *s, int argc, t_atom *argv)
{
    static uint8_t voices[32][DX7_VCED];
    char dir[MAXPDSTRING], path[2 * MAXPDSTRING + 2], *nameres = NULL;
    uint8_t *buf;
    FILE *f;
    long len;
    int fd, n, prog = 0;
    const char *name;
    (void)s;
    if (argc < 1 || argv[0].a_type != A_SYMBOL) { pd_error(x, "dx7~: usage: load <file.syx> [program]"); return; }
    name = atom_getsymbol(argv)->s_name;
    if (argc >= 2) prog = (int)atom_getfloat(argv + 1);
    fd = canvas_open(x->x_canvas, name, "", dir, &nameres, MAXPDSTRING, 1);
    if (fd >= 0) { sys_close(fd); snprintf(path, sizeof(path), "%s/%s", dir, nameres); }
    else snprintf(path, sizeof(path), "%s", name);
    f = sys_fopen(path, "rb");
    if (!f) { pd_error(x, "dx7~: can't open '%s'", name); return; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 100 || len > 65536) { fclose(f); pd_error(x, "dx7~: '%s' has an unexpected size (%ld bytes)", name, len); return; }
    buf = (uint8_t *)getbytes((size_t)len);
    if (!buf) { fclose(f); return; }
    if ((long)fread(buf, 1, (size_t)len, f) != len) {
        fclose(f); freebytes(buf, (size_t)len);
        pd_error(x, "dx7~: read error on '%s'", name);
        return;
    }
    fclose(f);
    n = dx7_parse_all(buf, (size_t)len, voices, 32);
    freebytes(buf, (size_t)len);
    if (n < 1) { pd_error(x, "dx7~: '%s' is not a DX7 SysEx bank or voice", name); return; }
    d7t_take_voices(x, voices, n);
    if (prog > 0) d7t_apply_program(x, prog);
}

static void d7t_print(t_dx7_tilde *x)
{
    const uint8_t *p = x->x_syn->patch;
    char nm[11];
    int n;
    memcpy(nm, p + DX7_P_NAME, 10); nm[10] = 0;
    post("dx7~: \"%s\"  algorithm %d  feedback %d  transpose %+d  bank %d/%d",
         nm, p[DX7_P_ALGO] + 1, p[DX7_P_FB], (int)p[DX7_P_TRANSPOSE] - 24, x->x_program, x->x_nbank);
    for (n = 1; n <= 6; n++) {
        const uint8_t *o = p + (6 - n) * DX7_OP_SIZE;
        post("  op%d  level %2d  %s %2d.%02d  detune %+d  EG R %d %d %d %d  L %d %d %d %d",
             n, o[DX7_OP_LEVEL], o[DX7_OP_MODE] ? "fixed" : "ratio", o[DX7_OP_COARSE], o[DX7_OP_FINE],
             (int)o[DX7_OP_DETUNE] - 7, o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7]);
    }
}

/* ------------------------------------------------------------------ */
/* DSP                                                                */
/* ------------------------------------------------------------------ */

static t_int *d7t_perform(t_int *w)
{
    t_dx7_tilde *x = (t_dx7_tilde *)w[1];
    t_sample *out = (t_sample *)w[2];
    int n = (int)w[3];
#if PD_FLOATSIZE == 32
    dx7_process(x->x_syn, (float *)out, n);
#else
    int i;
    if (n > x->x_tmpn) { while (n--) *out++ = 0; return w + 4; }
    dx7_process(x->x_syn, x->x_tmp, n);
    for (i = 0; i < n; i++) out[i] = x->x_tmp[i];
#endif
    return w + 4;
}

static void d7t_dsp(t_dx7_tilde *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    dx7_set_sr(x->x_syn, sp[0]->s_sr);
#if PD_FLOATSIZE != 32
    if (n > x->x_tmpn) {
        float *nb = (float *)resizebytes(x->x_tmp, (size_t)x->x_tmpn * sizeof(float), (size_t)n * sizeof(float));
        if (nb) { x->x_tmp = nb; x->x_tmpn = n; }
    }
#else
    (void)n;
#endif
    dsp_add(d7t_perform, 3, x, sp[0]->s_vec, n);
}

/* ------------------------------------------------------------------ */
/* object life cycle                                                  */
/* ------------------------------------------------------------------ */

static void *d7t_new(t_floatarg voices)
{
    t_dx7_tilde *x = (t_dx7_tilde *)pd_new(dx7_tilde_class);
    int nv = voices >= 1 ? (int)voices : 16;
    x->x_canvas = canvas_getcurrent();
    x->x_syn = dx7_new(nv, sys_getsr());
    if (!x->x_syn) { pd_error(x, "dx7~: out of memory"); return NULL; }
    x->x_tmp = NULL;
    x->x_tmpn = 0;
#if PD_FLOATSIZE != 32
    x->x_tmp = (float *)getbytes(8192 * sizeof(float));
    x->x_tmpn = x->x_tmp ? 8192 : 0;
#endif
    x->x_out_sig = outlet_new(&x->x_obj, &s_signal);
    x->x_out_info = outlet_new(&x->x_obj, 0);
    x->x_out_name = outlet_new(&x->x_obj, &s_symbol);
    x->x_out_file = outlet_new(&x->x_obj, &s_symbol);
    dx7_lib_init(&x->x_lib);
    x->x_cur = -1;
    x->x_has_ab[0] = x->x_has_ab[1] = 0;
    x->x_seed = (uint32_t)time(NULL) ^ (uint32_t)(size_t)x;
    x->x_nbank = 1;
    dx7_patch_init(x->x_bank[0]);
    x->x_program = 0;
    dx7_set_patch(x->x_syn, x->x_bank[0]);
    return x;
}

static void d7t_free_obj(t_dx7_tilde *x)
{
    if (x->x_syn) dx7_free(x->x_syn);
    dx7_lib_free(&x->x_lib);
#if PD_FLOATSIZE != 32
    if (x->x_tmp) freebytes(x->x_tmp, (size_t)x->x_tmpn * sizeof(float));
#endif
}

void dx7_tilde_setup(void)
{
    dx7_tilde_class = class_new(gensym("dx7~"), (t_newmethod)d7t_new, (t_method)d7t_free_obj,
                                sizeof(t_dx7_tilde), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(dx7_tilde_class, (t_method)d7t_list);

    class_addmethod(dx7_tilde_class, (t_method)d7t_note, gensym("note"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_noteoff, gensym("noteoff"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_bend, gensym("bend"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_bendrange, gensym("bendrange"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_modwheel, gensym("modwheel"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_modassign, gensym("modassign"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_sustain, gensym("sustain"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_tune, gensym("tune"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_gain, gensym("gain"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_panic, gensym("panic"), 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_dac, gensym("dac"), A_GIMME, 0);

    class_addmethod(dx7_tilde_class, (t_method)d7t_load, gensym("load"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_sysex, gensym("sysex"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_program, gensym("program"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_init_voice, gensym("init"), 0);

    class_addmethod(dx7_tilde_class, (t_method)d7t_op, gensym("op"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_set, gensym("set"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_param, gensym("param"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_name, gensym("name"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_opswitch, gensym("opswitch"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_wave, gensym("wave"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_print, gensym("print"), 0);

    class_addmethod(dx7_tilde_class, (t_method)d7t_lib, gensym("lib"), A_GIMME, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_savefavs, gensym("savefavs"), A_SYMBOL, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_savebank, gensym("savebank"), A_SYMBOL, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_savevoice, gensym("savevoice"), A_SYMBOL, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_favload, gensym("favload"), A_SYMBOL, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_morph, gensym("morph"), A_FLOAT, 0);
    class_addmethod(dx7_tilde_class, (t_method)d7t_mutate, gensym("mutate"), A_FLOAT, 0);

        post("=^..^=  dx7~  version 1 (2026)  Sofia Vallejo Budziszewski  IEM KUG Graz Austria  -  DX7-style 6-operator FM synth");
}
