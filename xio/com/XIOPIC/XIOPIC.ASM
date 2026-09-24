                                    ;************************************
                                    ;* XIOPIC.COM                       *
                                    ;************************************
                                    ;* Installs a Z80 ISR stub on PIC   *
                                    ;* channel 0's vector (SET ADDR     *
                                    ;* channel 0) and enables all 8 PIC *
                                    ;* channels (0-7) via SET MASK,     *
                                    ;* reporting whichever fires        *
                                    ;* (INTFLAGS low byte, bound via    *
                                    ;* XIO INIT and OR'd by the ROM on  *
                                    ;* every PIC interrupt, independent *
                                    ;* of the custom ISR). Discovers    *
                                    ;* XIO_IO_EXPANDER directly through *
                                    ;* EXTBIO, no BASIC interpreter     *
                                    ;* involved.                        *
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

;---------------------------

                                    ;************************************
                                    ;* UNAPI function numbers           *
                                    ;* (position in FN_TABLE, see       *
                                    ;* XIOROM.ASM - re-check whenever   *
                                    ;* the ROM changes)                 *
                                    ;************************************

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

                                    ; Locate the RAM helper, if any

              LD    DE,#02222
              LD    HL,0
              LD    A,#0FF
              CALL  EXTBIO
              LD    (HELPERADD),HL

                                    ; Discover XIO_IO_EXPANDER
                                    ; (A=0: count)

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

                                    ; Discover XIO_IO_EXPANDER
                                    ; (A=1: slot/segment/entry)

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

;---------------------------

                                    ;************************************
                                    ;* Install ISR stub at ISRCODE       *
                                    ;************************************

              LD    HL,ISRDATA
              LD    DE,ISRCODE
              LD    BC,ISRDATALEN
              LDIR

              LD    B,0              ; PIC channel 0 (fixed, matches
                                    ; the BASIC original)
              LD    C,#0FF
              LD    DE,ISRCODE
              LD    A,XIO_FN_SETADDR
              CALL  CALLFNXIO
              JP    C,SETADDRERR

;---------------------------

                                    ;************************************
                                    ;* Interrupt mask                    *
                                    ;************************************
                                    ; PIC &Hxx00 ... &HxxFF - all 8
                                    ; channels enabled

              LD    DE,#000FF
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO
              JP    C,SETMASKERR

              LD    DE,MMASKMSG
              CALL  PRSTR
              LD    HL,#000FF
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

MAINLOOP:                          ; live-read INTFLAGS low byte for
                                    ; each channel in turn (no DI/EI
                                    ; snapshot) - matches XIOPIC.ASC,
                                    ; where each of the 8 "IF (A AND
                                    ; &Hxx)..." checks re-reads the
                                    ; same live variable; a channel's
                                    ; own bit is cleared only inside
                                    ; CHANHANDLER once that channel is
                                    ; actually handled, leaving any bit
                                    ; set by a concurrent interrupt on
                                    ; another channel untouched

              LD    B,0

CHANLOOP:     PUSH  BC
              LD    H,0
              LD    L,B
              LD    DE,BITTAB
              ADD   HL,DE
              LD    A,(HL)
              LD    C,A
              LD    A,(INTFLAGS)
              AND   C
              POP   BC
              JR    Z,CHANLOOP_SKIP

              PUSH  BC
              CALL  CHANHANDLER
              POP   BC

CHANLOOP_SKIP: INC  B
              LD    A,B
              CP    8
              JR    NZ,CHANLOOP

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

              XOR   A
              LD    E,A
              LD    D,A
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO

              LD    B,0
              LD    C,#0FF
              LD    DE,0
              LD    A,XIO_FN_SETADDR
              CALL  CALLFNXIO

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

              LD    B,6
              LD    C,0
              CALL  PRLOC
              LD    DE,MDONE
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
                                    ;* incremented here, then clears     *
                                    ;* only this channel's own bit in    *
                                    ;* the INTFLAGS low byte             *
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
              PUSH  BC
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

              POP   BC
              LD    H,0
              LD    L,B
              LD    DE,BITTAB
              ADD   HL,DE
              LD    A,(HL)
              CPL
              LD    HL,INTFLAGS
              AND   (HL)
              LD    (HL),A
              RET

