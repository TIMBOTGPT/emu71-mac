# LIST of a numeric function inside an expression
name: list_vread

[run]
line: 10 X=VREAD(5)+1
line: LIST
expect_nolog: cmd
