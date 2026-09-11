                                    ;************************************
                                    ;* 8254.COM                         *
                                    ;************************************
                                    ;* RCX INIT, TIMER INIT, TIMER0/    *
                                    ;* TIMER1 both set to MODE 2 (Rate  *
                                    ;* Generator) via TIMER CTRL,       *
                                    ;* TIMER0 preset to 0xFFFF, TIMER1  *
                                    ;* to 16, TIMER2 (MODE 0) preset to *
                                    ;* 0xFFFF, all three gated on once  *
                                    ;* - OUT0 wired externally to CLK1, *
                                    ;* OUT1 to CLK2 - then reads TIMER2 *
                                    ;* back in a loop - but via         *
                                    ;* FN_CORE2/33/38/39/40/41/42/35/37 *
                                    ;* directly through EXTBIO          *
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
                                    ; per-operation A above, since
                                    ; that's what each function is
                                    ; documented/registered as.

TIMER_MODE_0WR:     EQU  1
TIMER_MODE_1WR:     EQU  3
TIMER_MODE_2RD:     EQU  4
TIMER_MODE_2WR:     EQU  5
TIMER_MODE_CTRL:    EQU  6
TIMER_MODE_SETGATE: EQU  7
TIMER_MODE_RESGATE: EQU  8

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
                                    ;* Timer setup - TIMER0/TIMER1 are  *
                                    ;* moved from the fixed MODE 0 that  *
                                    ;* TIMER INIT applies to all three   *
                                    ;* counters into MODE 2 (Rate        *
                                    ;* Generator), so their OUT pulses   *
                                    ;* repeatedly instead of a one-shot  *
                                    ;* transition - TIMER2 stays MODE 0. *
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
                                    ; gates on, set once and never
                                    ; toggled again

              LD    A,(TIMERADDR)
              LD    C,A
              LD    B,TIMER_MODE_SETGATE
              LD    D,0
              LD    E,7
              LD    A,TIMER_FN_SETGATE
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Main loop                         *
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

                                    ; PRINT HEX$(A) - single item, no
                                    ; comma/semicolon involved, so this
                                    ; is just the hex digits (no
                                    ; leading zeros, matching HEX$())
                                    ; followed by PRINT's own newline

              LD    HL,(RESULTBUF)
              CALL  PRHEXVW
              LD    DE,MCRLF
              CALL  PRSTR

              JP    MAINLOOP

;---------------------------

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
              DEFM  "8254 - UNAPI/EXTBIOS driver for RCX",13,10
              DEFM  "card type D (82C54 timer). Wire",13,10
              DEFM  "OUT0->CLK1, OUT1->CLK2. Reads",13,10
              DEFM  "TIMER 2 back in a loop.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "8254 - UNAPI/EXTBIOS port of 8254.ASC (card type",13,10
              DEFM  "D, 82C54). Wire OUT0 to CLK1, OUT1 to CLK2.",13,10
              DEFM  "TIMER0/1 MODE 2 (0xFFFF/16), TIMER2 MODE 0",13,10
              DEFM  "(0xFFFF), all gated on - then reads TIMER 2 back",13,10
              DEFM  "in a loop, via FN_CORE33/35/37/38/39/40/41/42",13,10
              DEFM  "directly, no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
MUSAGE:       DEFM  "Usage: 8254 /IO:<0..251>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"

TIMERADDR:    DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

RESULTBUF:    DEFW  0

WORKAREA:     DEFS  #0330

              END   START
