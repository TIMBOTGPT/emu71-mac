#!/usr/bin/env python3
"""Turn EMU71_DUMP ASCII LCD dumps (stdin) into PNGs: lcd_N.png (4x per pixel)."""
import sys
from PIL import Image
blocks=[];cur=[]
for ln in sys.stdin:
    ln=ln.rstrip('\n')
    if ln and set(ln)<=set('#.'): cur.append(ln)
    elif ln.startswith('pc='):
        if cur: blocks.append(cur); cur=[]
for i,b in enumerate(blocks):
    w=max(len(r) for r in b); im=Image.new('L',(w*4,len(b)*4),255)
    for y,r in enumerate(b):
        for x,c in enumerate(r):
            if c=='#':
                for dy in range(4):
                    for dx in range(4): im.putpixel((x*4+dx,y*4+dy),0)
    im.save(f'/tmp/lcd_{i}.png')
print(len(blocks))
