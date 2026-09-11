                                    ;************************************
                                    ;* 8254I.COM                        *
                                    ;************************************
                                    ;* TIMER0/TIMER1 both MODE 2,       *
                                    ;* TIMER0 0xFFFF, TIMER1 16, TIMER2 *
                                    ;* MODE 0 0xFFFF, all gated on,     *
                                    ;* OUT0->CLK1, OUT1->CLK2; OUT1 is  *
                                    ;* ALSO wired to a PIC channel -    *
                                    ;* TIMER2 is read/printed           *
                                    ;* continuously via LOCATE (no      *
                                    ;* scroll), and every PIC interrupt *
                                    ;* prints "TIMER1 fired" plus a     *
                                    ;* running interrupt count, also    *
                                    ;* via LOCATE. Discovers BOTH       *
                                    ;* RCX_INTERFACE and                *
                                    ;* XIO_IO_EXPANDER separately and   *
                                    ;* switches between them via CALLFN *
                                    ;* (RCX) / CALLFNXIO (XIO). Works   *
                                    ;* against either driver in ROM     *
                                    ;* (CALSLT) or in mapped RAM (RAM   *
                                    ;* helper), see CALLFN/CALLFNXIO.   *
                                    ;* Assembled with pasmo.            *
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
                                    ;* RCXROM.ASM / XIO_ROM.ASM -       *
                                    ;* re-check whenever either ROM     *
                                    ;* changes)                         *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT

TIMER_FN_INIT:    EQU  33          ; TIMER INIT
TIMER_FN_0WR:     EQU  35          ; TIMER 0 WR
TIMER_FN_1WR:     EQU  37          ; TIMER 1 WR
TIMER_FN_2RD:     EQU  38          ; TIMER 2 RD
TIMER_FN_2WR:     EQU  39          ; TIMER 2 WR
TIMER_FN_CTRL:    EQU  40          ; TIMER CTRL
TIMER_FN_SETGATE: EQU  41          ; TIMER SET GATE
TIMER_FN_RESGATE: EQU  42          ; TIMER RES GATE

                                    ; TIMER 34-42 all dispatch through a
                                    ; single shared FN_CORETIM in the
                                    ; ROM (FN_CORE34..42 are all just
                                    ; "JP FN_CORETIM") - the actual
                                    ; operation is selected by B, not
                                    ; by A. Still using the "correct"
                                    ; per-operation A above, matching
                                    ; 8254.ASM.

TIMER_MODE_0WR:     EQU  1
TIMER_MODE_1WR:     EQU  3
TIMER_MODE_2RD:     EQU  4
TIMER_MODE_2WR:     EQU  5
TIMER_MODE_CTRL:    EQU  6
TIMER_MODE_SETGATE: EQU  7
TIMER_MODE_RESGATE: EQU  8

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

                                    ; ICOUNT = 0

              XOR   A
              LD    (ICOUNT),A
              LD    (ICOUNT+1),A

              CALL  PARSEIO
              JP    C,PARAMERR
              CALL  PARSEINT
              JP    C,PARAMERR

                                    ; Locate the RAM helper, if any -
                                    ; needed alongside CALSLT to support
                                    ; a RAM-mapped driver; harmless to
                                    ; look this up even on a ROM-only
                                    ; system (HELPERADD only gets used
                                    ; when SEG<>FFh). Shared by both
                                    ; UNAPIs - the RAM helper mechanism
                                    ; is generic, not per-driver.

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
                                    ;* Setup: RCX ISINIT/DEINIT/INIT,   *
                                    ;* TIMER INIT                        *
                                    ;************************************

              LD    DE,ISIBUF
              LD    A,RCX_FN_ISINIT
              CALL  CALLFN
              LD    A,(ISIBUF)
              CP    1
              JR    NZ,SETUP_INIT

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

SETUP_INIT:   LD    HL,WORKAREA
              LD    A,RCX_FN_INIT
              CALL  CALLFN
              JP    C,INITERR

                                    ; _TIMER INIT(Q) - BASIC does not
                                    ; check a Carry result here, so
                                    ; neither does this port

              LD    A,(TIMERADDR)
              LD    C,A
              LD    A,TIMER_FN_INIT
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Timer setup                       *
                                    ;************************************

                                    ; _TIMER CTRL(Q,&H34) - TIMER0,
                                    ; MODE 2, RW=11, BCD=0

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_CTRL
              LD    D,0
              LD    E,#034
              LD    A,TIMER_FN_CTRL
              CALL  CALLFN

                                    ; _TIMER CTRL(Q,&H74) - TIMER1,
                                    ; MODE 2, RW=11, BCD=0

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_CTRL
              LD    D,0
              LD    E,#074
              LD    A,TIMER_FN_CTRL
              CALL  CALLFN

                                    ; _TIMER RES GATE(Q,7)

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_RESGATE
              LD    D,0
              LD    E,7
              LD    A,TIMER_FN_RESGATE
              CALL  CALLFN

                                    ; _TIMER 0 WR(Q,&HFFFF)

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_0WR
              LD    DE,#0FFFF
              LD    A,TIMER_FN_0WR
              CALL  CALLFN

                                    ; _TIMER 1 WR(Q,16)

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_1WR
              LD    DE,16
              LD    A,TIMER_FN_1WR
              CALL  CALLFN

                                    ; _TIMER 2 WR(Q,&HFFFF)

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_2WR
              LD    DE,#0FFFF
              LD    A,TIMER_FN_2WR
              CALL  CALLFN

                                    ; _TIMER SET GATE(Q,7) - all three
                                    ; gates on, once, and never toggled
                                    ; again

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_SETGATE
              LD    D,0
              LD    E,7
              LD    A,TIMER_FN_SETGATE
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Activate the PIC channel only    *
                                    ;* now - AFTER the timer setup is   *
                                    ;* fully done, not before: an       *
                                    ;* early-activated XIO handler can  *
                                    ;* disturb in-flight RCX bus        *
                                    ;* traffic.                         *
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

              JR    MAINLOOP

