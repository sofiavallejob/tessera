#!/usr/bin/env python3
"""End-to-end test in a real Pd (headless, batch mode): builds a small patch per effect, renders audio to a wav
file and checks the result. Usage: python3 test/pd_smoke.py <pd binary> <folder that contains the tessera folder>"""
import os, subprocess, sys, wave, struct
import numpy as np

PD, LIB = sys.argv[1], sys.argv[2]
OUT = "/tmp/pdsmoke"
os.makedirs(OUT, exist_ok=True)
SR = 44100

class P:
    def __init__(self): self.l = ["#N canvas 0 0 900 700 12;"]; self.c = []
    def o(self, t): self.l.append("#X obj 10 %d %s;" % (10 + 24 * len(self.l), t)); return len(self.l) - 2
    def m(self, t): self.l.append("#X msg 10 %d %s;" % (10 + 24 * len(self.l), t)); return len(self.l) - 2
    def w(self, a, ao, b, bi=0): self.c.append("#X connect %d %d %d %d;" % (a, ao, b, bi))
    def text(self): return "\n".join(self.l + self.c) + "\n"

def render(name, effect, nin_src, nch, script, secs=4.0, extra=None):
    """effect: 'tessera/freeze~ f1'. script: list of (time_ms, 'message text with ; separators')"""
    p = P()
    eff = p.o(effect)
    if nin_src >= 1:
        osc = p.o("osc~ 220"); osc2 = p.o("osc~ 330"); s = p.o("+~"); g = p.o("*~"); gl = p.o("line~"); gv = p.o("*~ 0.2")
        r = p.o("r gate"); p.w(osc, 0, s, 0); p.w(osc2, 0, s, 1); p.w(s, 0, g, 0); p.w(r, 0, gl, 0); p.w(gl, 0, g, 1); p.w(g, 0, gv, 0); p.w(gv, 0, eff, 1 if nin_src == 2 else 0)
        if nin_src == 2:
            nz = p.o("noise~"); nzs = p.o("*~ 0.2"); gn = p.o("*~"); rn = p.o("r gate2"); ln = p.o("line~")
            p.w(nz, 0, nzs, 0); p.w(nzs, 0, gn, 0); p.w(rn, 0, ln, 0); p.w(ln, 0, gn, 1); p.w(gn, 0, eff, 0)
    ws = p.o("writesf~ %d" % nch)
    for i in range(nch): p.w(eff, i, ws, i)
    lb = p.o("loadbang")
    wav = "%s/%s.wav" % (OUT, name)
    op = p.m("open -bytes 2 %s" % wav); st = p.m("start"); p.w(lb, 0, op, 0); p.w(op, 0, ws, 0); p.w(lb, 0, st, 0); p.w(st, 0, ws, 0)
    dsp = p.m("\\; pd dsp 1"); p.w(lb, 0, dsp, 0)
    for t, txt in script:
        d = p.o("delay %d" % max(t, 1)); t_ = txt[3:] if txt.startswith("\\; ") else txt; mm = p.m("\\; " + t_.replace("; ", " \\; ")); p.w(lb, 0, d, 0); p.w(d, 0, mm, 0)
    d = p.o("delay %d" % int(secs * 1000)); sp = p.m("stop"); p.w(lb, 0, d, 0); p.w(d, 0, sp, 0); p.w(sp, 0, ws, 0)
    d2 = p.o("delay %d" % int(secs * 1000 + 300)); q = p.m("\\; pd quit"); p.w(lb, 0, d2, 0); p.w(d2, 0, q, 0)
    path = "%s/%s.pd" % (OUT, name)
    open(path, "w").write(p.text())
    if os.path.exists(wav): os.remove(wav)
    r = subprocess.run([PD, "-nogui", "-nosound", "-stderr", "-path", LIB, "-open", path], capture_output=True, text=True, timeout=60)
    log = r.stdout + r.stderr
    if not os.path.exists(wav): print("no wav for", name); print(log[-800:]); return None, log
    global SR
    w = wave.open(wav); n = w.getnframes(); ch = w.getnchannels(); SR = w.getframerate()
    d = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float32) / 32768.0
    return d.reshape(-1, ch), log

def rms(x): return float(np.sqrt(np.mean(x ** 2))) if len(x) else 0.0
def seg(a, t0, t1, ch=0): return a[int(t0 * SR):int(t1 * SR), ch]
def peak_freq(x, lo, hi):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))); f = np.fft.rfftfreq(len(x), 1 / SR)
    m = (f >= lo) & (f <= hi); return float(f[m][np.argmax(X[m])])

fails = 0
def check(c, msg, log=""):
    global fails
    print(("ok:   " if c else "FAIL: ") + msg)
    if not c: fails += 1
def errs(log): return [l for l in log.splitlines() if any(k in l.lower() for k in ("error", "couldn't", "no such object", "warning"))]

# freeze: tone on until 1.0 s, freeze at 0.8 s -> still sounding at 2-3 s
a, log = render("freeze_on", "tessera/freeze~ f1", 1, 1, [(10, "\\; gate 1 10"), (800, "\\; f1 freeze 1"), (1000, "\\; gate 0 10")], 3.5)
b, log2 = render("freeze_off", "tessera/freeze~ f1", 1, 1, [(10, "\\; gate 1 10; f1 freeze 0"), (1000, "\\; gate 0 10")], 3.5)
if a is not None and b is not None:
    check(rms(seg(a, 2, 3)) > 0.02, "freeze~ keeps sounding after the input stops (rms %.4f)" % rms(seg(a, 2, 3)))
    check(rms(seg(b, 2, 3)) < 0.002, "freeze~ off lets the sound end (rms %.5f)" % rms(seg(b, 2, 3)))
    check(not errs(log) and not errs(log2), "freeze~ no Pd errors " + str(errs(log)[:2]))
