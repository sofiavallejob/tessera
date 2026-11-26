/*
 * tessera spectral core. See tspec_core.h.
 * Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause.
 *
 * Signal flow: input ring buffer -> Hann window -> FFT -> processor ->
 * IFFT -> Hann window -> overlap-add. Hop = N/4. Latency = N samples.
 * The dry signal is read from the same ring, so dry and wet stay aligned.
 */
#include "tspec_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TS_PI 3.14159265358979323846f
#define TS_2PI 6.28318530717958647692f

struct ts_core {
    int N, H, K, mask;
    float sr;
    float *win;
    float scale;
    float *cs, *sn;
    int *rev;
    float *inA, *inB, *ola;
    int pos, hopcount, count;
    float *re, *im, *bre, *bim;           /* analysis spectra A and B */
    float *mag, *ph, *magB, *phB;
    float *Yre, *Yim;
    float *tmp, *tmp2;
    float *prevph, *magavg;
    float *frz_mag, *frz_ph, *frz_dph;
    float *sm, *sm_ph, *sm_dph, *gg;
    float fade;
    int frz_active;
    unsigned int rng;
    float mixcur;
    ts_params p;
};

static float *falloc(int n) { return (float *)calloc((size_t)n, sizeof(float)); }

static float ts_rand(ts_core *c)             /* xorshift32, range -1..1 */
{
    unsigned int x = c->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    c->rng = x;
    return (float)(x & 0xFFFFFF) / 8388608.0f - 1.0f;
}

static float wrap_phase(float x)
{
    return x - TS_2PI * floorf(x / TS_2PI + 0.5f);
}

static void ts_defaults(ts_params *p)
{
    memset(p, 0, sizeof(*p));
    p->mode = TS_FREEZE;
    p->mix = 1.f;
    p->jitter = 0.15f;
    p->shimmer = 0.1f;
    p->time_ms = 400.f;
    p->shape = 0.f;
    p->width = 1.f;
    p->diffuse = 0.f;
    p->thresh_db = -30.f;
    p->floor_db = -60.f;
    p->attack_ms = 10.f;
    p->release_ms = 80.f;
    p->cmode = TS_CROSS_VOCODER;
    p->amount = 1.f;
}

ts_core *ts_new(int fftsize, float sr)
{
    ts_core *c;
    int N = 256, i;
    while (N < fftsize && N < 16384) N <<= 1;
    c = (ts_core *)calloc(1, sizeof(ts_core));
    if (!c) return NULL;
    c->N = N; c->H = N / 4; c->K = N / 2 + 1; c->mask = N - 1;
    c->sr = sr > 0 ? sr : 44100.f;
    c->win = falloc(N);
    c->cs = falloc(N / 2); c->sn = falloc(N / 2);
    c->rev = (int *)calloc((size_t)N, sizeof(int));
    c->inA = falloc(N); c->inB = falloc(N); c->ola = falloc(N);
    c->re = falloc(N); c->im = falloc(N); c->bre = falloc(N); c->bim = falloc(N);
    c->mag = falloc(c->K); c->ph = falloc(c->K);
    c->magB = falloc(c->K); c->phB = falloc(c->K);
    c->Yre = falloc(c->K); c->Yim = falloc(c->K);
    c->tmp = falloc(c->K); c->tmp2 = falloc(c->K);
    c->prevph = falloc(c->K); c->magavg = falloc(c->K);
    c->frz_mag = falloc(c->K); c->frz_ph = falloc(c->K); c->frz_dph = falloc(c->K);
    c->sm = falloc(c->K); c->sm_ph = falloc(c->K); c->sm_dph = falloc(c->K); c->gg = falloc(c->K);
    if (!c->win || !c->cs || !c->sn || !c->rev || !c->inA || !c->inB || !c->ola ||
        !c->re || !c->im || !c->bre || !c->bim || !c->mag || !c->ph || !c->magB ||
        !c->phB || !c->Yre || !c->Yim || !c->tmp || !c->tmp2 || !c->prevph ||
        !c->magavg || !c->frz_mag || !c->frz_ph || !c->frz_dph || !c->sm || !c->sm_ph ||
        !c->sm_dph || !c->gg)
    {
        ts_free(c);
        return NULL;
    }
    for (i = 0; i < N; i++) c->win[i] = 0.5f - 0.5f * cosf(TS_2PI * (float)i / (float)N);
    for (i = 0; i < N / 2; i++)
    {
        c->cs[i] = cosf(TS_2PI * (float)i / (float)N);
        c->sn[i] = sinf(TS_2PI * (float)i / (float)N);
    }
    for (i = 0; i < N; i++)                      /* bit reversal table */
    {
        int b, r = 0;
        for (b = 1; b < N; b <<= 1) r = (r << 1) | ((i & b) ? 1 : 0);
        c->rev[i] = r;
    }
    {
        float s = 0.f;
        for (i = 0; i < 4; i++) s += c->win[i * c->H] * c->win[i * c->H];
        c->scale = 1.f / ((float)N * s);
    }
    c->rng = 0x9E3779B9u;
    ts_defaults(&c->p);
    ts_reset(c);
    c->mixcur = c->p.mix;
    return c;
}

