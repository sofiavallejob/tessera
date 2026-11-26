/*
 * dx7core.h -- DX7-style 6-operator phase-modulation engine, header-only C99.
 *
 * Models the behaviour of the Yamaha DX7 (not a copy of any one emulator):
 *   - 6 operators, the 32 original algorithms, operator feedback (incl. the
 *     multi-operator feedback loops of algorithms 4 and 6)
 *   - 4-rate / 4-level envelopes with the DX7 level curve, attack curve,
 *     rate scaling, keyboard level scaling, velocity sensitivity
 *   - pitch envelope, LFO (6 waves, delay, key sync), pitch/amp mod sensitivity
 *   - coarse / fine / detune / fixed-frequency operators
 *   - DX7 SysEx: 32-voice bank, single voice (VCED), raw 4096 / 128 / 155 byte
 *
 * Extras in the spirit of Arturia's DX7 V:
 *   - per-operator waveform (sine + 7 OPL-style waves)
 *   - vintage / modern output stage ("DAC" mode)
 *
 * Time base: control-rate tick of DX7_TICK samples. Envelope and LFO speeds
 * are expressed in seconds, so they do not depend on the sample rate.
 *
 * Provenance: the implementation is original. Hardware constants (algorithm
 * routing, level / velocity / rate / pitch-EG curves, LFO and detune constants)
 * were taken from the open-source msfa emulator by Google (Apache-2.0,
 * github.com/google/music-synthesizer-for-android), which reverse-engineered them
 * from real DX7 behaviour. The amplitude-modulation curve and the stage-hold fit
 * follow what Dexed (GPL-3, github.com/asb2m10/dexed) found by measurement.
 * Check the licences of those projects before choosing a licence for this one.
 *
 * Operator level units: 1 unit = 1/256 octave of amplitude (1 DX7 output-level
 * step = 32 units = 0.75 dB). An operator at full level has amplitude 2.0,
 * which as a modulator is a phase swing of 2 cycles (4*pi rad), the DX7's
 * maximum modulation index.
 */
#ifndef DX7CORE_H
#define DX7CORE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define DX7_MAX_VOICES 32
#define DX7_TICK       32
#define DX7_VCED       155      /* unpacked single-voice size */

#ifndef DX7_ACCURATE_HOLD
#define DX7_ACCURATE_HOLD 1     /* emulate the hardware's rate-dependent hold when a stage has no level change */
#endif

/* VCED parameter offsets (operators are stored OP6 first, 21 bytes each) */
enum {
    DX7_OP_R1 = 0, DX7_OP_L1 = 4, DX7_OP_BP = 8, DX7_OP_LD = 9, DX7_OP_RD = 10,
    DX7_OP_LC = 11, DX7_OP_RC = 12, DX7_OP_RS = 13, DX7_OP_AMS = 14, DX7_OP_KVS = 15,
    DX7_OP_LEVEL = 16, DX7_OP_MODE = 17, DX7_OP_COARSE = 18, DX7_OP_FINE = 19,
    DX7_OP_DETUNE = 20, DX7_OP_SIZE = 21,
    DX7_P_PEG_R = 126, DX7_P_PEG_L = 130, DX7_P_ALGO = 134, DX7_P_FB = 135,
    DX7_P_OSCSYNC = 136, DX7_P_LFO_SPEED = 137, DX7_P_LFO_DELAY = 138,
    DX7_P_LFO_PMD = 139, DX7_P_LFO_AMD = 140, DX7_P_LFO_SYNC = 141,
    DX7_P_LFO_WAVE = 142, DX7_P_LFO_PMS = 143, DX7_P_TRANSPOSE = 144, DX7_P_NAME = 145
};

/* ------------------------------------------------------------------------- */
/* Tables (hardware facts)                                                   */
/* ------------------------------------------------------------------------- */

/* algorithm flags, one byte per operator, array order = OP6 .. OP1 */
enum {
    DX7_OUT_BUS1 = 1, DX7_OUT_BUS2 = 2, DX7_OUT_ADD = 4,
    DX7_IN_BUS1 = 16, DX7_IN_BUS2 = 32, DX7_FB_IN = 64, DX7_FB_OUT = 128
};

