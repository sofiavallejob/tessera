/* Offline checks for a synth def: finite output, level, pitch, filter, envelopes. Usage: skrender <name> */
#ifdef MELLOTRON
#include "mellotron.h"
#define DEF mel_def
#elif defined(CZ101)
#include "cz101.h"
#define DEF cz_def
#elif defined(MS20)
#include "ms20.h"
#define DEF ms_def
#elif defined(TX81Z)
#include "tx81z.h"
#define DEF tx_def
#elif defined(MINIMOOG)
#include "minimoog.h"
#define DEF mm_def
#elif defined(PROPHET5)
#include "prophet5.h"
#define DEF p5_def
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
#include <stdio.h>
static const sk_def *defs[] = { &DEF };
static double rms(const float *x, int n){ double s=0; for(int i=0;i<n;i++) s+=x[i]*x[i]; return sqrt(s/n); }
static int fails;
#define CHECK(c, ...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)
/* estimate fundamental by zero-crossings of low-passed-ish signal */
static double zc_freq(const float *x, int n, float sr){ int z=0; for(int i=1;i<n;i++) if(x[i-1]<=0 && x[i]>0) z++; return z*sr/n; }
int main(void){
    const sk_def *d = defs[0]; float sr = 48000.0f; int N = 48000;
    float *L = malloc(sizeof(float)*N), *R = malloc(sizeof(float)*N);
    for (int pi = 0; pi < d->npresets; pi++) {
        sk_engine *e = sk_new(d, sr, 0); sk_load_preset(e, pi);
        sk_note_on(e, 48, 0.8f); sk_render(e, L, R, N/2); sk_note_off(e, 48);
        sk_render(e, L + N/2, R + N/2, N/2);
        int bad = 0; float pk = 0, pkh = 0; for (int i = 0; i < N; i++) { if (!isfinite(L[i]) || !isfinite(R[i])) bad++; if (fabsf(L[i]) > pk) pk = fabsf(L[i]); if (i < N/2 && fabsf(L[i]) > pkh) pkh = fabsf(L[i]); }
        double r1 = rms(L, N/2), tail = rms(L + N - 4800, 4800);
        printf("%-16s peak %.2f rms(held) %.3f tail %.5f\n", d->presets[pi].name, pk, r1, tail);
        CHECK(!bad, "%s non-finite", d->presets[pi].name);
        CHECK(pk < 2.0f, "%s peak %.2f too hot", d->presets[pi].name, pk);
        CHECK(pkh > 0.05f, "%s too quiet while held (peak %.3f)", d->presets[pi].name, pkh);
        sk_free(e);
    }
#if defined(MELLOTRON)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"tape",0); sk_set_param(e,"wow",0); sk_set_param(e,"flutter",0); sk_set_param(e,"drift",0); sk_set_param(e,"hiss",0); sk_set_param(e,"tone",1); sk_set_param(e,"att",0); sk_set_param(e,"pitch",0.5f);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("synthetic pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
    { /* real tape: write a 220 Hz sine wav as 57.wav, load the folder, play note 57 and 69 */
      if (system("mkdir -p /tmp/meltest")) return 1; FILE *w = fopen("/tmp/meltest/57.wav","wb"); int fr = 44100; unsigned char h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x44,0xAC,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',0,0,0,0};
      unsigned ds = (unsigned)fr * 2, rs = ds + 36; memcpy(h+4,&rs,4); memcpy(h+40,&ds,4); fwrite(h,1,44,w);
      for (int i=0;i<fr;i++){ short s=(short)(sinf(6.2831853f*220.0f*(float)i/44100.0f)*20000); fwrite(&s,2,1,w);} fclose(w);
      sk_engine *e = sk_new(d, sr, 0); CHECK(d->cmd(e->shared,"samples","/tmp/meltest","/tmp",sr)==0,"samples load");
      sk_set_param(e,"wow",0); sk_set_param(e,"flutter",0); sk_set_param(e,"drift",0); sk_set_param(e,"hiss",0); sk_set_param(e,"tone",1); sk_set_param(e,"att",0); sk_set_param(e,"loop",1);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f1 = zc_freq(L+4800,N-4800,sr); sk_panic(e);
      sk_note_on(e,69,1); sk_render(e,L,R,N); double f2 = zc_freq(L+4800,N-4800,sr);
      printf("wav tape: note 57 -> %.2f Hz, note 69 -> %.2f Hz\n", f1, f2); CHECK(fabs(f1-220.0)<2.0,"wav root %.2f",f1); CHECK(fabs(f2-440.0)<4.0,"wav transposed %.2f",f2);
      d->cmd(e->shared,"samples","clear","/tmp",sr); sk_free(e); }
    { /* tape end: without loop the voice goes silent after the tape length */
      sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"length",0); sk_set_param(e,"loop",0); sk_set_param(e,"hiss",0); sk_set_param(e,"tape",0); sk_note_on(e,60,1);
      static float LL[48000*4], RR[48000*4]; sk_render(e,LL,RR,48000*4); double early = rms(LL+4800,9600), late = rms(LL+48000*3,9600);
      printf("tape end: early rms %.3f late rms %.5f\n", early, late); CHECK(early>0.01 && late<1e-4,"tape does not stop"); sk_free(e); }
