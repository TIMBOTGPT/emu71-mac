#!/usr/bin/env python3
"""Host-side LIF disk image tool for the Emu71 HP-IL virtual drive (tools/il_disk.py).

  lifimg.py init IMAGE                       create a blank formatted 77x2x16 image (like INITIALIZE :TAPE)
  lifimg.py dir  IMAGE                       list the directory
  lifimg.py put  IMAGE NAME FILE [--type TEXT|BASIC71]   add a host text file as a LIF file
  lifimg.py get  IMAGE NAME [OUTFILE]        extract a LIF1 text file (to stdout by default)
  lifimg.py del  IMAGE NAME                  mark a file purged

TEXT files are LIF type 1: one record per line, a 2-byte big-endian length, the bytes, a zero pad
byte when the length is odd, and an 0xFFFF end-of-file record. The HP-71 reads them with
COPY name:TAPE TO name, then GET name. The directory entry layout follows the LIF standard
(name 10, type 2, start 4, length in sectors 4, BCD date 6, volume flags 0x8001, 4 implementation bytes).
The image must already carry a LIF header, which the 71 writes with INITIALIZE :TAPE.
"""
import sys, time

SECT = 256

def i2(b, o, n): return int.from_bytes(b[o:o + n], 'big')

def bcd(v): return ((v // 10) % 10) << 4 | (v % 10)

class Img:
    def __init__(self, path):
        self.path = path
        self.d = bytearray(open(path, 'rb').read())
        if i2(self.d, 0, 2) != 0x8000:
            sys.exit("%s: no LIF header (run INITIALIZE :TAPE on the 71 first)" % path)
        self.dirstart = i2(self.d, 8, 4); self.dirlen = i2(self.d, 16, 4)

    def entries(self):
        """yield (offset, entry bytes) up to the end-of-directory marker"""
        for s in range(self.dirstart, self.dirstart + self.dirlen):
            for k in range(0, SECT, 32):
                o = s * SECT + k; e = self.d[o:o + 32]
                if i2(e, 10, 2) == 0xFFFF and e[0] == 0xFF or e == b'\xff' * 32:
                    yield o, None; return
                yield o, bytes(e)

    def find(self, name):
        n = name.upper().ljust(10).encode('ascii')
        for o, e in self.entries():
            if e is None: return None
            if e[:10] == n and i2(e, 10, 2) != 0: return o, e
        return None

    def end_slot(self):
        for o, e in self.entries():
            if e is None: return o
        sys.exit("directory full")

    def next_free_sector(self):
        n = self.dirstart + self.dirlen
        for o, e in self.entries():
            if e is None: break
            if i2(e, 10, 2): n = max(n, i2(e, 12, 4) + i2(e, 16, 4))
        return n

    def save(self): open(self.path, 'wb').write(self.d)

def text_records(text):
    out = bytearray()
    for line in text.replace('\r\n', '\n').split('\n')[:-1] if text.endswith('\n') else text.replace('\r\n', '\n').split('\n'):
        b = line.encode('latin-1', 'replace')
        out += len(b).to_bytes(2, 'big') + b + (b'\0' if len(b) & 1 else b'')
    out += b'\xff\xff'
    return bytes(out)

def cmd_put(a):
    img = Img(a[0]); name = a[1].upper(); src = a[2]
    if len(name) > 10: sys.exit("LIF names are at most 10 characters")
    ftype = {'TEXT': 1, 'BASIC71': 0xE214}[a[a.index('--type') + 1].upper()] if '--type' in a else 1
    if ftype != 1: sys.exit("only TEXT is supported so far")
    if img.find(name): sys.exit("%s already exists" % name)
    data = text_records(open(src, encoding='latin-1').read())
    blocks = (len(data) + SECT - 1) // SECT
    start = img.next_free_sector()
    if (start + blocks) * SECT > len(img.d): sys.exit("disk full")
    slot = img.end_slot()
    t = time.localtime()
    e = bytearray(32)
    e[0:10] = name.ljust(10).encode('ascii')
    e[10:12] = ftype.to_bytes(2, 'big'); e[12:16] = start.to_bytes(4, 'big'); e[16:20] = blocks.to_bytes(4, 'big')
    e[20:26] = bytes([bcd(t.tm_year % 100), bcd(t.tm_mon), bcd(t.tm_mday), bcd(t.tm_hour), bcd(t.tm_min), bcd(t.tm_sec)])
    e[26] = 0x80; e[27] = 0x01
    img.d[slot:slot + 32] = e
    nxt = slot + 32                       # keep the end-of-directory marker after the new entry
    if nxt % SECT or True:
        sec_end = (slot // SECT + 1) * SECT
        if nxt < sec_end: img.d[nxt:nxt + 32] = b'\xff' * 32
        else:
            ns = slot // SECT + 1
            if ns < img.dirstart + img.dirlen: img.d[ns * SECT:ns * SECT + 32] = b'\xff' * 32
    img.d[start * SECT:start * SECT + blocks * SECT] = data.ljust(blocks * SECT, b'\0')
    img.save()
    print("%s: %d bytes, %d sectors at %d" % (name, len(data), blocks, start))

def cmd_dir(a):
    img = Img(a[0])
    print("volume %r dir %d+%d" % (bytes(img.d[2:8]).decode('latin-1'), img.dirstart, img.dirlen))
    for o, e in img.entries():
        if e is None: break
        t = i2(e, 10, 2)
        if t: print("%-10s type %04X start %d sectors %d" % (e[:10].decode('latin-1'), t, i2(e, 12, 4), i2(e, 16, 4)))

def cmd_get(a):
    img = Img(a[0]); r = img.find(a[1])
    if not r: sys.exit("not found")
    e = r[1]
    if i2(e, 10, 2) != 1: sys.exit("not a LIF1 text file")
    p = i2(e, 12, 4) * SECT; lines = []
    while True:
        n = i2(img.d, p, 2); p += 2
        if n == 0xFFFF: break
        lines.append(bytes(img.d[p:p + n]).decode('latin-1')); p += n + (n & 1)
    txt = '\n'.join(lines) + '\n'
    if len(a) > 2: open(a[2], 'w').write(txt)
    else: sys.stdout.write(txt)

def cmd_del(a):
    img = Img(a[0]); r = img.find(a[1])
    if not r: sys.exit("not found")
    img.d[r[0] + 10:r[0] + 12] = b'\0\0'; img.save()

def cmd_init(a):
    hdr = bytes.fromhex('800020202020202000000002100000000000004d000100000000004d00000002000000100001010000190000')
    d = bytearray(b'\xff' * (77 * 2 * 16 * SECT))
    d[0:SECT] = hdr.ljust(SECT, b'\0')
    d[SECT:2 * SECT] = bytes(SECT)
    d[2 * SECT:3 * SECT] = (b'\xff' * 32).ljust(SECT, b'\0')
    open(a[0], 'wb').write(d)

if __name__ == '__main__':
    if len(sys.argv) < 3: sys.exit(__doc__)
    {'init': cmd_init, 'dir': cmd_dir, 'put': cmd_put, 'get': cmd_get, 'del': cmd_del}[sys.argv[1]](sys.argv[2:])
