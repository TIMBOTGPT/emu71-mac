#!/usr/bin/env python3
"""Usage: make_skin.py [dark|light]. Original scalable skin for Emu71 (dark body, gold legends, glossy keys).

Writes skin/REAL71' + SUF + '.svg, skin/REAL71' + SUF + '.KML (copy of REAL71BL.KML with new key
"Down" positions) and prints the sheet size. Render the PNG with render_skin.sh.
Sheet: 992 wide; faceplate 991x501 at top; lit-annunciator strip at y=502;
pressed-key tiles packed below from y=514.
"""
import json, os, re, html
import sys
THEME = sys.argv[1] if len(sys.argv) > 1 else 'dark'   # dark | light
SUF = {'dark': 'N', 'light': 'L', 'pro': 'P'}[THEME]
PRO = THEME == 'pro'
here = os.path.dirname(os.path.abspath(__file__))
skin = os.path.join(here, '..', 'skin')
L = json.load(open(os.path.join(skin, 'bl_layout.json')))
BTN = {b['id']: b for b in L['btn']}
FW, FH = 991, 501          # drawing coordinates (old layout)
DY = 22                     # rows cropped off the top of the faceplate
GW, GH = 4, 8               # keys drawn this much bigger (width, height)
FACE = FH - DY              # height of the faceplate in the sheet
for _b in L['btn']:
    _b['Size'] = [_b['Size'][0] + GW, _b['Size'][1] + GH]
    _b['Offset'] = [_b['Offset'][0] - GW // 2, _b['Offset'][1] - GH // 2]
for _a in L['ann']:
    _a['Down'] = [_a['Down'][0], FACE + 1]
W = 992

MAIN = {}
ORANGE = {}
BLUE = {}
def row(ids, mains, orange, blue=None):
    for i, k in enumerate(ids):
        MAIN[k] = mains[i]
        if orange and orange[i]: ORANGE[k] = orange[i]
        if blue and blue[i]: BLUE[k] = blue[i]
row(range(101, 111), list('QWERTYUIOP'), 'IF THEN ELSE FOR TO NEXT DEF KEY ADD LR'.split())
row(range(111, 115), list('789/'), 'PREDV MEAN SDEV SQR'.split(), list('!()^'))
row(range(201, 211), list('ASDFGHJKL='), 'CALL GOSUB RETURN GOTO INPUT PRINT DISP DIM BEEP FACT'.split())
row(range(211, 215), list('456*'), 'SIN COS TAN EXP'.split(), list('$%&:'))
row(range(301, 310), list('ZXCVBNM()'), 'EDIT CAT NAME PURGE FETCH LIST DELETE AUTO COPY'.split())
MAIN[310] = 'END LINE'; ORANGE[310] = 'RES'; BLUE[310] = 'CMDS'
row(range(311, 315), list('123−'), 'ASIN ACOS ATAN LOG'.split(), ["'", '"', '#', '@'])
for k, m in zip(range(401, 410), ['ON', 'f', 'g', 'RUN', '◀', '▶', 'SPC', '▲', '▼']):
    MAIN[k] = m
for k, o in {401: 'OFF', 404: 'SST', 405: 'BACK', 406: '−CHAR', 407: 'I/R', 408: 'LC', 409: '−LINE',
             411: 'USER', 412: 'VIEW', 413: 'CALC', 414: 'CONT'}.items(): ORANGE[k] = o
for k, b in {401: 'ATTN', 404: 'CTRL', 405: '|◀', 406: '▶|', 407: 'ERRM', 411: '1 USER',
             412: '<', 413: '>', 414: '?'}.items(): BLUE[k] = b
MAIN.update({411: '0', 412: '.', 413: ',', 414: '+'})

ANN = {4: '←', 5: 'AC', 6: 'USER', 7: 'RAD', 11: 'f', 12: 'g', 13: 'BAT', 20: '0', 21: '1', 22: '2',
       23: '3', 24: '4', 28: '((*))', 29: '→', 30: 'PRGM', 31: 'SUSP', 32: 'CALC'}

FONT = "Helvetica Neue, Helvetica, Arial, sans-serif"
e = html.escape

# ---------- pressed-tile packing ----------
pressed = {}
x, y, rowh = 50, FACE + 13, 0
tall = [k for k in BTN if BTN[k]['Size'][1] > 60]
for k in tall:
    pressed[k] = (0, FACE + 13)
for k in sorted(BTN, key=lambda k: (BTN[k]['Size'][1], k)):
    if k in tall: continue
    w, h = BTN[k]['Size']
    if x + w > W: x, y = 50, y + 46
    pressed[k] = (x, y); x += w + 2
SH = max(FACE + 13 + 108, y + 46) + 4

o = []
a = o.append
a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{SH}" viewBox="0 0 {W} {SH}" font-family="{FONT}">')
LT = THEME == 'light'
B0, B1 = ('#7a7a7a', '#555') if LT else ('#2c2c2c', '#1f1f1f')
NUM = lambda k: k >= 111 and k % 100 >= 11
a('''<defs>
<linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{B0}"/><stop offset="1" stop-color="{B1}"/></linearGradient>
<linearGradient id="top" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#333"/><stop offset="1" stop-color="#252525"/></linearGradient>
<linearGradient id="cap" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4e4e4e"/><stop offset="0.55" stop-color="#2e2e2e"/><stop offset="1" stop-color="#232323"/></linearGradient>
<linearGradient id="capdn" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#262626"/><stop offset="1" stop-color="#1b1b1b"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e8bd5c"/><stop offset="1" stop-color="#b88a30"/></linearGradient>
<linearGradient id="blue" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6fb4e2"/><stop offset="1" stop-color="#3b7eae"/></linearGradient>
<linearGradient id="glass" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#9a9b93"/><stop offset="1" stop-color="#7e7f79"/></linearGradient>
<linearGradient id="caplt" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f2f2f2"/><stop offset="1" stop-color="#c4c4c4"/></linearGradient>
</defs>'''.replace('{B0}', B0).replace('{B1}', B1))
if PRO:
    a('''<defs>
<linearGradient id="silver" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e4e5e7"/><stop offset="0.5" stop-color="#c3c5c8"/><stop offset="1" stop-color="#a4a6aa"/></linearGradient>
<linearGradient id="chrome" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f4f4f5"/><stop offset="0.45" stop-color="#9a9ca0"/><stop offset="1" stop-color="#d8d9db"/></linearGradient>
<linearGradient id="plate" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2e2e2e"/><stop offset="0.5" stop-color="#212121"/><stop offset="1" stop-color="#292929"/></linearGradient>
<linearGradient id="pface" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5e5e5e"/><stop offset="1" stop-color="#363636"/></linearGradient>
<linearGradient id="pfacedn" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#383838"/><stop offset="1" stop-color="#2a2a2a"/></linearGradient>
<linearGradient id="prim" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000"/><stop offset="1" stop-color="#555"/></linearGradient>
<linearGradient id="pband" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0a0a0a"/><stop offset="1" stop-color="#181818"/></linearGradient>
<linearGradient id="pgloss" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.22"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
<linearGradient id="pgloss2" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.6"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
<linearGradient id="pshade" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.16"/></linearGradient>
<linearGradient id="pbsh" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000" stop-opacity="0.16"/><stop offset="1" stop-color="#000" stop-opacity="0"/></linearGradient>
<linearGradient id="pcream" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e8e8dd"/><stop offset="0.5" stop-color="#d0d0c5"/><stop offset="1" stop-color="#bbbbb0"/></linearGradient>
<linearGradient id="pcreamdn" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#c2c2b7"/><stop offset="1" stop-color="#b6b6ab"/></linearGradient>
<linearGradient id="pcband" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#bcbcb1"/><stop offset="1" stop-color="#c6c6bb"/></linearGradient>
<linearGradient id="porange" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f09a5a"/><stop offset="0.6" stop-color="#d9733a"/><stop offset="1" stop-color="#b9561f"/></linearGradient>
<linearGradient id="pblue" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8cc8f4"/><stop offset="0.6" stop-color="#4a96d8"/><stop offset="1" stop-color="#2c6fb0"/></linearGradient>
<filter id="grain" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="7" result="n"/><feColorMatrix in="n" type="matrix" values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 0.9 -0.35"/></filter>
<filter id="brushed" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="0.004 0.8" numOctaves="2" seed="3" result="n"/><feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.9 -0.3"/></filter>
</defs>''')
a(f'<rect id="sheet-bg" width="{W}" height="{SH}" fill="#000"/>')

def key_pro(k, px, py, down):
    w, h = BTN[k]['Size']; m = MAIN[k]
    face = 'url(#pfacedn)' if down else 'url(#pface)'
    cream = NUM(k)
    if cream: face = 'url(#pcreamdn)' if down else 'url(#pcream)'
    if k == 402: face = 'url(#porange)'
    if k == 403: face = 'url(#pblue)'
    d = 1 if down else 0
    g = [f'<g id="key{k}{"-down" if down else ""}" transform="translate({px},{py})">']
    if down: g.append(f'<rect width="{w}" height="{h}" fill="#121212"/>')
    g.append(f'<rect width="{w}" height="{h}" rx="6" fill="url(#prim)"/>')
    g.append(f'<rect x="1.5" y="1" width="{w-3}" height="{h-3}" rx="5" fill="#050505"/>')
    g.append(f'<rect x="2.5" y="{2+d}" width="{w-5}" height="{h-6-d}" rx="4" fill="{face}"/>')
    if not down:
        g.append(f'<rect x="2.5" y="2" width="{w-5}" height="{(h-6)*0.5:.1f}" rx="4" fill="url(#{'pgloss2' if cream else 'pgloss'})"/>')
        if cream: g.append(f'<rect x="2.5" y="{2+(h-6)*0.4:.1f}" width="{w-5}" height="{(h-6)*0.6:.1f}" rx="4" fill="url(#pshade)"/>')
    else:
        g.append(f'<rect x="2.5" y="2" width="{w-5}" height="4" rx="2" fill="#000" fill-opacity="0.35"/>')
    band = True
    if k in (402, 403):
        g.append(f'<rect x="2.5" y="{h-16+d}" width="{w-5}" height="{13-d}" rx="3.5" fill="#000" fill-opacity="0.2"/>')
    else:
        g.append(f'<rect x="2.5" y="{h-16}" width="{w-5}" height="13" rx="3.5" fill="{"url(#pcband)" if cream else "url(#pband)"}"/>')
        if cream:
            g.append(f'<rect x="2.5" y="{h-16}" width="{w-5}" height="5" rx="2" fill="url(#pbsh)"/>')
        else:
            g.append(f'<rect x="3" y="{h-16}" width="{w-6}" height="1" fill="#fff" fill-opacity="0.12"/>')
            g.append(f'<rect x="2.5" y="{h-17}" width="{w-5}" height="1" fill="#000" fill-opacity="0.4"/>')
    tcol = '#2a1608' if k in (402, 403) else ('#1a1a18' if cream else '#f2f2f2')
    bl = '#1c4fb8' if cream else '#4c9fe6'
    if k == 310:
        g.append(f'<text x="{w/2}" y="{h/2-6+d}" fill="#f2f2f2" font-size="13" font-weight="500" text-anchor="middle">END</text>')
        g.append(f'<text x="{w/2}" y="{h/2+8+d}" fill="#f2f2f2" font-size="13" font-weight="500" text-anchor="middle">LINE</text>')
        g.append(f'<text x="{w/2}" y="{h-6.5}" fill="#4c9fe6" font-size="10" font-weight="500" text-anchor="middle">CMDS</text>')
    else:
        fs = 16 if k >= 411 or 111 <= k <= 114 or 211 <= k <= 214 or 311 <= k <= 314 else 14
        if len(m) > 2: fs = 12
        cy = (h - 16 if band else h - 3) / 2 + fs * 0.36 + 1 + d
        g.append(f'<text x="{w/2}" y="{cy:.1f}" fill="{tcol}" font-size="{fs}" font-weight="500" text-anchor="middle">{e(m)}</text>')
        if k in BLUE and k not in (402, 403):
            g.append(f'<text x="{w/2}" y="{h-6.5}" fill="{bl}" font-size="{10.5 if len(BLUE[k]) < 5 else 9}" font-weight="{600 if cream else 500}" text-anchor="middle">{e(BLUE[k])}</text>')
    g.append('</g>')
    return '\n'.join(g)

def key(k, px, py, down=False, bg='#232323'):
    if PRO: return key_pro(k, px, py, down)
    w, h = BTN[k]['Size']
    m = MAIN[k]
    fill = 'url(#capdn)' if down else 'url(#cap)'
    if LT and NUM(k): fill = 'url(#caplt)'
    if k == 402: fill = 'url(#gold)'
    if k == 403: fill = 'url(#blue)'
    g = [f'<g id="key{k}{"-down" if down else ""}" transform="translate({px},{py})">']
    g.append(f'<rect width="{w}" height="{h}" fill="{bg}"/>')
    g.append(f'<rect width="{w}" height="{h}" rx="4" fill="{fill}" stroke="#0a0a0a" stroke-width="1"/>')
    if k not in (402, 403):
        g.append(f'<rect x="1" y="1" width="{w-2}" height="{h-2}" rx="3" fill="none" stroke="#fff" stroke-opacity="{0.05 if down else 0.18}"/>')
    ty = 1 if down else 0
    tcol = '#10151a' if (k in (402, 403) or (LT and NUM(k))) else '#fff'
    bcol = '#2d5fb0' if (LT and NUM(k)) else '#6fb4e2'
    if k == 310:
        g.append(f'<text x="{w/2}" y="{h/2-6+ty}" fill="#fff" font-size="12" font-weight="700" text-anchor="middle">END</text>')
        g.append(f'<text x="{w/2}" y="{h/2+8+ty}" fill="#fff" font-size="12" font-weight="700" text-anchor="middle">LINE</text>')
        g.append(f'<text x="{w/2}" y="{h-7}" fill="#6fb4e2" font-size="8" text-anchor="middle">CMDS</text>')
    else:
        fs = 15 if k >= 411 or 111 <= k <= 114 or 211 <= k <= 214 or 311 <= k <= 314 else 13
        if len(m) > 2: fs = 11
        band = k in BLUE
        cy = h * (0.45 if band else 0.5) + fs * 0.36 + ty
        g.append(f'<text x="{w/2}" y="{cy:.1f}" fill="{tcol}" font-size="{fs}" font-weight="700" text-anchor="middle">{e(m)}</text>')
        if band:
            g.append(f'<rect x="1" y="{h-11}" width="{w-2}" height="10" rx="3" fill="#000" fill-opacity="{0.12 if LT and NUM(k) else 0.28}"/>')
            g.append(f'<text x="{w/2}" y="{h-3}" fill="{bcol}" font-size="8" text-anchor="middle">{e(BLUE[k])}</text>')
    g.append('</g>')
    return '\n'.join(g)

# ---------- faceplate ----------
a(f'<g id="faceplate" transform="translate(0,{-DY})">')
if PRO:
    YL, YR, SX = 172, 184, 642     # silver lower edge: left / right of the step, step x
    sil = f'M6,{DY+10} Q6,{DY+4} 12,{DY+4} H{FW-12} Q{FW-6},{DY+4} {FW-6},{DY+10} V{YR} H{SX+12} L{SX},{YL} H6 Z'
    fr_o = f'M22,{YL+8} H{SX-8} L{SX+4},{YR+8} H{FW-22} V472 H22 Z'
    fr_i = f'M28,{YL+14} H{SX-6} L{SX+6},{YR+14} H{FW-28} V466 H28 Z'
    a('<clipPath id="clip-top"><path d="%s"/></clipPath><clipPath id="clip-plate"><path d="%s"/></clipPath>' % (sil, fr_i))
    a(f'<rect id="case" width="{FW}" height="{FH}" rx="10" fill="#0a0a0a"/>')
    a(f'<path id="top-panel" d="{sil}" fill="url(#silver)"/>')
    a(f'<rect id="top-brushed" x="6" y="{DY+4}" width="{FW-12}" height="{YR-DY-4}" clip-path="url(#clip-top)" filter="url(#brushed)" opacity="0.35"/>')
    a('<rect id="lcd-bezel" x="118" y="56" width="552" height="86" rx="6" fill="url(#chrome)" stroke="#6d6f73"/>')
    a('<rect id="lcd-well" x="124" y="62" width="540" height="74" rx="3" fill="#2a2a2a"/>')
    a('<rect id="lcd-glass" x="128" y="66" width="532" height="66" rx="2" fill="#7e7f79"/>')
    a('<g id="logo-block">')
    a('<rect id="logo-plate" x="690" y="56" width="260" height="38" rx="4" fill="#1b1b1b" stroke="#080808" stroke-width="2"/>')
    a('<rect id="logo-plate-edge" x="691" y="57" width="258" height="36" rx="3" fill="none" stroke="#fff" stroke-opacity="0.18"/>')
    LOGO = os.environ.get('LOGO')   # optional: path to a supplied logo/wordmark image (PNG/SVG/JPG)
    if LOGO:
        import base64, mimetypes
        mt = mimetypes.guess_type(LOGO)[0] or 'image/png'
        b64 = base64.b64encode(open(LOGO, 'rb').read()).decode()
        a(f'<g id="logo-area"><image x="696" y="60" width="200" height="30" preserveAspectRatio="xMinYMid meet" href="data:{mt};base64,{b64}"/></g>')
    else:
        sys.path.insert(0, here)
        from make_logo import logo_group
        lg, _ = logo_group(698, 60, 30, '#e8e8e8', '#1b1b1b')
        a('<g id="logo-area">' + lg + '</g>')
    a('<text id="model" x="936" y="84" fill="#e6e6e6" font-size="24" font-weight="800" font-style="italic" text-anchor="end">71B</text>')
    a('</g>')
    a('<g id="card-reader">')
    a('<rect id="card-housing" x="690" y="98" width="260" height="66" rx="4" fill="#1b1b1b" stroke="#080808" stroke-width="2"/>')
    a('<rect x="691" y="99" width="258" height="64" rx="3" fill="none" stroke="#fff" stroke-opacity="0.15"/>')
    for gx in range(700, 752, 7):
        a(f'<rect x="{gx}" y="108" width="3" height="46" fill="#0c0c0c"/><rect x="{gx+3}" y="108" width="1" height="46" fill="#fff" fill-opacity="0.12"/>')
    a('<rect id="card-label" x="762" y="104" width="180" height="54" rx="2" fill="#f1f1ec" stroke="#000"/>')
    a('<rect x="762" y="104" width="180" height="5" rx="2" fill="#000" fill-opacity="0.18"/>')
    a('</g>')
    a(f'<path id="key-plate" d="{fr_o}" fill="url(#chrome)"/>')
    a(f'<path id="key-plate-inner" d="{fr_i}" fill="url(#plate)"/>')
    a(f'<rect id="key-plate-grain" x="28" y="{YL+14}" width="{FW-56}" height="{466-YL-14}" clip-path="url(#clip-plate)" filter="url(#grain)" opacity="0.26"/>')
    a(f'<rect id="chrome-strip" x="22" y="476" width="{FW-44}" height="12" rx="3" fill="url(#chrome)"/>')
else:
    a(f'<rect id="case" width="{FW}" height="{FH}" rx="10" fill="url(#body)"/>')
    a(f'<rect id="top-panel" x="6" y="6" width="{FW-12}" height="170" rx="8" fill="url(#top)" stroke="#444"/>')
    a('<rect id="lcd-bezel" x="124" y="62" width="540" height="74" rx="7" fill="#0e0e0e" stroke="#555"/>')
    a('<rect id="lcd-glass" x="180" y="82" width="408" height="32" rx="2" fill="url(#glass)"/>')
    a(f'<text id="model" x="{FW-40}" y="48" fill="#c9a24a" font-size="26" font-weight="700" font-style="italic" text-anchor="end">71B</text>')
    a('<rect id="module-slot" x="700" y="64" width="150" height="48" rx="4" fill="#1a1a1a" stroke="#444"/>')
    a(f'<rect id="key-frame" x="38" y="190" width="{FW-76}" height="276" rx="8" fill="none" stroke="{"#e6e6e6" if LT else "#c9a24a"}" stroke-width="2"/>')
a(f'<g id="orange-legends" fill="{'#e8782a' if PRO else '#d9a846'}" font-size="{10.5 if PRO else 9}" font-weight="{400 if PRO else 700}" text-anchor="middle">')
for k, t in ORANGE.items():
    b = BTN[k]; px, py = b['Offset']; w, h = b['Size']
    a(f'<text x="{px+w/2}" y="{py-(3.5 if PRO else 5)}">{e(t)}</text>')
a('</g>')
a('<g id="keys-up">')
for k in sorted(BTN):
    a(key(k, *BTN[k]['Offset']))
a('</g>')
a(f'<g id="annunciators-dim" fill="{"#6a6b65" if PRO else "#4a4a4a"}" font-weight="700" text-anchor="middle">')
for an in L['ann']:
    ax, ay = an['Offset']; w, h = an['Size']
    a(f'<text id="ann{an["id"]}-dim" x="{ax+w/2}" y="{ay+h-1}" font-size="{h-1}" textLength="{w}" lengthAdjust="spacingAndGlyphs">{e(ANN[an["id"]])}</text>')
a('</g>')
a('</g>')

# ---------- lit annunciators ----------
a(f'<g id="annunciators-lit" font-weight="700" text-anchor="middle" fill="{"#101010" if PRO else "#ffc94d"}">')
for an in L['ann']:
    ax, ay = an['Down']; w, h = an['Size']
    a(f'<rect x="{ax}" y="{ay}" width="{w}" height="{h}" fill="{'#7e7f79' if PRO else '#0e0e0e'}"/>')
    a(f'<text id="ann{an["id"]}-lit" x="{ax+w/2}" y="{ay+h-1}" font-size="{h-1}" textLength="{w}" lengthAdjust="spacingAndGlyphs">{e(ANN[an["id"]])}</text>')
a('</g>')

# ---------- pressed keys ----------
a('<g id="keys-down">')
for k in sorted(BTN):
    a(key(k, *pressed[k], down=True))
a('</g>')
a('</svg>')
open(os.path.join(skin, 'REAL71' + SUF + '.svg'), 'w').write('\n'.join(o))

# ---------- KML ----------
src = open(os.path.join(skin, 'REAL71BL.KML'), encoding='latin-1').read().replace('\r', '')
out, blk, cur = [], None, None
LCD_FG = lambda i: int(round(113 - (113 - 34) * (i - 1) / 7.0))   # lighter, softer LCD pixels
for line in src.split('\n'):
    m = re.match(r'(Background|Lcd|Annunciator|Button|Global)\b\s*(\d*)', line)
    if m:
        blk = m.group(1); cur = int(m.group(2)) if m.group(2) else None
    ms = re.match(r'(\s*)(Offset|Size|Down)\s+(-?\d+)\s+(-?\d+)', line)
    mc = re.match(r'(\s*)Color\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)', line)
    if ms:
        key_, a_, b_ = ms.group(2), int(ms.group(3)), int(ms.group(4))
        if blk == 'Background' and key_ == 'Size': a_, b_ = FW, FACE
        elif blk == 'Lcd' and key_ == 'Offset': b_ -= DY
        elif blk == 'Annunciator' and key_ == 'Offset': b_ -= DY
        elif blk == 'Annunciator' and key_ == 'Down': b_ = FACE + 1
        elif blk == 'Button' and cur in BTN:
            if key_ == 'Size': a_, b_ = BTN[cur]['Size']
            elif key_ == 'Offset': a_, b_ = BTN[cur]['Offset'][0], BTN[cur]['Offset'][1] - DY
            elif key_ == 'Down': a_, b_ = pressed[cur]
        line = f'\t{key_:<8} {a_} {b_}'
    elif mc:
        n = int(mc.group(2))
        if 1 <= n <= 8: v = LCD_FG(n); line = f'\tColor {n:>2} {v} {v} {max(v-2,0)}'
        elif 9 <= n <= 15: line = f'\tColor {n:>2} 30 30 28'
    if re.match(r'\s*Bitmap', line): line = f'\tBitmap   "REAL71{SUF}.png"'
    elif re.match(r'\s*Print', line): continue
    elif re.match(r'\s*Title', line): line = '\tTitle    "Emu71 original dark skin"'
    elif re.match(r'\s*Author', line): line = '\tAuthor   "Emu71 Mac port"'
    out.append(line)
open(os.path.join(skin, 'REAL71' + SUF + '.KML'), 'w', encoding='latin-1').write('\n'.join(out))
print('sheet', W, SH)