#elif defined(CZ101)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"wave1",0); sk_set_param(e,"dcw1",0); sk_set_param(e,"mix2",0); sk_set_param(e,"dcwkbd",0); sk_set_param(e,"sus",1); sk_set_param(e,"att",0); sk_set_param(e,"pitchamt",0.5f); sk_set_param(e,"vibdepth",0); sk_set_param(e,"dcwsus",1);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
#elif defined(MS20)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"wave1",1); sk_set_param(e,"range1",1); sk_set_param(e,"lvl2",0); sk_set_param(e,"hpcut",0); sk_set_param(e,"hppeak",0); sk_set_param(e,"lpcut",1); sk_set_param(e,"lppeak",0); sk_set_param(e,"sus",1); sk_set_param(e,"att",0); sk_set_param(e,"glide",0); sk_set_param(e,"mg2pitch",0.5f); sk_set_param(e,"eg2pitch",0.5f);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"lvl1",0); sk_set_param(e,"noise",0.02f); sk_set_param(e,"hpcut",0); sk_set_param(e,"hppeak",0); sk_set_param(e,"lppeak",1); sk_set_param(e,"lpcut",0.5f); sk_set_param(e,"kbd",0); sk_set_param(e,"sus",1); sk_set_param(e,"eg2lp",0.5f);
      sk_note_on(e,60,1); sk_render(e,L,R,N); double r = rms(L+24000,N-24000); float pk=0; for(int i=24000;i<N;i++) if(fabsf(L[i])>pk) pk=fabsf(L[i]); printf("scream rms %.3f peak %.2f\n", r, pk); CHECK(r>0.02,"filter does not ring"); CHECK(pk<2.0,"filter runaway"); sk_free(e); }
#elif defined(TX81Z)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"algo",7); sk_set_param(e,"level1",1); sk_set_param(e,"level2",0); sk_set_param(e,"level3",0); sk_set_param(e,"level4",0); sk_set_param(e,"sus1",1); sk_set_param(e,"att1",0); sk_set_param(e,"lfopm",0);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
#elif defined(MINIMOOG)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"wave1",2); sk_set_param(e,"range1",3); sk_set_param(e,"cutoff",1); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"asus",1); sk_set_param(e,"aatt",0); sk_set_param(e,"glide",0); sk_set_param(e,"lfopitch",0);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
    { double rr[2]; for(int k=0;k<2;k++){ sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"cutoff",k?0.15f:0.95f); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"asus",1); sk_set_param(e,"kbd",0);
      sk_note_on(e,60,1); sk_render(e,L,R,N); rr[k]=rms(L+9600,N-9600); sk_free(e);} printf("filter open %.3f closed %.3f\n", rr[0], rr[1]); CHECK(rr[1] < rr[0]*0.4, "filter does not attenuate"); }
#elif defined(PROPHET5)
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"asaw",1); sk_set_param(e,"cutoff",1); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"drift",0); sk_set_param(e,"asus",1); sk_set_param(e,"aatt",0);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f); CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
    { double rr[2]; for(int k=0;k<2;k++){ sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"asaw",1); sk_set_param(e,"cutoff",k?0.15f:0.95f); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"asus",1); sk_set_param(e,"kbd",0);
      sk_note_on(e,60,1); sk_render(e,L,R,N); rr[k]=rms(L+9600,N-9600); sk_free(e);} printf("filter open %.3f closed %.3f\n", rr[0], rr[1]); CHECK(rr[1] < rr[0]*0.4, "filter does not attenuate"); }
