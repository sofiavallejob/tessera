/*
 * synthcore.h - Pd-independent engine shared by every synth in the collection.
 * A synth supplies a sk_def (parameters, presets, voice callbacks, optional
 * effect). The engine does voice allocation (poly or mono with note stack),
 * sustain pedal, pitch bend, parameter smoothing, presets, user bank, A/B morph
 * and mutate. All parameters are normalised 0..1; "steps" > 0 marks a switch.
 */
#ifndef SYNTHCORE_H
#define SYNTHCORE_H

#include "synthkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SK_MAXP 48
#define SK_BLOCK 64
#define SK_USER_MAX 128
#define SK_NAME 24

typedef struct { const char *name; float def; int steps; } sk_param;
typedef struct { const char *name; const char *text; } sk_preset_def;
typedef struct { float sr, bend, mod, aftertouch, data; void *user; } sk_glob; /* bend in semitones, data 0..1, user = def private data */

typedef struct sk_def {
    const char *name;
    int nparams; const sk_param *params;
    int npresets; const sk_preset_def *presets;
    int nvoices;      /* polyphony (1 = mono) */
    int mono;         /* mono: note stack, legato aware */
    int legato_param; /* param index: >0.5 means do not retrigger in mono mode, -1 none */
    size_t voice_size, fx_size;
    void (*voice_init)(void *v, float sr);
    /* retrig: mono = envelope should restart; poly = this is the first held key */
    void (*voice_on)(void *v, const float *p, int note, float vel, int retrig, float sr);
    void (*voice_off)(void *v, const float *p);
    int (*voice_render)(void *v, const float *p, const sk_glob *g, float *out, int n); /* adds into out; returns 0 when silent */
    void (*fx_init)(void *fx, float sr);
    void (*fx_run)(void *fx, const float *p, const float *in, float *L, float *R, int n, float sr);
    /* optional shared data (sample banks): created with the engine, freed with it, handed to voices as g->user */
    void *(*user_new)(float sr);
    void (*user_free)(void *u);
    int (*cmd)(void *u, const char *name, const char *arg, const char *dir, float sr);  /* extra text messages (dir = folder of the patch); 0 = ok */
} sk_def;

typedef struct { int note, gate, sus, active; uint32_t age; float vel; } sk_slot;
typedef struct { char name[SK_NAME]; float v[SK_MAXP]; } sk_preset;

typedef struct {
    const sk_def *d;
    float sr;
    float p[SK_MAXP], ps[SK_MAXP];  /* target and smoothed */
    sk_glob g;
    int nv, sustain;
    uint32_t clock;
    sk_slot *slot; unsigned char *vmem; unsigned char *fxmem;
    int stack[16], nstack;
    int cur;                         /* current preset index, -1 none */
    sk_preset user[SK_USER_MAX]; int nuser;
    float A[SK_MAXP], B[SK_MAXP];
    sk_rng rng;
    float mono_buf[SK_BLOCK];
    int bad_preset_tokens;
    float data; int ndmap, dmap[4]; float damt[4];   /* data inlet: value 0..1 offsets up to 4 parameters */
    void *shared;                                   /* def private data (sample bank) */
} sk_engine;

/* ---------- parameter helpers ---------- */
/* stepped params are written by users as an index 0..steps-1 and stored as (k+0.5)/steps */
static inline float sk_from_user(const sk_param *pm, float x)
{
    if (pm->steps >= 2) {
        int k = (int)(x + 0.5f); if (k < 0) k = 0; if (k >= pm->steps) k = pm->steps - 1;
        return ((float)k + 0.5f) / (float)pm->steps;
    }
    return sk_clampf(x, 0.0f, 1.0f);
}
static inline float sk_to_user(const sk_param *pm, float p)
{
    if (pm->steps >= 2) { int k = (int)(p * (float)pm->steps); if (k >= pm->steps) k = pm->steps - 1; return (float)k; }
    return p;
}
/* read a switch as an integer inside a voice */
#define SK_SW(p, i, steps) ((int)((p)[i] * (float)(steps)) >= (steps) ? (steps) - 1 : (int)((p)[i] * (float)(steps)))