static const uint8_t dx7_algorithms[32][6] = {
    { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0x11, 0x14, 0xc1, 0x14 },
    { 0xc1, 0x11, 0x14, 0x01, 0x11, 0x14 }, { 0x41, 0x11, 0x94, 0x01, 0x11, 0x14 },
    { 0xc1, 0x14, 0x01, 0x14, 0x01, 0x14 }, { 0x41, 0x94, 0x01, 0x14, 0x01, 0x14 },
    { 0xc1, 0x11, 0x05, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0xc5, 0x14, 0x01, 0x14 },
    { 0x01, 0x11, 0x05, 0x14, 0xc1, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x11, 0x14 },
    { 0xc1, 0x05, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x05, 0x14, 0xc1, 0x14 },
    { 0xc1, 0x05, 0x05, 0x14, 0x01, 0x14 }, { 0xc1, 0x05, 0x11, 0x14, 0x01, 0x14 },
    { 0x01, 0x05, 0x11, 0x14, 0xc1, 0x14 }, { 0xc1, 0x11, 0x02, 0x25, 0x05, 0x14 },
    { 0x01, 0x11, 0x02, 0x25, 0xc5, 0x14 }, { 0x01, 0x11, 0x11, 0xc5, 0x05, 0x14 },
    { 0xc1, 0x14, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x14 },
    { 0x01, 0x14, 0x14, 0xc1, 0x14, 0x14 }, { 0xc1, 0x14, 0x14, 0x14, 0x01, 0x14 },
    { 0xc1, 0x14, 0x14, 0x01, 0x14, 0x04 }, { 0xc1, 0x14, 0x14, 0x14, 0x04, 0x04 },
    { 0xc1, 0x14, 0x14, 0x04, 0x04, 0x04 }, { 0xc1, 0x05, 0x14, 0x01, 0x14, 0x04 },
    { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x01, 0x14 },
    { 0xc1, 0x14, 0x01, 0x14, 0x04, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x04, 0x04 },
    { 0xc1, 0x14, 0x04, 0x04, 0x04, 0x04 }, { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 }
};

static const int dx7_levellut[20] = {
    0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46
};

/* DX7 output-level / EG-level 0..99 -> 0..127 (0.75 dB steps) */
static inline int dx7_scaleout(int l) { return l >= 20 ? 28 + l : dx7_levellut[l]; }

static const uint8_t dx7_velocity_data[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
    166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202,
    205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232,
    233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251,
    252, 253, 254
};

static const uint8_t dx7_exp_scale_data[33] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
    80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250
};

static const uint8_t dx7_pmstab[8] = { 0, 10, 20, 33, 55, 92, 153, 255 };
static const float   dx7_amstab[4] = { 0.f, 4342338.f / 16777216.f, 7171437.f / 16777216.f, 1.f };

static const uint8_t dx7_peg_ratetab[100] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47,
    49, 51, 53, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82,
    85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141, 147,
    153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255
};

static const int8_t dx7_peg_leveltab[100] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43,
    -41, -39, -37, -35, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24,
    -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
    -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73,
    82, 92, 103, 115, 127
};

/* ------------------------------------------------------------------------- */
/* Patch handling                                                            */
/* ------------------------------------------------------------------------- */

static inline void dx7_patch_init(uint8_t *p)
{
    int op, i;
    memset(p, 0, DX7_VCED);
    for (op = 0; op < 6; op++) {
        uint8_t *o = p + op * DX7_OP_SIZE;
        for (i = 0; i < 4; i++) { o[i] = 99; o[4 + i] = 99; }
        o[7] = 0;
        o[DX7_OP_BP] = 39;
        o[DX7_OP_COARSE] = 1;
        o[DX7_OP_DETUNE] = 7;
        o[DX7_OP_LEVEL] = (op == 5) ? 99 : 0;   /* OP1 only */
    }
    for (i = 0; i < 4; i++) { p[DX7_P_PEG_R + i] = 99; p[DX7_P_PEG_L + i] = 50; }
    p[DX7_P_OSCSYNC] = 1;
    p[DX7_P_LFO_SPEED] = 35;
    p[DX7_P_LFO_SYNC] = 1;
    p[DX7_P_LFO_PMS] = 3;
    p[DX7_P_TRANSPOSE] = 24;
    memcpy(p + DX7_P_NAME, "INIT VOICE", 10);
}

/* clamp every field to its legal range so corrupt data cannot index tables out of bounds */
static inline void dx7_patch_sanitize(uint8_t *p)
{
    int op, i;
    for (op = 0; op < 6; op++) {
        uint8_t *o = p + op * DX7_OP_SIZE;
        for (i = 0; i < 8; i++) if (o[i] > 99) o[i] = 99;
        if (o[DX7_OP_BP] > 99) o[DX7_OP_BP] = 99;
        if (o[DX7_OP_LD] > 99) o[DX7_OP_LD] = 99;
        if (o[DX7_OP_RD] > 99) o[DX7_OP_RD] = 99;
        o[DX7_OP_LC] &= 3; o[DX7_OP_RC] &= 3; o[DX7_OP_RS] &= 7;
        o[DX7_OP_AMS] &= 3; o[DX7_OP_KVS] &= 7;
        if (o[DX7_OP_LEVEL] > 99) o[DX7_OP_LEVEL] = 99;
        o[DX7_OP_MODE] &= 1;
        o[DX7_OP_COARSE] &= 31;
        if (o[DX7_OP_FINE] > 99) o[DX7_OP_FINE] = 99;
        if (o[DX7_OP_DETUNE] > 14) o[DX7_OP_DETUNE] = 14;
    }
    for (i = 0; i < 8; i++) if (p[DX7_P_PEG_R + i] > 99) p[DX7_P_PEG_R + i] = 99;
    p[DX7_P_ALGO] &= 31;
    p[DX7_P_FB] &= 7;
    p[DX7_P_OSCSYNC] &= 1;
    if (p[DX7_P_LFO_SPEED] > 99) p[DX7_P_LFO_SPEED] = 99;
    if (p[DX7_P_LFO_DELAY] > 99) p[DX7_P_LFO_DELAY] = 99;
    if (p[DX7_P_LFO_PMD] > 99) p[DX7_P_LFO_PMD] = 99;
    if (p[DX7_P_LFO_AMD] > 99) p[DX7_P_LFO_AMD] = 99;
    p[DX7_P_LFO_SYNC] &= 1;
    if (p[DX7_P_LFO_WAVE] > 5) p[DX7_P_LFO_WAVE] = 5;
    p[DX7_P_LFO_PMS] &= 7;
    if (p[DX7_P_TRANSPOSE] > 48) p[DX7_P_TRANSPOSE] = 48;
    for (i = 0; i < 10; i++) {
        uint8_t c = p[DX7_P_NAME + i];
        if (c < 32 || c > 126) p[DX7_P_NAME + i] = ' ';
    }
}