# smear
a, log = render("smear_long", "tessera/smear~ s1", 1, 1, [(10, "\\; gate 1 10; s1 time 3000; s1 shape 0.9"), (1000, "\\; gate 0 10")], 3.0)
b, log2 = render("smear_short", "tessera/smear~ s1", 1, 1, [(10, "\\; gate 1 10; s1 time 2; s1 shape 0.9"), (1000, "\\; gate 0 10")], 3.0)
if a is not None and b is not None:
    check(rms(seg(a, 1.8, 2.4)) > 5 * rms(seg(b, 1.8, 2.4)) + 0.001, "smear~ long time rings on (%.4f vs %.5f)" % (rms(seg(a, 1.8, 2.4)), rms(seg(b, 1.8, 2.4))))
    check(not errs(log), "smear~ no Pd errors " + str(errs(log)[:2]))
# gate: tone 220 + 330 only; high threshold keeps the stronger bins. check it runs and reduces level
a, log = render("gate", "tessera/gate~ t1", 1, 1, [(10, "\\; gate 1 10; t1 thresh -6; t1 floor -80")], 2.5)
if a is not None:
    check(rms(seg(a, 1.2, 2.2)) > 0.005 and rms(seg(a, 1.2, 2.2)) < 0.3, "gate~ passes sound (rms %.4f)" % rms(seg(a, 1.2, 2.2)))
    check(not errs(log), "gate~ no Pd errors " + str(errs(log)[:2]))
# cross: noise (a, left) takes the spectral shape of tones (b is the second source = the tone chain here)
a, log = render("cross", "tessera/cross~ c1", 2, 1, [(10, "\\; gate 1 10; gate2 1 10; c1 cmode 0; c1 width 3")], 3.0)
if a is not None:
    pf = peak_freq(seg(a, 1.5, 2.9), 100, 800)
    check(rms(seg(a, 1.5, 2.9)) > 0.001 and (abs(pf - 220) < 15 or abs(pf - 330) < 15), "cross~ noise takes the tone envelope (peak %.1f Hz rms %.4f)" % (pf, rms(seg(a, 1.5, 2.9))))
    check(not errs(log), "cross~ no Pd errors " + str(errs(log)[:2]))
# grain: pitch outlet and sound; the outlet is printed via the abstraction? use the engine directly to read it
a, log = render("grain", "tessera/grain~ g1", 1, 2, [(10, "\\; gate 1 10; g1 preset 8; g1 mix 1")], 3.0)
if a is not None:
    check(rms(seg(a, 1.0, 2.9)) > 0.01, "grain~ makes sound (rms %.4f)" % rms(seg(a, 1.0, 2.9)))
    check(not errs(log), "grain~ no Pd errors " + str(errs(log)[:2]))
# modal: strike, root 60 glass
a, log = render("modal", "tessera/modal~ m1", 1, 2, [(10, "\\; m1 root 60; m1 material 0; m1 mix 1; m1 decay 3"), (200, "\\; m1 strike 1")], 3.0)
if a is not None:
    pf = peak_freq(seg(a, 0.5, 2.0), 150, 700)
    check(rms(seg(a, 0.5, 2.0)) > 0.002 and abs(pf - 261.6) < 4, "modal~ strike rings at the root (peak %.1f Hz rms %.4f)" % (pf, rms(seg(a, 0.5, 2.0))))
    check(not errs(log), "modal~ no Pd errors " + str(errs(log)[:2]))
# datamod: csv from the patch folder
open(OUT + "/series.csv", "w").write("t,v\n" + "\n".join("%d,%g" % (i, np.sin(2 * np.pi * i / 40)) for i in range(400)) + "\n")
a, log = render("datamod", "tessera/datamod~ d1", 0, 4, [(10, "\\; d1 preset 0")], 4.0)
if a is not None:
    ok = np.all(a >= -1e-3) and np.all(a <= 1.001)
    check(ok and np.std(seg(a, 1, 3.9, 0)) > 0.05, "datamod~ lorenz moves inside 0..1 (std %.3f)" % np.std(seg(a, 1, 3.9, 0)))
    check(not errs(log), "datamod~ no Pd errors " + str(errs(log)[:2]))
b, log = render("datamod_csv", "tessera/datamod~ d1", 0, 4, [(10, "\\; d1 source series; d1 rate 2; d1 smooth 0; d1 read series.csv 2")], 3.0)
if b is not None:
    v = seg(b, 0.5, 2.9, 0)
    check(np.max(v) > 0.9 and np.min(v) < 0.1, "datamod~ plays a csv column as an lfo (range %.2f..%.2f)" % (np.min(v), np.max(v)))
    check(not [l for l in errs(log) if "could not read" in l or "failed" in l], "datamod~ csv read has no error " + str(errs(log)[:2]))
c, log = render("datamod_ca", "tessera/datamod~ d1", 0, 4, [(10, "\\; d1 preset 5")], 3.0)
if c is not None:
    v = seg(c, 0.5, 2.9, 0)
    check(set(np.round(np.unique(v), 3)).issubset({0.0, 1.0}) and len(np.unique(v)) == 2, "datamod~ automaton gives gates (values %s)" % np.unique(np.round(v, 3))[:5])
print("\n%s (%d failures)" % ("FAILED" if fails else "passed", fails))
sys.exit(1 if fails else 0)
