                                    ;************************************
                                    ;* XIOSLT.COM                       *
                                    ;************************************
                                    ;* Diagnostic tool, no XIO/RCX UNAPI *
                                    ;* calls - reads MSX BIOS structures *
                                    ;* directly. Dumps the 8-byte slot   *
                                    ;* work area (SLTWRK) for a chosen   *
                                    ;* main slot, then follows the       *
                                    ;* pointer stored at its bytes 6+7   *
                                    ;* and dumps &H60 bytes from there.  *
                                    ;* Assembled with pasmo.             *
                                    ;************************************

              ORG   #0100

;---------------------------

                                    ;************************************
                                    ;* MSX-DOS/BIOS constants            *
                                    ;************************************

BDOS:         EQU   #00005
LINLEN:       EQU   #0F3B0         ; current active screen text width
SLTWRK:       EQU   #0FD09         ; MSX BIOS slot work area, 32 bytes
                                    ; per main slot

CMDTAIL:      EQU   #0080         ; MSX-DOS command tail: length byte,
                                    ; followed by the raw text at
                                    ; CMDTAIL+1

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

              CALL  PARSESLOT
              JP    C,PARAMERR

                                    ; BASEADDR = SLTWRK + SLOTNUM*32

              LD    A,(SLOTNUM)
              LD    L,A
              LD    H,0
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,HL
              LD    DE,SLTWRK
              ADD   HL,DE
              LD    (BASEADDR),HL

;---------------------------

                                    ;************************************
                                    ;* Slot work area                    *
                                    ;************************************

              LD    DE,MDUMP1
              CALL  PRSTR

              LD    HL,(BASEADDR)
              LD    B,8
              CALL  DUMPLINE
              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* Dump via ptr bytes 6+7             *
                                    ;************************************

              LD    HL,(BASEADDR)
              LD    DE,6
              ADD   HL,DE
              LD    A,(HL)
              LD    C,A
              INC   HL
              LD    A,(HL)
              LD    H,A
              LD    L,C
              LD    (PTRADDR),HL

              LD    DE,MDUMP2
              CALL  PRSTR

              LD    HL,(PTRADDR)
              LD    C,12             ; 96 bytes = 12 lines of 8

DUMPLOOP:     PUSH  BC
              LD    B,8
              CALL  DUMPLINE
              LD    DE,MCRLF
              CALL  PRSTR
              POP   BC
              DEC   C
              JR    NZ,DUMPLOOP

              RET

PARAMERR:     LD    DE,MUSAGE
              CALL  PRSTR
              RET

;---------------------------

                                    ;************************************
                                    ;* DUMPLINE - prints B bytes from HL  *
                                    ;* as 2-digit hex plus a space each,  *
                                    ;* advancing HL                       *
                                    ;************************************
                                    ;* INPUT: HL=ptr, B=count             *
                                    ;************************************

DUMPLINE:     PUSH  BC
              LD    A,(HL)
              CALL  PRHEX2
              LD    A,' '
              CALL  PRCHR
              INC   HL
              POP   BC
              DJNZ  DUMPLINE
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSESLOT - finds "/SLOT:" in the *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..3, stores into SLOTNUM         *
                                    ;************************************
                                    ;* OUTPUT: (SLOTNUM) = 0..3,         *
                                    ;* Carry=1 if missing/invalid        *
                                    ;************************************

PARSESLOT:    LD    HL,SLOTTOK
              CALL  FINDTOK
              RET   C

              PUSH  HL
              POP   IX
              CALL  PARSEDEC
              RET   C

              LD    A,L
              CP    4
              JR    C,PARSESLOT_OK  ; 0..3 valid

              SCF
              RET

PARSESLOT_OK: LD    (SLOTNUM),A
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

;---------------------------

                                    ;************************************
                                    ;* Data                             *
                                    ;************************************

MSTART40:     DEFB  12
              DEFM  "XIOSLT - dumps the MSX BIOS slot",13,10
              DEFM  "work area (SLTWRK) for a chosen",13,10
              DEFM  "main slot, then follows the ptr",13,10
              DEFM  "at bytes 6+7 and dumps &H60",13,10
              DEFM  "bytes from there.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "XIOSLT - UNAPI-free port of XIOSLT.ASC. Dumps the",13,10
              DEFM  "8-byte MSX BIOS slot work area (SLTWRK+slot*32) for",13,10
              DEFM  "a chosen main slot, then follows the pointer stored",13,10
              DEFM  "at its bytes 6+7 and dumps &H60 bytes from there -",13,10
              DEFM  "no XIO/RCX UNAPI calls, direct BIOS memory reads.",13,10
              DEFM  13,10,"$"

SLOTTOK:      DEFM  "/SLOT:",0
MUSAGE:       DEFM  "Usage: XIOSLT /SLOT:<0..3>",13,10,"$"
MDUMP1:       DEFM  "Slot Work Area",13,10,"$"
MDUMP2:       DEFM  "Slot Work Area from Ptr Byte 6+7",13,10,"$"
MCRLF:        DEFM  13,10,"$"

SLOTNUM:      DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0
BASEADDR:     DEFW  0
PTRADDR:      DEFW  0

              END   START
