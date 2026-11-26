#!/usr/bin/env python3
"""Generates dx7~-player.pd: scan a folder, prev / next, MIDI in. Checks the layout for overlaps."""
import sys
CH = 7
objs, conns, boxes = [], [], []

def add(kind, x, y, t, w, h=20, name=None):
    objs.append("#X %s %d %d %s;" % (kind, x, y, t))
    boxes.append((name or t[:18], x, y, w, h))
    return len(objs) - 1

def text(x, y, s, wrap=None):
    cols = wrap or len(s)
    lines = max(1, -(-len(s) // max(1, cols - 6))) if wrap else 1
    return add("text", x, y, s + (", f %d" % wrap if wrap else ""), min(len(s), cols) * CH, lines * 17, s[:18])

def msg(x, y, s): return add("msg", x, y, s, len(s) * CH + 10)
def obj(x, y, s, w=None): return add("obj", x, y, s, w or len(s) * CH + 10)
def satom(x, y, w): return add("symbolatom", x, y, "%d 0 0 0 - - - 0" % w, w * CH + 10, 22)
def fatom(x, y, w): return add("floatatom", x, y, "%d 0 0 0 - - - 0" % w, w * CH + 10, 22)
def c(a, o, b, i): conns.append((a, o, b, i))

text(40, 20, "dx7~  player")
text(40, 46, "give it a folder of .syx banks then step through the voices and play with a MIDI keyboard.", 80)

scan = msg(40, 100, "lib scan sysex")
text(190, 102, "click once. Edit the folder name first or write a full path. It is read next to this patch.", 54)

prev = msg(40, 190, "lib prev"); nxt = msg(130, 190, "lib next")
rnd = msg(220, 190, "lib random"); fav = msg(330, 190, "lib fav"); pan = msg(420, 190, "panic")
r = obj(560, 250, "r dx7p")
dx = obj(40, 330, "dx7~ 16, f 40", 40 * CH)
dac = obj(40, 420, "dac~")
rt = obj(150, 420, "route lib")
up = obj(150, 460, "unpack f f")
pos = fatom(150, 510, 5); cnt = fatom(230, 510, 5)
text(150, 540, "position"); text(230, 540, "of total")
name = satom(290, 420, 12); file = satom(430, 420, 20)
text(290, 450, "voice"); text(430, 450, "bank file")
text(40, 600, "set your MIDI device in the Pd preferences. Notes and velocity and mod wheel and sustain pedal and pitch bend are wired inside [pd midi].", 80)
pm = obj(700, 100, "pd midi")

for m in (scan, prev, nxt, rnd, fav, pan): c(m, 0, dx, 0)
c(r, 0, dx, 0)
c(dx, 0, dac, 0); c(dx, 0, dac, 1)
c(dx, 1, rt, 0); c(rt, 0, up, 0); c(up, 0, pos, 0); c(up, 1, cnt, 0)
c(dx, 2, name, 0); c(dx, 3, file, 0)

def overlap(a, b, gap=6):
    return not (a[1] + a[3] + gap <= b[1] or b[1] + b[3] + gap <= a[1] or a[2] + a[4] + gap <= b[2] or b[2] + b[4] + gap <= a[2])
bad = [(boxes[i][0], boxes[j][0]) for i in range(len(boxes)) for j in range(i + 1, len(boxes)) if overlap(boxes[i], boxes[j])]
if bad: print("OVERLAPS", bad); sys.exit(1)

sub = ["#N canvas 0 50 760 560 midi 0;"]
so = [("obj", 30, 30, "notein"), ("obj", 30, 80, "pack f f"), ("obj", 30, 130, "s dx7p"),
      ("obj", 250, 30, "ctlin 1"), ("obj", 250, 80, "/ 127"), ("msg", 250, 130, "modwheel \\$1"), ("obj", 250, 180, "s dx7p"),
      ("obj", 30, 260, "ctlin 64"), ("obj", 30, 310, ">= 64"), ("msg", 30, 360, "sustain \\$1"), ("obj", 30, 410, "s dx7p"),
      ("obj", 250, 260, "bendin"), ("obj", 250, 310, "- 8192"), ("obj", 250, 360, "/ 8192"), ("msg", 250, 410, "bend \\$1"), ("obj", 250, 460, "s dx7p")]
sc = [(0, 0, 1, 0), (0, 1, 1, 1), (1, 0, 2, 0), (3, 0, 4, 0), (4, 0, 5, 0), (5, 0, 6, 0),
      (7, 0, 8, 0), (8, 0, 9, 0), (9, 0, 10, 0), (11, 0, 12, 0), (12, 0, 13, 0), (13, 0, 14, 0), (14, 0, 15, 0)]
for k, x, y, t in so: sub.append("#X %s %d %d %s;" % (k, x, y, t))
for a, o, b, i in sc: sub.append("#X connect %d %d %d %d;" % (a, o, b, i))

out = ["#N canvas 40 40 840 700 12;"]
for i, l in enumerate(objs):
    if i == pm: out += sub + ["#X restore 700 100 pd midi;"]
    else: out.append(l)
for a, o, b, i in conns: out.append("#X connect %d %d %d %d;" % (a, o, b, i))
open(sys.argv[1], "w").write("\n".join(out) + "\n")
print("ok", len(boxes), "boxes")
