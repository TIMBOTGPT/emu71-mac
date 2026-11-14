# GLOAD of a file that does not exist
name: err_gload_missing

[run]
line: GLOAD "NOFILE"
viddir: yes
expect_log: cmd 92 err 5
