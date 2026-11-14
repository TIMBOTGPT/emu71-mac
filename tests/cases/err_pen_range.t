# PEN 300 is out of range: ERR:Invalid Arg
name: err_pen_range

[run]
line: PEN 300
expect_log: cmd 05 err 3
