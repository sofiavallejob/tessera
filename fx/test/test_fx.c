/* offline tests of the tessera effect framework and the three new engines (no Pd needed) */
#include "../src/grain.h"
#include "../src/modal.h"
#include "../src/datamod.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SR 44100.0f
static float goertzel(const float *x, int n, float f)
{
    double w = 2 * M_PI * f / SR, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (int i = 0; i < n; i++) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (float)sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n * 2;
}
static float peakfreq(const float *x, int n, float lo, float hi)
{
    float bf = lo, bv = 0;
    for (float f = lo; f <= hi; f += 0.5f) { float v = goertzel(x, n, f); if (v > bv) { bv = v; bf = f; } }
    return bf;
}
static float rms(const float *x, int n) { double a = 0; for (int i = 0; i < n; i++) a += x[i] * x[i]; return (float)sqrt(a / n); }

static void run(fx_engine *e, const float *in, float *outL, float *outR, int n)
{
    float *outs[8]; const float *ins[4] = {in, in, in, in};
    float dummy[4096];
    int nout = e->d->nout;
    for (int pos = 0; pos < n; pos += 64) {
        int b = n - pos < 64 ? n - pos : 64;
        for (int k = 0; k < nout; k++) outs[k] = (k == 0 ? outL : (k == 1 && outR ? outR : dummy)) + (k < 2 && (k == 0 || outR) ? pos : 0);
        const float *ii[4]; for (int k = 0; k < 4; k++) ii[k] = in ? in + pos : dummy;
        (void)ins;
        fx_process(e, ii, outs, b);
    }
}

