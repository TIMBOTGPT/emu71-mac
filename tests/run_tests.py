#!/usr/bin/env python3
"""EMU71_KEYS regression tests for the Emu71 video LEX, virtual disk and state handling.

usage: tests/run_tests.py [--update] [--slow] [--keep] [NAME ...]

Each tests/cases/NAME.t is one case made of one or more [run] blocks (one emulator run each, always EMU71_NOSTATE=1
unless the run says `state: yes`, and then HOME points at a scratch folder so the real state.bin is never touched).
A run types its lines through EMU71_KEYS, waits `idle` seconds after the last key (EMU71_DUMPIDLE), then checks
  - expect_log / expect_nolog   regexes over the EMU71_VIDLOG mailbox lines (hand-written, independent of the goldens)
  - golden NAME.R.lcd           the 71 LCD dump (ASCII dots)
  - golden NAME.R.log           the mailbox command lines
  - golden NAME.R.png           the shown video layer (only with `ppm: yes`), compared pixel by pixel
Goldens live in tests/golden. `--update` rewrites them from the current output, look at the PNGs and LCDs before committing.

Directives in a [run] block:
  lex: video | none | clash,video | video,clash   LEX ROMs for ports 3 and 4 (clash = tests/lex/clash.asm, needs asm71)
  line: BASIC text        typed and ENTERed, followed by ATTN,ATTN and a wait unless it is the last item
  pause: MS               wait MS ms (use after a long command such as COPY, that line then gets no ATTN)
  idle: SECONDS           wait after the last key before the LCD dump (default 4)
  state: yes              keep state saving on (scratch HOME, shared by the runs of one case)
  ppm: yes                capture the shown video layer
  viddir: yes             EMU71_VIDDIR = a temp folder shared by the runs of one case
  disk: lex               HP-IL virtual disk carrying lex/video.lex (LIF type E208)
  keyms: N                key spacing in ms (default 100)
  slow: yes               only run with --slow or when named on the command line
Needs: python3 with Pillow, ./emu71 built, ~/Projects/asm71/build/asm71 only for the clash LEX.
"""
import os, sys, re, subprocess, tempfile, shutil, random, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMU = os.path.join(ROOT, 'emu71')
ASM71 = os.environ.get('ASM71', os.path.expanduser('~/Projects/asm71/build/asm71'))
CASES = os.path.join(ROOT, 'tests', 'cases')
GOLD = os.path.join(ROOT, 'tests', 'golden')

def parse_case(path):
    runs, cur = [], None
    for raw in open(path):
        ln = raw.rstrip('\n')
        if not ln.strip() or ln.lstrip().startswith('#'): continue
        if ln.strip() == '[run]':
            cur = dict(items=[], lex='video', idle=4.0, state=False, ppm=False, viddir=False, disk=None, keyms=100, expect=[], reject=[], slow=False)
            runs.append(cur); continue
        k, _, v = ln.partition(':'); k = k.strip(); v = v.strip()
        if cur is None:
            continue                                   # header lines such as name: are ignored
        if k == 'line': cur['items'].append(('line', v))
        elif k == 'pause': cur['items'].append(('pause', int(v)))
        elif k == 'idle': cur['idle'] = float(v)
        elif k == 'lex': cur['lex'] = v
        elif k in ('state', 'ppm', 'viddir'): cur[k] = v.lower() == 'yes'
        elif k == 'disk': cur['disk'] = v
        elif k == 'keyms': cur['keyms'] = int(v)
        elif k == 'expect_log': cur['expect'].append(v)
        elif k == 'expect_nolog': cur['reject'].append(v)
        elif k == 'slow': cur['slow'] = v.lower() == 'yes'
        else: sys.exit('%s: unknown directive %r' % (path, k))
    return runs

def keys_for(text):
    out = subprocess.check_output([sys.executable, os.path.join(ROOT, 'tools', 'keys.py'), text]).decode().strip()
    return out.split(',')[2:]                         # drop the leading ATTN,ATTN