/* 128-byte packed voice (as stored in a 32-voice bank) -> 155-byte VCED */
static inline void dx7_unpack(const uint8_t *b, uint8_t *p)
{
    int op;
    for (op = 0; op < 6; op++) {
        const uint8_t *s = b + op * 17;
        uint8_t *o = p + op * DX7_OP_SIZE;
        memcpy(o, s, 11);                      /* rates, levels, bp, ld, rd */
        o[DX7_OP_LC] = s[11] & 3;
        o[DX7_OP_RC] = (s[11] >> 2) & 3;
        o[DX7_OP_RS] = s[12] & 7;
        o[DX7_OP_DETUNE] = (s[12] >> 3) & 15;
        o[DX7_OP_AMS] = s[13] & 3;
        o[DX7_OP_KVS] = (s[13] >> 2) & 7;
        o[DX7_OP_LEVEL] = s[14];
        o[DX7_OP_MODE] = s[15] & 1;
        o[DX7_OP_COARSE] = (s[15] >> 1) & 31;
        o[DX7_OP_FINE] = s[16];
    }
    memcpy(p + 126, b + 102, 9);               /* pitch EG, algorithm */
    p[DX7_P_FB] = b[111] & 7;
    p[DX7_P_OSCSYNC] = (b[111] >> 3) & 1;
    memcpy(p + DX7_P_LFO_SPEED, b + 112, 4);
    p[DX7_P_LFO_SYNC] = b[116] & 1;
    p[DX7_P_LFO_WAVE] = (b[116] >> 1) & 7;
    p[DX7_P_LFO_PMS] = (b[116] >> 4) & 7;
    memcpy(p + DX7_P_TRANSPOSE, b + 117, 11);  /* transpose + name */
    dx7_patch_sanitize(p);
}

/*
 * Parse a SysEx / raw dump into VCED voices. Accepts: 32-voice bank message
 * (F0 43 0n 09 20 00 ...), single voice message (F0 43 0n 00 01 1B ...), and
 * header-less raw data of 4096, 128 or 155 bytes. Returns the number of voices
 * written to out (each DX7_VCED bytes), 0 if nothing recognised.
 */
static inline int dx7_parse_sysex(const uint8_t *b, size_t len, uint8_t (*out)[DX7_VCED], int maxn)
{
    int i, n = 0;
    if (len >= 6 && b[0] == 0xF0 && b[1] == 0x43) {
        if (b[3] == 0x09 && len >= 6 + 4096) {
            for (i = 0; i < 32 && n < maxn; i++) dx7_unpack(b + 6 + i * 128, out[n++]);
            return n;
        }
        if (b[3] == 0x00 && len >= 6 + DX7_VCED && maxn > 0) {
            memcpy(out[0], b + 6, DX7_VCED);
            dx7_patch_sanitize(out[0]);
            return 1;
        }
        return 0;
    }
    if (len == 4096) {
        for (i = 0; i < 32 && n < maxn; i++) dx7_unpack(b + i * 128, out[n++]);
        return n;
    }
    if (len == 128 && maxn > 0) { dx7_unpack(b, out[0]); return 1; }
    if (len == DX7_VCED && maxn > 0) { memcpy(out[0], b, DX7_VCED); dx7_patch_sanitize(out[0]); return 1; }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Sine table and operator waveforms                                         */
/* ------------------------------------------------------------------------- */

#define DX7_SIN_N 4096
static float dx7_sintab[DX7_SIN_N + 1];
static int   dx7_tables_ready = 0;

static inline void dx7_init_tables(void)
{
    int i;
    if (dx7_tables_ready) return;
    for (i = 0; i <= DX7_SIN_N; i++) dx7_sintab[i] = (float)sin(2.0 * 3.14159265358979323846 * i / DX7_SIN_N);
    dx7_tables_ready = 1;
}

static inline float dx7_sin(uint32_t ph)
{
    uint32_t i = ph >> 20;                       /* 12 bits */
    float f = (float)(ph & 0xFFFFF) * (1.0f / 1048576.0f);
    float a = dx7_sintab[i], b = dx7_sintab[i + 1];
    return a + (b - a) * f;
}

/* wave 0 = sine (DX7). 1..7 = extra waves, OPL-style, as offered by Arturia's DX7 V */
static inline float dx7_wave(int w, uint32_t ph)
{
    switch (w) {
    default:
    case 0: return dx7_sin(ph);
    case 1: return ph < 0x80000000u ? dx7_sin(ph) : 0.f;                       /* half sine */
    case 2: return fabsf(dx7_sin(ph));                                         /* abs sine */
    case 3: return (ph & 0x40000000u) ? 0.f : fabsf(dx7_sin(ph));              /* quarter sine */
    case 4: return ph < 0x80000000u ? dx7_sin(ph << 1) : 0.f;                  /* alternating sine */
    case 5: return ph < 0x80000000u ? fabsf(dx7_sin(ph << 1)) : 0.f;           /* abs alternating */
    case 6: return ph < 0x80000000u ? 1.f : -1.f;                              /* square */
    case 7: return (float)((double)ph * (2.0 / 4294967296.0) - 1.0);           /* saw */
    }
}

/* ------------------------------------------------------------------------- */
/* Envelope (operator) and pitch envelope                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    float   L;          /* current level, 1/256-octave units */
    float   target;
    float   inc;        /* units / second */
    float   hold;       /* seconds left in a "static" stage */
    int     stage;      /* 0..3 running, 4 = finished */
    int     rising;
    int     outlevel;   /* operator output level, units */
    int     ratescale;  /* qrate offset from keyboard rate scaling */
    uint8_t rate[4], level[4];
} dx7_eg;