MSETMASKERR:  LD    DE,MMASKERR
              CALL  PRSTR

                                    ;************************************
                                    ;* Main loop - re-reads/reprints    *
                                    ;* TIMER2 via LOCATE every pass,    *
                                    ;* checks the PIC flag atomically   *
                                    ;* (DI/EI), exit on SPACE            *
                                    ;************************************

MAINLOOP:                          ; _TIMER 2 RD(Q,A) - CORETIMCPR
                                    ; writes 2 bytes (both from
                                    ; consecutive 82C54 IN reads) into
                                    ; varptr(A), matching a BASIC
                                    ; integer variable's storage

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_2RD
              LD    DE,RESULTBUF
              LD    A,TIMER_FN_2RD
              CALL  CALLFN

                                    ; LOCATE 0,2 : PRINT "TIMER2: ";
                                    ; HEX$(A);"      "

              LD    B,2
              LD    C,0
              CALL  PRLOC
              LD    DE,MTIMER2
              CALL  PRSTR
              LD    HL,(RESULTBUF)
              CALL  PRHEXVW
              LD    DE,MPAD6
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

MAINLOOP_KEY:                      ; Non-blocking key poll: loop back
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

              LD    B,6
              LD    C,0
              CALL  PRLOC

              XOR   A
              LD    E,A
              LD    D,A
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

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
                                    ;* REFRESH - runs on interrupt (the *
                                    ;* flag itself is already cleared   *
                                    ;* atomically in MAINLOOP before    *
                                    ;* EI): ICOUNT + 1, then LOCATE 0,4 *
                                    ;* and print the message + count     *
                                    ;************************************

REFRESH:      LD    HL,(ICOUNT)
              INC   HL
              LD    (ICOUNT),HL

              LD    B,4
              LD    C,0
              CALL  PRLOC
              LD    DE,MFIRED
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
                                    ;* CALLFNXIO - calls XIO's          *
                                    ;* FN_CORExx. Identical dispatch to *
                                    ;* CALLFN, but against XSLOT/XSEG/  *
                                    ;* XENTRY (XIO_IO_EXPANDER's OWN    *
                                    ;* discovery result) instead of     *
                                    ;* SLOT/SEG/ENTRY (RCX_INTERFACE's) *
                                    ;* - RCXROM/RCXRAM and XIOROM are   *
                                    ;* two independent UNAPI drivers,   *
                                    ;* each needs its own dispatch      *
                                    ;* target. HELPERADD (the RAM       *
                                    ;* helper) is shared/generic.       *
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
                                    ;* PRLOC - positions the cursor via  *
                                    ;* the MSX-DOS console's "ESC Y"     *
                                    ;* code (row/col + 32, both 0-based, *
                                    ;* row 0/col 0 is top-left) - same   *
                                    ;* routine as SJA1000.ASM            *
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
                                    ;* (I only ever counts up from 0,   *
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

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..251, stores into TIMERADDR     *
                                    ;************************************
                                    ;* OUTPUT: (TIMERADDR) = 0..251,     *
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
              CP    252             ; 0..251 valid
              JR    C,PARSEIO_OK

              SCF
              RET

PARSEIO_OK:   LD    (TIMERADDR),A
              OR    A
              RET

                                    ;************************************
                                    ;* PARSEINT - finds "/INT:" in the   *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..7, stores CHANNEL and          *
                                    ;* precomputes MASKVAL = 2^CHANNEL   *
                                    ;************************************
                                    ;* OUTPUT: (CHANNEL) = 0..7,         *
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

PARSEINT_OK:  LD    (CHANNEL),A

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
              DEFM  "8254I - UNAPI/EXTBIOS driver for",13,10
              DEFM  "RCX card type D (82C54 timer),",13,10
              DEFM  "interrupt-driven like RTCLCDI.",13,10
              DEFM  "Wire OUT0->CLK1, OUT1->CLK2 and",13,10
              DEFM  "to a PIC channel.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "8254I - UNAPI/EXTBIOS port of 8254I.ASC (card type",13,10
              DEFM  "D, 82C54), interrupt-driven display refresh via",13,10
              DEFM  "XIO_IO_EXPANDER, like RTCLCDI.COM. Wire OUT0 to",13,10
              DEFM  "CLK1, OUT1 to CLK2 AND to a PIC channel. TIMER2 is",13,10
              DEFM  "shown continuously; every PIC interrupt (from",13,10
              DEFM  "TIMER1) also shows a running interrupt count.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
PINTTOK:      DEFM  "/INT:",0
MUSAGE:       DEFM  "Usage: 8254I /IO:<0..251> /INT:<0..7>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MXFOUND:      DEFM  "XIO_IO_EXPANDER found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MWAIT:        DEFM  "<SPACE> ends programme, waiting for INT",13,10,"$"
MTIMER2:      DEFM  "TIMER2: ","$"
MPAD6:        DEFM  "      ","$"
MFIRED:       DEFM  "TIMER1 fired  count=","$"
MPAD3:        DEFM  "   ","$"
MMASKERR:     DEFM  "XIO SET MASK failed (Carry).",13,10,"$"
MDONE:        DEFM  "Done.",13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"

TIMERADDR:    DEFB  0
CHANNEL:      DEFB  0
MASKVAL:      DEFB  0
MASKTAB:      DEFB  1,2,4,8,16,32,64,128
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

RESULTBUF:    DEFW  0

WORKAREA:     DEFS  #0330

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
INTFLAGS:     EQU   #0C100         ; 2 bytes

              END   START
