                                    ;************************************
                                    ;* Z80PIOI.COM                      *
                                    ;************************************
                                    ;* Uses RC2014 SC103 as DUT.        *
                                    ;************************************
                                    ;* Installs a Z80 ISR stub via IM2  *
                                    ;* (XIO SET ADDR, channel INT+8) at *
                                    ;* ISRCODE (own fixed page-3 area), *
                                    ;* toggles PA0, and reports which   *
                                    ;* PIO channel triggered the        *
                                    ;* interrupt (INTFLAGS high byte,   *
                                    ;* bound via XIO INIT and updated   *
                                    ;* by the ROM on every IM2          *
                                    ;* interrupt, independent of the    *
                                    ;* custom ISR). PIO registers       *
                                    ;* themselves are accessed via the  *
                                    ;* RCX PIO library functions        *
                                    ;* (FN_CORE16/18/21/22), not raw    *
                                    ;* XIO OUT - but via FN_CORE4/5/6/  *
                                    ;* 7/8 (XIO) and FN_CORE2/3/4/16/   *
                                    ;* 18/21/22 (RCX) directly through  *
                                    ;* EXTBIO discovery, no BASIC       *
                                    ;* interpreter involved.            *
                                    ;* Discovers both RCX_INTERFACE and *
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

PIO_FN_INIT:    EQU  16            ; PIO INIT
PIO_FN_AWR:     EQU  18            ; PIO A WR
PIO_FN_ACTRL:   EQU  21            ; PIO A CTRL
PIO_FN_BCTRL:   EQU  22            ; PIO B CTRL

                                    ; PIO 17-22 all dispatch through a
                                    ; single shared FN_COREPIO in the
                                    ; ROM (FN_CORE17..22 are all just
                                    ; "JP FN_COREPIO") - the actual
                                    ; operation is selected by B, not
                                    ; by A. Still using the "correct"
                                    ; per-operation A above.

PIO_MODE_AWR:     EQU  1
PIO_MODE_ACTRL:   EQU  4
PIO_MODE_BCTRL:   EQU  5

XIO_FN_INIT:    EQU  4             ; XIO INIT
XIO_FN_ISINIT:  EQU  5             ; XIO ISINIT
XIO_FN_DEINIT:  EQU  6             ; XIO DEINIT
XIO_FN_SETADDR: EQU  7             ; XIO SET ADDR
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

                                    ; XIOCHAN = INTCHAN + 8 (IM2 channel),
                                    ; VECTORBYTE = INTCHAN * 2 (Z80 PIO
                                    ; INT-vector byte)

              LD    A,(INTCHAN)
              ADD   A,8
              LD    (XIOCHAN),A
              LD    A,(INTCHAN)
              ADD   A,A
              LD    (VECTORBYTE),A

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

              LD    DE,MXIOINST
              CALL  PRSTR

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
                                    ;* PIO setup via SC103, using the    *
                                    ;* RCX PIO library functions         *
                                    ;************************************

                                    ; _PIO INIT(BC)

              LD    A,(PIOADDR)
              LD    C,A
              LD    A,PIO_FN_INIT
              CALL  CALLFN

                                    ; Port A as OUTPUT - _PIO A CTRL(BC,&HF)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_ACTRL
              LD    D,#00F
              LD    A,PIO_FN_ACTRL
              CALL  CALLFN

                                    ; Port A = 0 - _PIO A WR(BC,0)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_AWR
              LD    D,0
              LD    A,PIO_FN_AWR
              CALL  CALLFN

                                    ; Port B as MODE 3 and PB0 as Input -
                                    ; _PIO B CTRL(BC,&HCF)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    D,#0CF
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    D,1
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

                                    ; Port B INT-Vektor 0,2,4...14 -
                                    ; _PIO B CTRL(BC,VECTORBYTE)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    A,(VECTORBYTE)
              LD    D,A
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

                                    ; Port B INT-Control via PB0 -
                                    ; _PIO B CTRL(BC,&HB7)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    D,#0B7
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    D,254
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Install ISR stub at ISRCODE       *
                                    ;************************************

              LD    HL,ISRDATA
              LD    DE,ISRCODE
              LD    BC,ISRDATALEN
              LDIR

              LD    A,(XIOCHAN)
              LD    B,A
              LD    C,#0FF
              LD    DE,ISRCODE
              LD    A,XIO_FN_SETADDR
              CALL  CALLFNXIO
              JP    C,SETADDRERR