static inline void dx7_eg_advance(dx7_eg *e, int ix)
{
    e->stage = ix;
    if (ix < 4) {
        int newlevel = e->level[ix];
        int actual = dx7_scaleout(newlevel) >> 1;
        int q;
        actual = (actual << 6) + e->outlevel - 4256;
        if (actual < 16) actual = 16;
        e->target = (float)actual;
        e->rising = e->target > e->L;
        q = (e->rate[ix] * 41) >> 6;
        q += e->ratescale;
        if (q > 63) q = 63;
        /* msfa-derived timing: per 64-sample block at 44.1 kHz; convert to per second */
        e->inc = (float)((4 + (q & 3)) << (2 + 6 + (q >> 2))) * (1.0f / 65536.0f) * (44100.0f / 64.0f);
        e->hold = 0.f;
#if DX7_ACCURATE_HOLD
        if (e->target == e->L || (ix == 0 && newlevel == 0)) {
            int sr = e->rate[ix] + e->ratescale;
            float t;
            if (sr > 99) sr = 99;
            if (sr < 77) {
                t = 40.0f * exp2f(-(float)sr / 6.4f);     /* fit to measured hardware hold times */
                if (ix == 0 && newlevel == 0) t *= 0.05f; /* attack is scaled faster */
            } else {
                t = 20.0f * (float)(99 - sr) / 44100.0f;
            }
            e->hold = t;
        }
#endif
    }
}

static inline void dx7_eg_init(dx7_eg *e, const uint8_t *rates, const uint8_t *levels, int outlevel, int ratescale)
{
    memcpy(e->rate, rates, 4);
    memcpy(e->level, levels, 4);
    e->outlevel = outlevel;
    e->ratescale = ratescale;
    e->L = 0.f;
    e->hold = 0.f;
    dx7_eg_advance(e, 0);
}

static inline void dx7_eg_tick(dx7_eg *e, int down, float dt)
{
    if (e->stage < 3 || (e->stage < 4 && !down)) {
        if (e->hold > 0.f) {
            e->hold -= dt;
            if (e->hold <= 0.f) { e->hold = 0.f; dx7_eg_advance(e, e->stage + 1); }
        } else if (e->rising) {
            float rem;
            if (e->L < 1716.f) e->L = 1716.f;   /* the attack starts at an audible level */
            rem = floorf(17.f - e->L * (1.0f / 256.0f));
            if (rem < 1.f) rem = 1.f;
            e->L += rem * e->inc * dt;
            if (e->L >= e->target) { e->L = e->target; dx7_eg_advance(e, e->stage + 1); }
        } else {
            e->L -= e->inc * dt;
            if (e->L <= e->target) { e->L = e->target; dx7_eg_advance(e, e->stage + 1); }
        }
    }
}

typedef struct {
    float   L;          /* octaves */
    float   target, inc;
    int     stage, rising;
    uint8_t rate[4], level[4];
} dx7_peg;

static inline void dx7_peg_advance(dx7_peg *e, int ix)
{
    e->stage = ix;
    if (ix < 4) {
        e->target = dx7_peg_leveltab[e->level[ix]] * (1.0f / 32.0f);
        e->rising = e->target > e->L;
        e->inc = dx7_peg_ratetab[e->rate[ix]] / 21.3f;   /* octaves / second */
    }
}

