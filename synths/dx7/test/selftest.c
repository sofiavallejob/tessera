/* Self-tests for dx7core.h. Build: gcc -O2 -std=c99 -I.. selftest.c -lm -o selftest */
#include <stdio.h>
#include <stdlib.h>
#include "dx7core.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const double SR = 48000.0;

static void render(dx7_synth *s, float *buf, int n)
{
    int done = 0;
    while (done < n) { int c = n - done < 64 ? n - done : 64; dx7_process(s, buf + done, c); done += c; }
}

/* mean frequency of a sine-like signal between samples a..b via zero crossings */
static double zc_freq(const float *x, int a, int b)
{
    int i, cnt = 0; double first = 0, last = 0;
    for (i = a + 1; i < b; i++) {
        if (x[i - 1] < 0.f && x[i] >= 0.f) {
            double t = (double)(i - 1) + (-x[i - 1]) / (x[i] - x[i - 1]);
            if (cnt == 0) first = t;
            last = t; cnt++;
        }
    }
    return cnt > 1 ? (cnt - 1) * SR / (last - first) : 0.0;
}

static double peak(const float *x, int a, int b)
{
    double m = 0; int i;
    for (i = a; i < b; i++) if (fabsf(x[i]) > m) m = fabsf(x[i]);
    return m;
}

static void sine_patch(uint8_t *p)   /* alg 32, only OP1 audible, instant on, no decay */
{
    dx7_patch_init(p);
    p[DX7_P_ALGO] = 31;
}