;---------------------------

                                    ;************************************
                                    ;* Interrupt mask                    *
                                    ;************************************
                                    ; IM2 &H00xx ... &HFFxx

              LD    DE,#0FF00
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO
              JP    C,SETMASKERR

              LD    DE,MMASKMSG
              CALL  PRSTR
              LD    HL,#0FF00
              CALL  PRHEXVW
              LD    DE,MCRLF
              CALL  PRSTR

                                    ;************************************
                                    ;* Main loop                         *
                                    ;************************************

              LD    A,12             ; CLS (Ctrl-L)
              CALL  PRCHR
              LD    B,0
              LD    C,0
              CALL  PRLOC
              LD    DE,MWAIT
              CALL  PRSTR

              XOR   A
              LD    (OUTBYTE),A

MAINLOOP:                          ; snapshot + clear INTFLAGS high
                                    ; byte atomically (DI/EI)

              DI
              LD    A,(INTFLAGS+1)
              LD    (INTSNAP),A
              XOR   A
              LD    (INTFLAGS+1),A
              EI

              LD    A,(INTSNAP)
              LD    C,A
              LD    B,0

CHANLOOP:     SRL   C
              JR    NC,CHANLOOP_SKIP

              PUSH  BC
              CALL  CHANHANDLER
              POP   BC

CHANLOOP_SKIP: INC  B
              LD    A,B
              CP    8
              JR    NZ,CHANLOOP

                                    ; _PIO A WR(BC,OUTBYTE) - toggles
                                    ; PA0 with the current output byte

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_AWR
              LD    A,(OUTBYTE)
              LD    D,A
              LD    A,PIO_FN_AWR
              CALL  CALLFN

                                    ; Toggle the PA0 output bit

              LD    A,(OUTBYTE)
              XOR   1
              LD    (OUTBYTE),A

                                    ; Non-blocking key poll: loop back
                                    ; to MAINLOOP unless a key is
                                    ; waiting

              LD    C,#0B          ; CONST - console status
              CALL  BDOS
              OR    A
              JR    Z,MAINLOOP

              LD    C,1            ; CONIN - read the waiting char
              CALL  BDOS
              CP    ' '
              JR    NZ,MAINLOOP

;---------------------------

                                    ;************************************
                                    ;* Cleanup                           *
                                    ;************************************

                                    ; _PIO B CTRL(BC,&H07)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PIO_MODE_BCTRL
              LD    D,#007
              LD    A,PIO_FN_BCTRL
              CALL  CALLFN

              XOR   A
              LD    E,A
              LD    D,A
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO

              LD    A,(XIOCHAN)
              LD    B,A
              LD    C,#0FF
              LD    DE,0
              LD    A,XIO_FN_SETADDR
              CALL  CALLFNXIO

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

              LD    B,6
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

SETADDRERR:   LD    DE,MSETADDRERR
              CALL  PRSTR
              RET

SETMASKERR:   LD    DE,MSETMASKERR
              CALL  PRSTR
              RET

;---------------------------

                                    ;************************************
                                    ;* CHANHANDLER - LOCATE 0,2, print   *
                                    ;* "Here channel N, count: " plus    *
                                    ;* the per-channel counter,          *
                                    ;* incremented here                  *
                                    ;************************************
                                    ;* INPUT: B = channel number (0..7)  *
                                    ;************************************

