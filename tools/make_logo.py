#!/usr/bin/env python3
"""Vector 'SATURN CALCULATOR' logo (outlined text, no font dependency at render time).

Design follows the supplied reference: rounded square with a round cut-out,
SATURN in extra-bold geometric sans, CALCULATOR in light weight tracked out to
the same width. Fonts: Montserrat (SIL OFL) from ~/Library/Fonts.
logo_group(x, y, h, fg, hole) -> svg <g> placed with top-left (x,y), height h.
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FD = os.path.expanduser('~/Library/Fonts')
REF_H = 135.0          # reference logo height (mark height) in design units
MARK_W = 183.0
TEXT_X = 217.0         # text starts this far right of mark's left edge (255-38)

def outline(font, text, cap_h, tracking=0.0, sx=1.0):
    gs = font.getGlyphSet(); cmap = font.getBestCmap()
    upm = font['head'].unitsPerEm
    cap = font['OS/2'].sCapHeight or 700
    k = cap_h / cap
    x = 0.0; parts = []
    for i, ch in enumerate(text):
        g = gs[cmap[ord(ch)]]
        pen = SVGPathPen(gs)
        g.draw(TransformPen(pen, (k * sx, 0, 0, -k, x, 0)))
        parts.append(pen.getCommands())
        x += g.width * k * sx + (tracking if i < len(text) - 1 else 0)
    return ' '.join(parts), x

def natural_width(font, text, cap_h, sx=1.0):
    return outline(font, text, cap_h, 0.0, sx)[1]

def logo_group(x, y, h, fg='#e8e8e8', hole='#1b1b1b'):
    heavy = TTFont(os.path.join(FD, 'Montserrat-ExtraBold.ttf'))
    light = TTFont(os.path.join(FD, 'Montserrat-Light.ttf'))
    s = h / REF_H
    cap1, cap2 = 60.0, 43.0
    TARGET = 435.0                      # reference text width at this scale
    sx = TARGET / natural_width(heavy, 'SATURN', cap1)     # extended (wide) setting
    d1, w1 = outline(heavy, 'SATURN', cap1, 0.0, sx)
    n = len('CALCULATOR')
    sx2 = min(sx, TARGET * 0.93 / natural_width(light, 'CALCULATOR', cap2))
    w2 = natural_width(light, 'CALCULATOR', cap2, sx2)
    trk = (w1 - w2) / (n - 1)
    d2, _ = outline(light, 'CALCULATOR', cap2, trk, sx2)
    tx = TEXT_X
    g = [f'<g id="saturn-logo" transform="translate({x},{y}) scale({s:.5f})">']
    g.append(f'<rect width="{MARK_W}" height="{REF_H}" rx="20" fill="{fg}"/>')
    g.append(f'<circle cx="{MARK_W/2 - 1}" cy="{REF_H/2 + 1}" r="59" fill="{hole}"/>')
    g.append(f'<path d="{d1}" transform="translate({tx},{cap1})" fill="{fg}"/>')
    g.append(f'<path d="{d2}" transform="translate({tx},{REF_H})" fill="{fg}"/>')
    g.append('</g>')
    return '\n'.join(g), (tx + w1) * s

if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    for name, fg, hole, bg in (('dark-on-light', '#3a3a3a', '#ffffff', '#ffffff'), ('light-on-dark', '#e8e8e8', '#1b1b1b', '#1b1b1b')):
        grp, w = logo_group(20, 20, 135, fg, hole)
        open(os.path.join(here, '..', 'skin', f'saturn_logo_{name}.svg'), 'w').write(
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(w)+40}" height="175" viewBox="0 0 {int(w)+40} 175"><rect width="100%" height="100%" fill="{bg}"/>{grp}</svg>')
    print('ok')