#elif !defined(B3)
    /* pitch: plain saw, filter open, note 57 = 220 Hz */
    { sk_engine *e = sk_new(d, sr, 0); sk_set_param(e,"saw",1); sk_set_param(e,"chorus",0); sk_set_param(e,"cutoff",1); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"sus",1); sk_set_param(e,"att",0); sk_set_param(e,"range",1);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f = zc_freq(L+4800,N-4800,sr); printf("pitch test: %.2f Hz (want 220)\n", f);
      CHECK(fabs(f-220.0)<2.0,"pitch %.2f",f); sk_free(e); }
    /* filter: closing the cutoff must reduce level of a saw */
    { double rr[2]; for(int k=0;k<2;k++){ sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"saw",1); sk_set_param(e,"cutoff",k?0.15f:0.95f); sk_set_param(e,"res",0); sk_set_param(e,"envamt",0); sk_set_param(e,"sus",1); sk_set_param(e,"kbd",0);
      sk_note_on(e,60,1); sk_render(e,L,R,N); rr[k]=rms(L+9600,N-9600); sk_free(e);} printf("filter open %.3f closed %.3f\n", rr[0], rr[1]); CHECK(rr[1] < rr[0]*0.4, "filter does not attenuate"); }
#if !defined(ALPHAJUNO) && !defined(B3)
    /* self oscillation: no input, res=1 should ring at cutoff */
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"noise",0.02f); sk_set_param(e,"res",1); sk_set_param(e,"cutoff",0.5f); sk_set_param(e,"kbd",0); sk_set_param(e,"envamt",0); sk_set_param(e,"sus",1); sk_set_param(e,"vca",0);
      sk_note_on(e,60,1); sk_render(e,L,R,N); double r = rms(L+24000,N-24000); printf("self-osc rms %.3f\n", r); CHECK(r>0.05,"no self oscillation"); CHECK(r<1.0,"self-osc runaway"); sk_free(e);}
#endif
#endif
    /* release: after note off the tail decays to silence */
    { sk_engine *e=sk_new(d,sr,0); sk_load_preset(e,0); sk_note_on(e,40,1); sk_render(e,L,R,9600); sk_note_off(e,40); sk_render(e,L,R,N); CHECK(rms(L+N-2400,2400)<1e-4,"release does not reach silence"); sk_free(e);}
    /* legato + note stack: hold 60, press 64, release 64 -> back to 60 */
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"legato",1); sk_note_on(e,60,1); sk_note_on(e,64,1); sk_note_off(e,64); CHECK(e->slot[0].note==60 && e->slot[0].gate,"mono stack fallback"); sk_note_off(e,60); CHECK(!e->slot[0].gate,"gate not released"); sk_free(e);}
    /* preset text: no unknown tokens */
    { sk_engine *e=sk_new(d,sr,0); for(int i=0;i<d->npresets;i++) sk_load_preset(e,i); CHECK(e->bad_preset_tokens==0,"%d unknown tokens in presets", e->bad_preset_tokens); sk_free(e);}
    /* morph/mutate/save/load */
    { sk_engine *e=sk_new(d,sr,0); sk_load_preset(e,0); memcpy(e->A,e->p,sizeof e->p); sk_load_preset(e,2); memcpy(e->B,e->p,sizeof e->p);
      sk_morph(e,0.5f); int i=0; while(i<d->nparams && d->params[i].steps>=2) i++; CHECK(fabsf(e->p[i]-0.5f*(e->A[i]+e->B[i]))<1e-5,"morph");
      sk_mutate(e,0.5f); for(int k=0;k<d->nparams;k++) CHECK(e->p[k]>=0&&e->p[k]<=1,"mutate range");
      sk_store_user(e,"TEST ONE"); CHECK(sk_save_user(e,"/tmp/sk_test.bank")==1,"save");
      sk_engine *e2=sk_new(d,sr,0); CHECK(sk_load_user(e2,"/tmp/sk_test.bank")==1,"load");
      int same=1; for(int k=0;k<d->nparams;k++) if(fabsf(e2->user[0].v[k]-e->user[0].v[k])>1e-3f) same=0; CHECK(same,"bank round trip");
      sk_free(e); sk_free(e2);}

