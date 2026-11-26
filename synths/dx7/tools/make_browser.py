#!/usr/bin/env python3
"""Generates dx7~-browser.pd. The visible canvas holds only the interface (no wires);
the plumbing lives in three subpatches. Checks the visible layout for overlaps."""
import sys

CH, LH = 7, 17          # char width, line height at font size 12
main, boxes = [], []    # lines, (name, x, y, w, h)

def box(name, x, y, w, h):
    boxes.append((name, x, y, w, h))

def text(x, y, s, wrap=None, name=None):
    ncols = wrap or len(s)
    lines = max(1, -(-len(s) // max(1, ncols - 6))) if wrap else 1
    w = min(len(s), ncols) * CH
    main.append("#X text %d %d %s%s;" % (x, y, s, (", f %d" % wrap) if wrap else ""))
    box(name or s[:20], x, y, w, lines * LH)

def msg(x, y, s, raw=None):
    main.append("#X msg %d %d %s;" % (x, y, raw or s))
    box("msg " + s[:16], x, y, len(s) * CH + 10, 20)

def fatom(x, y, w, recv="-", send="-"):
    main.append("#X floatatom %d %d %d 0 0 0 - %s %s 0;" % (x, y, w, recv, send))
    box("fatom " + recv + send, x, y, w * CH + 10, 22)

def satom(x, y, w, recv="-", send="-"):
    main.append("#X symbolatom %d %d %d 0 0 0 - %s %s 0;" % (x, y, w, recv, send))
    box("satom " + recv + send, x, y, w * CH + 10, 22)

def bng(x, y, label, send, size=28):
    main.append("#X obj %d %d bng %d 250 50 0 %s empty empty 0 0 0 12 -262144 -1 -1;" % (x, y, size, send))
    box("bng " + label, x, y, size, size)
    text(x + size + 10, y + 6, label)

def tgl(x, y, label, send, init=0, size=28):
    main.append("#X obj %d %d tgl %d %d %s empty empty 0 0 0 12 -262144 -1 -1 %d 1;" % (x, y, size, init, send, init))
    box("tgl " + label, x, y, size, size)
    text(x + size + 10, y + 6, label)

def hsl(x, y, w, send):
    main.append("#X obj %d %d hsl %d 20 0 1 0 0 %s empty empty -2 -8 0 12 -262144 -1 -1 0 1;" % (x, y, w, send))
    box("hsl", x, y, w + 6, 20)

def header(x, y, s):
    text(x, y, s)

# ------------------------------------------------------------------ subpatches
def subpatch(title, objs, conns, x, y, w=900, h=700):
    """objs: list of (kind,x,y,text); conns: (a,out,b,in). Returns text lines incl. restore."""
    out = ["#N canvas 0 50 %d %d %s 0;" % (w, h, title)]
    for k, ox, oy, t in objs:
        out.append("#X %s %d %d %s;" % (k, ox, oy, t))
    for a, o, b, i in conns:
        out.append("#X connect %d %d %d %d;" % (a, o, b, i))
    return out

# ---- buttons: [r name] -> message ; browse buttons also trigger the audition
browse = {  # name: message text (after "; dx7h ")
    "d7-prev": "lib prev", "d7-next": "lib next", "d7-random": "lib random",
    "d7-bankprev": "lib bank prev", "d7-banknext": "lib bank next", "d7-bankall": "lib bank all",
}
plain = {
    "d7-fav": "lib fav", "d7-store": "lib store", "d7-savefavs": "savefavs myfavs.syx",
    "d7-favload": "favload myfavs.syx", "d7-savevoice": "voice.syx", "d7-savebank": "savebank bank.syx",
    "d7-A": "lib A", "d7-B": "lib B", "d7-mut1": "mutate 0.15", "d7-mut2": "mutate 0.4",
    "d7-mut3": "mutate 0.9", "d7-reset": "lib reset", "d7-init": "init",
}
plain["d7-savevoice"] = "savevoice voice.syx"
floaty = {  # float-input controls
    "d7-goto": ("lib goto \\$1", True), "d7-algof": ("lib algo \\$1", False),
    "d7-favs": ("lib favs \\$1", False), "d7-morph": ("morph \\$1", False),
}
symy = {"d7-search": "lib search \\$1", "d7-newname": "name \\$1"}

bo, bc = [], []
col, row = 0, 0
def cell():
    global col, row
    x, y = 30 + col * 280, 30 + row * 110
    col += 1
    if col == 3: col, row = 0, row + 1
    return x, y
def add(kind, x, y, t):
    bo.append((kind, x, y, t)); return len(bo) - 1
for n, m in browse.items():
    x, y = cell()
    r = add("obj", x, y, "r " + n); t = add("obj", x, y + 30, "t b b")
    ms = add("msg", x, y + 60, "\\; dx7h " + m); au = add("obj", x + 150, y + 60, "s d7-audition")
    bc += [(r, 0, t, 0), (t, 1, ms, 0), (t, 0, au, 0)]
for n, m in plain.items():
    x, y = cell()
    r = add("obj", x, y, "r " + n); ms = add("msg", x, y + 40, "\\; dx7h " + m)
    bc += [(r, 0, ms, 0)]
for n, (m, aud) in floaty.items():
    x, y = cell()
    r = add("obj", x, y, "r " + n); ms = add("msg", x, y + 40, "\\; dx7h " + m)
    bc += [(r, 0, ms, 0)]
    if aud:
        au = add("obj", x + 150, y + 40, "s d7-audition"); bc += [(r, 0, au, 0)]
for n, m in symy.items():
    x, y = cell()
    r = add("obj", x, y, "r " + n); ms = add("msg", x, y + 40, "\\; dx7h " + m)
    bc += [(r, 0, ms, 0)]
for n, note in (("d7-c3", 48), ("d7-c4", 60), ("d7-c5", 72)):
    x, y = cell()
    r = add("obj", x, y, "r " + n); f = add("msg", x, y + 40, str(note)); s = add("obj", x, y + 80, "s d7-play")
    bc += [(r, 0, f, 0), (f, 0, s, 0)]
buttons = subpatch("buttons", bo, bc, 0, 0, 940, 720)

# ---- engine: synth, MIDI in, audition
eo = [
    ("obj", 30, 30, "notein"),                 #0
    ("obj", 30, 80, "pack f f"),               #1
    ("obj", 250, 80, "r dx7h"),                #2
    ("obj", 30, 150, "dx7~ 16"),               #3
    ("obj", 30, 230, "outlet~"),               #4
    ("obj", 140, 230, "s d7-info"),            #5
    ("obj", 270, 230, "s d7-name"),            #6
    ("obj", 400, 230, "s d7-file"),            #7
    ("obj", 30, 330, "r d7-audition"),         #8
    ("obj", 30, 380, "spigot"),                #9
    ("obj", 160, 330, "r d7-auto"),            #10
    ("obj", 30, 430, "del 40"),                #11
    ("msg", 30, 480, "60"),                    #12
    ("obj", 30, 530, "s d7-play"),             #13
    ("obj", 400, 330, "r d7-play"),            #14
    ("obj", 400, 380, "t f f"),                #15
    ("msg", 480, 440, "\\$1 100"),             #16
    ("obj", 400, 440, "pipe 800"),             #17
    ("msg", 400, 500, "\\$1 0"),               #18
]
ec = [(0, 0, 1, 0), (0, 1, 1, 1), (1, 0, 3, 0), (2, 0, 3, 0),
      (3, 0, 4, 0), (3, 1, 5, 0), (3, 2, 6, 0), (3, 3, 7, 0),
      (8, 0, 9, 0), (10, 0, 9, 1), (9, 0, 11, 0), (11, 0, 12, 0), (12, 0, 13, 0),
      (14, 0, 15, 0), (15, 1, 16, 0), (15, 0, 17, 0), (16, 0, 3, 0), (17, 0, 18, 0), (18, 0, 3, 0)]
engine = subpatch("engine", eo, ec, 0, 0, 760, 620)

# ---- readout: info messages from dx7~ to the number boxes
ro = [("obj", 30, 30, "r d7-info"),
      ("obj", 30, 80, "route lib libbank slot fav algo fb ops scan"),            #1
      ("obj", 30, 150, "unpack f f"), ("obj", 30, 200, "s d7-pos"), ("obj", 130, 200, "s d7-count"),  #2,3,4
      ("obj", 260, 150, "unpack f f"), ("obj", 260, 200, "s d7-bank"),                      #5,6
      ("obj", 380, 150, "s d7-slot"), ("obj", 500, 150, "s d7-fav"),                        #7,8
      ("obj", 620, 150, "s d7-algo-show"), ("obj", 760, 150, "s d7-fb"),                    #9,10
      ("obj", 30, 280, "unpack f f f f f f"),                                              #11
      ("obj", 30, 340, "s d7-op1"), ("obj", 130, 340, "s d7-op2"), ("obj", 230, 340, "s d7-op3"),
      ("obj", 330, 340, "s d7-op4"), ("obj", 430, 340, "s d7-op5"), ("obj", 530, 340, "s d7-op6"),  #12-17
      ("obj", 620, 280, "unpack f f f f"),                                                 #18
      ("obj", 620, 340, "s d7-nv"), ("obj", 720, 340, "s d7-nb"), ("obj", 820, 340, "s d7-nd")]    #19-21
rc = [(0, 0, 1, 0), (1, 0, 2, 0), (2, 0, 3, 0), (2, 1, 4, 0), (1, 1, 5, 0), (5, 0, 6, 0),
      (1, 2, 7, 0), (1, 3, 8, 0), (1, 4, 9, 0), (1, 5, 10, 0), (1, 6, 11, 0)]
for i in range(6): rc.append((11, i, 12 + i, 0))
rc += [(1, 7, 18, 0), (18, 0, 19, 0), (18, 1, 20, 0), (18, 2, 21, 0)]
readout = subpatch("readout", ro, rc, 0, 0, 960, 420)

# ------------------------------------------------------------------ visible interface
W, H = 1180, 1130
text(40, 20, "dx7~  patch browser")
text(40, 46, "scan a folder of .syx banks and step through the voices. Mark favourites and save them. Morph between two sounds.", 100)

# 1 library
header(40, 110, "1  LIBRARY")
msg(40, 140, "lib scan sysex", "\\; dx7h lib scan sysex")
text(40, 178, "click the box to scan. Edit the folder name first: it is read next to this patch or write a full path. Sub-folders are included and duplicates are skipped.", 58)
fatom(40, 262, 6, recv="d7-nv"); fatom(140, 262, 6, recv="d7-nb"); fatom(240, 262, 6, recv="d7-nd")
text(40, 290, "voices"); text(140, 290, "banks"); text(240, 290, "duplicates skipped")

# 2 now playing
header(520, 110, "2  NOW PLAYING")
satom(520, 140, 12, recv="d7-name"); satom(660, 140, 20, recv="d7-file")
fatom(850, 140, 5, recv="d7-pos"); fatom(920, 140, 5, recv="d7-count")
text(520, 170, "voice"); text(660, 170, "bank file"); text(850, 170, "position"); text(920, 170, "of total")
fatom(520, 218, 4, recv="d7-algo-show"); fatom(610, 218, 4, recv="d7-fb"); fatom(700, 218, 4, recv="d7-fav")
fatom(790, 218, 4, recv="d7-bank"); fatom(880, 218, 4, recv="d7-slot")
text(520, 248, "algorithm"); text(610, 248, "feedback"); text(700, 248, "favourite"); text(790, 248, "bank no."); text(880, 248, "slot")
for i in range(6): fatom(520 + i * 70, 296, 3, recv="d7-op%d" % (i + 1))
text(520, 326, "output level of operators 1 to 6")

# 3 browse
header(40, 380, "3  BROWSE")
bng(40, 412, "previous", "d7-prev"); bng(210, 412, "next", "d7-next"); bng(340, 412, "random", "d7-random")
bng(40, 462, "bank down", "d7-bankprev"); bng(210, 462, "bank up", "d7-banknext"); bng(340, 462, "all banks", "d7-bankall")
fatom(40, 516, 5, send="d7-goto"); text(120, 518, "go to number")
tgl(40, 566, "play a note when browsing", "d7-auto", init=1)

# 4 filter
header(520, 380, "4  FILTER")
satom(520, 412, 16, send="d7-search"); text(660, 414, "search a name or a bank (type then press enter)", 40)
fatom(520, 482, 5, send="d7-algof"); text(600, 484, "algorithm 1 to 32 and 0 for any", 40)
tgl(520, 532, "favourites only", "d7-favs"); bng(760, 532, "reset filters", "d7-reset")

# 5 favourites and saving
header(40, 640, "5  FAVOURITES AND SAVING")
bng(40, 672, "favourite on/off", "d7-fav")
satom(300, 674, 10, send="d7-newname"); text(400, 676, "rename voice", 20)
bng(40, 722, "store as new voice", "d7-store")
bng(40, 772, "save favourites", "d7-savefavs"); bng(300, 772, "load favourites", "d7-favload")
bng(40, 822, "save this voice", "d7-savevoice"); bng(300, 822, "save the bank", "d7-savebank")
text(40, 872, "files go next to this patch: myfavs.syx and voice.syx and bank.syx. More than 32 favourites make myfavs_1.syx and myfavs_2.syx and so on. Change the names inside [pd buttons].", 62)

# 6 morph and mutate
header(520, 640, "6  MORPH AND MUTATE")
bng(520, 672, "set A", "d7-A"); bng(680, 672, "set B", "d7-B")
hsl(520, 726, 260, "d7-morph"); text(520, 756, "blend between A and B (applies to the next notes)", 44)
bng(520, 806, "mutate a little", "d7-mut1"); bng(720, 806, "mutate more", "d7-mut2"); bng(900, 806, "wild", "d7-mut3")

# 7 sound
header(520, 900, "7  SOUND")
bng(520, 928, "C3", "d7-c3"); bng(620, 928, "C4", "d7-c4"); bng(720, 928, "C5", "d7-c5")
main.append("#X obj 40 970 pd engine;"); box("pd engine", 40, 970, 90, 20)
main.append("#X obj 40 1020 dac~;"); box("dac~", 40, 1020, 50, 20)
main.append("#X obj 140 970 pd buttons;"); box("pd buttons", 140, 970, 100, 20)
main.append("#X obj 260 970 pd readout;"); box("pd readout", 260, 970, 100, 20)
text(140, 1020, "turn DSP on then play from a MIDI keyboard or the C buttons. The three boxes at the left are the plumbing.", 44)

# ------------------------------------------------------------------ overlap check
def overlap(a, b, gap=6):
    return not (a[1] + a[3] + gap <= b[1] or b[1] + b[3] + gap <= a[1] or
                a[2] + a[4] + gap <= b[2] or b[2] + b[4] + gap <= a[2])
bad = []
for i in range(len(boxes)):
    for j in range(i + 1, len(boxes)):
        if overlap(boxes[i], boxes[j]): bad.append((boxes[i][0], boxes[j][0]))
maxx = max(b[1] + b[3] for b in boxes); maxy = max(b[2] + b[4] for b in boxes)
if bad:
    print("OVERLAPS:", bad); sys.exit(1)
import json; json.dump(boxes, open("/tmp/boxes.json", "w"))
print("layout ok: %d boxes, extent %d x %d" % (len(boxes), maxx, maxy))

out = ["#N canvas 40 40 %d %d 12;" % (W, H)]
# subpatches first, defined in order; their restore lines place them on the main canvas
out += main[:0]
def with_restore(sub, name, x, y):
    return sub + ["#X restore %d %d pd %s;" % (x, y, name)]
# the visible objects were added in "main"; subpatch boxes must keep their object order
# so rebuild: iterate main and substitute the placeholder lines
res = []
subs = {"engine": engine, "buttons": buttons, "readout": readout}
for line in main:
    if line.startswith("#X obj ") and " pd " in line and line.rstrip(";").split()[-2] == "pd":
        nm = line.rstrip(";").split()[-1]
        parts = line.split()
        res += with_restore(subs[nm], nm, int(parts[2]), int(parts[3]))
    else:
        res.append(line)
out += res
# connect: engine (object index) -> dac~ left and right
idx = 0
eng = dac = None
for line in res:
    if line.startswith("#N canvas"): continue
for line in res:
    pass
# count top-level objects in order to find indices
count = 0; depth = 0; eng = dac = None
for line in res:
    if line.startswith("#N canvas"): depth += 1; continue
    if line.startswith("#X restore"):
        depth -= 1
        if depth == 0:
            if line.endswith("pd engine;"): eng = count
            count += 1
        continue
    if depth > 0: continue
    if line.startswith("#X connect"): continue
    if line.startswith("#X obj ") and line.rstrip(";").endswith("dac~"): dac = count
    count += 1
out.append("#X connect %d 0 %d 0;" % (eng, dac))
out.append("#X connect %d 0 %d 1;" % (eng, dac))
open(sys.argv[1], "w").write("\n".join(out) + "\n")
print("written", sys.argv[1], "engine", eng, "dac", dac)
