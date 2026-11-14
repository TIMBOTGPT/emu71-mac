# program with LEX statements and a variable survive a state save and restore
name: state_restore

[run]
state: yes
line: 10 GRAPH @ SCALE 0,799,0,599 @ CIRCLE 400,300,100
line: X=42

[run]
state: yes
ppm: yes
idle: 8
line: RUN
expect_log: cmd 02
expect_log: cmd 10
expect_log: cmd 21 err 0 R0..R3 = 400 300 100

[run]
state: yes
line: DISP X

[run]
state: yes
line: LIST