void ts_free(ts_core *c)
{
    if (!c) return;
    free(c->win); free(c->cs); free(c->sn); free(c->rev);
    free(c->inA); free(c->inB); free(c->ola);
    free(c->re); free(c->im); free(c->bre); free(c->bim);
    free(c->mag); free(c->ph); free(c->magB); free(c->phB);
    free(c->Yre); free(c->Yim); free(c->tmp); free(c->tmp2);
    free(c->prevph); free(c->magavg);
    free(c->frz_mag); free(c->frz_ph); free(c->frz_dph);
    free(c->sm); free(c->sm_ph); free(c->sm_dph); free(c->gg);
    free(c);
}

void ts_reset(ts_core *c)
{
    int N = c->N, K = c->K;
    memset(c->inA, 0, sizeof(float) * N); memset(c->inB, 0, sizeof(float) * N);
    memset(c->ola, 0, sizeof(float) * N);
    memset(c->prevph, 0, sizeof(float) * K); memset(c->magavg, 0, sizeof(float) * K);
    memset(c->frz_mag, 0, sizeof(float) * K); memset(c->frz_ph, 0, sizeof(float) * K);
    memset(c->frz_dph, 0, sizeof(float) * K);
    memset(c->sm, 0, sizeof(float) * K); memset(c->sm_ph, 0, sizeof(float) * K);
    memset(c->sm_dph, 0, sizeof(float) * K);
    { int k; for (k = 0; k < K; k++) c->gg[k] = 1.f; }
    c->pos = c->hopcount = c->count = 0;
    c->fade = 0.f; c->frz_active = 0;
}

ts_params *ts_get_params(ts_core *c) { return &c->p; }
int ts_get_size(ts_core *c) { return c->N; }
int ts_latency(ts_core *c) { return c->N; }

