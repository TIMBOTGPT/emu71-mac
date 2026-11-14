# GNEW GPUSH GPOP GROLL GSHOW as in examples/layers.bas, the circle must be shown
name: draw_layers

[run]
line: GRAPH @ SCALE 0,799,0,599
line: GNEW 1 @ GPUSH 1 @ PEN 2 @ BOX 100,300,100,300
line: GPOP @ GCLEAR @ PEN 3 @ CIRCLE 400,300,100
line: GROLL 2 @ GSHOW 1
ppm: yes
expect_nolog: err [1-9]
