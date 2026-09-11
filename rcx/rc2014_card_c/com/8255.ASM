                                    ;************************************
                                    ;* 8255.COM                         *
                                    ;************************************
                                    ;* RCX INIT, PPI INIT, PPI CTRL     *
                                    ;* (Mode 0, Port A output, Port B   *
                                    ;* input, Port C output), then      *
                                    ;* writes an incrementing byte to   *
                                    ;* Port A and reads Port A/B/C      *
                                    ;* back, toggles a Port C bit via   *
                                    ;* SET/RESET, all printed via       *
                                    ;* LOCATE (no scroll), exit on      *
                                    ;* SPACE - but via                  *
                                    ;* FN_CORE2/3/4/23/24/25/26/28/30/  *
                                    ;* 31/32 directly through EXTBIO    *
                                    ;* discovery, no BASIC interpreter  *
                                    ;* involved. Works against          *
                                    ;* RCX_INTERFACE in ROM (CALSLT) or *
                                    ;* in mapped RAM (RCXRAM.COM, via   *
                                    ;* the RAM helper), see CALLFN.     *
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
                                    ;* RCXROM.ASM - re-check whenever   *
                                    ;* the ROM changes)                 *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT

PPI_FN_INIT:    EQU  23            ; PPI INIT
PPI_FN_ARD:     EQU  24            ; PPI A RD
PPI_FN_BRD:     EQU  26            ; PPI B RD
PPI_FN_CRD:     EQU  28            ; PPI C RD
PPI_FN_AWR:     EQU  25            ; PPI A WR
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
PPI_MODE_CRD:     EQU  4
PPI_MODE_CTRL:    EQU  6
PPI_MODE_SET:     EQU  7
PPI_MODE_RESET:   EQU  8

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

                                    ; Locate the RAM helper, if any -
                                    ; needed alongside CALSLT to support
                                    ; a RAM-mapped RCX driver; harmless
                                    ; to look this up even on a ROM-only
                                    ; system (HELPERADD only gets used
                                    ; when SEG<>FFh)

              LD    DE,#02222
              LD    HL,0
              LD    A,#0FF
              CALL  EXTBIO
              LD    (HELPERADD),HL

                                    ; Discovery (A=0: count)

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

                                    ; Discovery (A=1: slot/segment/entry)

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

                                    ;************************************
                                    ;* Setup: RCX ISINIT/DEINIT/INIT,   *
                                    ;* PPI INIT, PPI CTRL               *
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

                                    ; _PPI INIT(Q)

              LD    A,(PIOADDR)
              LD    C,A
              LD    A,PPI_FN_INIT
              CALL  CALLFN

                                    ; _PPI CTRL(Q,&H82) - Mode 0, Port A
                                    ; output, Port B input, Port C
                                    ; output

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_CTRL
              LD    D,#082
              LD    A,PPI_FN_CTRL
              CALL  CALLFN

;---------------------------

                                    ; CLS : PRINT "<SPACE> ends
                                    ; programme"

              LD    A,12             ; CLS (Ctrl-L)
              CALL  PRCHR
              LD    DE,MWAIT
              CALL  PRSTR

              LD    A,0
              LD    (OUTBYTE),A

                                    ;************************************
                                    ;* Main loop                         *
                                    ;************************************

MAINLOOP:                          ; _PPI A WR(Q,J)

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

                                    ; _PPI B RD(Q,BR)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_BRD
              LD    DE,BRBUF
              LD    A,PPI_FN_BRD
              CALL  CALLFN

                                    ; K = J AND 7 : IF (J AND 8) = 0
                                    ; THEN _PPI SET(Q,K) ELSE
                                    ; _PPI RESET(Q,K)

              LD    A,(OUTBYTE)
              AND   #007
              LD    (BITIDX),A

              LD    A,(OUTBYTE)
              AND   #008
              JR    NZ,DORESET

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_SET
              LD    A,(BITIDX)
              LD    D,A
              LD    A,PPI_FN_SET
              CALL  CALLFN
              JR    SETDONE

DORESET:      LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_RESET
              LD    A,(BITIDX)
              LD    D,A
              LD    A,PPI_FN_RESET
              CALL  CALLFN

SETDONE:                          ; _PPI C RD(Q,CR)

              LD    A,(PIOADDR)
              LD    C,A
              LD    B,PPI_MODE_CRD
              LD    DE,CRBUF
              LD    A,PPI_FN_CRD
              CALL  CALLFN

                                    ; LOCATE 0,3 : PRINT "OUT: ";
                                    ; HEX$(J);" A-RD: ";HEX$(AR);"    "

              LD    B,3
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

                                    ; LOCATE 0,4 : PRINT "B-RD: ";
                                    ; HEX$(BR);" C-RD: ";HEX$(CR);"    "

              LD    B,4
              LD    C,0
              CALL  PRLOC
              LD    DE,MBRD
              CALL  PRSTR
              LD    HL,(BRBUF)
              CALL  PRHEXVW
              LD    DE,MCRD
              CALL  PRSTR
              LD    HL,(CRBUF)
              CALL  PRHEXVW
              LD    DE,MPAD4
              CALL  PRSTR

                                    ; J = J + 1 : IF J = 256 THEN J = 0

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

;---------------------------

                                    ;************************************
                                    ;* CALLFN - calls FN_CORExx.        *
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

MSTART40:     DEFB  12
              DEFM  "8255 - UNAPI/EXTBIOS driver for RCX",13,10
              DEFM  "card type C (82C55 PPI). Port A as byte",13,10
              DEFM  "output, Port B as byte input, Port C as",13,10
              DEFM  "byte output with bit set/reset - writes",13,10
              DEFM  "an incrementing byte and reads it back.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "8255 - UNAPI/EXTBIOS port of 8255.ASC (card type",13,10
              DEFM  "C, 82C55 PPI). Mode 0, Port A byte output, Port B",13,10
              DEFM  "byte input, Port C byte output - writes an",13,10
              DEFM  "incrementing byte to Port A and reads Port A/B/C",13,10
              DEFM  "back, toggles a Port C bit via SET/RESET, via",13,10
              DEFM  "FN_CORE2/3/4/23/24/25/26/28/30/31/32 directly,",13,10
              DEFM  "no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
MUSAGE:       DEFM  "Usage: 8255 /IO:<0..252>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MWAIT:        DEFM  "<SPACE> ends programme",13,10,"$"
MOUT:         DEFM  "OUT: ","$"
MARD:         DEFM  " A-RD: ","$"
MBRD:         DEFM  "B-RD: ","$"
MCRD:         DEFM  " C-RD: ","$"
MPAD4:        DEFM  "    ","$"
MDONE:        DEFM  "Done.",13,10,"$"

PIOADDR:      DEFB  0
OUTBYTE:      DEFB  0
BITIDX:       DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

ARBUF:        DEFW  0
BRBUF:        DEFW  0
CRBUF:        DEFW  0

WORKAREA:     DEFS  #0330

              END   START
