/*
 * datamod.h - data modulators. Header only, no Pd dependency.
 * Four sources turn into four control-rate signals in 0..1 (or -1..1):
 *   series   any data series (CSV column, list, Pd array), played back as an LFO
 *   lorenz   Lorenz attractor (x y z and speed)
 *   henon    Henon map (x y, activity, inverted x)
 *   ca       one-dimensional cellular automaton (any of the 256 rules), four gate tracks
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#ifndef DATAMOD_H
#define DATAMOD_H
#include "fxkit.h"

#define DM_CELLS 64
static const fx_param datamod_params[] = {
    {"source", 0, 3, 1, 0, 4, "series|lorenz|henon|ca"},
    {"rate",   0.02f, 200, 1, FX_LOG, 0, 0},
    {"smooth", 0, 2000, 60, 0, 0, 0},
    {"depth",  0, 1, 1, 0, 0, 0},
    {"offset", -1, 1, 0, 0, 0, 0},
    {"bipolar",0, 1, 0, FX_NOMUT, 2, "off|on"},
    {"interp", 0, 2, 1, 0, 3, "hold|linear|cubic"},
    {"loop",   0, 2, 1, 0, 3, "once|loop|pingpong"},
    {"chaos",  0, 1, 0.8f, 0, 0, 0},
    {"rule",   0, 255, 30, 0, 256, 0},
    {"width",  8, 64, 32, FX_INT, 0, 0},
    {"density",0, 1, 0.3f, 0, 0, 0},
    {"clock",  0, 1, 0, FX_NOMUT, 2, "internal|external"},
    {"warp",  -1, 1, 0, 0, 0, 0},
};
static const fx_preset datamod_presets[] = {
    {"Default", ""},
    {"Lorenz drift",  "source=lorenz rate=0.6 smooth=80 chaos=0.8"},
    {"Lorenz fast",   "source=lorenz rate=6 smooth=10 chaos=0.9"},
    {"Henon steps",   "source=henon rate=4 smooth=120 chaos=0.9"},
    {"Henon edge",    "source=henon rate=9 smooth=15 chaos=0.75"},
    {"Rule 30 gates", "source=ca rule=30 rate=6 smooth=0 width=32 density=0.2"},
    {"Rule 90 tree",  "source=ca rule=90 rate=4 smooth=0 width=32 density=0.05"},
    {"Rule 110 grid", "source=ca rule=110 rate=8 smooth=20 width=48 density=0.3"},
    {"Series loop",   "source=series rate=0.5 smooth=40 interp=cubic loop=loop"},
};

typedef struct {
    float sr;
    float *ser; int nser; float maxd;
    double pos; int dir; int once_done;
    double lx, ly, lz; double hx, hy;
    unsigned char cells[DM_CELLS]; int gen;
    double phase;
    float tgt[4], cur[4];
    float prim;                 /* out1 after mapping, 0..1, for the data outlet */
    unsigned rng;
    float since0, since1, last0; int stepflag, cellflag;
} dm_state;

static float dm_rand(dm_state *s) { s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5; return (s->rng >> 8) * (1.0f / 16777216.0f); }