int main(void)
{
    static float buf[48000 * 3];
    uint8_t p[DX7_VCED];
    dx7_synth *s = dx7_new(16, SR);
    double f;
    int i;

    s->gain = 1.0f / DX7_OUT_SCALE;   /* raw units: a full-level carrier has amplitude 2.0 */

    /* 1. pitch and level of a plain operator */
    sine_patch(p); dx7_set_patch(s, p);
    dx7_note_on(s, 69, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 440.0) < 0.2, "A4 = %.3f Hz", f);
    CHECK(fabs(peak(buf, 24000, 48000) - 2.0) < 0.05, "full-level carrier amplitude %.3f (expect 2.0)", peak(buf, 24000, 48000));
    dx7_all_off(s);

    /* 2. coarse / fine / detune / fixed */
    sine_patch(p); p[5 * 21 + DX7_OP_COARSE] = 3; p[5 * 21 + DX7_OP_FINE] = 50;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 440.0 * 3 * 1.5) < 0.5, "coarse 3 fine 50 = %.2f Hz (expect %.2f)", f, 440.0 * 4.5);
    dx7_all_off(s);
    sine_patch(p); p[5 * 21 + DX7_OP_MODE] = 1; p[5 * 21 + DX7_OP_COARSE] = 2; p[5 * 21 + DX7_OP_FINE] = 0;
    dx7_set_patch(s, p); dx7_note_on(s, 30, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 100.0) < 0.2, "fixed mode coarse 2 = %.2f Hz (expect 100, independent of key)", f);
    dx7_all_off(s);
    sine_patch(p); p[5 * 21 + DX7_OP_COARSE] = 0;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 220.0) < 0.2, "coarse 0 = ratio 0.5: %.2f Hz", f);
    dx7_all_off(s);

    /* 3. output level scale: 0.75 dB per step */
    sine_patch(p); p[5 * 21 + DX7_OP_LEVEL] = 80;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    {
        double a80 = peak(buf, 24000, 48000), db = 20 * log10(a80 / 2.0);
        CHECK(fabs(db - (-0.75 * (127 - (28 + 80))) ) < 0.1, "level 80 = %.2f dB (expect %.2f)", db, -0.75 * (127 - 108));
    }
    dx7_all_off(s);

    /* 4. transpose and pitch bend */
    sine_patch(p); p[DX7_P_TRANSPOSE] = 36;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 880.0) < 0.4, "transpose +12 = %.2f Hz", f);
    dx7_all_off(s);
    sine_patch(p); dx7_set_patch(s, p); s->bend = 1.f; s->bendrange = 2.f;
    dx7_note_on(s, 69, 127); render(s, buf, 48000);
    f = zc_freq(buf, 24000, 48000);
    CHECK(fabs(f - 440.0 * pow(2.0, 2.0 / 12)) < 0.5, "bend +2 semitones = %.2f Hz", f);
    s->bend = 0.f; dx7_all_off(s);

    /* 5. LFO pitch depth: PMD 99, PMS 7 should swing about +/-1 octave */
    sine_patch(p); p[DX7_P_LFO_PMD] = 99; p[DX7_P_LFO_PMS] = 7; p[DX7_P_LFO_WAVE] = 4; p[DX7_P_LFO_SPEED] = 30;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000 * 2);
    {
        double fmin = 1e9, fmax = 0; int w = 480;
        for (i = 24000; i + w < 96000; i += w) { double ff = zc_freq(buf, i, i + w); if (ff > 20) { if (ff < fmin) fmin = ff; if (ff > fmax) fmax = ff; } }
        CHECK(fmax > 440 * 1.8 && fmin < 440 * 0.6, "LFO full depth swings %.0f .. %.0f Hz around 440 (about +/-1 octave)", fmin, fmax);
    }
    dx7_all_off(s);

    /* 6. LFO delay: pitch modulation fades in after the delay */
    sine_patch(p); p[DX7_P_LFO_PMD] = 99; p[DX7_P_LFO_PMS] = 4; p[DX7_P_LFO_WAVE] = 4; p[DX7_P_LFO_SPEED] = 50; p[DX7_P_LFO_DELAY] = 80;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000 * 3);
    {
        double d1 = 0, d2 = 0; int w = 960;
        for (i = 0; i + w < 9600; i += w) { double ff = fabs(zc_freq(buf, i, i + w) - 440.0); if (ff > d1) d1 = ff; }
        for (i = 96000; i + w < 144000; i += w) { double ff = fabs(zc_freq(buf, i, i + w) - 440.0); if (ff > d2) d2 = ff; }
        CHECK(d1 < 30.0 && d2 > 60.0, "LFO delay: first 0.2 s max dev %.1f Hz, after 2 s max dev %.1f Hz", d1, d2);
    }
    dx7_all_off(s);

    /* 7. amp mod: AMD 99 + AMS 3 produces deep tremolo, AMS 0 none */
    sine_patch(p); p[DX7_P_LFO_AMD] = 99; p[5 * 21 + DX7_OP_AMS] = 3; p[DX7_P_LFO_WAVE] = 4; p[DX7_P_LFO_SPEED] = 40;
    dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000 * 2);
    {
        double lo = 1e9, hi = 0; int w = 480;
        for (i = 24000; i + w < 96000; i += w) { double a = peak(buf, i, i + w); if (a < lo) lo = a; if (a > hi) hi = a; }
        CHECK(lo < hi * 0.2, "amp mod AMS3/AMD99: envelope swings %.3f .. %.3f", lo, hi);
    }
    dx7_all_off(s);
    p[5 * 21 + DX7_OP_AMS] = 0; dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    CHECK(fabs(peak(buf, 24000, 48000) - 2.0) < 0.05, "AMS 0 leaves the level alone (%.3f)", peak(buf, 24000, 48000));
    dx7_all_off(s);
    sine_patch(p); p[5 * 21 + DX7_OP_AMS] = 3; dx7_set_patch(s, p); dx7_note_on(s, 69, 127); render(s, buf, 48000);
    CHECK(fabs(peak(buf, 24000, 48000) - 2.0) < 0.05, "AMS 3 with AMD 0 leaves the level alone (%.3f)", peak(buf, 24000, 48000));
    dx7_all_off(s);

    /* 8. release and voice life cycle */
    sine_patch(p); p[5 * 21 + 3] = 70; dx7_set_patch(s, p);
    dx7_note_on(s, 60, 100); render(s, buf, 4800);
    dx7_note_off(s, 60); render(s, buf, 48000 * 3);
    {
        int active = 0; for (i = 0; i < s->nvoices; i++) active += s->v[i].active;
        CHECK(active == 0, "voice frees itself after release (active=%d)", active);
        CHECK(peak(buf, 48000 * 2, 48000 * 3) < 1e-3, "silent after release (peak %.6f)", peak(buf, 48000 * 2, 48000 * 3));
    }

    /* 9. sustain pedal */
    dx7_set_patch(s, p);
    dx7_set_sustain(s, 1); dx7_note_on(s, 60, 100); dx7_note_off(s, 60); render(s, buf, 48000);
    CHECK(s->v[0].active && s->v[0].sustained, "sustain pedal holds a released note");
    dx7_set_sustain(s, 0); render(s, buf, 48000 * 2);
    CHECK(!s->v[0].active, "note ends after pedal up");

    /* 10. voice stealing keeps working past polyphony */
    for (i = 0; i < 40; i++) dx7_note_on(s, 40 + i, 100);
    render(s, buf, 4800);
    {
        int active = 0; for (i = 0; i < s->nvoices; i++) active += s->v[i].active;
        CHECK(active == 16, "40 notes on 16 voices -> %d active", active);
    }
    dx7_all_off(s);

    /* 11. SysEx round trips */
    {
        uint8_t bank[4104], out[32][DX7_VCED]; int n;
        memset(bank, 0, sizeof(bank));
        bank[0] = 0xF0; bank[1] = 0x43; bank[2] = 0x00; bank[3] = 0x09; bank[4] = 0x20; bank[5] = 0x00; bank[4103] = 0xF7;
        for (i = 0; i < 32; i++) { uint8_t *v = bank + 6 + i * 128; v[110] = (uint8_t)i; v[14] = 99; memcpy(v + 118, "TEST       ", 10); }
        n = dx7_parse_sysex(bank, sizeof(bank), out, 32);
        CHECK(n == 32 && out[5][DX7_P_ALGO] == 5 && out[31][DX7_P_ALGO] == 31, "bank sysex parses %d voices", n);
        n = dx7_parse_sysex(bank + 6, 4096, out, 32);
        CHECK(n == 32, "raw 4096-byte bank parses (%d)", n);
        n = dx7_parse_sysex(bank, 100, out, 32);
        CHECK(n == 0, "truncated sysex rejected (%d)", n);
    }

    /* 12. fuzz: random patches and notes must stay finite and bounded */
    {
        unsigned seed = 1; int trial, nan = 0, big = 0;
        for (trial = 0; trial < 300; trial++) {
            uint8_t rp[DX7_VCED]; int k, n;
            for (k = 0; k < DX7_VCED; k++) { seed = seed * 1664525u + 1013904223u; rp[k] = (uint8_t)(seed >> 24); }
            dx7_set_patch(s, rp);
            s->vintage = trial & 1;
            for (n = 0; n < 6; n++) { seed = seed * 1664525u + 1013904223u; dx7_note_on(s, (seed >> 20) & 127, 1 + ((seed >> 12) & 126)); }
            s->bend = (float)((int)((seed >> 5) & 255) - 128) / 128.f; s->wheel = 0.7f; s->wheel_amp = 1;
            render(s, buf, 12000);
            for (i = 0; i < 12000; i++) { if (!isfinite(buf[i])) nan++; if (fabsf(buf[i]) > 60.f) big++; }
            dx7_note_off(s, 60);
            dx7_all_off(s);
        }
        CHECK(nan == 0 && big == 0, "fuzz: 300 random patches, %d non-finite, %d out-of-range samples", nan, big);
    }

    dx7_free(s);
    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