static inline int sk_find(const sk_def *d, const char *name)
{
    for (int i = 0; i < d->nparams; i++) if (!strcmp(d->params[i].name, name)) return i;
    return -1;
}
static inline void sk_defaults(const sk_def *d, float *v)
{
    memset(v, 0, sizeof(float) * SK_MAXP);
    for (int i = 0; i < d->nparams; i++) v[i] = sk_from_user(&d->params[i], d->params[i].def);
}
/* "a=0.5 b=1" over defaults; returns number of unknown tokens */
static inline int sk_parse_text(const sk_def *d, const char *text, float *v)
{
    int bad = 0; char tok[64]; const char *s = text;
    sk_defaults(d, v);
    while (*s) {
        while (*s && isspace((unsigned char)*s)) s++;
        int n = 0;
        while (*s && !isspace((unsigned char)*s) && n < 63) tok[n++] = *s++;
        tok[n] = 0;
        if (!n) break;
        char *eq = strchr(tok, '=');
        if (!eq) { bad++; continue; }
        *eq = 0;
        int i = sk_find(d, tok);
        if (i < 0) { bad++; continue; }
        v[i] = sk_from_user(&d->params[i], (float)atof(eq + 1));
    }
    return bad;
}
/* ---------- lifecycle ---------- */
static inline sk_engine *sk_new(const sk_def *d, float sr, int nvoices)
{
    sk_engine *e = (sk_engine *)calloc(1, sizeof *e);
    if (!e) return 0;
    e->d = d; e->sr = sr; e->nv = nvoices > 0 ? nvoices : d->nvoices;
    if (d->mono) e->nv = 1;
    e->slot = (sk_slot *)calloc((size_t)e->nv, sizeof(sk_slot));
    e->vmem = (unsigned char *)calloc((size_t)e->nv, d->voice_size + 16);
    e->fxmem = (unsigned char *)calloc(1, d->fx_size + 16);
    if (!e->slot || !e->vmem || !e->fxmem) { free(e->slot); free(e->vmem); free(e->fxmem); free(e); return 0; }
    sk_rng_seed(&e->rng, 12345u);
    e->g.sr = sr;
    if (d->user_new) { e->shared = d->user_new(sr); e->g.user = e->shared; }
    e->data = 0.5f; e->g.data = 0.5f;
    sk_defaults(d, e->p); memcpy(e->ps, e->p, sizeof e->p);
    memcpy(e->A, e->p, sizeof e->p); memcpy(e->B, e->p, sizeof e->p);
    for (int i = 0; i < e->nv; i++) d->voice_init(e->vmem + (size_t)i * (d->voice_size + 16), sr);
    if (d->fx_init) d->fx_init(e->fxmem, sr);
    e->cur = -1;
    return e;
}
static inline void sk_free(sk_engine *e)
{
    if (!e) return;
    if (e->shared && e->d->user_free) e->d->user_free(e->shared);
    free(e->slot); free(e->vmem); free(e->fxmem); free(e);
}
static inline void *sk_vptr(sk_engine *e, int i) { return e->vmem + (size_t)i * (e->d->voice_size + 16); }

