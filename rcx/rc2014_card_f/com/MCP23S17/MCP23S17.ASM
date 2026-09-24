                                    ;************************************
                                    ;* MCP23S17.COM                     *
                                    ;************************************
                                    ;* RCX INIT, SPI INIT/MODE on the   *
                                    ;* card's SPI bus (/BUS:, 0..3),    *
                                    ;* then a continuous loop that      *
                                    ;* writes an incrementing byte to   *
                                    ;* MCP23S17 GPIO A and reads GPIO B *
                                    ;* back via SPI WRB/RDH - but via   *
                                    ;* FN_CORE2/50/52/55/56 directly    *
                                    ;* through EXTBIO discovery, no     *
                                    ;* BASIC interpreter involved.      *
                                    ;* Works against RCX_INTERFACE in   *
                                    ;* ROM (CALSLT) or in mapped RAM    *
                                    ;* (RCXRAM.COM, via the RAM         *
                                    ;* helper), see CALLFN.             *
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
                                    ;* RCXROM.ASM - re-check whenever   *
                                    ;* the ROM changes)                 *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT

SPI_FN_INIT:    EQU  50            ; SPI INIT
SPI_FN_WRB:     EQU  52            ; SPI WRB
SPI_FN_RDH:     EQU  55            ; SPI RDH
SPI_FN_MODE:    EQU  56            ; SPI MODE

UNAPI_DISC_COUNT: EQU 0            ; EXTBIO discovery: count implementations
UNAPI_DISC_ENTRY: EQU 1            ; EXTBIO discovery: slot/address

                                    ;************************************
                                    ;* MCP23S17 registers                *
                                    ;************************************

MCP_ADDR:     EQU   #040           ; MCP address (SPI opcode, write)
IODIRA:       EQU   #000
IODIRB:       EQU   #001
GPIOA:        EQU   #012
GPIOB:        EQU   #013
OLATA:        EQU   #014
OLATB:        EQU   #015
IOCON:        EQU   #00A

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
              CALL  PARSEBUS
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
                                    ;* SPI INIT/MODE                     *
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

                                    ; _SPI INIT(&H10) - no Carry check,
                                    ; not documented as a valid error
                                    ; signal for this call

              LD    A,(SPIADDR)
              LD    C,A
              LD    A,SPI_FN_INIT
              CALL  CALLFN

                                    ; _SPI MODE(&H10,0,0,0) - likewise
                                    ; no Carry check

              LD    A,(SPIADDR)
              LD    C,A
              LD    A,(SPIBUS)
              LD    B,A
              LD    D,0
              LD    E,0
              LD    A,SPI_FN_MODE
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* JVAR = 0 (A..H above are the     *
                                    ;* MCP23S17 register EQUs)          *
                                    ;************************************

              XOR   A
              LD    (JVAR),A

;---------------------------

                                    ;************************************
                                    ;* Set IODIR A/B                     *
                                    ;************************************

                                    ; K(0)=A: K(1)=B: K(2)=0

              LD    A,MCP_ADDR
              LD    (KBUF),A
              XOR   A
              LD    (KBUF+1),A
              LD    A,IODIRA
              LD    (KBUF+2),A
              XOR   A
              LD    (KBUF+3),A
              XOR   A
              LD    (KBUF+4),A
              LD    (KBUF+5),A

                                    ; _SPI WRB(&H10,0,3,K(0))

              LD    A,(SPIADDR)
              LD    H,A
              LD    A,(SPIBUS)
              LD    C,A
              LD    B,3
              LD    DE,KBUF
              LD    A,SPI_FN_WRB
              CALL  CALLFN

                                    ; K(0)=A: K(1)=C: K(2)=&HFF

              LD    A,MCP_ADDR
              LD    (KBUF),A
              XOR   A
              LD    (KBUF+1),A
              LD    A,IODIRB
              LD    (KBUF+2),A
              XOR   A
              LD    (KBUF+3),A
              LD    A,#0FF
              LD    (KBUF+4),A
              XOR   A
              LD    (KBUF+5),A

                                    ; _SPI WRB(&H10,0,3,K(0))

              LD    A,(SPIADDR)
              LD    H,A
              LD    A,(SPIBUS)
              LD    C,A
              LD    B,3
              LD    DE,KBUF
              LD    A,SPI_FN_WRB
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Main loop - JVAR is a single     *
                                    ;* byte (0..255): natural 8-bit     *
                                    ;* wraparound on INC takes it       *
                                    ;* straight from 255 back to 0, no  *
                                    ;* separate compare/reset needed.   *
                                    ;************************************

