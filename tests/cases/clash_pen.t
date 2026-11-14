# PEN exists in both LEX files, the first mounted (lower port) wins, ZZAP of the second still works
name: clash_pen

[run]
lex: video,clash
line: PEN 3
line: GRAPH
line: ZZAP
expect_log: cmd 05 err 0 R0..R3 = 3 
expect_log: cmd 02

[run]
lex: clash,video
line: PEN 3
line: GRAPH
line: ZZAP
expect_nolog: cmd 05
expect_log: cmd 02
