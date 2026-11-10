#!/usr/bin/env python3
"""Generate a neutral skin-layout template (SVG + PNG) from skin/bl_layout.json.

Reference sheet is 992x513 (faceplate 991x501 + 12px strip holding the lit
annunciators). Every rect is placed exactly where Emu71 expects it, so a
Photoshop skin painted at N times this size (any integer N) drops straight in.
Layers in the SVG: faceplate-guide, keys-up, keys-down, lcd, annunciators.
"""
import json, sys, os
from PIL import Image, ImageDraw

here = os.path.dirname(os.path.abspath(__file__))
L = json.load(open(os.path.join(here, '..', 'skin', 'bl_layout.json')))
W, H = 992, 513
BG = L['bg']['Size']
ANN_NAMES = {4: 'i4', 5: 'AC', 6: 'USER', 7: 'RAD', 11: 'f', 12: 'g', 13: 'BAT',
             20: '0', 21: '1', 22: '2', 23: '3', 24: '4', 28: '((*))', 29: '->',
             30: 'PRGM', 31: 'SUSP', 32: 'CALC'}

def svg():
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    o.append(f'<rect width="{W}" height="{H}" fill="#2a2a2a"/>')
    o.append('<g id="faceplate-guide">')
    o.append(f'<rect x="0" y="0" width="{BG[0]}" height="{BG[1]}" fill="#3a3a3a" stroke="#888"/>')
    o.append('</g><g id="keys-up" fill="none" stroke="#e0a030" stroke-width="1">')
    for b in L['btn']:
        x, y = b['Offset']; w, h = b['Size']
        o.append(f'<rect id="key{b["id"]}" x="{x}" y="{y}" width="{w}" height="{h}" rx="3"/>')
    o.append('</g><g id="keys-down" fill="none" stroke="#50a0e0" stroke-width="1">')
    for b in L['btn']:
        x, y = b['Down']; w, h = b['Size']
        o.append(f'<rect id="key{b["id"]}-down" x="{x}" y="{y}" width="{w}" height="{h}" rx="3"/>')
    o.append('</g>')
    z = L['lcd']['Zoom']; x, y = L['lcd']['Offset']
    o.append(f'<rect id="lcd" x="{x}" y="{y}" width="{132*z}" height="{8*z}" fill="#7e7f79" stroke="#000"/>')
    o.append('<g id="annunciators-lit" fill="none" stroke="#e05050" stroke-width="1">')
    for a in L['ann']:
        x, y = a['Down']; w, h = a['Size']
        o.append(f'<rect id="ann{a["id"]}-lit" x="{x}" y="{y}" width="{w}" height="{h}"><title>{ANN_NAMES.get(a["id"], a["id"])}</title></rect>')
    o.append('</g><g id="annunciators-pos" fill="none" stroke="#e05050" stroke-dasharray="2 2">')
    for a in L['ann']:
        x, y = a['Offset']; w, h = a['Size']
        o.append(f'<rect id="ann{a["id"]}" x="{x}" y="{y}" width="{w}" height="{h}"/>')
    o.append('</g></svg>')
    return '\n'.join(o)

def png(scale):
    im = Image.new('RGB', (W * scale, H * scale), (42, 42, 42))
    d = ImageDraw.Draw(im)
    s = lambda v: v * scale
    d.rectangle([0, 0, s(BG[0]) - 1, s(BG[1]) - 1], fill=(58, 58, 58), outline=(136, 136, 136))
    for b in L['btn']:
        x, y = b['Offset']; w, h = b['Size']
        d.rounded_rectangle([s(x), s(y), s(x + w) - 1, s(y + h) - 1], radius=3 * scale, outline=(224, 160, 48))
        x, y = b['Down']
        d.rounded_rectangle([s(x), s(y), s(x + w) - 1, s(y + h) - 1], radius=3 * scale, outline=(80, 160, 224))
    z = L['lcd']['Zoom']; x, y = L['lcd']['Offset']
    d.rectangle([s(x), s(y), s(x + 132 * z) - 1, s(y + 8 * z) - 1], fill=(126, 127, 121), outline=(0, 0, 0))
    for a in L['ann']:
        w, h = a['Size']
        for key in ('Offset', 'Down'):
            x, y = a[key]
            d.rectangle([s(x), s(y), s(x + w) - 1, s(y + h) - 1], outline=(224, 80, 80))
    return im

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', 'skin', 'template')
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, 'REAL71_template.svg'), 'w').write(svg())
    png(1).save(os.path.join(out, 'REAL71_template_1x.png'))
    png(3).save(os.path.join(out, 'REAL71_template_3x.png'))
    print('wrote', out)
