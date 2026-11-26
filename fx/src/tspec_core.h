/*
 * tessera spectral core: STFT engine (Hann, 75% overlap) with four
 * frequency-domain processors. No Pd dependency, so it can be tested alone.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 */
#ifndef TSPEC_CORE_H
#define TSPEC_CORE_H

#ifndef TS_SAMPLE
#define TS_SAMPLE float
#endif

enum { TS_FREEZE = 0, TS_SMEAR = 1, TS_GATE = 2, TS_CROSS = 3 };
enum { TS_CROSS_VOCODER = 0, TS_CROSS_PHASE = 1, TS_CROSS_MAG = 2, TS_CROSS_MORPH = 3 };

typedef struct ts_params {
    int   mode;
    float mix;        /* 0 dry .. 1 wet */
    /* freeze */
    int   freeze;     /* 0/1 */
    float jitter;     /* 0..1 random phase per frame */
    float shimmer;    /* 0..1 random magnitude per frame */
    /* smear */
    float time_ms;    /* temporal smoothing time */
    float shape;      /* 0 average .. 1 peak hold */
    float width;      /* spectral smoothing, in bins */
    float diffuse;    /* 0..1 random phase */
    /* gate */
    float thresh_db;  /* relative to the loudest bin of each frame */
    float floor_db;   /* gain of closed bins */
    float attack_ms;
    float release_ms;
    int   invert;     /* 0/1 keep the quiet bins instead */
    /* cross */
    int   cmode;      /* TS_CROSS_* */
    float amount;     /* 0..1 */
} ts_params;

typedef struct ts_core ts_core;

ts_core   *ts_new(int fftsize, float sr);          /* fftsize: power of two 256..16384 */
void       ts_free(ts_core *c);
void       ts_reset(ts_core *c);
ts_params *ts_get_params(ts_core *c);
int        ts_get_size(ts_core *c);
int        ts_latency(ts_core *c);                 /* in samples, equals fftsize */
void       ts_process(ts_core *c, const TS_SAMPLE *a, const TS_SAMPLE *b,
                      TS_SAMPLE *out, int n);       /* b may be NULL */

#endif
