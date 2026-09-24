                                    ;************************************
                                    ;* 8255I.COM                        *
                                    ;************************************
                                    ;* Mode 0, Port A output, Port C    *
                                    ;* upper output; Group B is Mode 1  *
                                    ;* (Port B strobed input) so the    *
                                    ;* 82C55 itself raises INTR B on    *
                                    ;* PC0 - fed to the PIC via the XIO *
                                    ;* STANDARD handler (INTFLAGS       *
                                    ;* bitmask, no custom ISR, no       *
                                    ;* XIO SET ADDR). PC4 is toggled    *
                                    ;* via PPI SET/RESET each loop      *
                                    ;* pass - wire it to PC2 (STB B) to *
                                    ;* trigger the interrupt. Discovers *
                                    ;* both RCX_INTERFACE and           *
                                    ;* XIO_IO_EXPANDER separately and   *
                                    ;* switches via CALLFN (RCX) /      *
                                    ;* CALLFNXIO (XIO).                 *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 13/09/26                *
                                    ;************************************

              ORG   #0100

;---------------------------

                                    ;************************************
                                    ;* MSX-DOS constants                *
                                    ;************************************

BDOS:         EQU   #00005
EXTBIO:       EQU   #0FFCA
CALSLT:       EQU   #0001C
ARG:          EQU   #0F847
LINLEN:       EQU   #0F3B0         ; current active screen text width

CMDTAIL:      EQU   #0080         ; MSX-DOS command tail: length byte,
                                    ; followed by the raw text at
                                    ; CMDTAIL+1

;---------------------------

                                    ;************************************
                                    ;* UNAPI function numbers           *
                                    ;* (position in FN_TABLE, see       *
                                    ;* RCXROM.ASM / XIOROM.ASM -        *
                                    ;* re-check whenever either ROM     *
                                    ;* changes)                         *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT

PPI_FN_INIT:    EQU  23            ; PPI INIT
PPI_FN_ARD:     EQU  24            ; PPI A RD
PPI_FN_AWR:     EQU  25            ; PPI A WR
PPI_FN_BRD:     EQU  26            ; PPI B RD
PPI_FN_CTRL:    EQU  30            ; PPI CTRL
PPI_FN_SET:     EQU  31            ; PPI SET
PPI_FN_RESET:   EQU  32            ; PPI RESET

                                    ; PPI 24-32 all dispatch through a
                                    ; single shared FN_COREPPI in the
                                    ; ROM (FN_CORE24..32 are all just
                                    ; "JP FN_COREPPI") - the actual
                                    ; operation is selected by B, not
                                    ; by A. Still using the "correct"
                                    ; per-operation A above.

PPI_MODE_ARD:     EQU  0
PPI_MODE_AWR:     EQU  1
PPI_MODE_BRD:     EQU  2
PPI_MODE_CTRL:    EQU  6
PPI_MODE_SET:     EQU  7
PPI_MODE_RESET:   EQU  8

XIO_FN_INIT:    EQU  4             ; XIO INIT
XIO_FN_ISINIT:  EQU  5             ; XIO ISINIT
XIO_FN_DEINIT:  EQU  6             ; XIO DEINIT
XIO_FN_SETMASK: EQU  8             ; XIO SET MASK

UNAPI_DISC_COUNT: EQU 0            ; EXTBIO discovery: count implementations
UNAPI_DISC_ENTRY: EQU 1            ; EXTBIO discovery: slot/address

;---------------------------

                                    ;************************************
                                    ;* Screen width: reads the currently *
                                    ;* active column count (LINLEN),     *
                                    ;* never changes it - 40-column      *
                                    ;* banner below the 41-column        *
                                    ;* threshold, 80-column banner at    *
                                    ;* or above it                       *
                                    ;************************************

START:        LD    A,(LINLEN)
              CP    41
              LD    A,40
              JR    C,SETWIDTH
              LD    A,80

SETWIDTH:     LD    (SCRCOLS),A

              LD    A,(SCRCOLS)
              CP    80
              LD    DE,MSTART80
              JR    Z,PRINTSTART
              LD    DE,MSTART40