MAINLOOP:                          ; K(0)=A: K(1)=D: K(2)=J

              LD    A,MCP_ADDR
              LD    (KBUF),A
              XOR   A
              LD    (KBUF+1),A
              LD    A,GPIOA
              LD    (KBUF+2),A
              XOR   A
              LD    (KBUF+3),A
              LD    A,(JVAR)
              LD    (KBUF+4),A
              XOR   A
              LD    (KBUF+5),A

                                    ; _SPI WRB(&H10,0,3,K(0))

              LD    A,(SPIADDR)
              LD    H,A
              LD    A,(SPIBUS)
              LD    C,A
              LD    B,3
              LD    DE,KBUF
              LD    A,SPI_FN_WRB
              CALL  CALLFN

                                    ; K(0)=A+1: K(1)=E: K(2)=0

              LD    A,MCP_ADDR+1
              LD    (KBUF),A
              XOR   A
              LD    (KBUF+1),A
              LD    A,GPIOB
              LD    (KBUF+2),A
              XOR   A
              LD    (KBUF+3),A
              XOR   A
              LD    (KBUF+4),A
              LD    (KBUF+5),A

                                    ; _SPI RDH(&H10,0,2,1,K(0))

              LD    A,(SPIADDR)
              LD    C,A
              LD    A,(SPIBUS)
              LD    B,A
              LD    H,2
              LD    L,1
              LD    DE,KBUF
              LD    A,SPI_FN_RDH
              CALL  CALLFN

                                    ; I = K(2)

              LD    A,(KBUF+4)
              LD    (IVAR),A

                                    ; PRINT"OUT: ",HEX$(J),"IN: ",HEX$(I)
                                    ; - commas, not semicolons, so this
                                    ; must reproduce BASIC's print-zone
                                    ; column advance (14-column zones),
                                    ; see PRZSTR/PRZHEXV/PRZONE below

              LD    DE,MOUT
              CALL  PRZSTR
              LD    A,(JVAR)
              CALL  PRZHEXV
              LD    DE,MIN
              CALL  PRZSTR
              LD    A,(IVAR)
              CALL  PRZHEXV
              CALL  PRZCRLF

              LD    A,(JVAR)
              INC   A
              LD    (JVAR),A

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

;---------------------------

                                    ;************************************
                                    ;* PRZSTR/PRZHEXV/PRZCRLF/PRZONE -  *
                                    ;* reproduce MSX-BASIC's comma-     *
                                    ;* separated PRINT column-zone      *
                                    ;* advance (fixed 14-column zones), *
                                    ;* only needed for the one PRINT    *
                                    ;* statement in MAINLOOP that uses  *
                                    ;* commas rather than semicolons.   *
                                    ;* COLPOS tracks the current output *
                                    ;* column, reset to 0 by PRZCRLF.   *
                                    ;************************************

                                    ;************************************
                                    ;* PRZSTR - prints a $-terminated   *
                                    ;* string, advances COLPOS by its   *
                                    ;* length, then pads to the next    *
                                    ;* zone via PRZONE                  *
                                    ;************************************
                                    ;* INPUT: DE = ptr to $-terminated  *
                                    ;* string                           *
                                    ;************************************

PRZSTR:       PUSH  DE
              CALL  PRSTR
              POP   HL
              PUSH  HL

PRZSTR_SCAN:  LD    A,(HL)
              CP    '$'
              JR    Z,PRZSTR_END
              INC   HL
              JR    PRZSTR_SCAN