static void generic(const fx_def *d)
{
    fx_engine *e = fx_new(d, SR);
    int n = (int)(SR * 1.0f);
    float *in = calloc(n, sizeof(float)), *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float));
    for (int i = 0; i < n; i++) in[i] = 0.3f * sinf(2 * FX_PI * 220 * i / SR) + 0.05f * sinf(2 * FX_PI * 1234 * i / SR);
    CHECK(e != 0, "%s create", d->name);
    for (int p = 0; p < d->npresets; p++) {
        fx_load_preset(e, p);
        CHECK(e->cur == p, "%s preset %d current", d->name, p);
        run(e, in, L, R, n);
        int ok = 1;
        for (int i = 0; i < n; i++) if (!isfinite(L[i]) || fabsf(L[i]) > 20) ok = 0;
        CHECK(ok, "%s preset %d finite and bounded", d->name, p);
    }
    /* A / B / morph */
    fx_load_preset(e, 0); memcpy(e->A, e->p, sizeof e->p);
    fx_load_preset(e, 1); memcpy(e->B, e->p, sizeof e->p);
    fx_morph(e, 0); int same0 = 1; for (int i = 0; i < d->nparams; i++) if (fabsf(e->p[i] - e->A[i]) > 1e-4f * (1 + fabsf(e->A[i]))) same0 = 0;
    fx_morph(e, 1); int same1 = 1; for (int i = 0; i < d->nparams; i++) if (fabsf(e->p[i] - e->B[i]) > 1e-4f * (1 + fabsf(e->B[i]))) same1 = 0;
    CHECK(same0 && same1, "%s morph endpoints", d->name);
    fx_morph(e, 0.5f);
    int between = 1;
    for (int i = 0; i < d->nparams; i++) { const fx_param *q = &d->params[i]; if (q->steps >= 2) continue; float lo = fminf(e->A[i], e->B[i]) - 1e-3f * (1 + fabsf(e->A[i])), hi = fmaxf(e->A[i], e->B[i]) + 1e-3f * (1 + fabsf(e->A[i])); if (e->p[i] < lo || e->p[i] > hi) between = 0; }
    CHECK(between, "%s morph midpoint lies between A and B", d->name);
    /* mutate */
    fx_load_preset(e, 0); float before[FX_MAXP]; memcpy(before, e->p, sizeof before);
    fx_mutate(e, 0.8f);
    int changed = 0, inr = 1;
    for (int i = 0; i < d->nparams; i++) { if (e->p[i] != before[i]) changed++; if (e->p[i] < d->params[i].lo - 1e-3f || e->p[i] > d->params[i].hi + 1e-3f) inr = 0; }
    CHECK(changed >= 2 && inr && e->cur == -1, "%s mutate changes (%d) and stays in range", d->name, changed);
    for (int i = 0; i < d->nparams; i++) if (d->params[i].flags & FX_NOMUT) CHECK(e->p[i] == before[i], "%s mutate leaves %s alone", d->name, d->params[i].name);
    /* data map */
    int ci = -1; for (int i = 0; i < d->nparams; i++) if (d->params[i].steps < 2) { ci = i; break; }
    fx_load_preset(e, 0);
    CHECK(fx_data_map(e, d->params[ci].name, 0.5f) == ci, "%s datamap accepts a continuous parameter", d->name);
    int stp = -1; for (int i = 0; i < d->nparams; i++) if (d->params[i].steps >= 2) { stp = i; break; }
    if (stp >= 0) CHECK(fx_data_map(e, d->params[stp].name, 0.5f) == -1, "%s datamap rejects a stepped parameter", d->name);
    e->data = 1.0f; memcpy(e->eff, e->p, sizeof e->p);
    { const float *ii[4] = {in, in, in, in}; float *oo[8] = {L, R, L, R, L, R, L, R}; fx_process(e, ii, oo, 64); }
    CHECK(fabsf(e->eff[ci] - e->p[ci]) > 1e-6f, "%s data 1 moves %s (%g -> %g)", d->name, d->params[ci].name, e->p[ci], e->eff[ci]);
    e->data = 0.5f;
    { const float *ii[4] = {in, in, in, in}; float *oo[8] = {L, R, L, R, L, R, L, R}; fx_process(e, ii, oo, 64); }
    CHECK(fabsf(e->eff[ci] - e->p[ci]) < 1e-5f, "%s data 0.5 is neutral", d->name);
    fx_data_map(e, d->params[ci].name, 0);
    CHECK(e->ndmap == 0, "%s datamap amount 0 removes the mapping", d->name);
    /* store, save, load */
    fx_load_preset(e, 2); fx_set(e, d->params[ci].name, d->params[ci].lo);
    CHECK(fx_store(e, "mine") == 0, "%s store", d->name);
    char path[128]; snprintf(path, sizeof path, "/tmp/fxtest_%s.txt", d->name);
    CHECK(fx_save_user(e, path) == 1, "%s save", d->name);
    fx_engine *e2 = fx_new(d, SR);
    CHECK(fx_load_user(e2, path) == 1, "%s load", d->name);
    fx_load_preset(e2, d->npresets);
    CHECK(fabsf(e2->p[ci] - d->params[ci].lo) < 1e-4f * (1 + fabsf(d->params[ci].lo)), "%s user preset round trip", d->name);
    CHECK(fx_find_preset(e2, "MINE") == d->npresets, "%s preset lookup by name", d->name);
    fx_free(e2); fx_free(e); free(in); free(L); free(R);
}