PRINTSTART:   CALL  PRSTR

              CALL  PARSEIO
              JP    C,PARAMERR
              CALL  PARSEINT
              JP    C,PARAMERR

                                    ; Locate the RAM helper, if any -
                                    ; shared by both UNAPI dispatch
                                    ; paths below

              LD    DE,#02222
              LD    HL,0
              LD    A,#0FF
              CALL  EXTBIO
              LD    (HELPERADD),HL

                                    ; Discover RCX_INTERFACE
                                    ; (A=0: count)

              LD    HL,IDSTR
              LD    DE,ARG
              LD    BC,IDSTRLEN
              LDIR

              LD    A,UNAPI_DISC_COUNT
              LD    B,0
              LD    DE,#02222
              CALL  EXTBIO
              LD    A,B
              OR    A
              JP    Z,NOTFOUND

                                    ; Discover RCX_INTERFACE
                                    ; (A=1: slot/segment/entry)

              LD    A,UNAPI_DISC_ENTRY
              LD    DE,#02222
              CALL  EXTBIO
              LD    (SLOT),A
              LD    A,B
              LD    (SEG),A
              LD    (ENTRY),HL

              LD    DE,MFOUND
              CALL  PRSTR
              LD    A,(SLOT)
              CALL  PRHEX2
              LD    DE,MCRLF
              CALL  PRSTR

                                    ; Discover XIO_IO_EXPANDER - a
                                    ; SEPARATE UNAPI, own slot/segment/
                                    ; entry, own discovery pass. ARG is
                                    ; reused (sequential, not
                                    ; concurrent with the RCX pass
                                    ; above).

              LD    HL,XIDSTR
              LD    DE,ARG
              LD    BC,XIDSTRLEN
              LDIR

              LD    A,UNAPI_DISC_COUNT
              LD    B,0
              LD    DE,#02222
              CALL  EXTBIO
              LD    A,B
              OR    A
              JP    Z,XNOTFOUND

              LD    A,UNAPI_DISC_ENTRY
              LD    DE,#02222
              CALL  EXTBIO
              LD    (XSLOT),A
              LD    A,B
              LD    (XSEG),A
              LD    (XENTRY),HL

              LD    DE,MXFOUND
              CALL  PRSTR
              LD    A,(XSLOT)
              CALL  PRHEX2
              LD    DE,MCRLF
              CALL  PRSTR

                                    ;************************************
                                    ;* Setup: XIO ISINIT/DEINIT/INIT     *
                                    ;************************************
                                    ; XIO's ISINIT writes its flag at
                                    ; (DE+1), not (DE) - unlike RCX's
                                    ; ISINIT which writes at (DE).

              LD    DE,XISIBUF
              LD    A,XIO_FN_ISINIT
              CALL  CALLFNXIO
              LD    A,(XISIBUF+1)
              OR    A
              JR    Z,XSETUP_INIT

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

XSETUP_INIT:  LD    HL,XWORKAREA
              LD    DE,INTFLAGS
              LD    A,XIO_FN_INIT
              CALL  CALLFNXIO
              JP    C,XINITERR

                                    ;************************************
                                    ;* Setup: RCX ISINIT/DEINIT/INIT     *
                                    ;************************************

              LD    DE,ISIBUF
              LD    A,RCX_FN_ISINIT
              CALL  CALLFN
              LD    A,(ISIBUF)
              CP    1
              JR    NZ,SETUP_INIT

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

SETUP_INIT:   LD    HL,RCXWORKAREA
              LD    A,RCX_FN_INIT
              CALL  CALLFN
              JP    C,INITERR

;---------------------------

                                    ;************************************
                                    ;* PPI setup                         *
                                    ;************************************

                                    ; _PPI INIT(Q)

              LD    A,(PIOADDR)
              LD    C,A
              LD    A,PPI_FN_INIT
              CALL  CALLFN

                                    ; _PPI CTRL(Q,&H87) - Group A Mode 0
                                    ; (Port A out, PC-hi out), Group B
                                    ; Mode 1 (Port B in, INTR B on PC0)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_CTRL
              LD    D,#087
              LD    A,PPI_FN_CTRL
              CALL  CALLFN

                                    ; _PPI SET(Q,2) - enable INTE B
                                    ; (BSR on PC2)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_SET
              LD    D,2
              LD    A,PPI_FN_SET
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Activate the PIC channel only    *
                                    ;* now - AFTER the PPI setup is     *
                                    ;* fully done, not before: an early *
                                    ;* interrupt could otherwise land   *
                                    ;* mid-setup.                       *
                                    ;************************************

              LD    A,(MASKVAL)
              LD    E,A
              LD    D,0
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO
              JR    C,MSETMASKERR

                                    ; CLS : LOCATE 0,0 : PRINT
                                    ; "<SPACE> ends..."

              LD    A,12             ; CLS (Ctrl-L)
              CALL  PRCHR
              LD    B,0
              LD    C,0
              CALL  PRLOC
              LD    DE,MWAIT
              CALL  PRSTR

              XOR   A
              LD    (OUTBYTE),A

              JP    MAINLOOP

