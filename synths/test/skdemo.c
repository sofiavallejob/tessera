/* Renders a short demo phrase for each preset of a synth to a 16-bit stereo WAV. Not shipped. */
#ifdef PROPHET5
#include "prophet5.h"
#define DEF p5_def
#elif defined(MINIMOOG)
#include "minimoog.h"
#define DEF mm_def
#elif defined(B3)
#include "b3.h"
#define DEF b3_def
#elif defined(ALPHAJUNO)
#include "alphajuno.h"
#define DEF aj_def
#else
#include "sh101.h"
#define DEF sh101_def
#endif
static void wr32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void wr16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
int main(int argc, char **argv)
{
    const sk_def *d = &DEF; float sr = 44100.0f; int per = (int)(sr * 4.5f), total = per * d->npresets;
    float *L = calloc((size_t)total, 4), *R = calloc((size_t)total, 4);
    sk_engine *e = sk_new(d, sr, 0);
    int chord[4] = { 48, 55, 60, 64 }; int mel[8] = { 60, 63, 67, 70, 72, 70, 67, 63 };
    for (int pi = 0; pi < d->npresets; pi++) {
        sk_load_preset(e, pi); float *l = L + (size_t)pi * per, *r = R + (size_t)pi * per; int pos = 0;
        int mono = d->mono;
        if (mono) {   /* melody then bass */
            for (int k = 0; k < 5; k++) { int n = mel[k] - (pi % 3 == 0 ? 24 : 0); sk_note_on(e, n, 0.9f); sk_render(e, l + pos, r + pos, per / 12); pos += per / 12; sk_note_off(e, n); sk_render(e, l + pos, r + pos, per / 24); pos += per / 24; }
            sk_note_on(e, 36, 1); sk_render(e, l + pos, r + pos, per / 3); pos += per / 3; sk_note_off(e, 36);
        } else {      /* chord, then a short run */
            for (int k = 0; k < 4; k++) sk_note_on(e, chord[k], 0.8f);
            sk_render(e, l + pos, r + pos, per / 2); pos += per / 2;
            for (int k = 0; k < 4; k++) sk_note_off(e, chord[k]);
            for (int k = 0; k < 4; k++) { sk_note_on(e, mel[k], 0.9f); sk_render(e, l + pos, r + pos, per / 12); pos += per / 12; sk_note_off(e, mel[k]); sk_render(e, l + pos, r + pos, per / 24); pos += per / 24; }
        }
        sk_render(e, l + pos, r + pos, per - pos); sk_panic(e);
    }
    FILE *f = fopen(argv[1], "wb"); uint32_t bytes = (uint32_t)total * 4;
    fwrite("RIFF", 1, 4, f); wr32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f); wr32(f, 16); wr16(f, 1); wr16(f, 2); wr32(f, 44100); wr32(f, 44100 * 4); wr16(f, 4); wr16(f, 16); fwrite("data", 1, 4, f); wr32(f, bytes);
    for (int i = 0; i < total; i++) { float a = sk_clampf(L[i] * 0.8f, -1, 1), b = sk_clampf(R[i] * 0.8f, -1, 1); wr16(f, (uint16_t)(int16_t)(a * 32767)); wr16(f, (uint16_t)(int16_t)(b * 32767)); }
    fclose(f); sk_free(e); free(L); free(R); return 0;
}
