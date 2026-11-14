# GSTORE then GCLEAR then GLOAD brings the circle back (examples/store.bas)
name: store_load

[run]
line: GRAPH @ SCALE 0,799,0,599
line: GCLEAR @ PEN 2 @ CIRCLE 400,300,150
line: GSTORE "CIRC1"
line: GCLEAR
line: GLOAD "CIRC1"
viddir: yes
ppm: yes
expect_log: cmd 91 err 0
expect_log: cmd 92 err 0
expect_nolog: err [1-9]