/* ---------- notes ---------- */
static inline void sk_panic(sk_engine *e)           /* hard stop: voices are reset, no release tail */
{
    for (int i = 0; i < e->nv; i++) {
        if (e->slot[i].active) e->d->voice_init(sk_vptr(e, i), e->sr);
        e->slot[i].active = e->slot[i].gate = e->slot[i].sus = 0;
    }
    e->nstack = 0;
    if (e->d->fx_init) e->d->fx_init(e->fxmem, e->sr);
}
static inline int sk_mono_legato(sk_engine *e)
{
    return e->d->legato_param >= 0 && e->p[e->d->legato_param] > 0.5f;
}
static inline void sk_note_on(sk_engine *e, int note, float vel)
{
    const sk_def *d = e->d;
    note = note < 0 ? 0 : (note > 127 ? 127 : note);
    vel = sk_clampf(vel, 0.0f, 1.0f);
    e->clock++;
    if (d->mono) {
        int was_gate = e->slot[0].gate;
        for (int i = 0; i < e->nstack; i++) if (e->stack[i] == note) { memmove(e->stack + i, e->stack + i + 1, (size_t)(e->nstack - i - 1) * sizeof(int)); e->nstack--; break; }
        if (e->nstack < 16) e->stack[e->nstack++] = note;
        int retrig = !(was_gate && sk_mono_legato(e));
        e->slot[0].note = note; e->slot[0].gate = 1; e->slot[0].active = 1; e->slot[0].vel = vel; e->slot[0].age = e->clock;
        d->voice_on(sk_vptr(e, 0), e->p, note, vel, retrig, e->sr);
        return;
    }
    int pick = -1;
    for (int i = 0; i < e->nv; i++) if (e->slot[i].active && e->slot[i].note == note) { pick = i; break; }
    if (pick < 0) for (int i = 0; i < e->nv; i++) if (!e->slot[i].active) { pick = i; break; }
    if (pick < 0) { /* steal: oldest released voice, else oldest */
        uint32_t best = 0xffffffffu; int bi = 0;
        for (int pass = 0; pass < 2 && pick < 0; pass++) {
            for (int i = 0; i < e->nv; i++) {
                if (pass == 0 && e->slot[i].gate) continue;
                if (e->slot[i].age < best) { best = e->slot[i].age; bi = i; }
            }
            if (best != 0xffffffffu) pick = bi;
        }
        if (pick < 0) pick = 0;
    }
    int first = 1;                       /* poly: the flag says "no other key is held" */
    for (int i = 0; i < e->nv; i++) if (i != pick && e->slot[i].active && e->slot[i].gate) first = 0;
    e->slot[pick].note = note; e->slot[pick].gate = 1; e->slot[pick].sus = 0; e->slot[pick].active = 1;
    e->slot[pick].vel = vel; e->slot[pick].age = e->clock;
    d->voice_on(sk_vptr(e, pick), e->p, note, vel, first, e->sr);
}
static inline void sk_note_off(sk_engine *e, int note)
{
    const sk_def *d = e->d;
    if (d->mono) {
        for (int i = 0; i < e->nstack; i++) if (e->stack[i] == note) { memmove(e->stack + i, e->stack + i + 1, (size_t)(e->nstack - i - 1) * sizeof(int)); e->nstack--; break; }
        if (!e->slot[0].gate) return;
        if (e->nstack > 0) {                 /* fall back to the previous held key, no retrigger */
            int n = e->stack[e->nstack - 1];
            if (n != e->slot[0].note) { e->slot[0].note = n; d->voice_on(sk_vptr(e, 0), e->p, n, e->slot[0].vel, 0, e->sr); }
        } else if (e->sustain) {
            e->slot[0].sus = 1;
        } else {
            e->slot[0].gate = 0; d->voice_off(sk_vptr(e, 0), e->p);
        }
        return;
    }
    for (int i = 0; i < e->nv; i++) {
        if (e->slot[i].active && e->slot[i].gate && e->slot[i].note == note) {
            if (e->sustain) e->slot[i].sus = 1;
            else { e->slot[i].gate = 0; d->voice_off(sk_vptr(e, i), e->p); }
        }
    }
}
static inline void sk_set_sustain(sk_engine *e, int on)
{
    e->sustain = on;
    if (!on) for (int i = 0; i < e->nv; i++) if (e->slot[i].sus) {
        e->slot[i].sus = 0; e->slot[i].gate = 0; e->d->voice_off(sk_vptr(e, i), e->p);
    }
}

/* ---------- render ---------- */
static inline void sk_render(sk_engine *e, float *L, float *R, int n)
{
    const sk_def *d = e->d;
    int done = 0;
    while (done < n) {
        int m = n - done; if (m > SK_BLOCK) m = SK_BLOCK;
        for (int i = 0; i < d->nparams; i++) {
            float t = e->p[i];
            for (int k = 0; k < e->ndmap; k++) if (e->dmap[k] == i && d->params[i].steps < 2) t = sk_clampf(t + e->damt[k] * (e->data * 2.0f - 1.0f), 0.0f, 1.0f);
            if (d->params[i].steps >= 2) e->ps[i] = t;
            else e->ps[i] += (t - e->ps[i]) * 0.25f;
        }
        float *mb = e->mono_buf; memset(mb, 0, sizeof(float) * (size_t)m);
        for (int i = 0; i < e->nv; i++) {
            if (!e->slot[i].active) continue;
            if (!d->voice_render(sk_vptr(e, i), e->ps, &e->g, mb, m)) { e->slot[i].active = 0; e->slot[i].gate = 0; e->slot[i].sus = 0; }
        }
        if (d->fx_run) d->fx_run(e->fxmem, e->ps, mb, L + done, R + done, m, e->sr);
        else for (int i = 0; i < m; i++) { L[done + i] = mb[i]; R[done + i] = mb[i]; }
        done += m;
    }
}

