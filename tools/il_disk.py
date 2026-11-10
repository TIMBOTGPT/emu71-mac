#!/usr/bin/env python3
"""Headless HP-IL mass storage partner for Emu71 (an HP 9114 style drive on the virtual loop).

The emulator connects to OUT (we listen) and reads frames back on IN (we connect to it), the
same transport as il_echo.py. This file is a headless port of the device state machine and the
AMIGO disk commands of pyILPER (pildevbase.py / pildrive.py, GPL v2, J-F Garnier, B. Hoekstra
and contributors): HP-IL addressing, SST/SDI/SAI, DDL/DDT disk commands, 256-byte sectors.

usage: il_disk.py image.dat [out_port [in_port]] [--tracks N --surfaces N --blocks N] [--log]
The image is created and formatted when it does not exist (the 71 writes the LIF header on
INITIALIZE). Default geometry is 77 tracks x 2 surfaces x 16 blocks = 2464 sectors, like a 9114.
"""
import os, socket, struct, sys, time, threading

def lif_int(b, off, n):
    return int.from_bytes(bytes(b[off:off + n]), 'big')

def put_lif_int(b, off, n, v):
    b[off:off + n] = int(v).to_bytes(n, 'big')

class Drive:
    def __init__(self, image, tracks=77, surfaces=2, blocks=16, did="HP9114B", log=False):
        self.aid, self.defaddr, self.did = 0x10, 2, did
        self.addr = self.addr2nd = 0
        self.ilstate = 0
        self.talker_frame = 0
        self.ptsdi = self.ptssi = 0
        self.status_len = 1
        self.status = 0
        self.devl = self.devt = 31
        self.oc = self.pe = self.pe0 = 0
        self.fpt = False
        self.flpwr = 0
        self.ptout = 0
        self.tracks, self.surfaces, self.blocks = tracks, surfaces, blocks
        self.nbe = tracks * surfaces * blocks
        self.lif = bytearray(12)
        put_lif_int(self.lif, 0, 4, tracks); put_lif_int(self.lif, 4, 4, surfaces); put_lif_int(self.lif, 8, 4, blocks)
        self.buf0, self.buf1 = bytearray(256), bytearray(256)
        self.image, self.log = image, log
        self.nframes = 0
        if not os.path.exists(image):
            self.format_disc()
        self.clear_device(); self.status = 0

    # ---------- disk
    def rrec(self):
        try:
            with open(self.image, 'rb') as f:
                f.seek(self.pe * 256); b = f.read(256)
            self.status = 0
            self.buf0[:] = b.ljust(256, b'\0')
        except OSError:
            self.status = 20

    def fix_header(self):
        b = self.buf0
        if b[0] == 0x80 and b[1] == 0:
            if lif_int(b, 24, 4) == lif_int(b, 28, 4) == lif_int(b, 32, 4):
                put_lif_int(b, 24, 4, self.tracks); put_lif_int(b, 28, 4, self.surfaces); put_lif_int(b, 32, 4, self.blocks)
            if b[0x14] != 0 or b[0x15] != 1:
                b[0x14], b[0x15] = 0, 1
            if b[2] != 0x20 and (b[2] < 0x41 or b[2] > 0x5A):
                b[2:8] = b'\x20' * 6
                if b[0x12] & 0x40: b[0x12] &= ~0x40

    def wrec(self):
        try:
            with open(self.image, 'r+b') as f:
                f.seek(self.pe * 256)
                if self.pe == 0: self.fix_header()
                f.write(self.buf0)
            self.status = 0
            if self.log: print("write sector", self.pe, flush=True)
        except OSError:
            self.status = 29

    def format_disc(self):
        try:
            with open(self.image, 'wb') as f:
                for _ in range(self.nbe): f.write(b'\xFF' * 256)
            self.status = 0
        except OSError:
            self.status = 29

    def clear_device(self):
        self.fpt = False; self.pe = 0; self.oc = 0
        self.buf0[:] = bytes(256); self.buf1[:] = bytes(256)

    def clear_hpil(self):
        self.clear_device(); self.status &= ~0x40

    # ---------- AMIGO data phases
    def indata(self, n):
        if self.devl in (0, 2, 6):
            self.buf0[self.oc] = n & 255; self.oc += 1
            if self.oc > 255:
                self.oc = 0; self.wrec(); self.pe += 1
                if self.flpwr: self.rrec()
            elif n & 0x200:
                self.wrec()
                if self.flpwr == 0: self.pe += 1
        elif self.devl == 1:
            self.buf1[self.oc] = n & 255; self.oc += 1
            if self.oc > 255: self.oc = 0
        elif self.devl == 3:
            self.oc = n & 255
        elif self.devl == 4:
            n &= 255
            if self.fpt:
                self.pe0 = (self.pe0 & 0xFF00) | n
                if self.pe0 < self.nbe: self.pe = self.pe0; self.status = 0
                else: self.status = 28
                self.fpt = False
            else:
                self.pe0 = (self.pe0 & 255) | (n << 8); self.fpt = True

    def outdata(self, frame):
        if frame == 0x560: self.ptout = 0
        if self.devt in (0, 2):
            frame = self.buf0[self.oc]; self.oc += 1
            if self.oc > 255: self.oc = 0; self.rrec(); self.pe += 1
        elif self.devt == 1:
            frame = self.buf1[self.oc]; self.oc += 1
            if self.oc > 255: self.oc = 0; self.devt = 15
        elif self.devt == 3:
            if self.ptout == 0: frame = self.pe >> 8
            elif self.ptout == 1: frame = self.pe & 255
            elif self.ptout == 2: frame = self.oc & 255
            else: frame = 0x540
            self.ptout += 1
        elif self.devt == 6:
            if self.ptout < 12: frame = self.lif[self.ptout]; self.ptout += 1
            else: frame = 0x540
        elif self.devt == 7:
            if self.ptout == 0: frame = (self.nbe - 1) >> 8
            elif self.ptout == 1: frame = (self.nbe - 1) & 255
            else: frame = 0x540
            self.ptout += 1
        else:
            frame = 0x540
        return frame

    def cmd_ext(self, frame):
        n = frame & 0xFF; t = n >> 5
        if t == 5:                                   # DDL
            n &= 31
            if (self.ilstate & 0xC0) == 0x80:
                self.devl = n
                if n == 1: self.flpwr = 0
                elif n == 2: self.oc = 0; self.flpwr = 0
                elif n == 4: self.flpwr = 0; self.fpt = False
                elif n == 5: self.format_disc()
                elif n == 6: self.flpwr = 0x80; self.rrec()
                elif n == 7: self.fpt = False; self.pe = 0; self.oc = 0
                elif n == 8:
                    self.wrec()
                    if self.flpwr == 0: self.pe += 1
                elif n == 9: self.oc = 0; self.buf1[:] = self.buf0
                elif n == 10: self.oc = 0; self.buf0[:], self.buf1[:] = bytes(self.buf1), bytes(self.buf0)
        elif t == 6:                                 # DDT
            n &= 31
            if (self.ilstate & 0x40) == 0x40:
                self.devt = n
                if n == 0: self.flpwr = 0
                elif n == 2: self.rrec(); self.oc = 0; self.flpwr = 0; self.pe += 1
                elif n == 4: self.oc = 0; self.buf0[:], self.buf1[:] = bytes(self.buf1), bytes(self.buf0)
        return frame

    # ---------- generic HP-IL device
    def process(self, frame):
        if (frame & 0x400) == 0: frame = self.do_doe(frame)
        elif (frame & 0x700) == 0x400: frame = self.do_cmd(frame)
        elif (frame & 0x700) == 0x500: frame = self.do_rdy(frame)
        if self.status & 0x40:
            if (frame & 0x700) in (0x000, 0x200, 0x600): frame |= 0x100
        return frame

    def do_doe(self, frame):
        talker_error = False
        if (self.ilstate & 0xC0) == 0x40:
            if self.ilstate & 0x03:
                talker_error = (frame & 0x6FF) != (self.talker_frame & 0x6FF)
                if not talker_error and (self.ilstate & 0x02):
                    srq = frame & 0x100
                    if self.ilstate & 0x01:
                        if self.ptssi > 0:
                            self.ptssi -= 1
                            if self.ptssi > 0:
                                frame = (self.status >> ((self.status_len - self.ptssi) * 8)) & 0xFF
                        if self.ptsdi > 0:
                            if self.ptsdi == len(self.did): frame = 0; self.ptsdi = 0
                            else: frame = ord(self.did[self.ptsdi]); self.ptsdi += 1
                        if self.ptssi == 0 and self.ptsdi == 0: frame = 0x540
                    else:
                        frame = self.outdata(frame)
                    frame |= srq
                else:
                    frame = 0x540
            if frame == 0x540:
                if talker_error: frame = 0x541
                self.ilstate &= ~0x03
            self.talker_frame = frame
        if (self.ilstate & 0xC0) == 0x80:
            self.indata(frame)
        return frame

    def do_cmd(self, frame):
        n = frame & 0xFF; t = n >> 5
        if t == 0:
            if n == 4:
                if self.ilstate & 0x80: self.clear_hpil()
            elif n == 20: self.clear_hpil()
        elif t == 1:
            n &= 31
            if n == 31:
                if self.ilstate & 0xA0: self.ilstate &= 0x50
            elif (self.ilstate & 0x80) == 0 and n == (self.addr & 31):
                self.ilstate = 0x80 if (self.addr2nd & 0x80) == 0 else 0x20
        elif t == 2:
            n &= 31
            if n == (self.addr & 31):
                self.ilstate = 0x40 if (self.addr2nd & 0x80) == 0 else 0x10
            elif self.ilstate & 0x50: self.ilstate &= 0xA0
        elif t == 3:
            if self.ilstate & 0x30:
                n &= 31
                if n == (self.addr2nd & 31): self.ilstate <<= 2
                else: self.ilstate = 0
        elif t == 4:
            n &= 31
            if n == 16: self.ilstate = 0
            elif n == 26: self.addr = self.defaddr; self.addr2nd = 0
        else:
            frame = self.cmd_ext(frame)
        return frame

    def do_rdy(self, frame):
        n = frame & 0xFF
        if n <= 127:
            if self.ilstate & 0x40:
                if n == 66: self.ptsdi = 0; self.ptssi = 0; self.ilstate = 0x41
                elif n == 96:
                    frame = self.outdata(frame)
                    if frame != 0x560: self.ilstate = 0x42; self.talker_frame = frame
                elif n == 97:
                    self.status &= ~0x40
                    self.ptssi = self.status_len
                    if self.ptssi > 0:
                        frame = (self.status >> ((self.status_len - self.ptssi) * 8)) & 0xFF
                        self.ilstate = 0x43; self.talker_frame = frame
                elif n == 98:
                    if self.did:
                        frame = ord(self.did[0]); self.ptsdi = 1; self.ilstate = 0x43; self.talker_frame = frame
                elif n == 99:
                    frame = self.aid & 0xFF; self.ilstate = 0x41; self.talker_frame = frame
        else:
            if n < 0x80 + 31:
                if (self.addr & 0x80) == 0 and self.addr2nd == 0:
                    self.addr = n; frame += 1
            elif 0xA0 <= n < 0xA0 + 31:
                if (self.addr & 0x80) == 0 and (self.addr2nd & 0x80) != 0: self.addr = n & 0x9F
            elif 0xC0 <= n < 0xC0 + 31:
                if (self.addr & 0x80) == 0: self.addr2nd = n & 0x9F; frame += 1
        return frame

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    opt = {}
    args = sys.argv[1:]
    for k in ('tracks', 'surfaces', 'blocks'):
        if '--' + k in args: opt[k] = int(args[args.index('--' + k) + 1]); a.remove(str(opt[k]))
    image = a[0]
    out_port = int(a[1]) if len(a) > 1 else 60001
    in_port = int(a[2]) if len(a) > 2 else 60000
    d = Drive(image, log='--log' in args, **opt)
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", out_port)); srv.listen(1)
    print("il_disk: image %s, waiting for emulator on %d" % (image, out_port), flush=True)
    c, _ = srv.accept(); print("emulator connected", flush=True)
    back = None
    while True:
        data = c.recv(2)
        if len(data) < 2: break
        f = struct.unpack(">H", data)[0]
        f0 = f; f = d.process(f)
        if d.log and d.nframes < 80: print('frame %03X -> %03X  st=%02X addr=%02X' % (f0, f, d.ilstate, d.addr), flush=True)
        d.nframes += 1
        while back is None:
            try: back = socket.create_connection(("127.0.0.1", in_port), timeout=2)
            except OSError: time.sleep(0.2)
        back.sendall(struct.pack(">H", (struct.unpack(">H", data)[0] & 0xF800) | (f & 0x7FF)))
    print("il_disk: loop closed")

if __name__ == "__main__":
    main()
