# COPY VIDEO:TAPE TO VIDEO installs the LEX, before that PEN is an unknown keyword
name: lex_install_lif

[run]
lex: none
disk: lex
slow: yes
line: PEN 3
line: RESTORE IO
line: ~40
line: COPY VIDEO:TAPE TO VIDEO
pause: 90000
line: PEN 4
line: GRAPH
line: CIRCLE 10,20,30
expect_nolog: cmd 05 err 0 R0..R3 = 3 
expect_log: cmd 05 err 0 R0..R3 = 4 
expect_log: cmd 02
expect_log: cmd 21 err 0 R0..R3 = 10 20 30