static inline void dx7_peg_init(dx7_peg *e, const uint8_t *rates, const uint8_t *levels)
{
    memcpy(e->rate, rates, 4);
    memcpy(e->level, levels, 4);
    e->L = dx7_peg_leveltab[levels[3]] * (1.0f / 32.0f);
    dx7_peg_advance(e, 0);
}

static inline void dx7_peg_tick(dx7_peg *e, int down, float dt)
{
    if (e->stage < 3 || (e->stage < 4 && !down)) {
        if (e->rising) {
            e->L += e->inc * dt;
            if (e->L >= e->target) { e->L = e->target; dx7_peg_advance(e, e->stage + 1); }
        } else {
            e->L -= e->inc * dt;
            if (e->L <= e->target) { e->L = e->target; dx7_peg_advance(e, e->stage + 1); }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Scaling helpers                                                           */
/* ------------------------------------------------------------------------- */

static inline int dx7_scale_velocity(int velocity, int sens)
{
    int v = velocity < 0 ? 0 : (velocity > 127 ? 127 : velocity);
    int val = (int)dx7_velocity_data[v >> 1] - 239;
    return ((sens * val + 7) >> 3) * 16;          /* units */
}

static inline int dx7_scale_rate(int note, int sens)
{
    int x = note / 3 - 7;
    if (x < 0) x = 0;
    if (x > 31) x = 31;
    return (sens * x) >> 3;
}

static inline int dx7_scale_curve(int group, int depth, int curve)
{
    int scale;
    if (curve == 0 || curve == 3) {
        scale = (group * depth * 329) >> 12;
    } else {
        int raw = dx7_exp_scale_data[group < 32 ? group : 32];
        scale = (raw * depth * 329) >> 15;
    }
    return (curve < 2) ? -scale : scale;
}

static inline int dx7_scale_level(int note, int bp, int ld, int rd, int lc, int rc)
{
    int off = note - bp - 17;
    return off >= 0 ? dx7_scale_curve(off / 3, rd, rc) : dx7_scale_curve((-off) / 3, ld, lc);
}

/* log2(Hz) of an operator at a given note */
static inline float dx7_osc_log2(int note, int mode, int coarse, int fine, int detune)
{
    if (mode == 0) {
        float lf = 8.78135971f + (note - 69) * (1.0f / 12.0f);  /* log2(440) = 8.78136 */
        lf += log2f(coarse == 0 ? 0.5f : (float)coarse);
        if (fine) lf += log2f(1.0f + 0.01f * (float)fine);
        lf += (12606.0f / 16777216.0f) * (float)(detune - 7);   /* ~0.9 cents per count */
        return lf;
    } else {
        float lf = 3.32192809f * (float)((coarse & 3) * 100 + fine) * 0.01f;   /* 1 Hz .. ~9.77 kHz */
        if (detune > 7) lf += (13457.0f / 16777216.0f) * (float)(detune - 7);
        return lf;
    }
}

/* ------------------------------------------------------------------------- */
/* LFO (global, as on the DX7)                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t phase, delta, delaystate, delayinc, delayinc2, unit;
    int      wave, sync, randstate;
} dx7_lfo;

static inline void dx7_lfo_setup(dx7_lfo *l, const uint8_t *p, double sr)
{
    int rate = p[DX7_P_LFO_SPEED];
    int sp = rate == 0 ? 1 : (165 * rate) >> 6;
    int a;
    l->unit = (uint32_t)(DX7_TICK * 25190424.0 / sr + 0.5);
    sp *= sp < 160 ? 11 : (11 + ((sp - 160) >> 4));
    l->delta = l->unit * (uint32_t)sp;
    a = 99 - p[DX7_P_LFO_DELAY];
    if (a == 99) {
        l->delayinc = ~0u;
        l->delayinc2 = ~0u;
    } else {
        a = (16 + (a & 15)) << (1 + (a >> 4));
        l->delayinc = l->unit * (uint32_t)a;
        a &= 0xff80;
        if (a < 0x80) a = 0x80;
        l->delayinc2 = l->unit * (uint32_t)a;
    }
    l->wave = p[DX7_P_LFO_WAVE];
    l->sync = p[DX7_P_LFO_SYNC] != 0;
}

/* returns 0..1 */
static inline float dx7_lfo_sample(dx7_lfo *l)
{
    uint32_t ph;
    int32_t x;
    l->phase += l->delta;
    ph = l->phase;
    switch (l->wave) {
    case 0: x = (int32_t)(ph >> 7); x ^= -(int32_t)(ph >> 31); x &= (1 << 24) - 1; break;
    case 1: x = (int32_t)(((~ph) ^ (1u << 31)) >> 8); break;
    case 2: x = (int32_t)((ph ^ (1u << 31)) >> 8); break;
    case 3: x = (int32_t)(((~ph) >> 7) & (1u << 24)); break;
    case 4: return 0.5f + 0.5f * dx7_sin(ph);
    case 5:
        if (ph < l->delta) l->randstate = (l->randstate * 179 + 17) & 0xff;
        x = ((l->randstate ^ 0x80) + 1) << 16;
        break;
    default: x = 1 << 23; break;
    }
    return (float)x * (1.0f / 16777216.0f);
}

/* returns 0..1: 0 during the delay, then fades in */
static inline float dx7_lfo_delay(dx7_lfo *l)
{
    uint32_t delta = l->delaystate < (1u << 31) ? l->delayinc : l->delayinc2;
    uint32_t d = l->delaystate + delta;
    if (d < l->delayinc) return 1.0f;
    l->delaystate = d;
    if (d < (1u << 31)) return 0.f;
    return (float)((d >> 7) & ((1u << 24) - 1)) * (1.0f / 16777216.0f);
}

/* ------------------------------------------------------------------------- */
/* Voice and synth                                                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    int      active, down, sustained;
    int      note, mnote;           /* played note, note after transpose */
    uint32_t age;
    uint8_t  p[DX7_VCED];
    dx7_eg   eg[6];
    dx7_peg  peg;
    uint32_t phase[6], inc[6];
    float    logf[6];
    uint8_t  fixed[6];
    float    g0[6], g1[6];
    float    fb0, fb1;              /* feedback history */
    float    fbscale;
} dx7_voice;

typedef struct {
    double    sr;
    int       nvoices;
    dx7_voice v[DX7_MAX_VOICES];
    uint8_t   patch[DX7_VCED];
    uint8_t   opmask;               /* bit n set = OP(n+1) on */
    uint8_t   wave[6];              /* per operator, indexed OP1..OP6 -> 0..5 */
    dx7_lfo   lfo;
    float     bend, bendrange;      /* -1..1, semitones */
    float     wheel;                /* 0..1 */
    int       wheel_pitch, wheel_amp;
    int       sustain;
    float     tune_cents;
    float     gain;
    int       vintage;              /* output stage: 0 modern, 1 vintage */
    int       tickpos;
    uint32_t  agecount;
    uint32_t  rng;
} dx7_synth;

/* Output scale: a full-level carrier has amplitude 2.0; 0.1 leaves headroom for chords. `gain` multiplies this. */
#define DX7_OUT_SCALE 0.1f

#define DX7_SILENT 6.7e-5f   /* below this an operator is inaudible (DX7 floor) */

static inline void dx7_set_sr(dx7_synth *s, double sr)
{
    s->sr = sr > 1000.0 ? sr : 44100.0;
    dx7_lfo_setup(&s->lfo, s->patch, s->sr);
}

static inline void dx7_set_patch(dx7_synth *s, const uint8_t *vced)
{
    memcpy(s->patch, vced, DX7_VCED);
    dx7_patch_sanitize(s->patch);
    dx7_lfo_setup(&s->lfo, s->patch, s->sr);
}

/* edit one VCED byte; applies to notes played afterwards (LFO applies at once) */
static inline void dx7_set_param(dx7_synth *s, int idx, int val)
{
    if (idx < 0 || idx >= DX7_VCED) return;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    s->patch[idx] = (uint8_t)val;
    dx7_patch_sanitize(s->patch);
    if (idx >= DX7_P_LFO_SPEED && idx <= DX7_P_LFO_WAVE) dx7_lfo_setup(&s->lfo, s->patch, s->sr);
}

static inline dx7_synth *dx7_new(int nvoices, double sr)
{
    dx7_synth *s = (dx7_synth *)calloc(1, sizeof(dx7_synth));
    if (!s) return NULL;
    dx7_init_tables();
    if (nvoices < 1) nvoices = 1;
    if (nvoices > DX7_MAX_VOICES) nvoices = DX7_MAX_VOICES;
    s->nvoices = nvoices;
    dx7_patch_init(s->patch);
    s->opmask = 0x3f;
    s->bendrange = 2.f;
    s->wheel_pitch = 1;
    s->gain = 1.f;
    s->rng = 12345u;
    dx7_set_sr(s, sr);
    return s;
}

static inline void dx7_free(dx7_synth *s) { free(s); }

static inline int dx7_is_carrier(int alg, int k) { return (dx7_algorithms[alg][k] & 3) == 0; }

static inline void dx7_voice_start(dx7_synth *s, dx7_voice *v, int note, int vel)
{
    const uint8_t *p = s->patch;
    int op, mn = note + (p[DX7_P_TRANSPOSE] - 24);
    uint8_t rates[4], levels[4];
    if (mn < 0) mn = 0;
    if (mn > 127) mn = 127;
    memset(v, 0, sizeof(*v));
    memcpy(v->p, p, DX7_VCED);
    v->active = 1; v->down = 1;
    v->note = note; v->mnote = mn;
    v->age = ++s->agecount;
    for (op = 0; op < 6; op++) {
        const uint8_t *o = p + op * DX7_OP_SIZE;
        int outlevel = dx7_scaleout(o[DX7_OP_LEVEL]);
        int rsc, i;
        outlevel += dx7_scale_level(mn, o[DX7_OP_BP], o[DX7_OP_LD], o[DX7_OP_RD], o[DX7_OP_LC], o[DX7_OP_RC]);
        if (outlevel > 127) outlevel = 127;
        outlevel *= 32;
        outlevel += dx7_scale_velocity(vel, o[DX7_OP_KVS]);
        if (outlevel < 0) outlevel = 0;
        rsc = dx7_scale_rate(mn, o[DX7_OP_RS]);
        for (i = 0; i < 4; i++) { rates[i] = o[i]; levels[i] = o[4 + i]; }
        dx7_eg_init(&v->eg[op], rates, levels, outlevel, rsc);
        v->fixed[op] = o[DX7_OP_MODE];
        v->logf[op] = dx7_osc_log2(mn, o[DX7_OP_MODE], o[DX7_OP_COARSE], o[DX7_OP_FINE], o[DX7_OP_DETUNE]);
        if (p[DX7_P_OSCSYNC]) {
            v->phase[op] = 0;
        } else {
            s->rng = s->rng * 1664525u + 1013904223u;
            v->phase[op] = s->rng;
        }
        v->g0[op] = v->g1[op] = 0.f;
    }
    {
        int i;
        for (i = 0; i < 4; i++) { rates[i] = p[DX7_P_PEG_R + i]; levels[i] = p[DX7_P_PEG_L + i]; }
        dx7_peg_init(&v->peg, rates, levels);
    }
    v->fbscale = p[DX7_P_FB] ? exp2f(-(float)(8 - p[DX7_P_FB]) - 1.0f) : 0.f;
}

static inline void dx7_voice_release(dx7_voice *v)
{
    int op;
    if (!v->down) return;
    v->down = 0;
    v->sustained = 0;
    for (op = 0; op < 6; op++) dx7_eg_advance(&v->eg[op], 3);
    dx7_peg_advance(&v->peg, 3);
}

static inline void dx7_note_on(dx7_synth *s, int note, int vel)
{
    int i, pick = -1;
    uint32_t best;
    if (vel <= 0) return;
    if (note < 0 || note > 127) return;
    if (vel > 127) vel = 127;
    for (i = 0; i < s->nvoices; i++) if (!s->v[i].active) { pick = i; break; }
    if (pick < 0) {                                 /* steal: oldest released, else oldest */
        best = 0xffffffffu;
        for (i = 0; i < s->nvoices; i++)
            if (!s->v[i].down && s->v[i].age < best) { best = s->v[i].age; pick = i; }
        if (pick < 0) {
            best = 0xffffffffu;
            for (i = 0; i < s->nvoices; i++)
                if (s->v[i].age < best) { best = s->v[i].age; pick = i; }
        }
    }
    dx7_voice_start(s, &s->v[pick], note, vel);
    if (s->lfo.sync) s->lfo.phase = (1u << 31) - 1;
    s->lfo.delaystate = 0;
}

static inline void dx7_note_off(dx7_synth *s, int note)
{
    int i;
    for (i = 0; i < s->nvoices; i++) {
        dx7_voice *v = &s->v[i];
        if (v->active && v->down && !v->sustained && v->note == note) {
            if (s->sustain) v->sustained = 1; else dx7_voice_release(v);
        }
    }
}

static inline void dx7_set_sustain(dx7_synth *s, int on)
{
    int i;
    s->sustain = on != 0;
    if (!on)
        for (i = 0; i < s->nvoices; i++)
            if (s->v[i].active && s->v[i].sustained) dx7_voice_release(&s->v[i]);
}

static inline void dx7_all_off(dx7_synth *s)
{
    int i;
    for (i = 0; i < s->nvoices; i++) s->v[i].active = 0;
}

/* ---- per-tick update of one voice: envelopes, pitch, gains ---- */
static inline void dx7_voice_tick(dx7_synth *s, dx7_voice *v, float lfo_val, float lfo_delay)
{
    const float dt = (float)DX7_TICK / (float)s->sr;
    const uint8_t *p = v->p;
    float lfoC = lfo_val - 0.5f;
    float pmd = (float)((p[DX7_P_LFO_PMD] * 165) >> 6);
    float pms = (float)dx7_pmstab[p[DX7_P_LFO_PMS] & 7];
    float pm1 = fabsf(pmd * lfo_delay * pms * lfoC) * (1.0f / 32768.0f);
    float pm2 = s->wheel_pitch ? fabsf(s->wheel * 127.f * pms * lfoC) * (1.0f / 16384.0f) : 0.f;
    float pm = pm1 > pm2 ? pm1 : pm2;
    float base, pitchmod, amd, amd1, amd2, lfoInv;
    int op, alg = p[DX7_P_ALGO] & 31, silent_carriers = 1;

    dx7_peg_tick(&v->peg, v->down, dt);
    if (lfoC < 0.f) pm = -pm;
    base = s->bend * s->bendrange * (1.0f / 12.0f) + s->tune_cents * (1.0f / 1200.0f);
    pitchmod = base + v->peg.L + pm;

    lfoInv = 1.0f - lfo_val;
    amd1 = (float)((p[DX7_P_LFO_AMD] * 165) >> 6) * (1.0f / 256.0f) * lfo_delay * lfoInv;
    amd2 = s->wheel_amp ? s->wheel * lfoInv * (127.0f / 128.0f) : 0.f;
    amd = amd1 > amd2 ? amd1 : amd2;

    for (op = 0; op < 6; op++) {
        float lg = v->fixed[op] ? v->logf[op] + base : v->logf[op] + pitchmod;
        double f = exp2((double)lg) / s->sr;
        float level, g;
        int ams = p[op * DX7_OP_SIZE + DX7_OP_AMS] & 3;
        f -= floor(f);
        v->inc[op] = (uint32_t)(uint64_t)(f * 4294967296.0);

        dx7_eg_tick(&v->eg[op], v->down, dt);
        level = v->eg[op].L;
        if (ams) {
            /* attenuation grows exponentially with depth (curve fitted to hardware); no offset at zero depth */
            float x = amd * dx7_amstab[ams];
            float frac = expf(4.48f * x - 4.4355f) - 0.01185f;
            if (frac > 0.f) level -= level * frac;
        }
        g = exp2f(level * (1.0f / 256.0f) - 14.0f);
        if (!(s->opmask & (1u << (5 - op)))) g = 0.f;   /* array index 0 = OP6 */
        v->g0[op] = v->g1[op];
        v->g1[op] = g;
        if (dx7_is_carrier(alg, op) && (g >= DX7_SILENT || v->g0[op] >= DX7_SILENT)) silent_carriers = 0;
    }
    if (!v->down && silent_carriers) v->active = 0;
}

/* ---- render n samples of one voice, starting pos samples into the tick ---- */
static inline void dx7_voice_render(dx7_synth *s, dx7_voice *v, float *out, int n, int pos)
{
    const uint8_t *alg = dx7_algorithms[v->p[DX7_P_ALGO] & 31];
    const float invk = 1.0f / (float)DX7_TICK;
    float gain[6], dgain[6];
    int silent[6], i, k;
    for (k = 0; k < 6; k++) {
        dgain[k] = (v->g1[k] - v->g0[k]) * invk;
        gain[k] = v->g0[k] + dgain[k] * (float)pos;
        silent[k] = (v->g0[k] < DX7_SILENT && v->g1[k] < DX7_SILENT);
    }
    for (i = 0; i < n; i++) {
        float bus1 = 0.f, bus2 = 0.f, acc = 0.f;
        for (k = 0; k < 6; k++) {
            int fl = alg[k];
            float m = 0.f, y;
            gain[k] += dgain[k];
            if (fl & DX7_IN_BUS1) m += bus1;
            else if (fl & DX7_IN_BUS2) m += bus2;
            if (fl & DX7_FB_IN) m += (v->fb0 + v->fb1) * v->fbscale;
            if (silent[k]) {
                y = 0.f;
            } else {
                uint32_t ph = v->phase[k] + (uint32_t)(int64_t)(m * 4294967296.0f);
                y = dx7_wave(s->wave[5 - k], ph) * gain[k];
            }
            v->phase[k] += v->inc[k];
            if (fl & DX7_FB_OUT) { v->fb0 = v->fb1; v->fb1 = y; }
            switch (fl & 3) {
            case 0: acc += y; break;
            case 1: if (fl & DX7_OUT_ADD) bus1 += y; else bus1 = y; break;
            default: if (fl & DX7_OUT_ADD) bus2 += y; else bus2 = y; break;
            }
        }
        out[i] += acc;
    }
}

static inline void dx7_tick(dx7_synth *s)
{
    float lv = dx7_lfo_sample(&s->lfo);
    float ld = dx7_lfo_delay(&s->lfo);
    int i;
    for (i = 0; i < s->nvoices; i++)
        if (s->v[i].active) dx7_voice_tick(s, &s->v[i], lv, ld);
}

/* Render n mono samples into out (overwritten). */
static inline void dx7_process(dx7_synth *s, float *out, int n)
{
    int done = 0, i;
    const float scale = DX7_OUT_SCALE * s->gain;
    memset(out, 0, (size_t)n * sizeof(float));
    while (done < n) {
        int chunk = n - done, v;
        if (s->tickpos == 0) dx7_tick(s);
        if (chunk > DX7_TICK - s->tickpos) chunk = DX7_TICK - s->tickpos;
        for (v = 0; v < s->nvoices; v++)
            if (s->v[v].active) dx7_voice_render(s, &s->v[v], out + done, chunk, s->tickpos);
        s->tickpos += chunk;
        if (s->tickpos >= DX7_TICK) s->tickpos = 0;
        done += chunk;
    }
    for (i = 0; i < n; i++) {
        float y = out[i] * scale;
        if (s->vintage) {                      /* hard-clipping 12-bit converter */
            if (y > 1.f) y = 1.f;
            if (y < -1.f) y = -1.f;
            y = floorf(y * 2047.f + 0.5f) * (1.0f / 2047.f);
        }
        out[i] = y;
    }
}

#endif /* DX7CORE_H */
