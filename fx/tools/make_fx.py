#!/usr/bin/env python3
"""Generates the tessera effect abstractions (abs/) and help patches (help/).

    python3 tools/make_fx.py

Parameters and presets are read from the C sources, so the GUI can never disagree with the engine.
Every abstraction is a graph-on-parent panel made of vanilla GUI objects on one fixed grid.
Every help patch has a main page (documentation) with the subpatches presets, parameters,
messages, examples and engine, and is checked for overlapping boxes: the generator refuses to write if there are any.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxdocs as D

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ABS, HELP = os.path.join(ROOT, "abs"), os.path.join(ROOT, "help")
CH, LH, GAP = 8, 20, 10
BANNER = "=^..^=   %s   version 1 (2026)   sofia vallejo budziszewski   iem kug graz austria"
PANEL, HEADER, INK, BG, CTL, LB = "#f6f6f6", "#ececec", "#202020", "#ffffff", "#5a5a5a", "#303030"


# ---------------------------------------------------------------- read the C tables
def num(s):
    return float(s.strip().rstrip("f"))

def load(name):
    src, ptab, stab, obj, inst = D.SRC[name]
    txt = open(os.path.join(ROOT, src)).read()
    m = re.search(r"static const fx_param %s\[\] = \{(.*?)\n\};" % ptab, txt, re.S)
    params = []
    for e in re.finditer(r'\{"(\w+)",\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*(\d+),\s*("[^"]*"|0)\}', m.group(1)):
        n, lo, hi, df, fl, st, op = e.groups()
        st = int(st)
        params.append(dict(name=n, lo=num(lo), hi=num(hi), default=num(df), log="FX_LOG" in fl, int_="FX_INT" in fl,
                           nomut="FX_NOMUT" in fl, steps=st, opts=[] if op == "0" else op.strip('"').split("|")))
    m = re.search(r"static const fx_preset %s\[\] = \{(.*?)\n\};" % stab, txt, re.S)
    presets = [(a, b) for a, b in re.findall(r'\{"([^"]+)",\s*"([^"]*)"\}', m.group(1))]
    return params, presets

def fmt(v):
    return "%g" % v


# ---------------------------------------------------------------- abstraction
class Patch:
    def __init__(self, canvas):
        self.lines, self.conns, self.n = [canvas], [], 0
    def add(self, line):
        self.lines.append(line); self.n += 1; return self.n - 1
    def obj(self, x, y, t): return self.add("#X obj %d %d %s;" % (x, y, t))
    def msg(self, x, y, t): return self.add("#X msg %d %d %s;" % (x, y, t))
    def text(self, x, y, t, w=None): return self.add("#X text %d %d %s%s;" % (x, y, t, (", f %d" % w) if w else ""))
    def c(self, a, ao, b, bi): self.conns.append((a, ao, b, bi))
    def save(self, path, tail=()):
        for ln in self.lines: assert ln.rstrip().endswith(";"), ln
        for a, ao, b, bi in self.conns: assert 0 <= a < self.n and 0 <= b < self.n, (a, b)
        with open(path, "w") as f:
            f.write("\n".join(self.lines + ["#X connect %d %d %d %d;" % c for c in self.conns] + list(tail)) + "\n")

CW, PITCH = 360, 48
def layout(params):
    n = len(params)
    ncols = 1 if n <= 8 else 2
    top = 218                                  # height of the preset block in the left column
    if ncols == 1: return 1, [(0, top + i * PITCH) for i in range(n)], top + n * PITCH + 24
    nr = (n + 4) // 2                          # right column takes more rows because it starts higher
    nl = n - nr
    pos = [(0, top + i * PITCH) for i in range(nl)] + [(1, 56 + i * PITCH) for i in range(nr)]
    return 2, pos, max(top + nl * PITCH, 56 + nr * PITCH) + 24

def build_abstraction(name):
    params, presets = load(name)
    obj, inst = D.SRC[name][3], D.SRC[name][4]
    nin, nout, nctl = D.NIN[name], D.NOUT[name], D.NCTL[name]
    ncols, pos, H = layout(params)
    W = ncols * CW
    p = Patch("#N canvas 60 40 %d %d 12;" % (W + 760, max(H + 60, 760)))
    p.add("#X obj 0 0 cnv 1 %d %d empty empty empty 20 12 0 14 %s %s 0;" % (W, H, PANEL, LB))
    p.add("#X obj 0 0 cnv 1 %d 44 empty empty %s 20 22 0 16 %s %s 0;" % (W, name, HEADER, INK))
    p.text(W - 130, 14, "=^..^=")
    # preset block (left column)
    p.add("#X symbolatom 20 56 18 0 0 0 - \\$1-name -;")
    def bng(x, y, snd, label):
        p.add("#X obj %d %d bng 18 250 50 0 \\$1-%s empty %s 24 9 0 12 %s %s %s;" % (x, y, snd, label, BG, LB, LB))
    bng(180, 56, "prev", "prev"); bng(270, 56, "next", "next")
    bng(20, 100, "A", "a"); bng(90, 100, "B", "b"); bng(160, 100, "mutate", "mutate")
    p.add("#X obj 20 156 hsl 220 18 0 1 0 0 \\$1-morph empty morph 0 -9 0 12 %s %s %s 0 1;" % (BG, CTL, LB))
    p.add("#X obj 20 196 hsl 220 18 0 1 0 0 \\$1-data-in empty data 0 -9 0 12 %s %s %s 0 1;" % (BG, CTL, LB))
    # parameter rows
    for q, (col, y) in zip(params, pos):
        x = 20 + col * CW
        nme = q["name"]
        if q["steps"] >= 2 and q["steps"] <= 16:
            cell = 18 if q["steps"] <= 9 else 14
            p.add("#X obj %d %d hradio %d 1 0 %d \\$1-%s \\$1-%s-r %s 0 -9 0 12 %s %s %s 0;" % (x, y + 14, cell, q["steps"], nme, nme, nme, BG, CTL, LB))
            p.add("#X symbolatom %d %d 9 0 0 0 - \\$1-%s-t -;" % (x + q["steps"] * cell + 12, y + 14, nme))
        else:
            lo, hi = (0, q["steps"] - 1) if q["steps"] >= 2 else (q["lo"], q["hi"])
            p.add("#X obj %d %d hsl 200 18 %s %s %d 0 \\$1-%s \\$1-%s-r %s 0 -9 0 12 %s %s %s 0 1;" % (x, y + 14, fmt(lo), fmt(hi), 1 if q["log"] else 0, nme, nme, nme, BG, CTL, LB))
            p.add("#X obj %d %d nbx 6 18 -1e+37 1e+37 0 0 empty \\$1-%s-r empty 0 -8 0 11 %s %s %s 0 256;" % (x + 214, y + 14, nme, BG, LB, LB))
    # plumbing, outside the visible panel to the right
    bx = W + 60
    ins = []
    for i in range(nin): ins.append(p.obj(bx + 150 * i, 20, "inlet~"))
    ctl = p.obj(bx + 150 * max(nin, 1), 20, "inlet")
    arg = {"freeze~": "tessera/tspec~ freeze 2048 \\$1", "smear~": "tessera/tspec~ smear 2048 \\$1", "gate~": "tessera/tspec~ gate 2048 \\$1",
           "cross~": "tessera/tspec~ cross 2048 \\$1", "grain~": "tessera/tgrain~ \\$1", "modal~": "tessera/tmodal~ \\$1", "datamod~": "tessera/tdata~ \\$1"}[name]
    eng = p.obj(bx, 420, arg)
    for i in range(nin): p.c(ins[i], 0, eng, i)
    p.c(ctl, 0, eng, 0)
    for i, (snd, m) in enumerate([("prev", "prev"), ("next", "next"), ("A", "A"), ("B", "B"), ("mutate", "mutate")]):
        r = p.obj(bx + 100 * i, 120, "r \\$1-%s" % snd); mm = p.msg(bx + 100 * i, 160, m)
        p.c(r, 0, mm, 0); p.c(mm, 0, eng, 0)
    r = p.obj(bx, 230, "r \\$1-data-in"); mm = p.msg(bx, 270, "data \\$1"); p.c(r, 0, mm, 0); p.c(mm, 0, eng, 0)
    r = p.obj(bx + 200, 230, "r \\$1-morph"); mm = p.msg(bx + 200, 270, "morph \\$1"); p.c(r, 0, mm, 0); p.c(mm, 0, eng, 0)
    lb = p.obj(bx + 400, 230, "loadbang"); bb = p.msg(bx + 400, 270, "bang"); p.c(lb, 0, bb, 0); p.c(bb, 0, eng, 0)
    outs = []
    for i in range(nout):
        o = p.obj(bx + 150 * i, 520, "outlet~"); p.c(eng, i, o, 0); outs.append(o)
    for i in range(nctl):
        o = p.obj(bx + 150 * (nout + i), 520, "outlet"); p.c(eng, nout + i, o, 0)
    p.save(os.path.join(ABS, name + ".pd"), ["#X coords 0 -1 1 1 %d %d 1 0 0;" % (W, H)])
    return W, H


# ---------------------------------------------------------------- help pages with an overlap check
class Page:
    def __init__(self, name, w, h=None):
        self.name, self.w, self.h = name, w, h
        self.items, self.boxes, self.conns, self.ymax = [], [], [], 0
    def _add(self, line, name, x, y, w, h):
        self.items.append(line); self.boxes.append((name, x, y, w, h)); self.ymax = max(self.ymax, y + h); return len(self.items) - 1
    def text(self, x, y, s, wrap=None):
        assert not re.search(r'[,;$\\]', s), "comment has a Pd-special character: " + s
        s = s.lower()
        lines = 1 if not wrap else -(-len(s) // max(1, wrap - 6))
        self._add("#X text %d %d %s%s;" % (x, y, s, (", f %d" % wrap) if wrap else ""), "text " + s[:24], x, y, (len(s) if not wrap else min(wrap, len(s))) * CH, lines * LH)
        return y + lines * LH
    def para(self, x, y, s, wrap=100, gap=10): return self.text(x, y, s, wrap) + gap
    def msg(self, x, y, shown, raw=None):
        return self._add("#X msg %d %d %s;" % (x, y, raw if raw else "\\; " + shown), "msg " + shown[:24], x, y, (max(len(q.strip()) for q in raw.split("\\;")) if raw else len(shown)) * CH + 22, 19 * ((raw.count("\\;") + 1) if raw else 2) + 6)
    def obj(self, x, y, s): return self._add("#X obj %d %d %s;" % (x, y, s), "obj " + s[:24], x, y, (len(s) + 1) * CH + 6, 22)
    def tgl(self, x, y): return self._add("#X obj %d %d tgl 18 0 empty empty empty 17 7 0 10 -262144 -1 -1 0 1;" % (x, y), "tgl", x, y, 20, 20)
    def sat(self, x, y, w, recv): return self._add("#X symbolatom %d %d %d 0 0 0 - %s -;" % (x, y, w, recv), "symbolatom", x, y, w * CH + 12, 22)
    def fat(self, x, y, w, recv=None): return self._add("#X floatatom %d %d %d 0 0 0 - %s -;" % (x, y, w, recv or "-"), "floatatom", x, y, w * CH + 12, 22)
    def hsl(self, x, y, w, send): return self._add("#X obj %d %d hsl %d 18 0 1 0 0 %s empty empty -2 -8 0 12 -262144 -1 -1 0 1;" % (x, y, w, send), "hsl", x, y, w + 8, 20)
    def absbox(self, x, y, w, h, text): return self._add("#X obj %d %d %s;" % (x, y, text), "abs " + text, x, y, w, h)
    def sub(self, x, y, page):
        body = ["#N canvas 0 50 %d %d %s 0;" % (page.w, page.height(), page.name)] + page.items + ["#X connect %d %d %d %d;" % c for c in page.conns]
        body.append("#X restore %d %d pd %s;" % (x, y, page.name))
        return self._add("\n".join(body), "pd " + page.name, x, y, (len(page.name) + 4) * CH + 6, 22)
    def connect(self, a, ao, b, bi): self.conns.append((a, ao, b, bi))
    def height(self): return max(self.h or 0, self.ymax + 60)
    def lines(self): return self.items + ["#X connect %d %d %d %d;" % c for c in self.conns]
    def check(self):
        bad = 0
        for i in range(len(self.boxes)):
            for j in range(i + 1, len(self.boxes)):
                a, b = self.boxes[i], self.boxes[j]
                if a[1] < b[1] + b[3] + GAP and b[1] < a[1] + a[3] + GAP and a[2] < b[2] + b[4] + GAP and b[2] < a[2] + a[4] + GAP:
                    print("OVERLAP on page", self.name, a, b); bad += 1
        wide = max(b[1] + b[3] for b in self.boxes)
        if wide > self.w - 10: print("WIDE page", self.name, wide, "of", self.w); bad += 1
        return bad

def heading(pg, y, s): return pg.text(30, y, "--- " + s + " ---") + 24

def range_text(q):
    if q["steps"] >= 2:
        return " | ".join(q["opts"]) if q["opts"] else "0 to %d" % (q["steps"] - 1)
    r = "%s to %s" % (fmt(q["lo"]), fmt(q["hi"]))
    return r + (" log" if q["log"] else "") + (" whole numbers" if q["int_"] else "")

def pdesc(name, pn):
    return D.PDESC_OVERRIDE.get((name, pn), D.PDESC.get(pn, ""))


def page_presets(name, inst, presets):
    pg = Page("presets", 900)
    y = 16
    y = pg.para(30, y, "factory sounds are stored as plain text. click a box to load that sound. every slider in the effect follows", 100)
    y = heading(pg, y, "factory presets")
    top = y
    half = -(-len(presets) // 2)
    for i, (n, _) in enumerate(presets):
        cx = 30 + (i // half) * 430; cy = top + (i % half) * 54
        pg.msg(cx, cy, "%s preset %d" % (inst, i)); pg.text(cx + 210, cy + 2, "%d  %s" % (i, n.lower()))
    y = top + half * 54 + 20
    y = heading(pg, y, "now playing")
    pg.sat(30, y, 20, inst + "-name"); pg.fat(30 + 20 * CH + 40, y, 4, inst + "-index")
    pg.text(30 + 20 * CH + 150, y + 2, "name and number (minus one means edited)"); y += 44
    x = 30
    for shown in ["next", "prev", "mutate 0.3"]:
        s = "%s %s" % (inst, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    y += 60
    y = pg.para(30, y, "next and prev step through the presets and wrap around. mutate varies the current sound by a random amount from 0 to 1. mutate leaves mix and level alone", 100)
    y = heading(pg, y, "morph between two sounds")
    x = 30
    for shown in ["A", "B"]:
        s = "%s %s" % (inst, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    pg.text(x, y + 2, "a and b remember the current sound"); y += 60
    pg.text(30, y + 2, "morph"); pg.hsl(110, y, 300, inst + "-morph"); y += 44
    y = pg.para(30, y, "load one preset and press a then load another and press b then move the morph slider. Every continuous parameter blends smoothly. Switches change at the middle", 100)
    y = heading(pg, y, "your own user bank")
    x = 30
    for shown in ["store mysound", "save my.bank", "load my.bank"]:
        s = "%s %s" % (inst, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    y += 60
    y = pg.para(30, y, "store keeps the current sound after the factory presets. save writes the user bank to a text file next to this patch and load reads such a file", 100)
    y = heading(pg, y, "presets with stash")
    y = pg.para(30, y, "every slider of the effect sends to %s-param and listens on %s-param-r. This is the naming scheme of the stash preset library: create [stash/bank %s] and send it a scan message with the name of the patch that holds the effect. It follows the stash rules but has not been tested together yet" % (inst, inst, inst), 100)
    return pg

def page_parameters(name, inst, params):
    pg = Page("parameters", 1200)
    y = 16
    y = pg.para(30, y, "every parameter is also a message: [%s value( in real units or for a switch the option name or number. the controls in the effect panel and these messages do the same" % "name", 120)
    pg.text(30, y, "parameter"); pg.text(210, y, "values"); pg.text(560, y, "what it does"); y += 34
    for q in params:
        rt = range_text(q); d = pdesc(name, q["name"])
        rl = -(-len(rt) // 30); dl = -(-len(d) // 48)
        pg.text(30, y, q["name"]); pg.text(210, y, rt, 36); pg.text(560, y, d, 54)
        y += max(1, rl, dl) * LH + 16
    pg.ymax = max(pg.ymax, y)
    y = pg.para(30, y + 10, "any continuous parameter can be driven by data: send datamap name amount and then data 0 to 1. see the messages page", 120)
    return pg

def page_messages(name, inst, presets):
    pg = Page("messages", 1300)
    y = 16
    y = pg.para(30, y, "click any box to send it to the effect. The effect listens to its name so you can use [s %s] from anywhere in your patch" % inst, 110)
    rows = [("params", "prints every parameter and its range in the console"),
            ("preset 3", "loads preset 3 (or a preset by name: preset %s)" % presets[1][0].lower()),
            ("next", "next preset (wraps around)"), ("prev", "previous preset"),
            ("mutate 0.4", "random variation of the current sound. 0 is nothing and 1 is a lot"),
            ("A", "remember the current sound as a"), ("B", "remember the current sound as b"),
            ("morph 0.5", "blend between a and b from 0 to 1"),
            ("data 0.7", "data inlet: a value from 0 to 1"),
            ("datamap %s 0.5" % D.SWEEP.get(name, "mix"), "map the data inlet to a continuous parameter. 0.5 means the full range of the parameter at data 0 and 1. amount 0 removes the mapping. up to four mappings"),
            ("datamap clear", "remove all data mappings"),
            ("store mysound", "keep the current sound in the user bank"),
            ("save my.bank", "write the user bank to a text file"), ("load my.bank", "read a user bank")]
    rows += D.MSGS.get(name, [])
    pg.text(30, y, "message"); pg.text(430, y, "what it does"); y += 34
    for shown, d in rows:
        pg.msg(30, y, "%s %s" % (inst, shown)); n = -(-len(d) // 70)
        pg.text(430, y + 2, d, 76); y += max(54, n * LH + 14)
    if name == "grain~" or name == "modal~" or name == "datamod~":
        pass
    return pg

SWEEP_RANGE = {"freeze~": ("jitter", 0, 1), "smear~": ("time", 50, 3000), "gate~": ("thresh", -60, -5), "cross~": ("amount", 0, 1),
               "grain~": ("scrub", 0, 0.6), "modal~": ("root", 36, 72), "datamod~": ("rate", 0.2, 8)}
D.SWEEP = {k: v[0] for k, v in SWEEP_RANGE.items()}

def page_examples(name, inst, presets):
    sw, lo, hi = SWEEP_RANGE[name]
    pg = Page("examples", 1300, 1000)
    y = 16
    y = pg.para(30, y, "copy these into your own patch. They all talk to the effect through [s %s] so the engine page must be open with dsp on" % inst, 130)
    x0, y0 = 30, y + 10
    pg.text(x0, y0, "1  walk through the presets")
    t = pg.tgl(x0, y0 + 34); m = pg.obj(x0, y0 + 74, "metro 2500"); r = pg.obj(x0, y0 + 114, "random %d" % len(presets))
    pp = pg.obj(x0, y0 + 154, "list prepend preset"); tr = pg.obj(x0, y0 + 194, "list trim"); sn = pg.obj(x0, y0 + 234, "s " + inst)
    for a, b in [(t, m), (m, r), (r, pp), (pp, tr), (tr, sn)]: pg.connect(a, 0, b, 0)
    x1 = 330
    pg.text(x1, y0, "2  slow morph between preset 0 and 1")
    s1 = pg.msg(x1, y0 + 34, "%s preset 0 and a and preset 1 and b" % inst, "\\; %s preset 0 \\; %s A \\; %s preset 1 \\; %s B" % (inst, inst, inst, inst))
    t2 = pg.tgl(x1, y0 + 154); m2 = pg.obj(x1, y0 + 194, "metro 6000")
    g2 = pg.msg(x1, y0 + 234, "0 1 3000 0 3000 3000", "0 \\, 1 3000 \\, 0 3000 3000")
    l2 = pg.obj(x1, y0 + 274, "line 0 30"); p2 = pg.obj(x1, y0 + 314, "list prepend morph"); u2 = pg.obj(x1, y0 + 354, "list trim"); n2 = pg.obj(x1, y0 + 394, "s " + inst)
    for a, b in [(t2, m2), (m2, g2), (g2, l2), (l2, p2), (p2, u2), (u2, n2)]: pg.connect(a, 0, b, 0)
    x2 = 740
    pg.text(x2, y0, "3  sweep %s with a ramp" % sw)
    t3 = pg.tgl(x2, y0 + 34); m3 = pg.obj(x2, y0 + 74, "metro 4000")
    g3 = pg.msg(x2, y0 + 114, "%s %s" % (fmt(lo), fmt(hi)), "%s \\, %s 2000 \\, %s 2000 2000" % (fmt(lo), fmt(hi), fmt(lo)))
    l3 = pg.obj(x2, y0 + 154, "line %s 30" % fmt(lo)); p3 = pg.obj(x2, y0 + 194, "list prepend %s" % sw); u3 = pg.obj(x2, y0 + 234, "list trim"); n3 = pg.obj(x2, y0 + 274, "s " + inst)
    for a, b in [(t3, m3), (m3, g3), (g3, l3), (l3, p3), (p3, u3), (u3, n3)]: pg.connect(a, 0, b, 0)
    yy = y0 + 500
    pg.text(30, yy, "4  data in: a ramp stands in for a sensor or a csv column. Click the map box once first")
    mm = pg.msg(30, yy + 34, "%s datamap %s 0.4" % (inst, D.SWEEP.get(name, "mix")))
    t5 = pg.tgl(30, yy + 94); m5 = pg.obj(30, yy + 134, "metro 4000")
    g5 = pg.msg(30, yy + 174, "0 1 2000 0 2000 2000", "0 \\, 1 2000 \\, 0 2000 2000")
    l5 = pg.obj(30, yy + 214, "line 0 30"); p5 = pg.obj(30, yy + 254, "list prepend data"); u5 = pg.obj(30, yy + 294, "list trim"); s5 = pg.obj(30, yy + 334, "s " + inst)
    for a, b in [(t5, m5), (m5, g5), (g5, l5), (l5, p5), (p5, u5), (u5, s5)]: pg.connect(a, 0, b, 0)
    pg.text(330, yy + 94, "in your own patch send any value from 0 to 1 to [s %s] as: data 0.7. For a csv series scale it to the range 0 to 1 first. [tessera/datamod~] does that for you and its data outlet can be connected straight to the inlet of any tessera effect" % inst, 70)
    pg.ymax = max(pg.ymax, yy + 380)
    return pg

def chain(pg, x, y, kind):
    """a sound source that can be switched on and off. returns the index of its output object"""
    t = pg.tgl(x, y); pg.text(x + 30, y + 2, "sound")
    tm = pg.msg(x, y + 36, "1 80", "\\$1 80"); ln = pg.obj(x, y + 74, "line~")
    pg.connect(t, 0, tm, 0); pg.connect(tm, 0, ln, 0)
    if kind == "noise":
        src = pg.obj(x + 100, y + 36, "noise~"); gm = pg.obj(x, y + 112, "*~"); gv = pg.obj(x, y + 150, "*~ 0.15")
        pg.connect(src, 0, gm, 0)
    elif kind == "gate":
        o1 = pg.obj(x + 100, y + 36, "osc~ 440"); nz = pg.obj(x + 200, y + 36, "noise~"); nzs = pg.obj(x + 200, y + 74, "*~ 0.15"); sm = pg.obj(x + 100, y + 112, "+~")
        pg.connect(o1, 0, sm, 0); pg.connect(nz, 0, nzs, 0); pg.connect(nzs, 0, sm, 1)
        gm = pg.obj(x, y + 150, "*~"); gv = pg.obj(x, y + 188, "*~ 0.15"); pg.connect(sm, 0, gm, 0)
    else:
        o1 = pg.obj(x + 100, y + 36, "osc~ 220"); o2 = pg.obj(x + 200, y + 36, "osc~ 330"); o3 = pg.obj(x + 300, y + 36, "osc~ 495")
        s1 = pg.obj(x + 100, y + 74, "+~"); sm = pg.obj(x + 100, y + 112, "+~")
        pg.connect(o1, 0, s1, 0); pg.connect(o2, 0, s1, 1); pg.connect(s1, 0, sm, 0); pg.connect(o3, 0, sm, 1)
        gm = pg.obj(x, y + 150, "*~"); gv = pg.obj(x, y + 188, "*~ 0.15"); pg.connect(sm, 0, gm, 0)
    pg.connect(ln, 0, gm, 1); pg.connect(gm, 0, gv, 0)
    return gv

def page_engine(name, inst, W, H):
    pg = Page("engine", 430 + W + 400, 900)
    nin, nout, nctl = D.NIN[name], D.NOUT[name], D.NCTL[name]
    pg.text(30, 16, "the effect below is the abstraction itself with its panel. Its name is %s" % inst)
    pg.msg(30, 52, "pd dsp 1", "\\; pd dsp 1"); pg.msg(170, 52, "pd dsp 0", "\\; pd dsp 0"); pg.text(310, 56, "dsp on and off")
    ex = 430
    box = pg.absbox(ex, 100, W + 14, H + 8, "tessera/%s %s" % (name, inst))
    if name == "datamod~":
        pg.text(30, 110, "the four lanes make an oscillator sing: lane 1 is the pitch and lane 2 is the level", 40)
        mul = pg.obj(30, 190, "*~ 600"); add = pg.obj(30, 230, "+~ 200"); osc = pg.obj(30, 270, "osc~")
        gm = pg.obj(30, 310, "*~"); gv = pg.obj(30, 350, "*~ 0.1"); dac = pg.obj(30, 440, "dac~")
        lv = pg.obj(130, 270, "*~ 0.8")
        pg.connect(box, 0, mul, 0); pg.connect(mul, 0, add, 0); pg.connect(add, 0, osc, 0); pg.connect(osc, 0, gm, 0)
        pg.connect(box, 1, lv, 0); pg.connect(lv, 0, gm, 1); pg.connect(gm, 0, gv, 0); pg.connect(gv, 0, dac, 0); pg.connect(gv, 0, dac, 1)
        pg.text(30, 500, "lane 3 and lane 4 are free. Connect them to any signal input", 40)
        rt = pg.obj(ex + W + 60, 100, "route data cells"); pg.connect(box, 4, rt, 0)
        pg.text(ex + W + 60, 64, "control outlet: data")
        fa = pg.fat(ex + W + 60, 150, 8)
        pg.connect(rt, 0, fa, 0)
        pg.text(ex + W + 60, 200, "the second control outlet sends the four lanes as a list or the cells of the automaton", 36)
        pr = pg.obj(ex + W + 60, 290, "print tick"); pg.connect(box, 5, pr, 0)
        pr2 = pg.obj(ex + W + 200, 150, "print cells"); pg.connect(rt, 1, pr2, 0)
        return pg
    if name == "cross~":
        a = chain(pg, 30, 150, "noise"); pg.text(30, 110, "sound a: noise")
        b = chain(pg, 30, 420, "tones"); pg.text(30, 390, "sound b: three tones")
        pg.connect(a, 0, box, 0); pg.connect(b, 0, box, 1)
    else:
        src = chain(pg, 30, 130, "gate" if name == "gate~" else "tones")
        pg.connect(src, 0, box, 0)
        pg.text(30, 370, "use [adc~] instead of this source to process a live input", 36)
    vol = pg.obj(ex, 100 + H + 40, "*~ 0.7"); dac = pg.obj(ex, 100 + H + 80, "dac~")
    pg.connect(box, 0, vol, 0); pg.connect(vol, 0, dac, 0)
    if nout == 2:
        vol2 = pg.obj(ex + 120, 100 + H + 40, "*~ 0.7"); pg.connect(box, 1, vol2, 0); pg.connect(vol2, 0, dac, 1)
    else:
        pg.connect(vol, 0, dac, 1)
    if name == "modal~":
        pg.msg(30, 600, "m1 strike 1", "\\; %s strike 1" % inst); pg.text(30, 656, "strike rings the modes without an input", 36)
        pg.msg(30, 716, "m1 learn", "\\; %s learn" % inst); pg.text(30, 772, "switch the sound on and press learn. then choose material learned in the panel", 36)
        fa = pg.fat(ex + W + 60, 100, 6); pg.text(ex + W + 60, 64, "modes found by learn")
        sel = pg.obj(ex + W + 60, 150, "route learned"); pg.connect(box, 2, sel, 0); pg.connect(sel, 0, fa, 0)
    if name == "grain~":
        pg.text(ex + W + 60, 64, "pitch outlet: hz and confidence")
        rt = pg.obj(ex + W + 60, 130, "route pitch"); pg.connect(box, 2, rt, 0)
        fa = pg.fat(ex + W + 60, 180, 8); pg.connect(rt, 0, fa, 0)
    pg.ymax = max(pg.ymax, 100 + H + 130)
    return pg

def build_help(name, W, H):
    params, presets = load(name)
    inst = D.SRC[name][4]
    pr, pm, ms, ex, en = page_presets(name, inst, presets), page_parameters(name, inst, params), page_messages(name, inst, presets), page_examples(name, inst, presets), page_engine(name, inst, W, H)
    main = Page("main", 1000)
    y = 16
    main.text(30, y, BANNER % name); y += 30
    main.text(30, y, D.TITLE[name]); y += 40
    y = main.para(30, y, D.DESC[name], 110, 20)
    y = heading(main, y, "how it was made")
    y = main.para(30, y, D.HOW[name], 110, 10) + 10
    y = heading(main, y, "how to use")
    for i, s in enumerate(D.HOWTO[name]): y = main.para(30, y, "%d  %s" % (i + 1, s), 110, 10)
    y = main.para(30, y, "everything is a message: see the messages page for the full list and the examples page for patches to copy. Every effect has presets and a and b morph and mutate and a data inlet", 110, 10) + 10
    for s in D.TIPS[name]: y = main.para(30, y, "try:  " + s, 110, 10)
    y += 14
    y = heading(main, y, "common problems")
    for s in D.PROBLEMS[name] + ["names: give every instance its own name as the first argument like [tessera/%s %s]. The name is also the stash namespace" % (name, inst)]: y = main.para(30, y, s, 110, 10)
    y += 14
    y = heading(main, y, "click a box to open that page")
    labels = [("presets", pr, "all sounds and morph"), ("parameters", pm, "every control"), ("messages", ms, "clickable list"), ("examples", ex, "patches to copy"), ("engine", en, "the effect and its dsp")]
    for i, (n, page, d) in enumerate(labels):
        x = 30 + i * 190
        main.sub(x, y, page); main.text(x, y + 34, d, 22)
    y += 100
    y = main.para(30, y, "this is an original design. Sounds depend on your material and settings. It has been tested offline and has not been tested on every platform yet", 110, 0)
    bad = sum(p.check() for p in (pr, pm, ms, ex, en, main))
    out = ["#N canvas 0 50 1040 %d 12;" % main.height()] + main.lines()
    path = os.path.join(HELP, name + "-help.pd")
    if bad: print("not written:", name); return bad
    with open(path, "w") as f: f.write("\n".join(out) + "\n")
    print("%s: help written, abstraction %dx%d" % (name, W, H))
    return 0

def main():
    os.makedirs(ABS, exist_ok=True); os.makedirs(HELP, exist_ok=True)
    bad = 0
    for name in D.ORDER:
        W, H = build_abstraction(name)
        bad += build_help(name, W, H)
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