MSETMASKERR:  LD    DE,MMASKERR
              CALL  PRSTR

                                    ;************************************
                                    ;* Main loop                         *
                                    ;************************************

MAINLOOP:                          ; _PPI A WR(Q,OUTBYTE)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_AWR
              LD    A,(OUTBYTE)
              LD    D,A
              LD    A,PPI_FN_AWR
              CALL  CALLFN

                                    ; _PPI A RD(Q,AR)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_ARD
              LD    DE,ARBUF
              LD    A,PPI_FN_ARD
              CALL  CALLFN

                                    ; Toggle PC4 - wire to PC2 (STB B)
                                    ; to trigger

              LD    A,(OUTBYTE)
              AND   1
              JR    NZ,DORESET

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_SET
              LD    D,4
              LD    A,PPI_FN_SET
              CALL  CALLFN
              JR    TOGGLEDONE

DORESET:      LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_RESET
              LD    D,4
              LD    A,PPI_FN_RESET
              CALL  CALLFN

TOGGLEDONE:                        ; LOCATE 0,2 : PRINT "OUT: ";
                                    ; HEX$(J);" A-RD: ";HEX$(AR);"    "

              LD    B,2
              LD    C,0
              CALL  PRLOC
              LD    DE,MOUT
              CALL  PRSTR
              LD    A,(OUTBYTE)
              CALL  PRHEXV
              LD    DE,MARD
              CALL  PRSTR
              LD    HL,(ARBUF)
              CALL  PRHEXVW
              LD    DE,MPAD4
              CALL  PRSTR

                                    ; Test and clear the interrupt bit
                                    ; atomically (DI/EI)

              DI
              LD    A,(INTFLAGS)
              LD    B,A
              LD    A,(MASKVAL)
              AND   B
              JR    Z,MAINLOOP_NOINT

              LD    A,(MASKVAL)
              CPL
              LD    B,A
              LD    A,(INTFLAGS)
              AND   B
              LD    (INTFLAGS),A
              EI
              CALL  REFRESH
              JR    MAINLOOP_KEY

MAINLOOP_NOINT: EI

MAINLOOP_KEY:                      ; J = J + 1 : IF J = 256 THEN J = 0

              LD    A,(OUTBYTE)
              INC   A
              LD    (OUTBYTE),A

                                    ; Non-blocking key poll: loop back
                                    ; to MAINLOOP unless a key is
                                    ; waiting

              LD    C,#0B          ; CONST - console status
              CALL  BDOS
              OR    A
              JP    Z,MAINLOOP

              LD    C,1            ; CONIN - read the waiting char
              CALL  BDOS
              CP    ' '
              JP    NZ,MAINLOOP

;---------------------------

                                    ;************************************
                                    ;* Cleanup                           *
                                    ;************************************

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_RESET
              LD    D,2
              LD    A,PPI_FN_RESET
              CALL  CALLFN

              XOR   A
              LD    E,A
              LD    D,A
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

              LD    B,5
              LD    C,0
              CALL  PRLOC
              LD    DE,MDONE
              CALL  PRSTR
              RET

PARAMERR:     LD    DE,MUSAGE
              CALL  PRSTR
              RET

NOTFOUND:     LD    DE,MNOTFOUND
              CALL  PRSTR
              RET

INITERR:      LD    DE,MINITERR
              CALL  PRSTR
              RET

XNOTFOUND:    LD    DE,MXNOTFOUND
              CALL  PRSTR
              RET

