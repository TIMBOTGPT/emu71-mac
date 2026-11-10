#!/usr/bin/env python3
"""Wrap an asm71 LEX file (LIF, 32-byte directory entry + body) as a soft-configured
16 KB ROM module image for Emu71: 8 zero nibbles, 32-nibble file header, 5-nibble chain
length, LEX body, zero byte ending the chain, zero padding.
usage: mklexrom.py in.lex out.rom [header_template.bin]"""
import sys

def nibbles(bs):
    out = []
    for b in bs:
        out += [b & 15, b >> 4]
    return out

src = open(sys.argv[1], 'rb').read()
name = src[:10].decode('ascii').rstrip()[:8].ljust(8)
body = src[32:].rstrip(b'\x20')          # LIF pads the last sector with spaces
bn = nibbles(body)
hdr = nibbles(name.encode('ascii'))      # 16 nibbles
hdr += [8, 0, 2, 14]                     # file type E208 (LEX), low nibble first
hdr += [0] * 12                          # flags, copy code, time, date: unused here
chain = 5 + len(bn)
cl = [(chain >> (4 * i)) & 15 for i in range(5)]
img = [0] * 8 + hdr + cl + bn + [0, 0]
img += [0] * (32768 - len(img))
assert len(img) == 32768
out = bytearray(16384)
for i in range(16384):
    out[i] = img[2 * i] | (img[2 * i + 1] << 4)
open(sys.argv[2], 'wb').write(out)
print('LEX body %d nibbles, ROM image %s' % (len(bn), sys.argv[2]))
