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
       ENTRY  GBLIT
       CHAR   #D
       ENTRY  GCLEAR
       CHAR   #D
       ENTRY  GLABEL
       CHAR   #D
       ENTRY  GLOAD
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
       ENTRY  GSTORE
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
       ENTRY  VREAD
       CHAR   #F
       ENTRY  VWRITE
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
       KEY    'GBLIT'
       TOKEN  8
       KEY    'GCLEAR'
       TOKEN  9
       KEY    'GLABEL'
       TOKEN  10
       KEY    'GLOAD'
       TOKEN  11
       KEY    'GNEW'
       TOKEN  12
       KEY    'GPOP'
       TOKEN  13
       KEY    'GPUSH'
       TOKEN  14
       KEY    'GRAPH'
       TOKEN  15
       KEY    'GRID'
       TOKEN  16
       KEY    'GROLL'
       TOKEN  17
       KEY    'GSHOW'
       TOKEN  18
       KEY    'GSTORE'
       TOKEN  19
       KEY    'GTARGET'
       TOKEN  20
       KEY    'MOVE'
       TOKEN  21
       KEY    'PEN'
       TOKEN  22
       KEY    'PLOT'
       TOKEN  23
       KEY    'POLY'
       TOKEN  24
       KEY    'SCALE'
       TOKEN  25
       KEY    'VREAD'
       TOKEN  26
       KEY    'VWRITE'
       TOKEN  27
       KEY    'XAXIS'
       TOKEN  28
       KEY    'YAXIS'
       TOKEN  29
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
       REL(5) A3d
       REL(5) A3p
VWRITE LC(4)  #3383
       GOTO   XQ
       REL(5) A4d
       REL(5) A4p
GBLIT  LC(4)  #4490
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
*      String-argument statements.  The OS evaluates the string (EXPEXC), POP1S leaves A(A) = length in
*      nibbles and D1 = first character.  Text goes into the mailbox DATA field, 8 characters per command.
*
SPd    GOVLNG =FIXDC
SPp    P=     0
       GOVLNG =STRNGP
*
*      GLABEL string: command #34 starts the label at the pen position, then #30 for each chunk of 8 characters.
*
       REL(5) SPd
       REL(5) SPp
GLABEL SETHEX
       GOSBVL =EXPEXC
       GOSBVL =REVPOP
       R3=A
       CD1EX
       R2=C
       GOSUB  FINDV
       GOC    FVK0
       LC(4)  #000B
       GOVLNG =BSERR
FVK0
       R4=A
       LC(2)  #34
       GOSUB  SENDR4
GL10   C=R3
       ?C=0   A
       GOYES  GL90
       A=C    A
       LC(5)  16
       ?A<=C  A
       GOYES  GL20
       A=C    A
GL20   B=A    A
       A=R4
       LC(5)  #50
       A=A+C  A
       D0=A
       C=R2
       D1=C
       A=DAT1 W
       DAT0=A W
       A=R4
       LC(5)  #64
       A=A+C  A
       D0=A
       C=0    W
       C=B    A
       CSRB
       DAT0=C B
       LC(2)  #30
       GOSUB  SENDR4
       C=R2
       C=C+B  A
       R2=C
       C=R3
       C=C-B  A
       R3=C
       GOTO   GL10
GL90   GOVLNG =NXTSTM
*
*      GSTORE name / GLOAD name: file name (up to 8 characters) in DATA, length in LEN (the host rejects > 8).
*
       REL(5) SPd
       REL(5) SPp
GSTORE LC(2)  #91
       GOTO   FILEQ
       REL(5) SPd
       REL(5) SPp
GLOAD  LC(2)  #92
FILEQ  SETHEX
       D1=(5) =STMTR0
       DAT1=C A
       GOSBVL =EXPEXC
       GOSBVL =REVPOP
       R3=A
       CD1EX
       R2=C
       GOSUB  FINDV
       GOC    FVK1
       LC(4)  #000B
       GOVLNG =BSERR
FVK1
       R4=A
       LC(5)  #50
       A=A+C  A
       D0=A
       C=R2
       D1=C
       A=DAT1 W
       DAT0=A W
       A=R4
       LC(5)  #64
       A=A+C  A
       D0=A
       A=R3
       C=0    W
       C=A    A
       CSRB
       A=C    A
       LC(5)  255
       ?A<=C  A
       GOYES  FQ10
       A=C    A
FQ10   C=A    A
       DAT0=C B
       D1=(5) =STMTR0
       C=DAT1 A
       GOSUB  SENDR4
       GOVLNG =NXTSTM
*
*      VREAD(p): numeric function.  The REAL argument goes to mailbox R0, command #84 returns the pixel in R0.
*
       NIBHEX 8
       NIBHEX 11
VREAD  GOSBVL =POP1N
       SETHEX
       R3=A
       CD0EX
       R1=C
       CD1EX
       R2=C
       GOSUB  FINDV
       GOC    FVK2
       LC(4)  #000B
       GOVLNG =BSERR
FVK2
       R4=A
       D0=A
       C=R3
       DAT0=C W
       LC(2)  #84
       GOSUB  SENDR4
       C=R4
       D0=C
       C=DAT0 W
       A=R1
       D0=A
       A=R2
       D1=A
       GOVLNG =FNRTN4
*
*      Write command C(1:0) to the mailbox whose base is in R4, then check the device result code.
*
SENDR4 P=     0
       D=C    A
       A=R4
       LC(5)  #7E
       A=A+C  A
       D0=A
       C=D    A
       DAT0=C B
ERRCK  A=R4
       LC(5)  #61
       A=A+C  A
       D0=A
       A=0    A
       A=DAT0 P
       ?A=0   P
       RTNYES
       LC(1)  2
       ?A=C   P
       GOYES  CKMEM
       LC(4)  #000B
       GOVLNG =BSERR
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