XINITERR:     LD    DE,MXINITERR
              CALL  PRSTR
              RET

;---------------------------

                                    ;************************************
                                    ;* REFRESH - runs on interrupt:      *
                                    ;* _PPI B RD (also clears INTR B/    *
                                    ;* IBF B in the 82C55 itself),       *
                                    ;* ICOUNT + 1, then LOCATE 0,3 and   *
                                    ;* print the message + count         *
                                    ;************************************

REFRESH:      LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_BRD
              LD    DE,BRBUF
              LD    A,PPI_FN_BRD
              CALL  CALLFN

              LD    HL,(ICOUNT)
              INC   HL
              LD    (ICOUNT),HL

              LD    B,3
              LD    C,0
              CALL  PRLOC
              LD    DE,MFIRED
              CALL  PRSTR
              LD    HL,(BRBUF)
              CALL  PRHEXVW
              LD    DE,MCOUNT
              CALL  PRSTR
              LD    HL,(ICOUNT)
              CALL  PRDECW
              LD    DE,MPAD3
              CALL  PRSTR
              RET

;---------------------------

                                    ;************************************
                                    ;* CALLFN - calls RCX's FN_CORExx.  *
                                    ;* Segment-aware dispatch: SEG=FFh  *
                                    ;* (ROM) uses CALSLT, SEG<>FFh      *
                                    ;* (mapped RAM) jumps through the   *
                                    ;* RAM helper instead.              *
                                    ;************************************
                                    ;* INPUT: A=function number, B/C/D/E*
                                    ;* preset as documented by the      *
                                    ;* target function                  *
                                    ;************************************

CALLFN:       PUSH  AF
              PUSH  HL

              LD    HL,(ENTRY)
              PUSH  HL
              POP   IX

              LD    A,(SEG)
              CP    #0FF
              JR    Z,CALLFN_ROM

              LD    A,(SLOT)
              LD    H,A
              LD    A,(SEG)
              LD    L,A
              PUSH  HL
              POP   IY

              POP   HL
              POP   AF

              PUSH  HL
              LD    HL,(HELPERADD)
              EX    (SP),HL
              RET

CALLFN_ROM:   LD    A,(SLOT)
              LD    H,A
              LD    L,0
              PUSH  HL
              POP   IY

              POP   HL
              POP   AF

              CALL  CALSLT
              RET

                                    ;************************************
                                    ;* CALLFNXIO - calls XIO's           *
                                    ;* FN_CORExx. Identical dispatch to  *
                                    ;* CALLFN, but against XSLOT/XSEG/   *
                                    ;* XENTRY (XIO_IO_EXPANDER's OWN     *
                                    ;* discovery result) instead of      *
                                    ;* SLOT/SEG/ENTRY (RCX_INTERFACE) -  *
                                    ;* two independent UNAPI drivers,    *
                                    ;* each with its own dispatch        *
                                    ;* target.                           *
                                    ;************************************
                                    ;* INPUT: A=function number, B/C/D/E*
                                    ;* preset as documented by the      *
                                    ;* target function                  *
                                    ;************************************

CALLFNXIO:    PUSH  AF
              PUSH  HL

              LD    HL,(XENTRY)
              PUSH  HL
              POP   IX

              LD    A,(XSEG)
              CP    #0FF
              JR    Z,CALLFNXIO_ROM

              LD    A,(XSLOT)
              LD    H,A
              LD    A,(XSEG)
              LD    L,A
              PUSH  HL
              POP   IY

              POP   HL
              POP   AF

              PUSH  HL
              LD    HL,(HELPERADD)
              EX    (SP),HL
              RET

CALLFNXIO_ROM: LD   A,(XSLOT)
              LD    H,A
              LD    L,0
              PUSH  HL
              POP   IY

              POP   HL
              POP   AF

              CALL  CALSLT
              RET

;---------------------------

                                    ;************************************
                                    ;* Print helpers                    *
                                    ;************************************

PRSTR:        PUSH  BC
              PUSH  AF
              PUSH  HL
              LD    C,9
              CALL  BDOS
              POP   HL
              POP   AF
              POP   BC
              RET

PRCHR:        PUSH  BC
              PUSH  DE
              PUSH  HL
              LD    E,A
              LD    C,2
              CALL  BDOS
              POP   HL
              POP   DE
              POP   BC
              RET