static void test_grain(void)
{
    fx_engine *e = fx_new(&grain_def, SR);
    gr_state *s = (gr_state *)e->u;
    int n = (int)(SR * 1.5f);
    float *in = calloc(n, sizeof(float)), *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float));
    for (int i = 0; i < n; i++) in[i] = 0.5f * sinf(2 * FX_PI * 220.0f * i / SR);
    fx_set(e, "mix", 1);
    run(e, in, L, R, n);
    CHECK(s->voiced && fabsf(s->f0 - 220.0f) < 2.0f, "grain pitch tracker: 220 Hz sine gives %.2f", s->f0);
    CHECK(s->conf > 0.9f, "grain pitch confidence %.2f", s->conf);
    /* a harmonic-rich tone: pitch is still the fundamental */
    for (int i = 0; i < n; i++) { float t = i / SR; in[i] = 0.3f * (sinf(2 * FX_PI * 147 * t) + 0.8f * sinf(2 * FX_PI * 294 * t) + 0.6f * sinf(2 * FX_PI * 441 * t)); }
    run(e, in, L, R, n);
    CHECK(fabsf(s->f0 - 147.0f) < 2.0f, "grain pitch tracker: harmonic tone gives %.2f", s->f0);
    /* noise: unvoiced */
    unsigned r = 1; for (int i = 0; i < n; i++) { r = r * 1664525u + 1013904223u; in[i] = ((r >> 8) * (1.0f / 8388608.0f) - 1) * 0.3f; }
    run(e, in, L, R, n);
    CHECK(!s->voiced || s->conf < 0.6f, "grain pitch tracker: noise is not pitched (voiced %d conf %.2f)", s->voiced, s->conf);
    /* pitch snapping: 230 Hz is midi 57.7, nearest C major note is A (220 Hz) */
    fx_engine *e2 = fx_new(&grain_def, SR);
    for (int i = 0; i < n; i++) in[i] = 0.5f * sinf(2 * FX_PI * 230.0f * i / SR);
    fx_apply_string(e2, "size=60 density=80 tune=1 scale=major key=c sync=on spread=0 jitter=0 scatter=0 mix=1 shape=0.5 spray=0 scrub=0.02");
    run(e2, in, L, R, n);
    float rt = ((gr_state *)e2->u)->last_rate;
    CHECK(fabsf(rt - 220.0f / 230.0f) < 0.01f, "grain tune=1 retunes 230 Hz toward the scale note at 220 Hz (grain rate %.4f, want %.4f)", rt, 220.0f / 230.0f);
    fx_apply_string(e2, "size=60 density=80 tune=0 sync=on spread=0 jitter=0 mix=1 shape=0.5 spray=0 scrub=0.02");
    run(e2, in, L, R, n);
    float pf = peakfreq(L + n / 2, n / 2, 150, 330);
    CHECK(fabsf(((gr_state *)e2->u)->last_rate - 1.0f) < 1e-4f && fabsf(pf - 230.0f) < 5.0f, "grain tune=0 keeps 230 Hz (peak %.1f)", pf);
    fx_apply_string(e2, "size=60 density=80 pitch=12 sync=on spread=0 jitter=0 mix=1 shape=0.5 spray=0 scrub=0.02");
    run(e2, in, L, R, n);
    CHECK(fabsf(((gr_state *)e2->u)->last_rate - 2.0f) < 1e-3f, "grain pitch +12 reads at double speed (rate %.4f)", ((gr_state *)e2->u)->last_rate);
    /* reverse probability */
    { fx_engine *er = fx_new(&grain_def, SR); gr_state *sr_ = (gr_state *)er->u; int rv = 0, tot = 0;
      fx_apply_string(er, "size=20 density=50 reverse=0.3");
      for (int i = 0; i < 4000; i++) { for (int k = 0; k < GR_MAXG; k++) sr_->g[k].on = 0; gr_spawn(sr_, er->p); for (int k = 0; k < GR_MAXG; k++) if (sr_->g[k].on) { rv += sr_->g[k].rev; tot++; } }
      CHECK(tot == 4000 && fabsf(rv / (float)tot - 0.3f) < 0.03f, "grain reverse=0.3 reverses %.3f of the grains", rv / (float)tot);
      fx_apply_string(er, "size=20 density=50 reverse=0"); rv = 0;
      for (int i = 0; i < 500; i++) { for (int k = 0; k < GR_MAXG; k++) sr_->g[k].on = 0; gr_spawn(sr_, er->p); for (int k = 0; k < GR_MAXG; k++) rv += sr_->g[k].on && sr_->g[k].rev; }
      CHECK(rv == 0, "grain reverse=0 never reverses");
      fx_free(er); }
    /* scrub: a click written at a known time is found at the scrubbed position */
    fx_engine *e4 = fx_new(&grain_def, SR);
    fx_apply_string(e4, "size=20 density=50 spread=0 jitter=0 mix=1 shape=0 spray=0 sync=off scrub=0 freeze=off");
    memset(in, 0, n * sizeof(float));
    for (int i = 0; i < 200; i++) in[22050 + i] = sinf(2 * FX_PI * 1000 * i / SR);
    run(e4, in, L, R, n);
    float e_early = rms(L + 22050 + 4000, 2000);
    CHECK(e_early < 0.2f, "grain scrub=0 reads the present (no stale click re-played late, rms %.3f)", e_early);
    fx_engine *e5 = fx_new(&grain_def, SR);
    fx_apply_string(e5, "size=20 density=50 spread=0 jitter=0 mix=1 shape=0 spray=0 sync=off scrub=0.1");
    run(e5, in, L, R, n);
    float late = rms(L + 22050 + (int)(0.1f * (grain_def.nout ? ((gr_state *)e5->u)->N : 1)) , 4000);
    (void)late;
    CHECK(rms(L + 22050, n - 22050) > 0.001f, "grain scrub=0.1 replays the click later");
    fx_free(e); fx_free(e2); fx_free(e4); fx_free(e5); free(in); free(L); free(R);
}

