#!/usr/bin/env python3
"""Draws a rough preview of an abstraction panel (rectangles and text) so the layout can be checked without Pd.
   python3 tools/preview.py abs/grain~.pd out.png"""
import re, sys
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
txt = open(src).read().replace("\n", " ").split(";")
W = H = 0
for s in txt:
    m = re.match(r"\s*#X coords \S+ \S+ \S+ \S+ (\d+) (\d+)", s)
    if m: W, H = int(m.group(1)), int(m.group(2))
im = Image.new("RGB", (W, H), "#f6f6f6"); d = ImageDraw.Draw(im)
boxes = []
for s in txt:
    t = s.split()
    if len(t) < 5 or t[0] != "#X": continue
    if t[1] == "obj" and t[4] == "cnv":
        if int(t[5]) == 1 and int(t[7]) > 0: d.rectangle([int(t[2]), int(t[3]), int(t[2]) + int(t[6]), int(t[3]) + int(t[7])], fill=t[-4] if t[-4].startswith("#") else "#ececec")
        d.text((int(t[2]) + 20, int(t[3]) + 14), t[9] if t[9] != "empty" else "", fill="#202020")
        continue
    if t[1] == "text":
        x, y = int(t[2]), int(t[3]); words = " ".join(t[4:]); d.text((x, y), words, fill="#000"); boxes.append(("text", x, y, 7 * len(words), 16)); continue
    if t[1] == "symbolatom":
        x, y, w = int(t[2]), int(t[3]), int(t[4]); d.rectangle([x, y, x + 7 * w + 6, y + 18], outline="#000"); boxes.append(("sym", x, y, 7 * w + 6, 18)); continue
    if t[1] == "obj" and t[4] in ("hsl", "nbx", "hradio", "bng"):
        x, y = int(t[2]), int(t[3]); k = t[4]
        if k == "hsl": w, h = int(t[5]), int(t[6]); lab = t[13]; yo = int(t[15]); d.rectangle([x, y, x + w, y + h], outline="#000", fill="#fff"); d.text((x, y + yo - 2), lab, fill="#000"); boxes.append((lab, x, y + yo - 2, w, h - yo + 2))
        elif k == "nbx": w, h = int(t[5]) * 7 + 6, int(t[6]); d.rectangle([x, y, x + w, y + h], outline="#000", fill="#fff"); boxes.append(("nbx", x, y, w, h))
        elif k == "hradio":
            c, n = int(t[5]), int(t[8]); lab = t[13]
            for i in range(n): d.rectangle([x + i * c, y, x + (i + 1) * c, y + c], outline="#000", fill="#fff")
            d.text((x, y - 10), lab, fill="#000"); boxes.append((lab, x, y - 10, c * n, c + 10))
        else: d.ellipse([x, y, x + 18, y + 18], outline="#000", fill="#fff"); d.text((x + 24, y + 3), t[12], fill="#000"); boxes.append((t[12], x, y, 24 + 7 * len(t[12]), 18))
bad = 0
for i in range(len(boxes)):
    for j in range(i + 1, len(boxes)):
        a, b = boxes[i], boxes[j]
        if a[1] < b[1] + b[3] and b[1] < a[1] + a[3] and a[2] < b[2] + b[4] and b[2] < a[2] + a[4]: print("overlap", a, b); bad += 1
for b in boxes:
    if b[1] + b[3] > W or b[2] + b[4] > H: print("outside panel", b); bad += 1
print(src, "panel", W, "x", H, "overlaps/outside:", bad)
im.save(out)