PRHEX2:       PUSH  AF
              RRCA
              RRCA
              RRCA
              RRCA
              CALL  PRNIB
              POP   AF
              CALL  PRNIB
              RET

PRNIB:        PUSH  AF
              AND   #0F
              CP    #0A
              JR    C,PRNIBD
              ADD   A,7
PRNIBD:       ADD   A,'0'
              CALL  PRCHR
              POP   AF
              RET

                                    ;************************************
                                    ;* PRHEXV - byte hex, no leading    *
                                    ;* zero (matches plain HEX$())      *
                                    ;************************************

PRHEXV:       PUSH  AF
              RRCA
              RRCA
              RRCA
              RRCA
              AND   #0F
              JR    Z,PHV_SKIP
              CALL  PRNIB2
PHV_SKIP:     POP   AF
              CALL  PRNIB2
              RET

PRNIB2:       PUSH  AF
              AND   #0F
              CP    #0A
              JR    C,PN2_D
              ADD   A,7
PN2_D:        ADD   A,'0'
              CALL  PRCHR
              POP   AF
              RET

                                    ;************************************
                                    ;* PRHEXVW - word hex, no leading   *
                                    ;* zero (matches plain HEX$() on a  *
                                    ;* 16-bit BASIC integer - 1..4      *
                                    ;* digits, e.g. HEX$(256)="100",    *
                                    ;* 3 digits, not 4)                 *
                                    ;************************************
                                    ;* INPUT: HL = word                 *
                                    ;************************************

PRHEXVW:      LD    A,H
              OR    A
              JR    NZ,PHVW_HNZ

                                    ; H=0: just the low byte, still
                                    ; skipping its own leading zero

              LD    A,L
              JP    PRHEXV

PHVW_HNZ:                          ; H<>0: print H with leading-zero
                                    ; skip, then L as a FULL 2 digits
                                    ; (no longer the leading position)

              LD    A,H
              CALL  PRHEXV
              LD    A,L
              JP    PRHEX2

                                    ;************************************
                                    ;* PRDECW - word decimal, matches   *
                                    ;* plain BASIC PRINT of a positive  *
                                    ;* integer: one leading space       *
                                    ;* (reserved sign column), then the *
                                    ;* digits with no leading zeros     *
                                    ;* (IC only ever counts up from 0,  *
                                    ;* so always non-negative here)     *
                                    ;************************************
                                    ;* INPUT: HL = word (0..32767)      *
                                    ;************************************

PRDECW:       LD    A,' '
              CALL  PRCHR
              XOR   A
              LD    (PRDECW_ST),A

              LD    BC,-10000
              CALL  PRDECW_DGT
              LD    BC,-1000
              CALL  PRDECW_DGT
              LD    BC,-100
              CALL  PRDECW_DGT
              LD    BC,-10
              CALL  PRDECW_DGT

              LD    A,L
              ADD   A,'0'
              JP    PRCHR

                                    ; PRDECW_DGT - counts how many
                                    ; times BC (negative) can be added
                                    ; to HL before it would go
                                    ; negative, prints that count as a
                                    ; digit unless it's a suppressed
                                    ; leading zero

PRDECW_DGT:   LD    A,'0'-1
PRDECW_DGT_LP: INC  A
              ADD   HL,BC
              JR    C,PRDECW_DGT_LP
              SBC   HL,BC

              CP    '0'
              JR    NZ,PRDECW_DGT_PR
              LD    A,(PRDECW_ST)
              OR    A
              RET   Z             ; suppress this leading zero

              LD    A,'0'

PRDECW_DGT_PR: PUSH AF
              LD    A,1
              LD    (PRDECW_ST),A
              POP   AF
              JP    PRCHR

                                    ;************************************
                                    ;* PRLOC - positions the cursor via  *
                                    ;* the MSX-DOS console's "ESC Y"     *
                                    ;* code (row/col + 32, both 0-based, *
                                    ;* row 0/col 0 is top-left)          *
                                    ;************************************
                                    ;* INPUT: B=row, C=col               *
                                    ;************************************

