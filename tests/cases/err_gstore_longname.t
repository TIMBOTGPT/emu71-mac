# GSTORE with a name over 8 characters
name: err_gstore_longname

[run]
line: GSTORE "TOOLONGNAME"
viddir: yes
expect_log: cmd 91 err 3 .*len 11