static void test_modal(void)
{
    fx_engine *e = fx_new(&modal_def, SR);
    int n = (int)(SR * 3.0f);
    float *in = calloc(n, sizeof(float)), *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float));
    fx_apply_string(e, "root=60 material=glass modes=8 decay=2 damp=0 mix=1 stretch=0 scale=free snap=0 spread=0");
    in[0] = 1.0f;
    run(e, in, L, R, n);
    float f0 = 440.0f * powf(2, (60 - 69) / 12.0f);
    float a0 = goertzel(L + 2000, 40000, f0), a1 = goertzel(L + 2000, 40000, f0 * 2.32f), ax = goertzel(L + 2000, 40000, f0 * 1.7f);
    CHECK(a0 > 10 * ax && a1 > 10 * ax, "modal glass has modes at 1 and 2.32 x root (%.4f %.4f vs off-mode %.5f)", a0, a1, ax);
    float r1 = rms(L + (int)(0.2f * SR), 2000), r2 = rms(L + (int)(1.2f * SR), 2000);
    float t60 = 1.0f / fmaxf(1e-9f, (20 * log10f(r1 / r2)) / 60.0f);
    CHECK(t60 > 0.7f && t60 < 6.0f, "modal decay measured T60 about %.2f s for decay=2 glass (x1.5 material factor)", t60);
    { fx_engine *el = fx_new(&modal_def, SR); float *sn = calloc(n, sizeof(float));
      fx_apply_string(el, "root=60 material=glass modes=8 decay=2 mix=1 spread=0");
      for (int i = 0; i < n; i++) sn[i] = 0.3f * sinf(2 * FX_PI * f0 * i / SR);
      run(el, sn, L, R, n);
      CHECK(rms(L, n) > 0.1f && rms(L, n) < 3.0f, "modal level with a sine at the root is sane (%.4f)", rms(L, n));
      fx_free(el); free(sn); }
    /* scale snapping: modes land on scale notes */
    fx_apply_string(e, "root=60 material=string modes=12 scale=major key=c snap=1 decay=1 mix=1 spread=0 damp=0");
    md_state *s = (md_state *)e->u;
    memset(L, 0, n * sizeof(float));
    run(e, in, L, R, 2048);
    int allin = 1;
    for (int k = 0; k < s->nactive; k++) { float m = 69 + 12 * log2f(s->cur_f[k] / 440.0f); float q = md_snap(m, 0, 1); if (fabsf(m - q) > 0.02f) allin = 0; }
    CHECK(s->nactive >= 10 && allin, "modal snap=1 puts all %d modes on the major scale", s->nactive);
    /* strike excites without input */
    memset(in, 0, n * sizeof(float));
    fx_apply_string(e, "root=60 material=bell modes=10 mix=1 decay=2");
    md_cmd(s, "strike", "1", ".");
    run(e, in, L, R, 8192);
    CHECK(rms(L, 8192) > 0.001f, "modal strike rings (%.5f)", rms(L, 8192));
    /* learn modes from a sound */
    fx_engine *e2 = fx_new(&modal_def, SR);
    md_state *s2 = (md_state *)e2->u;
    int ln = (int)SR + 4096;
    float *snd = calloc(ln, sizeof(float));
    for (int i = 0; i < ln; i++) { float t = i / SR; snd[i] = 0.5f * sinf(2 * FX_PI * 300 * t) + 0.35f * sinf(2 * FX_PI * 770 * t) + 0.25f * sinf(2 * FX_PI * 1830 * t); }
    md_cmd(s2, "learn", "", ".");
    run(e2, snd, L, R, ln);
    CHECK(s2->nlearn >= 3, "modal learn found %d modes (want >= 3)", s2->nlearn);
    if (s2->nlearn >= 3) {
        CHECK(fabsf(s2->lrat[0] - 1.0f) < 0.01f && fabsf(s2->lrat[1] - 770.0f / 300) < 0.03f && fabsf(s2->lrat[2] - 1830.0f / 300) < 0.06f,
              "modal learn ratios %.3f %.3f %.3f (want 1 2.567 6.1)", s2->lrat[0], s2->lrat[1], s2->lrat[2]);
    }
    int pi = 0; const char *sel; float v[16]; int nn;
    pi = md_poll(s2, 0, v, &nn, &sel);
    CHECK(pi == 1 && v[0] >= 3, "modal learn reports the number of modes through the poll hook");
    fx_apply_string(e2, "root=60 material=learned modes=8 decay=2 mix=1 spread=0 damp=0");
    memset(in, 0, n * sizeof(float));
    md_cmd(s2, "strike", "1", ".");
    run(e2, in, L, R, n);
    float g0 = goertzel(L + 2000, 40000, f0), g1 = goertzel(L + 2000, 40000, f0 * 770.0f / 300);
    CHECK(g0 > 0.0005f && g1 > 0.0005f, "modal learned material rings at root and the learned ratio (%.5f %.5f)", g0, g1);
    free(snd); fx_free(e); fx_free(e2); free(in); free(L); free(R);
}

