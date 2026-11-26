#!/usr/bin/env python3
"""Builds dx7_demo_bank.syx: 16 original voices (no Yamaha data), rest INIT VOICE."""
import sys

def init_voice():
    v = bytearray(155)
    for n in range(1, 7):
        b = (6 - n) * 21
        v[b:b+4] = bytes([99]*4); v[b+4:b+8] = bytes([99,99,99,0])
        v[b+8] = 39; v[b+18] = 1; v[b+20] = 7
        v[b+16] = 99 if n == 1 else 0
    v[126:130] = bytes([99]*4); v[130:134] = bytes([50]*4)
    v[136] = 1; v[137] = 35; v[141] = 1; v[143] = 3; v[144] = 24
    v[145:155] = b"INIT VOICE"
    return v

def mk(name, algo, fb, ops, lfo=None, peg=None, transpose=24):
    """ops: dict n -> dict(r=[4], l=[4], lvl, c, f, det, kvs, rs, ams)"""
    v = init_voice()
    for n in range(1, 7):
        b = (6 - n) * 21
        o = ops.get(n)
        if o is None:
            v[b+16] = 0; continue
        v[b:b+4] = bytes(o.get('r', [99, 99, 99, 60]))
        v[b+4:b+8] = bytes(o.get('l', [99, 99, 99, 0]))
        v[b+13] = o.get('rs', 0)
        v[b+14] = o.get('ams', 0)
        v[b+15] = o.get('kvs', 2)
        v[b+16] = o['lvl']
        v[b+18] = o.get('c', 1)
        v[b+19] = o.get('f', 0)
        v[b+20] = o.get('det', 7)
    v[134] = algo - 1
    v[135] = fb
    if lfo:  # speed, delay, pmd, amd, wave, pms
        v[137], v[138], v[139], v[140], v[142], v[143] = lfo
    else:
        v[139] = v[140] = 0
    if peg:
        v[126:130] = bytes(peg[0]); v[130:134] = bytes(peg[1])
    v[144] = transpose
    v[145:155] = name.ljust(10)[:10].encode()
    return v

# envelope shorthands
PLUCK  = dict(r=[99, 45, 30, 60], l=[99, 70, 0, 0])
SUSTN  = dict(r=[90, 99, 99, 55], l=[99, 99, 99, 0])
BELLE  = dict(r=[99, 28, 25, 40], l=[99, 40, 0, 0])
def e(r, l): return dict(r=r, l=l)

