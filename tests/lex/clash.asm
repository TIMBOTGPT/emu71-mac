       LEX    'CLASH'
*      Test LEX with a keyword that clashes with the video LEX (PEN) and one that does not (ZZAP).
       ID     #5D
       MSG    0
       POLL   0
*
       ENTRY  PEN
       CHAR   #D
       ENTRY  ZZAP
       CHAR   #D
*
       KEY    'PEN'
       TOKEN  1
       KEY    'ZZAP'
       TOKEN  2
       ENDTXT
*
       REL(5) A1d
       REL(5) A1p
PEN    GOVLNG =NXTSTM
       REL(5) NAd
       REL(5) NAp
ZZAP   LC(4)  #000B
       GOVLNG =BSERR
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
