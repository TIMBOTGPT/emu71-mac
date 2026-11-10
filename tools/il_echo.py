#!/usr/bin/env python3
"""Minimal HP-IL loop partner: an empty loop that returns every frame unchanged.
Emu71 connects to OUT port (listen here) and expects frames back on its IN port."""
import socket, struct, sys, threading, time
OUT, IN = (int(sys.argv[1]) if len(sys.argv) > 1 else 60001), (int(sys.argv[2]) if len(sys.argv) > 2 else 60000)
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", OUT)); srv.listen(1)
c, _ = srv.accept(); print("emu connected", flush=True)
back = None
n = 0
while True:
    d = c.recv(2)
    if len(d) < 2: break
    f = struct.unpack(">H", d)[0]; n += 1
    if n <= 20: print("frame %03X" % f, flush=True)
    while back is None:
        try: back = socket.create_connection(("127.0.0.1", IN), timeout=2)
        except OSError: time.sleep(0.2)
    back.sendall(d)
print("frames:", n)