P = []
# 1 E.PIANO: alg 5, three FM pairs (tine, body, detuned layer)
P.append(mk("SOFT EP", 5, 4, {
  1: dict(e([96, 28, 25, 67], [99, 76, 0, 0]), lvl=96, kvs=2, rs=2),
  2: dict(e([95, 50, 35, 78], [99, 70, 0, 0]), lvl=78, kvs=3, rs=2),
  3: dict(e([96, 25, 25, 67], [99, 72, 0, 0]), lvl=90, det=8, kvs=2, rs=2),
  4: dict(e([99, 80, 40, 80], [99, 50, 0, 0]), lvl=64, c=14, kvs=5),
  5: dict(e([96, 28, 25, 67], [99, 74, 0, 0]), lvl=88, det=6, kvs=2, rs=2),
  6: dict(e([95, 50, 35, 78], [99, 60, 0, 0]), lvl=70, kvs=3),
}, lfo=(30, 0, 0, 0, 4, 3)))
# 2 FM BASS
P.append(mk("FM BASS", 5, 3, {
  1: dict(e([99, 55, 40, 70], [99, 85, 70, 0]), lvl=99, rs=1, kvs=2),
  2: dict(e([99, 62, 40, 80], [99, 55, 20, 0]), lvl=82, c=1, kvs=4),
}, transpose=12))
# 3 BRASS
P.append(mk("BRASS", 5, 5, {
  1: dict(e([72, 60, 99, 50], [99, 96, 90, 0]), lvl=96, kvs=3),
  2: dict(e([68, 55, 99, 50], [99, 88, 78, 0]), lvl=84, kvs=4),
  3: dict(e([74, 60, 99, 50], [99, 96, 90, 0]), lvl=92, det=8, kvs=3),
  4: dict(e([70, 55, 99, 50], [99, 86, 76, 0]), lvl=82, det=6, kvs=4),
}, lfo=(34, 45, 10, 0, 4, 2)))
# 4 BELLS
P.append(mk("GLASS BEL", 5, 0, {
  1: dict(BELLE, lvl=94, kvs=2),
  2: dict(e([99, 30, 28, 45], [99, 50, 0, 0]), lvl=80, c=3, f=17, kvs=4),
  3: dict(e([99, 24, 22, 40], [99, 45, 0, 0]), lvl=80, c=2, kvs=2),
  4: dict(e([99, 34, 30, 50], [99, 42, 0, 0]), lvl=70, c=5, f=4, kvs=4),
  5: dict(e([99, 20, 20, 38], [99, 35, 0, 0]), lvl=62, c=3, kvs=2),
  6: dict(e([99, 40, 32, 50], [99, 38, 0, 0]), lvl=58, c=7, f=1, kvs=4),
}))
# 5 MARIMBA
P.append(mk("MARIMBA", 5, 0, {
  1: dict(e([99, 52, 40, 70], [99, 20, 0, 0]), lvl=99, kvs=3, rs=2),
  2: dict(e([99, 70, 50, 80], [99, 10, 0, 0]), lvl=74, c=4, kvs=5),
  3: dict(e([99, 85, 60, 80], [99, 10, 0, 0]), lvl=70, c=10, det=7, kvs=4),
}))
# 6 ORGAN: alg 32 additive drawbars
P.append(mk("DRAWBAR", 32, 2, {
  1: dict(SUSTN, lvl=88, c=0), 2: dict(SUSTN, lvl=84, c=1),
  3: dict(SUSTN, lvl=80, c=2), 4: dict(SUSTN, lvl=72, c=3),
  5: dict(SUSTN, lvl=60, c=4), 6: dict(SUSTN, lvl=55, c=6),
}, lfo=(36, 0, 0, 0, 4, 0)))
# 7 STRINGS
P.append(mk("STRINGS", 5, 2, {
  1: dict(e([52, 40, 99, 42], [99, 95, 95, 0]), lvl=92, det=5),
  2: dict(e([58, 40, 99, 42], [99, 70, 60, 0]), lvl=60),
  3: dict(e([50, 40, 99, 42], [99, 95, 95, 0]), lvl=92, det=9),
  4: dict(e([56, 40, 99, 42], [99, 70, 60, 0]), lvl=58, det=8),
  5: dict(e([54, 40, 99, 42], [99, 95, 95, 0]), lvl=90, det=7),
  6: dict(e([60, 40, 99, 42], [99, 68, 58, 0]), lvl=56, det=6),
}, lfo=(28, 40, 8, 0, 0, 2)))
# 8 CLAV
P.append(mk("CLAVINET", 5, 3, {
  1: dict(e([99, 68, 55, 85], [99, 74, 40, 0]), lvl=96, kvs=3),
  2: dict(e([99, 75, 50, 85], [99, 60, 20, 0]), lvl=88, c=1, kvs=5),
  3: dict(e([99, 68, 55, 85], [99, 72, 38, 0]), lvl=88, c=2, det=8, kvs=3),
  4: dict(e([99, 80, 50, 85], [99, 50, 10, 0]), lvl=76, c=3, kvs=5),
}))
# 9 VIBES with tremolo (amp mod)
P.append(mk("VIBES", 5, 0, {
  1: dict(e([99, 30, 28, 55], [99, 88, 0, 0]), lvl=96, ams=2, kvs=3),
  2: dict(e([99, 55, 40, 70], [99, 28, 0, 0]), lvl=64, c=4, f=2, kvs=5),
  3: dict(e([99, 30, 28, 55], [99, 86, 0, 0]), lvl=70, ams=2, c=1, det=8),
}, lfo=(31, 0, 0, 40, 4, 0)))
# 10 SYN LEAD
P.append(mk("SYN LEAD", 5, 6, {
  1: dict(e([85, 70, 99, 60], [99, 96, 92, 0]), lvl=92, det=7, kvs=2),
  2: dict(e([85, 70, 99, 60], [99, 84, 76, 0]), lvl=70, kvs=3),
  3: dict(e([85, 70, 99, 60], [99, 96, 92, 0]), lvl=88, det=9, kvs=2),
  5: dict(e([85, 70, 99, 60], [99, 96, 92, 0]), lvl=86, det=5, kvs=2),
  6: dict(e([85, 70, 99, 60], [99, 90, 84, 0]), lvl=80, kvs=3),
}, lfo=(37, 60, 14, 0, 4, 3)))
# 11 FLUTE
P.append(mk("AIR FLUTE", 5, 4, {
  1: dict(e([68, 60, 99, 55], [99, 95, 92, 0]), lvl=94, kvs=2),
  2: dict(e([72, 70, 99, 60], [99, 40, 36, 0]), lvl=44, kvs=2),
  5: dict(e([70, 60, 99, 55], [99, 70, 66, 0]), lvl=46, c=2, kvs=2),
}, lfo=(38, 55, 12, 0, 4, 2)))
# 12 HARPSICHORD
P.append(mk("HARPSI", 5, 3, {
  1: dict(e([99, 60, 45, 90], [99, 55, 20, 0]), lvl=94, kvs=2),
  2: dict(e([99, 75, 55, 90], [99, 40, 10, 0]), lvl=70, c=3, kvs=4),
  3: dict(e([99, 62, 45, 90], [99, 52, 18, 0]), lvl=90, c=2, det=8, kvs=2),
  4: dict(e([99, 80, 55, 90], [99, 36, 8, 0]), lvl=68, c=5, kvs=4),
}))
# 13 CHIMES: alg 32 inharmonic partials
P.append(mk("CHIMES", 32, 0, {
  1: dict(e([99, 26, 22, 40], [99, 0, 0, 0]), lvl=86, c=1),
  2: dict(e([99, 30, 22, 40], [99, 0, 0, 0]), lvl=78, c=2, f=38),
  3: dict(e([99, 34, 22, 40], [99, 0, 0, 0]), lvl=72, c=5, f=40),
  4: dict(e([99, 40, 22, 40], [99, 0, 0, 0]), lvl=66, c=8, f=93),
  5: dict(e([99, 46, 22, 40], [99, 0, 0, 0]), lvl=60, c=13, f=34),
}))
# 14 SINE SUB
P.append(mk("SINE SUB", 5, 0, {
  1: dict(e([95, 60, 99, 70], [99, 96, 90, 0]), lvl=99, kvs=1),
  2: dict(e([99, 70, 99, 70], [99, 20, 10, 0]), lvl=40, kvs=3),
}, transpose=12))
# 15 SLOW PAD
P.append(mk("SLOW PAD", 5, 3, {
  1: dict(e([34, 30, 99, 28], [99, 96, 96, 0]), lvl=90, det=5),
  2: dict(e([38, 30, 99, 30], [99, 64, 62, 0]), lvl=56, c=1),
  3: dict(e([32, 30, 99, 28], [99, 96, 96, 0]), lvl=90, det=9),
  4: dict(e([40, 30, 99, 30], [99, 60, 58, 0]), lvl=52, c=2, det=8),
  5: dict(e([36, 30, 99, 28], [99, 96, 96, 0]), lvl=88, c=2),
  6: dict(e([40, 30, 99, 30], [99, 56, 54, 0]), lvl=48),
}, lfo=(22, 20, 6, 0, 0, 3)))
# 16 KICK with pitch EG sweep (start level = PL4)
P.append(mk("FM KICK", 5, 2, {
  1: dict(e([99, 60, 60, 99], [99, 0, 0, 0]), lvl=99, kvs=0),
  2: dict(e([99, 75, 60, 99], [99, 0, 0, 0]), lvl=70, kvs=0),
}, peg=([88, 99, 99, 99], [50, 50, 50, 72]), transpose=12))

def pack(v):
    b = bytearray(128)
    for n in range(6):
        s = v[n*21:(n+1)*21]; d = n * 17
        b[d:d+11] = s[0:11]
        b[d+11] = (s[11] & 3) | ((s[12] & 3) << 2)
        b[d+12] = (s[13] & 7) | ((s[20] & 15) << 3)
        b[d+13] = (s[14] & 3) | ((s[15] & 7) << 2)
        b[d+14] = s[16]
        b[d+15] = (s[17] & 1) | ((s[18] & 31) << 1)
        b[d+16] = s[19]
    b[102:111] = v[126:135]
    b[111] = (v[135] & 7) | ((v[136] & 1) << 3)
    b[112:116] = v[137:141]
    b[116] = (v[141] & 1) | ((v[142] & 7) << 1) | ((v[143] & 7) << 4)
    b[117] = v[144]
    b[118:128] = v[145:155]
    return b

voices = P + [init_voice() for _ in range(32 - len(P))]
data = b''.join(bytes(pack(v)) for v in voices)
chk = (-sum(data)) & 0x7f
out = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + data + bytes([chk, 0xF7])
open(sys.argv[1], 'wb').write(out)
print(len(out), "bytes,", len(P), "voices")
