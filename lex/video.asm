       LEX    'VIDEO'
*
*      Video mailbox LEX for the Emu71 external video device (phase 1).
*      Statements take numeric arguments, are evaluated by the OS, and are
*      copied as REALs into mailbox slots R0..R3, then the command is written.
*
       ID     #5C
       MSG    0
       POLL   0
*
       ENTRY  ALPHA
       CHAR   #D
       ENTRY  BOX
       CHAR   #D
       ENTRY  CIRCLE
       CHAR   #D
       ENTRY  CLIP
       CHAR   #D
       ENTRY  DRAW
       CHAR   #D
       ENTRY  FILL
       CHAR   #D
       ENTRY  FRAME
       CHAR   #D
       ENTRY  GCLEAR
       CHAR   #D
       ENTRY  GNEW
       CHAR   #D
       ENTRY  GPOP
       CHAR   #D
       ENTRY  GPUSH
       CHAR   #D
       ENTRY  GRAPH
       CHAR   #D
       ENTRY  GRID
       CHAR   #D
       ENTRY  GROLL
       CHAR   #D
       ENTRY  GSHOW
       CHAR   #D
       ENTRY  GTARGET
       CHAR   #D
       ENTRY  MOVE
       CHAR   #D
       ENTRY  PEN
       CHAR   #D
       ENTRY  PLOT
       CHAR   #D
       ENTRY  POLY
       CHAR   #D
       ENTRY  SCALE
       CHAR   #D
       ENTRY  XAXIS
       CHAR   #D
       ENTRY  YAXIS
       CHAR   #D
*
       KEY    'ALPHA'
       TOKEN  1
       KEY    'BOX'
       TOKEN  2
       KEY    'CIRCLE'
       TOKEN  3
       KEY    'CLIP'
       TOKEN  4
       KEY    'DRAW'
       TOKEN  5
       KEY    'FILL'
       TOKEN  6
       KEY    'FRAME'
       TOKEN  7
       KEY    'GCLEAR'
       TOKEN  8
       KEY    'GNEW'
       TOKEN  9
       KEY    'GPOP'
       TOKEN  10
       KEY    'GPUSH'
       TOKEN  11
       KEY    'GRAPH'
       TOKEN  12
       KEY    'GRID'
       TOKEN  13
       KEY    'GROLL'
       TOKEN  14
       KEY    'GSHOW'
       TOKEN  15
       KEY    'GTARGET'
       TOKEN  16
       KEY    'MOVE'
       TOKEN  17
       KEY    'PEN'
       TOKEN  18
       KEY    'PLOT'
       TOKEN  19
       KEY    'POLY'
       TOKEN  20
       KEY    'SCALE'
       TOKEN  21
       KEY    'XAXIS'
       TOKEN  22
       KEY    'YAXIS'
       TOKEN  23
       ENDTXT
*
*      Find the video mailbox (MMIO class 5) in the configuration buffer.
*      Exit: carry set and A(A) = mailbox base address, else carry clear.
*      Buffer ID (2 nibbles), size (3 nibbles, data only), then data.
*      ID FD entries are 10 nibbles: nib 4-6 address field, nib 8 class.
*      Address = 20000 + field * 16.
*
FINDV  SETHEX
       D0=(5) =CONFST
FV10   A=DAT0 B
       ?A=0   B
       GOYES  FVNO
       LCHEX  FD
       ?A=C   B
       GOYES  FV20
       D0=D0+ 2
       C=0    A
       C=DAT0 X
       D0=D0+ 3
       AD0EX
       A=A+C  A
       AD0EX
       GOTO   FV10
FV20   D0=D0+ 2
       C=0    A
       C=DAT0 X
       D0=D0+ 3
       B=C    A
FV30   ?B=0   A
       GOYES  FVNO
       D0=D0+ 8
       P=     0
       A=0    A
       A=DAT0 P
       LCHEX  5
       ?A=C   P
       GOYES  FV40
       D0=D0+ 2
       LC(5)  #A
       B=B-C  A
       GOTO   FV30
FV40   D0=D0- 4
       A=0    A
       A=DAT0 X
       ASL    A
       ASL    A
       ASL    A
       ASL    A
       LC(5)  #20000
       A=A+C  A
       RTNSC
FVNO   RTNCC
*
*      Statement execute entries.  Each loads C = total args, args left, command.
*
       REL(5) NAd
       REL(5) NAp
ALPHA  LC(2)  #03
       GOTO   NOARG
       REL(5) A2d
       REL(5) A2p
DRAW   LC(4)  #2214
       GOTO   XQ
       REL(5) NAd
       REL(5) NAp
GCLEAR LC(2)  #04
       GOTO   NOARG
       REL(5) NAd
       REL(5) NAp
GRAPH  LC(2)  #02
       GOTO   NOARG
       REL(5) A1d
       REL(5) A1p
GSHOW  LC(4)  #1177
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
GTARGET LC(4) #1172
       GOTO   XQ
       REL(5) A2d
       REL(5) A2p