/* ---------- data inlet ---------- */
static inline void sk_set_data(sk_engine *e, float x) { e->data = sk_clampf(x, 0.0f, 1.0f); e->g.data = e->data; }
/* map the data value to a parameter: p + amount * (data*2-1). amount 0 removes the mapping. returns index or -1 */
static inline int sk_data_map(sk_engine *e, const char *name, float amount)
{
    int i = sk_find(e->d, name);
    if (i < 0 || e->d->params[i].steps >= 2) return -1;
    for (int k = 0; k < e->ndmap; k++) if (e->dmap[k] == i) {
        if (amount == 0.0f) { for (int j = k; j < e->ndmap - 1; j++) { e->dmap[j] = e->dmap[j + 1]; e->damt[j] = e->damt[j + 1]; } e->ndmap--; }
        else e->damt[k] = amount;
        return i;
    }
    if (amount != 0.0f && e->ndmap < 4) { e->dmap[e->ndmap] = i; e->damt[e->ndmap++] = amount; }
    return i;
}

/* ---------- presets ---------- */
static inline int sk_total_presets(const sk_engine *e) { return e->d->npresets + e->nuser; }
static inline const char *sk_preset_name(const sk_engine *e, int i)
{
    if (i < 0) return "";
    if (i < e->d->npresets) return e->d->presets[i].name;
    i -= e->d->npresets;
    return i < e->nuser ? e->user[i].name : "";
}
static inline int sk_load_preset(sk_engine *e, int i)
{
    int n = sk_total_presets(e);
    if (n <= 0) return -1;
    i = ((i % n) + n) % n;
    float v[SK_MAXP];
    if (i < e->d->npresets) e->bad_preset_tokens += sk_parse_text(e->d, e->d->presets[i].text, v);
    else memcpy(v, e->user[i - e->d->npresets].v, sizeof v);
    memcpy(e->p, v, sizeof v);
    e->cur = i;
    return i;
}
static inline int sk_set_param(sk_engine *e, const char *name, float x)
{
    int i = sk_find(e->d, name);
    if (i < 0) return -1;
    e->p[i] = sk_from_user(&e->d->params[i], x);
    return i;
}
static inline void sk_store_user(sk_engine *e, const char *name)
{
    if (e->nuser >= SK_USER_MAX) return;
    sk_preset *u = &e->user[e->nuser++];
    memset(u, 0, sizeof *u);
    snprintf(u->name, SK_NAME, "%.*s", SK_NAME - 1, name && *name ? name : "USER");
    memcpy(u->v, e->p, sizeof u->v);
    e->cur = e->d->npresets + e->nuser - 1;
}
static inline void sk_morph(sk_engine *e, float x)
{
    x = sk_clampf(x, 0.0f, 1.0f);
    for (int i = 0; i < e->d->nparams; i++) {
        if (e->d->params[i].steps >= 2) e->p[i] = x < 0.5f ? e->A[i] : e->B[i];
        else e->p[i] = sk_lerp(e->A[i], e->B[i], x);
    }
}
static inline void sk_mutate(sk_engine *e, float amount)
{
    amount = sk_clampf(amount, 0.0f, 1.0f);
    for (int i = 0; i < e->d->nparams; i++) {
        if (e->d->params[i].steps >= 2) {
            if (sk_rng_uni(&e->rng) < amount * 0.15f) e->p[i] = ((float)(sk_rng_u32(&e->rng) % (unsigned)e->d->params[i].steps) + 0.5f) / (float)e->d->params[i].steps;
        } else {
            e->p[i] = sk_clampf(e->p[i] + sk_rng_bi(&e->rng) * amount * 0.5f, 0.0f, 1.0f);
        }
    }
}
/* text bank: one preset per line "NAME|param=value param=value ..." */
static inline int sk_save_user(const sk_engine *e, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int k = 0; k < e->nuser; k++) {
        fprintf(f, "%s|", e->user[k].name);
        for (int i = 0; i < e->d->nparams; i++) fprintf(f, "%s=%.4f ", e->d->params[i].name, sk_to_user(&e->d->params[i], e->user[k].v[i]));
        fprintf(f, "\n");
    }
    fclose(f);
    return e->nuser;
}
static inline int sk_load_user(sk_engine *e, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[2048]; int n = 0;
    while (fgets(line, sizeof line, f) && e->nuser < SK_USER_MAX) {
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = 0;
        sk_preset *u = &e->user[e->nuser];
        memset(u, 0, sizeof *u);
        snprintf(u->name, SK_NAME, "%.*s", SK_NAME - 1, line);
        sk_parse_text(e->d, bar + 1, u->v);
        e->nuser++; n++;
    }
    fclose(f);
    return n;
}

#endif