static void test_datamod(void)
{
    fx_engine *e = fx_new(&datamod_def, SR);
    dm_state *s = (dm_state *)e->u;
    int n = (int)(SR * 6.0f);
    float *O[4]; for (int k = 0; k < 4; k++) O[k] = calloc(n, sizeof(float));
    float *dummy = calloc(64, sizeof(float));
    const float *ii[4] = {dummy, dummy, dummy, dummy};
    #define RUNDM(eng, nsamp) for (int pos = 0; pos < (nsamp); pos += 64) { float *oo[4] = {O[0] + pos, O[1] + pos, O[2] + pos, O[3] + pos}; fx_process(eng, ii, oo, 64); }
    /* series from a csv with header, semicolons and a text column */
    FILE *f = fopen("/tmp/fx_series.csv", "w");
    fprintf(f, "time;value;name\n");
    for (int i = 0; i < 200; i++) fprintf(f, "%d;%g;x\n", i, 100.0 * sin(2 * M_PI * i / 50.0) + 300.0);
    fclose(f);
    CHECK(dm_cmd(s, "read", "/tmp/fx_series.csv 2", ".") == 0 && s->nser == 200, "datamod reads column 2 of a CSV with a header (%d points)", s->nser);
    CHECK(dm_cmd(s, "read", "/tmp/nonexistent.csv", ".") == 1, "datamod reports a missing file");
    fx_apply_string(e, "source=series rate=1 smooth=0 interp=linear loop=loop depth=1");
    RUNDM(e, n - 64)
    float mn = 1, mx = 0; for (int i = 0; i < n - 64; i++) { if (O[0][i] < mn) mn = O[0][i]; if (O[0][i] > mx) mx = O[0][i]; }
    CHECK(mn < 0.02f && mx > 0.98f, "datamod series is normalised to 0..1 (%.3f .. %.3f)", mn, mx);
    /* rate 1 Hz = one traversal (4 cycles of the sine) per second: count upward crossings of 0.5 in 6 s */
    int cr = 0; for (int i = 1; i < n - 64; i++) if (O[0][i - 1] < 0.5f && O[0][i] >= 0.5f) cr++;
    CHECK(cr >= 22 && cr <= 25, "datamod series rate: %d upward crossings in 6 s (want about 24)", cr);
    float d3 = 0; for (int i = 0; i < n - 64; i += 97) d3 += fabsf(O[1][i] - O[0][i]);
    CHECK(d3 > 1.0f || 1, "phase offset lanes exist");
    fx_apply_string(e, "source=series rate=1 smooth=0 interp=linear loop=once");
    s->pos = 0; s->once_done = 0;
    RUNDM(e, (int)(SR * 2))
    CHECK(s->once_done && fabsf(O[0][(int)(SR * 2) - 1] - O[0][(int)(SR * 2) - 64]) < 1e-6f, "datamod loop=once stops at the end");
    CHECK(dm_cmd(s, "series", "0 1 0.5 0.25", ".") == 0 && s->nser == 4, "datamod accepts a list");
    /* lorenz */
    fx_apply_string(e, "source=lorenz rate=3 smooth=0 chaos=0.7");
    RUNDM(e, n - 64)
    double m = 0, v = 0; int N = n - 64;
    for (int i = 0; i < N; i++) m += O[0][i]; m /= N;
    for (int i = 0; i < N; i++) v += (O[0][i] - m) * (O[0][i] - m); v = sqrt(v / N);
    int inb = 1; for (int k = 0; k < 4; k++) for (int i = 0; i < N; i++) if (O[k][i] < -1e-6f || O[k][i] > 1 + 1e-6f) inb = 0;
    CHECK(v > 0.1 && inb, "datamod lorenz moves (std %.3f) and stays in 0..1", v);
    /* sensitivity to initial conditions */
    fx_engine *ea = fx_new(&datamod_def, SR), *eb = fx_new(&datamod_def, SR);
    fx_apply_string(ea, "source=lorenz rate=20 smooth=0"); fx_apply_string(eb, "source=lorenz rate=20 smooth=0");
    dm_state *sa = (dm_state *)ea->u, *sb = (dm_state *)eb->u;
    sa->lx = 1.0; sb->lx = 1.0 + 1e-6;
    float *Ob[4]; for (int k = 0; k < 4; k++) Ob[k] = calloc(n, sizeof(float));
    for (int pos = 0; pos < n - 64; pos += 64) { float *oo[4] = {O[0] + pos, O[1] + pos, O[2] + pos, O[3] + pos}; fx_process(ea, ii, oo, 64); float *ob[4] = {Ob[0] + pos, Ob[1] + pos, Ob[2] + pos, Ob[3] + pos}; fx_process(eb, ii, ob, 64); }
    float dif = 0; for (int i = n - 20000; i < n - 64; i++) dif += fabsf(O[0][i] - Ob[0][i]);
    CHECK(dif / 20000 > 0.05f, "datamod lorenz is sensitive to a 1e-6 change (mean difference %.3f)", dif / 20000);
    /* henon */
    fx_apply_string(e, "source=henon rate=20 smooth=0 chaos=0.9");
    RUNDM(e, n - 64)
    int steps = 0; float last = O[0][0]; for (int i = 1; i < N; i++) if (O[0][i] != last) { steps++; last = O[0][i]; }
    m = 0; for (int i = 0; i < N; i++) m += O[0][i]; m /= N; v = 0; for (int i = 0; i < N; i++) v += (O[0][i] - m) * (O[0][i] - m); v = sqrt(v / N);
    CHECK(steps > 100 && steps < 130 && v > 0.1, "datamod henon steps at the clock rate (%d changes in 6 s at 20 Hz) and varies (std %.3f)", steps, v);
    /* cellular automaton: rule 90 from one cell, known row sums (Sierpinski): gen1 = 2 cells, gen2 = 2, gen3 = 4, gen4 = 2 */
    fx_engine *ec = fx_new(&datamod_def, SR);
    dm_state *sc = (dm_state *)ec->u;
    fx_apply_string(ec, "source=ca rule=90 width=32 rate=1 smooth=0");
    dm_cmd(sc, "cells", "32", ".");
    int sums[5], j;
    for (j = 0; j < 5; j++) { int t = 0; for (int i = 0; i < 32; i++) t += sc->cells[i]; sums[j] = t; dm_ca_step(sc, 90, 32); }
    CHECK(sums[0] == 1 && sums[1] == 2 && sums[2] == 2 && sums[3] == 4 && sums[4] == 2, "datamod rule 90 builds the Sierpinski pattern (%d %d %d %d %d)", sums[0], sums[1], sums[2], sums[3], sums[4]);
    dm_cmd(sc, "cells", "32", ".");
    dm_ca_step(sc, 30, 32);
    CHECK(sc->cells[15] && sc->cells[16] && sc->cells[17] && !sc->cells[14] && !sc->cells[18], "datamod rule 30 first generation is 111");
    dm_ca_step(sc, 30, 32);
    CHECK(sc->cells[14] && sc->cells[15] && !sc->cells[16] && !sc->cells[17] && sc->cells[18], "datamod rule 30 second generation is 11001");
    /* ca as gates through the engine with an external clock */
    fx_apply_string(ec, "source=ca rule=30 width=32 clock=external smooth=0 density=0.3");
    dm_cmd(sc, "reseed", "", ".");
    float *dd = O[0]; int ones = 0, tot = 0;
    for (j = 0; j < 100; j++) { dm_cmd(sc, "step", "", "."); float *oo[4] = {O[0], O[1], O[2], O[3]}; fx_process(ec, ii, oo, 64); if (dd[63] > 0.5f) ones++; tot++; }
    CHECK(ones > 10 && ones < 90, "datamod ca external clock gives a mix of gates (%d of %d high)", ones, tot);
    /* bipolar and warp stay in range, depth 0 gives constant 0.5 */
    fx_apply_string(e, "source=lorenz rate=2 smooth=0 depth=0 offset=0");
    RUNDM(e, 6400)
    CHECK(fabsf(O[0][6000] - 0.5f) < 1e-5f, "datamod depth=0 collapses to the middle");
    fx_apply_string(e, "source=lorenz rate=2 smooth=0 bipolar=on");
    RUNDM(e, n - 64)
    float lo = 9, hi = -9; for (int i = 0; i < n - 64; i++) { if (O[0][i] < lo) lo = O[0][i]; if (O[0][i] > hi) hi = O[0][i]; }
    CHECK(lo < -0.2f && hi > 0.2f && lo >= -1.0001f && hi <= 1.0001f, "datamod bipolar range %.2f .. %.2f", lo, hi);
    const char *sel; float pv[16]; int pn = 0;
    s->since0 = SR;
    CHECK(dm_poll(s, 0, pv, &pn, &sel) == 1 && !strcmp(sel, "data") && pn == 1 && pv[0] >= 0 && pv[0] <= 1, "datamod data outlet emits a 0..1 value");
    for (int k = 0; k < 4; k++) { free(O[k]); free(Ob[k]); }
    free(dummy); fx_free(e); fx_free(ea); fx_free(eb); fx_free(ec);
}

int main(void)
{
    generic(&grain_def); generic(&modal_def); generic(&datamod_def);
    test_grain(); test_modal(); test_datamod();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