PRLOC:        PUSH  AF
              LD    A,27
              CALL  PRCHR
              LD    A,'Y'
              CALL  PRCHR
              LD    A,B
              ADD   A,32
              CALL  PRCHR
              LD    A,C
              ADD   A,32
              CALL  PRCHR
              POP   AF
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..252, stores into PIOADDR       *
                                    ;************************************
                                    ;* OUTPUT: (PIOADDR) = 0..252,       *
                                    ;* Carry=1 if missing/invalid        *
                                    ;************************************

PARSEIO:      LD    HL,PIOTOK
              CALL  FINDTOK
              RET   C

              PUSH  HL
              POP   IX
              CALL  PARSEDEC
              RET   C

              LD    A,L
              CP    253             ; 0..252 valid
              JR    C,PARSEIO_OK

              SCF
              RET

PARSEIO_OK:   LD    (PIOADDR),A
              OR    A
              RET

                                    ;************************************
                                    ;* PARSEINT - finds "/INT:" in the   *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..7, stores INTCHAN and          *
                                    ;* precomputes MASKVAL = 2^INTCHAN   *
                                    ;************************************
                                    ;* OUTPUT: (INTCHAN) = 0..7,         *
                                    ;* (MASKVAL) = 1,2,4,...,128,        *
                                    ;* Carry=1 if missing/invalid        *
                                    ;************************************

PARSEINT:     LD    HL,PINTTOK
              CALL  FINDTOK
              RET   C

              PUSH  HL
              POP   IX
              CALL  PARSEDEC
              RET   C

              LD    A,L
              CP    8
              JR    C,PARSEINT_OK   ; 0..7 valid

              SCF
              RET

PARSEINT_OK:  LD    (INTCHAN),A

              LD    HL,MASKTAB
              LD    D,0
              LD    E,A
              ADD   HL,DE
              LD    A,(HL)
              LD    (MASKVAL),A
              OR    A
              RET

                                    ;************************************
                                    ;* FINDTOK - case-insensitive search *
                                    ;* (letters only - '/' and ':' match *
                                    ;* literally) for a 0-terminated     *
                                    ;* needle inside the MSX-DOS command *
                                    ;* tail (CMDTAIL, length-prefixed)   *
                                    ;************************************
                                    ;* INPUT: HL = needle (0-terminated) *
                                    ;* OUTPUT: HL = ptr just past the    *
                                    ;* match in the command tail;        *
                                    ;* Carry=1 if not found              *
                                    ;************************************

FINDTOK:      LD    (FINDTOK_NDL),HL

              LD    A,(CMDTAIL)
              LD    C,A             ; C = remaining length to scan
              LD    DE,CMDTAIL+1    ; DE = scan pointer

FINDTOK_SCAN: LD    A,C
              OR    A
              JR    Z,FINDTOK_FAIL

              LD    HL,(FINDTOK_NDL)
              PUSH  DE              ; save this attempt's start position

FINDTOK_TRY:  LD    A,(HL)
              OR    A
              JR    Z,FINDTOK_HIT   ; whole needle matched (0 reached)

              LD    B,A             ; B = needle char (fixed case)
              LD    A,(DE)
              CP    'a'
              JR    C,FINDTOK_NOFOLD
              CP    'z'+1
              JR    NC,FINDTOK_NOFOLD
              AND   #0DF            ; fold lowercase text char to upper
FINDTOK_NOFOLD: CP  B
              JR    NZ,FINDTOK_MISS

              INC   HL
              INC   DE
              JR    FINDTOK_TRY

FINDTOK_HIT:  POP   HL              ; discard saved attempt-start
              EX    DE,HL           ; HL = pointer just past the match
              OR    A
              RET

FINDTOK_MISS: POP   DE              ; restore this attempt's start
              INC   DE              ; advance scan by one character
              DEC   C
              JR    FINDTOK_SCAN

FINDTOK_FAIL: SCF
              RET

                                    ;************************************
                                    ;* PARSEDEC - parses a run of        *
                                    ;* decimal digits at (IX), stopping  *
                                    ;* at the first non-digit            *
                                    ;************************************
                                    ;* OUTPUT: HL = value (0..255),      *
                                    ;* Carry=1 if zero digits were found *
                                    ;* or the value exceeds 255          *
                                    ;************************************