;---------------------------

                                    ;************************************
                                    ;* CALLFNXIO - calls XIO's           *
                                    ;* FN_CORExx. Segment-aware          *
                                    ;* dispatch: SEG=FFh (ROM) uses      *
                                    ;* CALSLT, SEG<>FFh (mapped RAM)     *
                                    ;* jumps through the RAM helper      *
                                    ;* instead.                          *
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
                                    ;* Data                             *
                                    ;************************************

XIDSTR:       DEFM  "XIO_IO_EXPANDER",0
XIDSTRLEN:    EQU   $-XIDSTR

MSTART40:     DEFB  12
              DEFM  "XIOPIC - UNAPI/EXTBIOS driver for",13,10
              DEFM  "XIO_IO_EXPANDER, PIC test routine.",13,10
              DEFM  "Installs a Z80 ISR stub and reports",13,10
              DEFM  "which PIC channel fired.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "XIOPIC - UNAPI/EXTBIOS port of XIOPIC.ASC (PIC test",13,10
              DEFM  "routine). Installs a Z80 ISR stub via XIO SET ADDR",13,10
              DEFM  "on PIC channel 0, enables all 8 PIC channels and",13,10
              DEFM  "reports which one fired the interrupt, via",13,10
              DEFM  "FN_CORE4/5/6/7/8 (XIO) directly, no BASIC interpreter.",13,10
              DEFM  13,10,"$"

MCRLF:        DEFM  13,10,"$"
MXFOUND:      DEFM  "XIO_IO_EXPANDER found, slot=","$"
MXIOINST:     DEFM  "[XIO] is installed",13,10,"$"
MMASKMSG:     DEFM  "[XIO] mask for PIC is set to &H","$"
MWAIT:        DEFM  "<SPACE> ends programme, waiting for INT",13,10,"$"
MCHANPRE:     DEFM  "Here channel ","$"
MCHANPOST:    DEFM  ", count: ","$"
MPAD4:        DEFM  "    ","$"
MDONE:        DEFM  "DONE",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"
MSETADDRERR:  DEFM  "XIO SET ADDR failed (Carry).",13,10,"$"
MSETMASKERR:  DEFM  "XIO SET MASK failed (Carry).",13,10,"$"

SCRCOLS:      DEFB  0             ; 40 or 80
PRDECW_ST:    DEFB  0

HELPERADD:    DEFW  0

XSLOT:        DEFB  0
XSEG:         DEFB  0
XENTRY:       DEFW  0
XISIBUF:      DEFW  0

BITTAB:       DEFB  1,2,4,8,16,32,64,128
COUNTERS:     DEFS  16              ; 8 words, one per channel

                                    ;************************************
                                    ;* Z80 ISR code, LDIR'd to ISRCODE: *
                                    ;* ESC Y row=4 col=0, 13x space     *
                                    ;* (clear), ESC Y row=4 col=0       *
                                    ;* again, "ISR has fired", RET      *
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
              DEFB  #0C9
ISRDATALEN:   EQU   $-ISRDATA

                                    ; XWORKAREA/INTFLAGS/ISRCODE must
                                    ; NOT live in this .COM's own image
                                    ; (page 0/1) - the MSX BIOS
                                    ; interrupt dispatcher forces page 0
                                    ; to the system ROM while H.KEYI/
                                    ; INTSRV run, so any write the PIC
                                    ; ISR makes to an address in our own
                                    ; low-memory image would silently go
                                    ; to ROM instead of our real RAM.

XWORKAREA:    EQU   #0C000         ; 97 bytes, #0C000-#0C060
INTFLAGS:     EQU   #0C100         ; 2 bytes - low byte holds the
                                    ; per-channel PIC pending bits
                                    ; (bit0=channel0...bit7=channel7),
                                    ; written by the ROM on every PIC
                                    ; interrupt regardless of a custom
                                    ; SET ADDR handler being registered
ISRCODE:      EQU   #0C200         ; ISR stub target, right after
                                    ; XWORKAREA

              END   START