#ifdef B3
    /* drawbars: 4' only on note 57 sounds at 440 Hz; 16' only at 110 Hz */
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"d8",0); sk_set_param(e,"d4",1); sk_set_param(e,"leslie",0); sk_set_param(e,"vib",0); sk_set_param(e,"drive",0); sk_set_param(e,"click",0);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f=zc_freq(L+4800,N-4800,sr); CHECK(fabs(f-440.0)<2.0,"4' drawbar %.1f",f); sk_free(e); }
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"d8",0); sk_set_param(e,"d16",1); sk_set_param(e,"leslie",0); sk_set_param(e,"vib",0); sk_set_param(e,"drive",0); sk_set_param(e,"click",0);
      sk_note_on(e,57,1); sk_render(e,L,R,N); double f=zc_freq(L+4800,N-4800,sr); CHECK(fabs(f-110.0)<2.0,"16' drawbar %.1f",f); sk_free(e); }
    /* percussion: fires on the first key only, decays away */
    { sk_engine *e=sk_new(d,sr,0); sk_set_param(e,"d8",1); sk_set_param(e,"perc",1); sk_set_param(e,"pharm",1); sk_set_param(e,"pdec",0); sk_set_param(e,"pvol",1); sk_set_param(e,"leslie",0); sk_set_param(e,"vib",0); sk_set_param(e,"click",0); sk_set_param(e,"drive",0);
      sk_note_on(e,60,1); CHECK(((b3_voice*)sk_vptr(e,0))->perc_env>0.9f,"perc not triggered on first key");
      sk_note_on(e,64,1); int other=0; for(int k=0;k<e->nv;k++) if(e->slot[k].active && e->slot[k].note==64) other=k; CHECK(((b3_voice*)sk_vptr(e,other))->perc_env==0.0f,"perc fired on second key"); sk_free(e); }
#endif
    /* data inlet: a mapped continuous parameter follows the data value */
    { sk_engine *e=sk_new(d,sr,0); int i=0; while(i<d->nparams && d->params[i].steps>=2) i++;
      e->p[i]=0.5f; CHECK(sk_data_map(e,d->params[i].name,0.3f)==i,"datamap"); sk_set_data(e,1.0f); for(int k=0;k<40;k++) sk_render(e,L,R,64); CHECK(fabsf(e->ps[i]-0.8f)<0.01f,"data +1 -> %.3f",e->ps[i]);
      sk_set_data(e,0.0f); for(int k=0;k<40;k++) sk_render(e,L,R,64); CHECK(fabsf(e->ps[i]-0.2f)<0.01f,"data 0 -> %.3f",e->ps[i]);
      CHECK(sk_data_map(e,"nonexistent",0.3f)==-1,"bad datamap name"); sk_data_map(e,d->params[i].name,0.0f); CHECK(e->ndmap==0,"datamap remove"); sk_free(e); }
    /* polyphony: 12 notes on 6 voices, stealing never produces NaN or runaway, sustain holds, panic silences */
    { sk_engine *e=sk_new(d,sr,6); sk_load_preset(e,0); for(int k=0;k<12;k++){ sk_note_on(e,40+k*3,0.7f); sk_render(e,L,R,480);} int bad=0; float pk=0;
      sk_render(e,L,R,N); for(int i=0;i<N;i++){ if(!isfinite(L[i])||!isfinite(R[i])) bad++; if(fabsf(L[i])>pk) pk=fabsf(L[i]); }
      CHECK(!bad,"poly NaN"); CHECK(pk<4.0f,"poly peak %.2f",pk);
      sk_set_sustain(e,1); for(int k=0;k<12;k++) sk_note_off(e,40+k*3); sk_render(e,L,R,N); double rs=rms(L+N-2400,2400); sk_set_sustain(e,0); sk_render(e,L,R,N); double rr=rms(L+N-2400,2400);
      CHECK(!d->mono ? rs>1e-4 || d->nvoices==1 : 1,"sustain pedal did not hold (rms %.5f)",rs); CHECK(rr<1e-3,"release after pedal up (rms %.5f)",rr);
      sk_note_on(e,60,1); sk_panic(e); sk_render(e,L,R,N); CHECK(rms(L+N-2400,2400)<1e-3,"panic"); sk_free(e); }
    printf(fails ? "\n%d FAILURES\n" : "\nall checks passed\n", fails);
    return fails != 0;
}
