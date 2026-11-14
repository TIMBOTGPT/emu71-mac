# VWRITE, GBLIT and VREAD() as in examples/pixels.bas, the 71 must display 3
name: draw_pixels

[run]
line: GRAPH @ SCALE 0,799,0,599
line: GCLEAR @ VWRITE 0,800,2 @ VWRITE 240000,800,3
line: GNEW 1 @ GTARGET 1 @ PEN 4 @ FILL 1 @ BOX 100,200,100,200
line: GTARGET 0 @ GBLIT 1,300,100,1
line: DISP VREAD(240005)
ppm: yes
expect_log: cmd 84 err 0 R0..R3 = 3 
expect_nolog: err [1-9]