/* in-place forward complex FFT, radix 2. Inverse: call with re and im swapped. */
static void ts_fft(ts_core *c, float *re, float *im)
{
    int N = c->N, i, j, half, step;
    for (i = 0; i < N; i++)
    {
        j = c->rev[i];
        if (j > i)
        {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (half = 1; half < N; half <<= 1)
    {
        step = N / (2 * half);
        for (i = 0; i < N; i += 2 * half)
            for (j = 0; j < half; j++)
            {
                float wr = c->cs[j * step], wi = -c->sn[j * step];
                int a = i + j, b = a + half;
                float tr = re[b] * wr - im[b] * wi;
                float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
            }
    }
}

/* moving average over w bins (w odd), edges use the bins that exist */
static void box_smooth(const float *in, float *out, int K, float width)
{
    int w = (int)(width + 0.5f), half, k;
    double *cum;
    if (w < 1) w = 1;
    if (w > K) w = K;
    if (!(w & 1)) w++;
    if (w == 1) { memcpy(out, in, sizeof(float) * K); return; }
    half = w / 2;
    cum = (double *)malloc(sizeof(double) * (size_t)(K + 1));
    if (!cum) { memcpy(out, in, sizeof(float) * K); return; }
    cum[0] = 0.0;
    for (k = 0; k < K; k++) cum[k + 1] = cum[k] + in[k];
    for (k = 0; k < K; k++)
    {
        int lo = k - half, hi = k + half;
        if (lo < 0) lo = 0;
        if (hi > K - 1) hi = K - 1;
        out[k] = (float)((cum[hi + 1] - cum[lo]) / (double)(hi - lo + 1));
    }
    free(cum);
}

static void analyse(ts_core *c, const float *ring, float *re, float *im, float *mag, float *ph)
{
    int N = c->N, K = c->K, i, k;
    for (i = 0; i < N; i++) { re[i] = ring[(c->pos + i) & c->mask] * c->win[i]; im[i] = 0.f; }
    ts_fft(c, re, im);
    for (k = 0; k < K; k++)
    {
        mag[k] = sqrtf(re[k] * re[k] + im[k] * im[k]);
        ph[k] = atan2f(im[k], re[k]);
    }
}

static void process_frame(ts_core *c)
{
    ts_params *p = &c->p;
    int N = c->N, H = c->H, K = c->K, k, i;
    float hopsec = (float)H / c->sr;

    analyse(c, c->inA, c->re, c->im, c->mag, c->ph);

    switch (p->mode)
    {
    case TS_SMEAR:
    {
        float a = p->time_ms > 0.f ? expf(-hopsec / (p->time_ms * 0.001f)) : 0.f;
        float shape = p->shape < 0.f ? 0.f : (p->shape > 1.f ? 1.f : p->shape);
        float dif = p->diffuse < 0.f ? 0.f : (p->diffuse > 1.f ? 1.f : p->diffuse);
        float om_step = TS_2PI * (float)H / (float)N;
        for (k = 0; k < K; k++)
        {
            float s = c->sm[k];
            float avg = a * s + (1.f - a) * c->mag[k];
            float hold = c->mag[k] > a * s ? c->mag[k] : a * s;
            c->sm[k] = avg + (hold - avg) * shape;
        }
        /* Phase: lock to the input while a bin is driven by it, otherwise keep
         * running at the last frequency seen, so decaying tones stay tonal
         * instead of collapsing into one click per hop. */
        for (k = 0; k < K; k++)
        {
            float om = om_step * (float)k;
            if (c->mag[k] >= 0.3f * c->sm[k])
            {
                c->sm_dph[k] = wrap_phase(c->ph[k] - c->prevph[k] - om) + om;
                c->sm_ph[k] = c->ph[k];
            }
            else
                c->sm_ph[k] = wrap_phase(c->sm_ph[k] + c->sm_dph[k]);
        }
        box_smooth(c->sm, c->tmp, K, p->width);
        for (k = 0; k < K; k++)
        {
            float ph = c->sm_ph[k] + dif * TS_PI * ts_rand(c);
            c->Yre[k] = c->tmp[k] * cosf(ph);
            c->Yim[k] = c->tmp[k] * sinf(ph);
        }
        break;
    }
    case TS_GATE:
    {
        float peak = 0.f, thr, fl, ac, rc;
        float att = p->attack_ms < 0.1f ? 0.1f : p->attack_ms;
        float rel = p->release_ms < 0.1f ? 0.1f : p->release_ms;
        for (k = 0; k < K; k++) if (c->mag[k] > peak) peak = c->mag[k];
        thr = peak * powf(10.f, p->thresh_db / 20.f);
        fl = powf(10.f, p->floor_db / 20.f);
        ac = 1.f - expf(-hopsec / (att * 0.001f));
        rc = 1.f - expf(-hopsec / (rel * 0.001f));
        for (k = 0; k < K; k++)
        {
            int open = c->mag[k] >= thr;
            float target = (open != (p->invert != 0)) ? 1.f : fl;
            float g = c->gg[k];
            g += (target - g) * (target > g ? ac : rc);
            c->gg[k] = g;
            c->Yre[k] = c->re[k] * g;
            c->Yim[k] = c->im[k] * g;
        }
        break;
    }
    case TS_CROSS:
    {
        float amt = p->amount < 0.f ? 0.f : (p->amount > 1.f ? 1.f : p->amount);
        analyse(c, c->inB, c->bre, c->bim, c->magB, c->phB);
        if (p->cmode == TS_CROSS_VOCODER)
        {
            box_smooth(c->mag, c->tmp, K, p->width);
            box_smooth(c->magB, c->tmp2, K, p->width);
            for (k = 0; k < K; k++)
            {
                float fine = c->mag[k] / (c->tmp[k] + 1e-9f);
                float m = c->mag[k] + amt * (fine * c->tmp2[k] - c->mag[k]);
                c->Yre[k] = m * cosf(c->ph[k]);
                c->Yim[k] = m * sinf(c->ph[k]);
            }
        }
        else
        {
            for (k = 0; k < K; k++)
            {
                float m, ur, ui, len;
                if (p->cmode == TS_CROSS_MAG)
                {
                    m = (1.f - amt) * c->mag[k] + amt * c->magB[k];
                    ur = cosf(c->ph[k]); ui = sinf(c->ph[k]);
                }
                else
                {
                    ur = (1.f - amt) * cosf(c->ph[k]) + amt * cosf(c->phB[k]);
                    ui = (1.f - amt) * sinf(c->ph[k]) + amt * sinf(c->phB[k]);
                    len = sqrtf(ur * ur + ui * ui);
                    if (len < 1e-6f) { ur = cosf(c->ph[k]); ui = sinf(c->ph[k]); }
                    else { ur /= len; ui /= len; }
                    if (p->cmode == TS_CROSS_PHASE) m = c->mag[k];
                    else m = powf(c->mag[k] + 1e-9f, 1.f - amt) * powf(c->magB[k] + 1e-9f, amt);
                }
                c->Yre[k] = m * ur;
                c->Yim[k] = m * ui;
            }
        }
        break;
    }
    case TS_FREEZE:
    default:
    {
        float om_step = TS_2PI * (float)H / (float)N;
        float fcoef = 1.f - expf(-hopsec / 0.05f);
        float jit = p->jitter < 0.f ? 0.f : (p->jitter > 1.f ? 1.f : p->jitter);
        float shm = p->shimmer < 0.f ? 0.f : (p->shimmer > 1.f ? 1.f : p->shimmer);
        for (k = 0; k < K; k++)
        {
            float om = om_step * (float)k;
            c->tmp2[k] = wrap_phase(c->ph[k] - c->prevph[k] - om) + om;
            c->magavg[k] = c->magavg[k] * 0.6f + c->mag[k] * 0.4f;
        }
        if (p->freeze && !c->frz_active)
        {
            memcpy(c->frz_mag, c->magavg, sizeof(float) * K);
            memcpy(c->frz_ph, c->ph, sizeof(float) * K);
            memcpy(c->frz_dph, c->tmp2, sizeof(float) * K);
            c->frz_active = 1;
        }
        if (!p->freeze) c->frz_active = 0;
        c->fade += ((p->freeze ? 1.f : 0.f) - c->fade) * fcoef;
        if (!p->freeze && c->fade < 1e-3f) c->fade = 0.f;
        for (k = 0; k < K; k++)
        {
            if (c->fade > 0.f)
            {
                float fm, fp, fre, fim;
                c->frz_ph[k] = wrap_phase(c->frz_ph[k] + c->frz_dph[k]);
                fm = c->frz_mag[k] * (1.f + shm * ts_rand(c));
                if (fm < 0.f) fm = 0.f;
                fp = c->frz_ph[k] + jit * TS_PI * ts_rand(c);
                fre = fm * cosf(fp); fim = fm * sinf(fp);
                c->Yre[k] = (1.f - c->fade) * c->re[k] + c->fade * fre;
                c->Yim[k] = (1.f - c->fade) * c->im[k] + c->fade * fim;
            }
            else
            {
                c->Yre[k] = c->re[k];
                c->Yim[k] = c->im[k];
            }
        }
        break;
    }
    }
    memcpy(c->prevph, c->ph, sizeof(float) * K);

    /* back to the time domain through a Hermitian-symmetric spectrum */
    c->re[0] = c->Yre[0]; c->im[0] = 0.f;
    c->re[N / 2] = c->Yre[N / 2]; c->im[N / 2] = 0.f;
    for (k = 1; k < N / 2; k++)
    {
        c->re[k] = c->Yre[k]; c->im[k] = c->Yim[k];
        c->re[N - k] = c->Yre[k]; c->im[N - k] = -c->Yim[k];
    }
    ts_fft(c, c->im, c->re);                      /* inverse via swapped arguments */
    for (i = 0; i < N; i++)
        c->ola[(c->pos + i) & c->mask] += c->re[i] * c->win[i] * c->scale;
}

void ts_process(ts_core *c, const TS_SAMPLE *a, const TS_SAMPLE *b, TS_SAMPLE *out, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        int p = c->pos;
        float wet = c->ola[p], dry = c->inA[p], m;
        c->ola[p] = 0.f;
        c->inA[p] = (float)a[i];
        c->inB[p] = b ? (float)b[i] : 0.f;
        c->pos = (p + 1) & c->mask;
        c->mixcur += (c->p.mix - c->mixcur) * 0.01f;
        m = c->mixcur < 0.f ? 0.f : (c->mixcur > 1.f ? 1.f : c->mixcur);
        out[i] = (TS_SAMPLE)(dry * (1.f - m) + wet * m);
        if (c->count < c->N) c->count++;
        if (++c->hopcount >= c->H)
        {
            c->hopcount = 0;
            if (c->count >= c->N) process_frame(c);
        }
    }
}