CHANHANDLER:  PUSH  BC

              LD    B,2
              LD    C,0
              CALL  PRLOC
              LD    DE,MCHANPRE
              CALL  PRSTR

              POP   BC
              PUSH  BC
              LD    A,B
              ADD   A,'0'
              CALL  PRCHR

              LD    DE,MCHANPOST
              CALL  PRSTR

              POP   BC
              LD    H,0
              LD    L,B
              ADD   HL,HL           ; HL = channel*2
              LD    DE,COUNTERS
              ADD   HL,DE           ; HL = &COUNTERS[channel]

              LD    E,(HL)
              INC   HL
              LD    D,(HL)
              INC   DE
              LD    (HL),D
              DEC   HL
              LD    (HL),E

              EX    DE,HL
              CALL  PRDECW

              LD    DE,MPAD4
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
                                    ;* (the per-channel counters only   *
                                    ;* ever count up from 0, so always  *
                                    ;* non-negative here)                *
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
                                    ;* 0..7, stores INTCHAN              *
                                    ;************************************
                                    ;* OUTPUT: (INTCHAN) = 0..7,         *
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
              DEFM  "Z80PIOI - UNAPI/EXTBIOS driver for RCX",13,10
              DEFM  "card type B (Z80 PIO), IM2 test routine.",13,10
              DEFM  "Installs a Z80 ISR stub, toggles PA0 and",13,10
              DEFM  "reports which PIO channel fired.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "Z80PIOI - UNAPI/EXTBIOS port of Z80PIOI.ASC (card",13,10
              DEFM  "type B, Z80 PIO, IM2 test routine). Installs a Z80",13,10
              DEFM  "ISR stub via XIO SET ADDR, toggles PA0 and reports",13,10
              DEFM  "which PIO channel fired the interrupt, via",13,10
              DEFM  "FN_CORE4/5/6/7/8 (XIO) and FN_CORE2/3/4/16/18/21/22",13,10
              DEFM  "(RCX) directly, no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
PINTTOK:      DEFM  "/INT:",0
MUSAGE:       DEFM  "Usage: Z80PIOI /IO:<0..252> /INT:<0..7>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MXFOUND:      DEFM  "XIO_IO_EXPANDER found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MXIOINST:     DEFM  "[XIO] is installed",13,10,"$"
MMASKMSG:     DEFM  "[XIO] mask for IM2 is set to &H","$"
MWAIT:        DEFM  "<SPACE> ends programme, waiting for INT",13,10,"$"
MCHANPRE:     DEFM  "Here channel ","$"
MCHANPOST:    DEFM  ", count: ","$"
MPAD4:        DEFM  "    ","$"
MDONE:        DEFM  "DONE",13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"
MSETADDRERR:  DEFM  "XIO SET ADDR failed (Carry).",13,10,"$"
MSETMASKERR:  DEFM  "XIO SET MASK failed (Carry).",13,10,"$"

PIOADDR:      DEFB  0
INTCHAN:      DEFB  0
XIOCHAN:      DEFB  0
VECTORBYTE:   DEFB  0
OUTBYTE:      DEFB  0
INTSNAP:      DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
PRDECW_ST:    DEFB  0
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

COUNTERS:     DEFS  16              ; 8 words, one per channel

                                    ;************************************
                                    ;* Z80 ISR code, LDIR'd to ISRCODE: *
                                    ;* ESC Y row=4 col=0, 13x space     *
                                    ;* (clear), ESC Y row=4 col=0       *
                                    ;* again, "ISR has fired", RETI     *
                                    ;************************************

ISRDATA:      DEFB  #03E,#01B,#0CD,#0A2,#000
              DEFB  #03E,#059,#0CD,#0A2,#000
              DEFB  #03E,#024,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#01B,#0CD,#0A2,#000
              DEFB  #03E,#059,#0CD,#0A2,#000
              DEFB  #03E,#024,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#049,#0CD,#0A2,#000
              DEFB  #03E,#053,#0CD,#0A2,#000
              DEFB  #03E,#052,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#068,#0CD,#0A2,#000
              DEFB  #03E,#061,#0CD,#0A2,#000
              DEFB  #03E,#073,#0CD,#0A2,#000
              DEFB  #03E,#020,#0CD,#0A2,#000
              DEFB  #03E,#066,#0CD,#0A2,#000
              DEFB  #03E,#069,#0CD,#0A2,#000
              DEFB  #03E,#072,#0CD,#0A2,#000
              DEFB  #03E,#065,#0CD,#0A2,#000
              DEFB  #03E,#064,#0CD,#0A2,#000
              DEFB  #0ED,#04D
ISRDATALEN:   EQU   $-ISRDATA

                                    ; XWORKAREA/INTFLAGS/RCXWORKAREA/
                                    ; ISRCODE must NOT live in this
                                    ; .COM's own image (page 0/1) - the
                                    ; MSX BIOS interrupt dispatcher
                                    ; forces page 0 to the system ROM
                                    ; while H.KEYI/INTSRV run, so any
                                    ; write the PIC/IM2 ISR makes to an
                                    ; address in our own low-memory
                                    ; image would silently go to ROM
                                    ; instead of our real RAM. ISRCODE
                                    ; is placed right after RCXWORKAREA
                                    ; ends.

XWORKAREA:    EQU   #0C000         ; 97 bytes, #0C000-#0C060
INTFLAGS:     EQU   #0C100         ; 2 bytes - high byte holds the
                                    ; per-channel IM2 pending bits
                                    ; (bit0=channel0...bit7=channel7),
                                    ; written by the ROM on every IM2
                                    ; interrupt regardless of a custom
                                    ; SET ADDR handler being registered
RCXWORKAREA:  EQU   #0C200         ; 816 bytes (#0330), #0C200-#0C52F
ISRCODE:      EQU   #0C600         ; ISR stub target, right after
                                    ; RCXWORKAREA (#0C200-#0C52F)

              END   START
