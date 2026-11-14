# BOX CIRCLE POLY FILL GRID FRAME as in examples/shapes.bas
name: draw_shapes

[run]
line: GRAPH @ SCALE 0,799,0,599
line: GCLEAR @ PEN 8 @ GRID 0,0,100,100
line: PEN 2 @ BOX 50,250,50,200 @ FILL 1
line: PEN 3 @ BOX 300,500,50,200 @ PEN 4
line: CIRCLE 650,125,60 @ FILL 0 @ PEN 5 @ CIRCLE 650,400,60
line: PEN 6 @ POLY 100,300,0 @ POLY 250,300,1 @ POLY 175,450,2
line: FILL 1 @ PEN 7 @ POLY 300,300,0 @ POLY 500,300,1 @ POLY 400,450,1 @ POLY 350,380,2
line: FILL 0 @ PEN 1 @ FRAME
ppm: yes
expect_nolog: err [1-9]