static void dm_seed_ca(dm_state *s, int width, float density)
{
    memset(s->cells, 0, sizeof s->cells);
    for (int i = 0; i < width && i < DM_CELLS; i++) s->cells[i] = dm_rand(s) < density;
    if (density <= 0.0f) s->cells[width / 2] = 1;
    s->gen = 0;
}
static void dm_reseed(dm_state *s, unsigned seed)
{
    s->rng = seed ? seed * 2654435761u + 1u : 2463534242u;
    s->lx = 4.0 + dm_rand(s) * 4.0; s->ly = 4.0 + dm_rand(s) * 4.0; s->lz = 22.0 + dm_rand(s) * 4.0;      /* start on the attractor so the first seconds already move */
    s->hx = 0.1 + dm_rand(s) * 0.2; s->hy = 0.1;
    s->pos = 0; s->dir = 1; s->once_done = 0;
}
static void *dm_create(float sr, const fx_def *d)
{
    (void)d;
    dm_state *s = (dm_state *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->sr = sr; s->dir = 1;
    dm_reseed(s, 7);
    dm_seed_ca(s, 32, 0.3f);
    for (int i = 0; i < 4; i++) s->tgt[i] = s->cur[i] = 0.5f;
    return s;
}
static void dm_destroy(void *u) { dm_state *s = (dm_state *)u; free(s->ser); free(s); }

/* ---- series ---- */
static int dm_set_series(dm_state *s, const float *v, int n)
{
    if (n < 2) return -1;
    float *c = (float *)malloc((size_t)n * sizeof(float)), lo = v[0], hi = v[0], md = 0;
    if (!c) return -1;
    for (int i = 0; i < n; i++) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
    for (int i = 0; i < n; i++) c[i] = hi > lo ? (v[i] - lo) / (hi - lo) : 0.5f;
    for (int i = 1; i < n; i++) { float dd = fabsf(c[i] - c[i - 1]); if (dd > md) md = dd; }
    float *old = s->ser;
    s->nser = 0; s->ser = c; s->maxd = md > 1e-6f ? md : 1.0f; s->nser = n;
    s->pos = 0; s->dir = 1; s->once_done = 0;
    free(old);
    return 0;
}
/* CSV or text: one value per line or several columns separated by , ; tab or space. Lines that are not numbers (headers) are skipped. */
static int dm_read_file(dm_state *s, const char *path, int col)
{
    FILE *f = fopen(path, "r");
    char line[8192];
    float *v; int n = 0, cap = 4096, skipped = 0;
    if (!f) return -1;
    if (col < 1) col = 1;
    v = (float *)malloc((size_t)cap * sizeof(float));
    if (!v) { fclose(f); return -1; }
    while (fgets(line, sizeof line, f)) {
        char *p = line; int c = 1; int got = 0; double x = 0;
        while (*p) {
            while (*p == ',' || *p == ';' || *p == '\t' || *p == ' ' || *p == '\r' || *p == '\n') p++;
            if (!*p) break;
            char *end; double y = strtod(p, &end);
            if (end == p) { while (*p && *p != ',' && *p != ';' && *p != '\t' && *p != ' ' && *p != '\n' && *p != '\r') p++; }
            else { if (c == col) { x = y; got = 1; } p = end; }
            if (c == col && !got) { /* non-numeric cell in the wanted column */ }
            c++;
            if (c > col && got) break;
        }
        if (got && isfinite(x)) {
            if (n >= cap) { cap *= 2; float *t = (float *)realloc(v, (size_t)cap * sizeof(float)); if (!t) break; v = t; }
            v[n++] = (float)x;
        } else skipped++;
    }
    fclose(f);
    int r = dm_set_series(s, v, n);
    free(v);
    (void)skipped;
    return r == 0 ? n : -1;
}
static float dm_ser_at(const dm_state *s, double p, int interp, int wrap)
{
    int N = s->nser;
    if (N < 2) return 0.5f;
    double q = p;
    if (wrap) { q = fmod(q, N); if (q < 0) q += N; } else { if (q < 0) q = 0; if (q > N - 1) q = N - 1; }
    int i = (int)floor(q); float fr = (float)(q - i);
#define IDX(k) (wrap ? (((k) % N + N) % N) : ((k) < 0 ? 0 : ((k) > N - 1 ? N - 1 : (k))))
    if (interp == 0) return s->ser[IDX(i)];
    float y1 = s->ser[IDX(i)], y2 = s->ser[IDX(i + 1)];
    if (interp == 1) return y1 + (y2 - y1) * fr;
    float y0 = s->ser[IDX(i - 1)], y3 = s->ser[IDX(i + 2)];
    float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2 * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    float v = ((c3 * fr + c2) * fr + c1) * fr + y1;
    return v < 0 ? 0 : (v > 1 ? 1 : v);
#undef IDX
}

static void dm_lorenz(dm_state *s, double dt, double rho)
{
    const double sg = 10.0, be = 8.0 / 3.0;
    double x = s->lx, y = s->ly, z = s->lz;
#define F(X, Y, Z, dx, dy, dz) dx = sg * ((Y) - (X)); dy = (X) * (rho - (Z)) - (Y); dz = (X) * (Y) - be * (Z);
    double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
    F(x, y, z, k1x, k1y, k1z)
    F(x + 0.5 * dt * k1x, y + 0.5 * dt * k1y, z + 0.5 * dt * k1z, k2x, k2y, k2z)
    F(x + 0.5 * dt * k2x, y + 0.5 * dt * k2y, z + 0.5 * dt * k2z, k3x, k3y, k3z)
    F(x + dt * k3x, y + dt * k3y, z + dt * k3z, k4x, k4y, k4z)
#undef F
    s->lx = x + dt / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
    s->ly = y + dt / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
    s->lz = z + dt / 6 * (k1z + 2 * k2z + 2 * k3z + k4z);
    if (!isfinite(s->lx) || fabs(s->lx) > 200) { s->lx = 1; s->ly = 1; s->lz = 1; }
}
static inline float dm_c01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

static void dm_ca_step(dm_state *s, int rule, int width)
{
    unsigned char nw[DM_CELLS];
    for (int i = 0; i < width; i++) {
        int l = s->cells[(i + width - 1) % width], c = s->cells[i], r = s->cells[(i + 1) % width];
        nw[i] = (unsigned char)((rule >> (l * 4 + c * 2 + r)) & 1);
    }
    memcpy(s->cells, nw, (size_t)width);
    s->gen++;
}
static void dm_ca_tracks(dm_state *s, int width, float *t)
{
    for (int k = 0; k < 4; k++) t[k] = (float)s->cells[(width * k) / 4 + width / 8];
}

static void dm_process(void *u, const float *P, const float **in, float **out, int n)
{
    dm_state *s = (dm_state *)u;
    int src = (int)P[0], interp = (int)P[6], loop = (int)P[7], rule = (int)P[9], width = (int)P[10], ext = P[12] >= 0.5f;
    float rate = P[1], depth = P[3], offset = P[4], bip = P[5], chaos = P[8], warp = powf(2.0f, 2 * P[13]);
    float sm = P[2] * 0.001f * s->sr, coef = sm > 1.0f ? 1.0f - expf(-1.0f / sm) : 1.0f;
    (void)in;
    if (width < 8) width = 8; if (width > DM_CELLS) width = DM_CELLS;
    for (int i = 0; i < n; i++) {
        switch (src) {
        case 0:
            if (s->nser >= 2) {
                int N = s->nser;
                double step = (double)rate * (double)N / s->sr * s->dir;       /* rate = full traversals of the series per second */
                if (!s->once_done) s->pos += step;
                if (loop == 0) { if (s->pos >= N - 1) { s->pos = N - 1; s->once_done = 1; } }
                else if (loop == 1) { if (s->pos >= N) s->pos -= N; if (s->pos < 0) s->pos += N; }
                else { if (s->pos >= N - 1) { s->pos = N - 1; s->dir = -1; } if (s->pos <= 0) { s->pos = 0; s->dir = 1; } }
                int wr = loop == 1;
                s->tgt[0] = dm_ser_at(s, s->pos, interp, wr);
                s->tgt[1] = dm_ser_at(s, s->pos + N / 3.0, interp, 1);
                s->tgt[2] = dm_ser_at(s, s->pos + 2.0 * N / 3.0, interp, 1);
                int ii = (int)s->pos; if (ii > N - 2) ii = N - 2; if (ii < 0) ii = 0;
                s->tgt[3] = dm_c01(fabsf(s->ser[ii + 1] - s->ser[ii]) / s->maxd);
            }
            break;
        case 1: {
            double dt = (double)rate / s->sr;
            dm_lorenz(s, dt, 14.0 + 20.0 * chaos);
            s->tgt[0] = dm_c01((float)(0.5 + s->lx / 38.0));
            s->tgt[1] = dm_c01((float)(0.5 + s->ly / 50.0));
            s->tgt[2] = dm_c01((float)((s->lz - 2.0) / 46.0));
            s->tgt[3] = dm_c01((float)(sqrt(10.0 * (s->ly - s->lx) * 10.0 * (s->ly - s->lx)) / 300.0));
            break; }
        default: {
            int tick = 0;
            if (!ext) { s->phase += rate / s->sr; if (s->phase >= 1.0) { s->phase -= 1.0; tick = 1; } }
            if (s->stepflag) { s->stepflag = 0; tick = 1; }
            if (tick) {
                if (src == 2) {
                    double a = 1.0 + 0.45 * chaos, nx = 1 - a * s->hx * s->hx + s->hy, ny = 0.3 * s->hx;
                    if (!isfinite(nx) || fabs(nx) > 4) { nx = 0.1; ny = 0.1; }
                    float dx = (float)fabs(nx - s->hx);
                    s->hx = nx; s->hy = ny;
                    s->tgt[0] = dm_c01((float)((nx + 1.4) / 2.8));
                    s->tgt[1] = dm_c01((float)((ny + 0.45) / 0.9));
                    s->tgt[2] = dm_c01(dx / 2.5f);
                    s->tgt[3] = 1.0f - s->tgt[0];
                } else {
                    dm_ca_step(s, rule, width);
                    dm_ca_tracks(s, width, s->tgt);
                    s->cellflag = 1;
                }
                s->since1 = 1e9f;
            }
            break; }
        }
        for (int k = 0; k < 4; k++) {
            s->cur[k] += (s->tgt[k] - s->cur[k]) * coef;
            float v = powf(dm_c01(s->cur[k]), warp);
            v = 0.5f + (v - 0.5f) * depth + offset * 0.5f;
            v = dm_c01(v);
            if (k == 0) s->prim = v;
            out[k][i] = bip >= 0.5f ? v * 2 - 1 : v;
        }
    }
    s->since0 += (float)n; s->since1 += (float)n;
}

static int dm_cmd(void *u, const char *name, const char *arg, const char *dir)
{
    dm_state *s = (dm_state *)u;
    (void)dir;
    if (!strcmp(name, "read")) {
        char path[1024]; int col = 1;
        const char *sp = strrchr(arg, ' ');
        snprintf(path, sizeof path, "%s", arg);
        if (sp && atoi(sp + 1) > 0 && strchr(arg, ' ') && !strchr(sp + 1, '.')) { col = atoi(sp + 1); path[sp - arg] = 0; }
        char full[2048];
        if (path[0] == '/') snprintf(full, sizeof full, "%s", path);
        else if (path[0] == '~') snprintf(full, sizeof full, "%s%s", getenv("HOME") ? getenv("HOME") : "", path + 1);
        else snprintf(full, sizeof full, "%s/%s", dir, path);
        int n = dm_read_file(s, full, col);
        if (n < 2) { fprintf(stderr, "datamod~: could not read at least 2 numbers from column %d of %s\n", col, full); return 1; }
        return 0;
    }
    if (!strcmp(name, "series")) {
        float *v = (float *)malloc(strlen(arg) / 1 * sizeof(float) + 16); int n = 0;
        const char *p = arg;
        if (!v) return 1;
        while (*p) { char *end; double x = strtod(p, &end); if (end == p) break; v[n++] = (float)x; p = end; while (*p == ' ') p++; }
        int r = dm_set_series(s, v, n);
        free(v);
        return r;
    }
    if (!strcmp(name, "seed")) { dm_reseed(s, (unsigned)atoi(arg)); return 0; }
    if (!strcmp(name, "restart")) { s->pos = 0; s->dir = 1; s->once_done = 0; return 0; }
    if (!strcmp(name, "step")) { s->stepflag = 1; return 0; }
    if (!strcmp(name, "reseed")) { dm_seed_ca(s, 32, 0.3f); return 0; }
    if (!strcmp(name, "cells")) { int w = atoi(arg); dm_seed_ca(s, w >= 8 ? w : 32, 0.0f); return 0; }
    return 1;
}
static int dm_poll(void *u, int k, float *v, int *n, const char **sel)
{
    dm_state *s = (dm_state *)u;
    if (k == 0) {
        if (s->since0 < s->sr * 0.02f) return 0;
        s->since0 = 0; v[0] = s->prim; *n = 1; *sel = "data";
        return 1;
    }
    if (s->cellflag) {                                           /* cellular automaton: the first 16 cells of the new row */
        s->cellflag = 0;
        for (int i = 0; i < 16; i++) v[i] = (float)s->cells[i];
        *n = 16; *sel = "cells";
        return 1;
    }
    if (s->since1 < s->sr * 0.05f) return 0;
    s->since1 = 0;
    for (int i = 0; i < 4; i++) v[i] = s->cur[i];
    *n = 4; *sel = "";
    return 1;
}

static const fx_def datamod_def = {
    "datamod~", (int)(sizeof datamod_params / sizeof *datamod_params), datamod_params,
    (int)(sizeof datamod_presets / sizeof *datamod_presets), datamod_presets,
    0, 4, 2, dm_create, dm_destroy, dm_process, dm_cmd, dm_poll
};
#endif