PRZSTR_END:   POP   DE
              OR    A
              SBC   HL,DE           ; HL = string length (excl. "$")

              LD    A,(COLPOS)
              ADD   A,L
              LD    (COLPOS),A

              JP    PRZONE

                                    ;************************************
                                    ;* PRZHEXV - PRHEXV, then advances  *
                                    ;* COLPOS by 1 or 2 (matching how   *
                                    ;* many digits PRHEXV printed),     *
                                    ;* then pads to the next zone       *
                                    ;************************************
                                    ;* INPUT: A = byte                  *
                                    ;************************************

PRZHEXV:      PUSH  AF
              CALL  PRHEXV
              POP   AF

              CP    #010
              JR    NC,PRZHV_2DIGIT

              LD    A,(COLPOS)
              INC   A
              JR    PRZHV_SET

PRZHV_2DIGIT: LD    A,(COLPOS)
              INC   A
              INC   A

PRZHV_SET:    LD    (COLPOS),A

              JP    PRZONE

                                    ;************************************
                                    ;* PRZCRLF - CRLF, resets COLPOS    *
                                    ;* to 0 (matches PRINT's own end-   *
                                    ;* of-statement newline)            *
                                    ;************************************

PRZCRLF:      LD    DE,MCRLF
              CALL  PRSTR
              XOR   A
              LD    (COLPOS),A
              RET

                                    ;************************************
                                    ;* PRZONE - pads with spaces from   *
                                    ;* the current COLPOS to the next   *
                                    ;* 14-column print zone boundary,   *
                                    ;* always at least 1 space (matches *
                                    ;* MSX-BASIC's comma-PRINT zone     *
                                    ;* advance)                         *
                                    ;************************************

PRZONE:       LD    A,' '
              CALL  PRCHR
              LD    A,(COLPOS)
              INC   A
              LD    (COLPOS),A

              LD    B,A
PRZONE_MOD:   LD    A,B
PRZONE_MODLP: CP    14
              JR    C,PRZONE_MODDONE
              SUB   14
              LD    B,A
              JR    PRZONE_MODLP
PRZONE_MODDONE: OR  A
              JR    NZ,PRZONE

              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..252, stores into SPIADDR       *
                                    ;************************************
                                    ;* OUTPUT: (SPIADDR) = 0..252,       *
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

PARSEIO_OK:   LD    (SPIADDR),A
              OR    A
              RET

                                    ;************************************
                                    ;* PARSEBUS - finds "/BUS:" in the   *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..3, stores into SPIBUS - the    *
                                    ;* SPI bus number is 2 bits (see     *
                                    ;* RCXROM.ASM's CORE52/55/56,       *
                                    ;* "AND #03")                        *
                                    ;************************************
                                    ;* OUTPUT: (SPIBUS) = 0..3,          *
                                    ;* Carry=1 if missing/invalid        *
                                    ;************************************

PARSEBUS:     LD    HL,PBUSTOK
              CALL  FINDTOK
              RET   C

              PUSH  HL
              POP   IX
              CALL  PARSEDEC
              RET   C

              LD    A,L
              CP    4
              JR    C,PARSEBUS_OK   ; 0..3 valid

              SCF
              RET

PARSEBUS_OK:  LD    (SPIBUS),A
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
              DEFM  "MCP23S17 - UNAPI/EXTBIOS driver",13,10
              DEFM  "for RCX card type F (SPI) with an",13,10
              DEFM  "MCP23S17 GPIO expander.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "MCP23S17 - UNAPI/EXTBIOS port of MCP23S17.ASC",13,10
              DEFM  "(card type F, SPI). Writes an incrementing byte",13,10
              DEFM  "to MCP23S17 GPIO A and reads GPIO B back via",13,10
              DEFM  "FN_CORE50/52/55/56 directly, no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
PBUSTOK:      DEFM  "/BUS:",0
MUSAGE:       DEFM  "Usage: MCP23S17 /IO:<0..252> /BUS:<0..3>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MOUT:         DEFM  "OUT: ","$"
MIN:          DEFM  "IN: ","$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"

SPIADDR:      DEFB  0
SPIBUS:       DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0
COLPOS:       DEFB  0             ; current PRINT column, see PRZONE

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

JVAR:         DEFB  0
IVAR:         DEFB  0
KBUF:         DEFW  0,0,0          ; K(0..2)

WORKAREA:     DEFS  #0330

              END   START
