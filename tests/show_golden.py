#!/usr/bin/env python3
"""Render tests/golden/*.lcd files to one PNG so they can be checked by eye.
usage: tests/show_golden.py OUT.png NAME.R [NAME.R ...]   (NAME.R without extension, e.g. err_pen_range.1)"""
import os, sys
from PIL import Image
G = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'golden')
ims = []
for n in sys.argv[2:]:
    rows = open(os.path.join(G, n + '.lcd')).read().split()
    w = max(len(r) for r in rows)
    im = Image.new('L', (w * 3, len(rows) * 3 + 14), 255)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#':
                for dy in range(3):
                    for dx in range(3): im.putpixel((x * 3 + dx, y * 3 + dy + 12), 0)
    ims.append((n, im))
H = sum(i.height for _, i in ims)
out = Image.new('L', (max(i.width for _, i in ims), H), 255)
y = 0
for n, i in ims:
    out.paste(i, (0, y)); y += i.height
out.save(sys.argv[1])
print(sys.argv[2:])