MOVE   LC(4)  #2213
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
PEN    LC(4)  #1105
       GOTO   XQ
       REL(5) A3d
       REL(5) A3p
PLOT   LC(4)  #3317
       GOTO   XQ
       REL(5) A4d
       REL(5) A4p
SCALE  LC(4)  #4410
       GOTO   XQ
       REL(5) A4d
       REL(5) A4p
XAXIS  LC(4)  #4440
       GOTO   XQ
       REL(5) A4d
       REL(5) A4p
YAXIS  LC(4)  #4441
       GOTO   XQ
       REL(5) A4d
       REL(5) A4p
BOX    LC(4)  #4420
       GOTO   XQ
       REL(5) A3d
       REL(5) A3p
CIRCLE LC(4)  #3321
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
CLIP   LC(4)  #1112
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
FILL   LC(4)  #1119
       GOTO   XQ
       REL(5) NAd
       REL(5) NAp
FRAME  LC(2)  #43
       GOTO   NOARG
       REL(5) A4d
       REL(5) A4p
GRID   LC(4)  #4442
       GOTO   XQ
       REL(5) A3d
       REL(5) A3p
POLY   LC(4)  #3322
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
GNEW   LC(4)  #1170
       GOTO   XQ
       REL(5) A1d
       REL(5) A1p
GPUSH  LC(4)  #1173
       GOTO   XQ
       REL(5) NAd
       REL(5) NAp
GPOP   LC(2)  #74
       GOTO   NOARG
       REL(5) A1d
       REL(5) A1p
GROLL  LC(4)  #1176
       GOTO   XQ
*
*      No-argument statement: C(1:0) = command.
*
NOARG  R3=C
       GOSUB  FINDV
       GONC   NOVID
       LC(5)  #7E
       A=A+C  A
       D0=A
       C=R3
       DAT0=C B
       GOTO   CHK
NOVID  LC(4)  #000B
       GOVLNG =BSERR
*
*      Argument statement: C(3)=total, C(2)=args left, C(1:0)=command.
*      First argument through EXPEXC, the rest through EXPR (stack stays put).
*
XQ     SETHEX
       D1=(5) =STMTR0
       DAT1=C A
       GOSBVL =EXPEXC
XQ10   CD1EX
       R1=C
       D1=(5) =STMTR0
       C=DAT1 A
       P=     2
       C=C-1  P
       DAT1=C A
       ?C=0   P
       GOYES  XQ20
       P=     0
       C=R1
       D1=C
       D0=D0+ 2
       GOSBVL =EXPR
       GOTO   XQ10
XQ20   P=     0
       GOSUB  FINDV
       GONC   NOVID
       R2=A
       D1=(5) =STMTR0
       C=DAT1 A
       R3=C
       CSR    A
       CSR    A
       CSR    A
       A=0    A
       A=C    P
       B=A    A
       A=A-1  A
       ASL    A
       C=R2
       A=A+C  A
       D0=A
       C=R1
       D1=C
XQ30   A=DAT1 W
       DAT0=A W
       D1=D1+ 16
       D0=D0- 16
       B=B-1  A
       ?B#0   A
       GOYES  XQ30
       A=R2
       LC(5)  #7E
       A=A+C  A
       D0=A
       C=R3
       DAT0=C B
       GOTO   CHK
*
*      Read the device result code (mailbox ERRC, nibble #61) after a command.
*      Entry: A(A) = mailbox base + #7E (the command address just written).
*      0 = ok.  2 (no such layer) -> Insufficient Memory (#0018), anything else -> Invalid Arg (#000B).
*
CHK    P=     0
       LC(5)  #1D
       A=A-C  A
       D0=A
       A=0    A
       A=DAT0 P
       ?A=0   P
       GOYES  CK90
       LC(1)  2
       ?A=C   P
       GOYES  CKMEM
       LC(4)  #000B
       GOVLNG =BSERR
CKMEM  LC(4)  #0018
       GOVLNG =BSERR
CK90   GOVLNG =NXTSTM
*
*      Parse routines: n numeric arguments separated by commas.
*
NAp    GOSBVL =EOLCK
       GOC    PEND
PERR   LC(4)  #004B
       ST=1   4
       CD0EX
       D0=D0- 1
       GOVLNG =PARERR
A1p    P=     0
       LC(1)  1
       GOTO   PARN
A2p    P=     0
       LC(1)  2
       GOTO   PARN
A3p    P=     0
       LC(1)  3
       GOTO   PARN
A4p    P=     0
       LC(1)  4
PARN   R2=C
PN10   GOSBVL =NUMCK
       P=     0
       C=R2
       C=C-1  P
       R2=C
       ?C=0   P
       GOYES  PN20
       GOSBVL =COMCK+
       GOC    PN10
       GOTO   PERR
PN20   GOSBVL =EOLCKR
       GONC   PERR
PEND   GOVLNG =RESPTR
*
*      Decompile routines.
*
NAd    GOVLNG =OUTEL1
A1d
A2d
A3d
A4d    GOVLNG =FIXDC
       END