def build_keys(items):
    toks = ['27', '27', 'w1500']
    for i, (kind, val) in enumerate(items):
        last = i == len(items) - 1
        nxt = None if last else items[i + 1][0]
        if kind == 'line':
            toks += keys_for(val)
            if not last and nxt != 'pause': toks += ['w1500', '27', '27', 'w600']
        else:
            toks += ['w%d' % val, '27', '27', 'w600']
    return ','.join(toks)

def lcd_rows(text):
    rows = []
    for ln in text.splitlines():
        if ln and set(ln) <= set('#.'): rows.append(ln)
        elif ln.startswith('pc='): break
    return rows

def nocursor(lcd):
    # the block cursor blinks, so a dump can catch it on or off. Blank any fully set 5x7 cell (no font glyph fills it).
    rows = [list(r) for r in lcd.splitlines()]
    if len(rows) < 7: return lcd
    for c in range(len(rows[0]) // 6):
        x = 6 * c
        if all(rows[y][x + k] == '#' for y in range(7) for k in range(5)):
            for y in range(7):
                for k in range(5): rows[y][x + k] = '.'
    return '\n'.join(''.join(r) for r in rows) + '\n'

class Tools:
    def __init__(self, tmp):
        self.tmp = tmp; self.rom = {}
    def lexrom(self, name):
        if name in self.rom: return self.rom[name]
        out = os.path.join(self.tmp, name + '.rom')
        if name == 'video':
            subprocess.check_call([sys.executable, os.path.join(ROOT, 'tools', 'mklexrom.py'), os.path.join(ROOT, 'lex', 'video.lex'), out], stdout=subprocess.DEVNULL)
        elif name == 'clash':
            d = os.path.join(self.tmp, 'clash'); os.makedirs(d, exist_ok=True)
            shutil.copy(os.path.join(ROOT, 'tests', 'lex', 'clash.asm'), d)
            subprocess.check_call([ASM71, 'clash.asm'], cwd=d, stdout=subprocess.DEVNULL)
            subprocess.check_call([sys.executable, os.path.join(ROOT, 'tools', 'mklexrom.py'), os.path.join(d, 'clash.lex'), out], stdout=subprocess.DEVNULL)
        else: sys.exit('unknown lex %r' % name)
        self.rom[name] = out; return out

def run_once(run, tools, shared, label):
    env = dict(os.environ)
    env['EMU71_KEYS'] = build_keys(run['items']); env['EMU71_KEYMS'] = str(run['keyms'])
    env['EMU71_VIDLOG'] = '1'; env['EMU71_DUMPIDLE'] = str(run['idle']); env['EMU71_EXIT'] = '1'; env['EMU71_FASTAT'] = '2.5'
    env['HOME'] = shared['home']
    if run['state']: env.pop('EMU71_NOSTATE', None)
    else: env['EMU71_NOSTATE'] = '1'
    parts = [p for p in run['lex'].split(',') if p and p != 'none']
    env['EMU71_LEXROM'] = tools.lexrom(parts[0]) if parts else 'none'
    if len(parts) > 1: env['EMU71_LEXROM2'] = tools.lexrom(parts[1])
    if run['viddir']: env['EMU71_VIDDIR'] = shared['viddir']
    ppm = os.path.join(shared['tmp'], label + '.ppm')
    if run['ppm']: env['EMU71_VIDSHOT'] = ppm
    args = [EMU, '--vid']
    dp = None
    if run['disk']:
        img = os.path.join(shared['tmp'], 'disk.dat')
        subprocess.check_call([sys.executable, os.path.join(ROOT, 'tools', 'lifimg.py'), 'init', img])
        subprocess.check_call([sys.executable, os.path.join(ROOT, 'tools', 'lifimg.py'), 'putlif', img, os.path.join(ROOT, 'lex', 'video.lex')], stdout=subprocess.DEVNULL)
        o = random.randint(50000, 52000); i = o + 1
        dp = subprocess.Popen([sys.executable, os.path.join(ROOT, 'tools', 'il_disk.py'), img, str(o), str(i)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1); args += ['--hpil', '127.0.0.1:%d:%d' % (o, i)]
    try:
        p = subprocess.run(args, env=env, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
    finally:
        if dp: dp.kill()
    text = p.stdout.decode('latin-1')
    return text, (ppm if run['ppm'] and os.path.exists(ppm) else None)

def png_equal(a, b):
    from PIL import Image, ImageChops
    ia, ib = Image.open(a).convert('RGB'), Image.open(b).convert('RGB')
    return ia.size == ib.size and ImageChops.difference(ia, ib).getbbox() is None

def main():
    a = sys.argv[1:]; update = '--update' in a; slow = '--slow' in a; keep = '--keep' in a
    names = [x for x in a if not x.startswith('--')]
    if not os.path.exists(EMU): sys.exit('build ./emu71 first (make)')
    os.makedirs(GOLD, exist_ok=True)
    cases = sorted(f[:-2] for f in os.listdir(CASES) if f.endswith('.t'))
    if names: cases = [c for c in cases if c in names]
    tmp = tempfile.mkdtemp(prefix='emu71test.'); tools = Tools(tmp)
    fails, passed, skipped = [], 0, []
    for name in cases:
        runs = parse_case(os.path.join(CASES, name + '.t'))
        if any(r['slow'] for r in runs) and not slow and not names: skipped.append(name); continue
        home = os.path.join(tmp, 'home_' + name); os.makedirs(os.path.join(home, 'Library', 'Application Support'))
        shared = dict(home=home, tmp=tmp, viddir=tempfile.mkdtemp(prefix='viddir.', dir=tmp))
        problems = []
        for n, run in enumerate(runs, 1):
            label = '%s.%d' % (name, n)
            t0 = time.time(); text, ppm = run_once(run, tools, shared, label)
            log = '\n'.join(l for l in text.splitlines() if l.startswith('VIDEO: cmd')) + '\n'
            lcd = '\n'.join(lcd_rows(text)) + '\n'
            for rx in run['expect']:
                if not re.search(rx, log): problems.append('%s: expected log /%s/' % (label, rx))
            for rx in run['reject']:
                if re.search(rx, log): problems.append('%s: unexpected log /%s/' % (label, rx))
            if len(lcd.strip()) == 0: problems.append('%s: no LCD dump (emulator did not reach the dump)' % label)
            g = lambda ext: os.path.join(GOLD, '%s.%s' % (label, ext))
            if update:
                open(g('lcd'), 'w').write(lcd); open(g('log'), 'w').write(log)
                if ppm:
                    from PIL import Image; Image.open(ppm).save(g('png'))
            else:
                for ext, cur in (('lcd', lcd), ('log', log)):
                    if not os.path.exists(g(ext)): problems.append('%s: no golden .%s (run --update)' % (label, ext))
                    elif (nocursor(open(g(ext)).read()) != nocursor(cur) if ext == 'lcd' else open(g(ext)).read() != cur): problems.append('%s: .%s differs from golden' % (label, ext)); open(os.path.join(tmp, label + '.' + ext), 'w').write(cur)
                if run['ppm']:
                    if not ppm: problems.append('%s: no video snapshot' % label)
                    elif not os.path.exists(g('png')): problems.append('%s: no golden .png (run --update)' % label)
                    else:
                        from PIL import Image; cur = os.path.join(tmp, label + '.png'); Image.open(ppm).save(cur)
                        if not png_equal(cur, g('png')): problems.append('%s: video layer differs from golden (%s)' % (label, cur))
            print('  %-28s %5.1fs' % (label, time.time() - t0), flush=True)
        if problems: fails.append((name, problems)); print('FAIL', name); [print('   ', p) for p in problems]
        else: passed += 1; print('ok  ', name)
    print('\n%d passed, %d failed%s' % (passed, len(fails), (', skipped slow: ' + ' '.join(skipped)) if skipped else ''))
    if keep or fails: print('scratch files kept in', tmp)
    else: shutil.rmtree(tmp, ignore_errors=True)
    sys.exit(1 if fails else 0)

if __name__ == '__main__':
    main()
