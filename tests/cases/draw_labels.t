# GLABEL with strings of several 8 character chunks as in examples/labels.bas
name: draw_labels

[run]
line: GRAPH @ SCALE 0,799,0,599
line: GCLEAR @ PEN 1 @ MOVE 50,500 @ GLABEL "HELLO WORLD"
line: PEN 3 @ MOVE 50,450 @ GLABEL "THE QUICK BROWN FOX 0123456789"
line: PEN 5 @ MOVE 50,400 @ GLABEL "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
line: PEN 6 @ MOVE 50,350 @ GLABEL "()*+,-./:;=@"
ppm: yes
expect_log: text \[HELLO WO\]
expect_log: text \[RLD\]
expect_nolog: err [1-9]