PARSEDEC:     LD    HL,0
              LD    B,0             ; digit counter

PARSEDEC_LP:  LD    A,(IX+0)
              CP    '0'
              JR    C,PARSEDEC_END
              CP    '9'+1
              JR    NC,PARSEDEC_END
              SUB   '0'

              LD    E,A
              LD    D,0
              PUSH  DE
              LD    D,H
              LD    E,L
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,DE
              ADD   HL,HL           ; HL = HL*10
              POP   DE
              ADD   HL,DE           ; HL = HL*10 + digit

              LD    A,H
              OR    A
              JR    NZ,PARSEDEC_BAD ; exceeded 255

              INC   B
              INC   IX
              JR    PARSEDEC_LP

PARSEDEC_END: LD    A,B
              OR    A
              JR    Z,PARSEDEC_BAD  ; no digits at all

              OR    A
              RET

PARSEDEC_BAD: SCF
              RET

;---------------------------

                                    ;************************************
                                    ;* Data                             *
                                    ;************************************

IDSTR:        DEFM  "RCX_INTERFACE",0
IDSTRLEN:     EQU   $-IDSTR

XIDSTR:       DEFM  "XIO_IO_EXPANDER",0
XIDSTRLEN:    EQU   $-XIDSTR

MSTART40:     DEFB  12
              DEFM  "8255I - UNAPI/EXTBIOS driver for RCX",13,10
              DEFM  "card type C (82C55 PPI), interrupt test.",13,10
              DEFM  "Port B Mode 1 - the 82C55 itself raises",13,10
              DEFM  "INTR B on PC0.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "8255I - UNAPI/EXTBIOS port of 8255I.ASC (card",13,10
              DEFM  "type C, 82C55 PPI, interrupt test). Port B Mode 1",13,10
              DEFM  "(strobed input) - the 82C55 itself raises INTR B",13,10
              DEFM  "on PC0, reported via the XIO standard handler (no",13,10
              DEFM  "custom ISR), via FN_CORE4/5/6/8 (XIO) and",13,10
              DEFM  "FN_CORE2/3/4/23/24/25/26/30/31/32 (RCX) directly,",13,10
              DEFM  "no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
PINTTOK:      DEFM  "/INT:",0
MUSAGE:       DEFM  "Usage: 8255I /IO:<0..252> /INT:<0..7>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MXFOUND:      DEFM  "XIO_IO_EXPANDER found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MWAIT:        DEFM  "<SPACE> ends programme, waiting for INT",13,10,"$"
MOUT:         DEFM  "OUT: ","$"
MARD:         DEFM  " A-RD: ","$"
MPAD4:        DEFM  "    ","$"
MFIRED:       DEFM  "INT fired, B-RD: ","$"
MCOUNT:       DEFM  " count:","$"
MPAD3:        DEFM  "   ","$"
MMASKERR:     DEFM  "XIO SET MASK failed (Carry).",13,10,"$"
MDONE:        DEFM  "Done.",13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"

PIOADDR:      DEFB  0
INTCHAN:      DEFB  0
MASKVAL:      DEFB  0
MASKTAB:      DEFB  1,2,4,8,16,32,64,128
OUTBYTE:      DEFB  0
ICOUNT:       DEFW  0
PRDECW_ST:    DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

XSLOT:        DEFB  0
XSEG:         DEFB  0
XENTRY:       DEFW  0
XISIBUF:      DEFW  0

ARBUF:        DEFW  0
BRBUF:        DEFW  0

                                    ; XWORKAREA/INTFLAGS must NOT live in
                                    ; this .COM's own image (page 0/1) -
                                    ; the MSX BIOS interrupt dispatcher
                                    ; forces page 0 to the system ROM
                                    ; while H.KEYI/INTSRV run, so any
                                    ; write the PIC ISR makes to an
                                    ; address in our own low-memory image
                                    ; would silently go to ROM instead of
                                    ; our real RAM.

XWORKAREA:    EQU   #0C000         ; 97 bytes, #0C000-#0C060
INTFLAGS:     EQU   #0C100         ; 2 bytes (only low byte used - PIC
                                    ; channel 0..7, not IM2)
RCXWORKAREA:  EQU   #0C200         ; 816 bytes (#0330), #0C200-#0C52F

              END   START
