/* Standalone tests for the tessera spectral core. Build and run: make -C test */
#include "../src/tspec_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SR 48000.f
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float urand(unsigned *s) { *s = *s * 1664525u + 1013904223u; return ((*s >> 8) & 0xFFFF) / 32768.f - 1.f; }

static double energy_at(const float *x, int n, float freq)
{
    double re = 0, im = 0; int i;
    for (i = 0; i < n; i++)
    {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n);
        re += w * x[i] * cos(2 * M_PI * freq * i / SR);
        im += w * x[i] * sin(2 * M_PI * freq * i / SR);
    }
    return (re * re + im * im) / ((double)n * n);
}
static double rms(const float *x, int n) { double s = 0; int i; for (i = 0; i < n; i++) s += x[i] * x[i]; return sqrt(s / n); }

static void run(ts_core *c, const float *a, const float *b, float *out, int n)
{
    int i = 0;
    while (i < n) { int blk = n - i < 64 ? n - i : 64; ts_process(c, a + i, b ? b + i : NULL, out + i, blk); i += blk; }
}

int main(void)
{
    int n = 48000, N = 1024, i;
    float *in = malloc(sizeof(float) * n), *in2 = malloc(sizeof(float) * n);
    float *out = malloc(sizeof(float) * n), *zero = calloc(n, sizeof(float));
    unsigned s = 1234;
    for (i = 0; i < n; i++) in[i] = 0.5f * urand(&s);

    /* 1. every mode must be transparent with neutral settings */
    {
        const char *names[] = { "freeze off", "smear neutral", "gate open", "cross amount 0" };
        int m;
        for (m = 0; m < 4; m++)
        {
            ts_core *c = ts_new(N, SR); ts_params *p = ts_get_params(c);
            double maxerr = 0;
            p->mode = m; p->freeze = 0; p->time_ms = 0; p->width = 1; p->diffuse = 0;
            p->thresh_db = -150; p->floor_db = 0; p->amount = 0; p->cmode = TS_CROSS_MAG;
            run(c, in, in, out, n);
            for (i = 2 * N; i < n; i++) { double e = fabs(out[i] - in[i - N]); if (e > maxerr) maxerr = e; }
            CHECK(maxerr < 2e-3, "%s reconstructs input delayed by %d (max error %.2e)", names[m], ts_latency(c), maxerr);
            ts_free(c);
        }
    }
    /* 2. mix 0 is the dry signal, delayed */
    {
        ts_core *c = ts_new(N, SR); double maxerr = 0;
        ts_get_params(c)->mix = 0.f;
        run(c, in, NULL, out, n);
        for (i = 4000; i < n; i++) { double e = fabs(out[i] - in[i - N]); if (e > maxerr) maxerr = e; }
        CHECK(maxerr < 1e-6, "mix 0 gives delayed dry (max error %.2e)", maxerr);
        ts_free(c);
    }
    /* 3. freeze holds a tone after the input stops, then releases */
    {
        ts_core *c = ts_new(2048, SR); ts_params *p = ts_get_params(c);
        float *sig = calloc(n, sizeof(float)); double e1k, e3k;
        for (i = 0; i < 12000; i++) sig[i] = 0.5f * sinf(2 * M_PI * 1000.f * i / SR);
        p->mode = TS_FREEZE; p->freeze = 0;
        run(c, sig, NULL, out, 10000);
        p->freeze = 1;
        run(c, sig + 10000, NULL, out + 10000, n - 10000);
        e1k = energy_at(out + 30000, 8192, 1000.f); e3k = energy_at(out + 30000, 8192, 3000.f);
        CHECK(rms(out + 30000, 8192) > 0.05, "freeze sustains sound 0.4 s after input stopped (rms %.3f)", rms(out + 30000, 8192));
        CHECK(e1k > 20 * e3k, "frozen sound stays at 1 kHz (ratio %.0f)", e1k / (e3k + 1e-30));
        p->freeze = 0;
        run(c, zero, NULL, out, 24000);
        CHECK(rms(out + 16000, 4096) < 1e-3, "freeze releases to silence (rms %.2e)", rms(out + 16000, 4096));
        free(sig); ts_free(c);
    }
    /* 4. gate removes quiet bins */
    {
        ts_core *c = ts_new(N, SR); ts_params *p = ts_get_params(c);
        double rinv;
        for (i = 0; i < n; i++) in2[i] = 0.5f * sinf(2 * M_PI * 1000.f * i / SR) + 0.02f * urand(&s);
        p->mode = TS_GATE; p->thresh_db = -25; p->floor_db = -80; p->attack_ms = 1; p->release_ms = 1;
        run(c, in2, NULL, out, n);
        {
            double bin = 0, bout = 0; float f;
            for (f = 4000; f <= 9000; f += 250) { bin += energy_at(in2 + 20000, 8192, f); bout += energy_at(out + 20000, 8192, f); }
            CHECK(bout < 0.05 * bin, "gate drops the noise band (in %.2e out %.2e)", bin, bout);
            CHECK(energy_at(out + 20000, 8192, 1000.f) > 0.5 * energy_at(in2 + 20000, 8192, 1000.f), "gate keeps the 1 kHz tone");
        }
        ts_free(c);
        c = ts_new(N, SR); p = ts_get_params(c);
        p->mode = TS_GATE; p->thresh_db = -25; p->floor_db = -80; p->attack_ms = 1; p->release_ms = 1; p->invert = 1;
        run(c, in2, NULL, out, n);
        rinv = rms(out + 20000, 8192);
        CHECK(rinv < 0.05, "inverted gate keeps only the quiet bins (rms %.3f)", rinv);
        ts_free(c);
    }
    /* 5. smear spreads a short click into a long tail */
    {
        ts_core *c = ts_new(N, SR); ts_params *p = ts_get_params(c);
        float *clk = calloc(n, sizeof(float));
        for (i = 8000; i < 8512; i++) clk[i] = 0.5f * urand(&s);
        p->mode = TS_SMEAR; p->time_ms = 800; p->shape = 1.f; p->width = 5;
        run(c, clk, NULL, out, n);
        CHECK(rms(out + 20000, 4096) > 0.02, "smear leaves a long tail 0.25 s after a short click (rms %.3f)", rms(out + 20000, 4096));
        CHECK(rms(out + 40000, 4096) < rms(out + 20000, 4096), "smear tail decays over time");
        free(clk); ts_free(c);
    }
    /* 6. cross-synthesis: magnitude from B takes over the spectrum */
    {
        ts_core *c = ts_new(N, SR); ts_params *p = ts_get_params(c);
        float *a = malloc(sizeof(float) * n), *b = malloc(sizeof(float) * n);
        double e500, e2k;
        for (i = 0; i < n; i++) { a[i] = 0.5f * sinf(2 * M_PI * 500.f * i / SR); b[i] = 0.5f * sinf(2 * M_PI * 2000.f * i / SR); }
        p->mode = TS_CROSS; p->cmode = TS_CROSS_MAG; p->amount = 1.f;
        run(c, a, b, out, n);
        e500 = energy_at(out + 20000, 4096, 500.f); e2k = energy_at(out + 20000, 4096, 2000.f);
        CHECK(e2k > 20 * e500, "cross (magnitude) output follows input B (ratio %.0f)", e2k / (e500 + 1e-30));
        free(a); free(b); ts_free(c);
    }
    printf(fails ? "\n%d test(s) FAILED\n" : "\nall tests passed\n", fails);
    return fails ? 1 : 0;
}
