# VREAD beyond the layer
name: err_vread_range

[run]
line: DISP VREAD(500000)
expect_log: cmd 84 err 3
