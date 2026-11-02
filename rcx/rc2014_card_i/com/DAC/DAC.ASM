                                    ;************************************
                                    ;* DAC.COM                          *
                                    ;************************************
                                    ;* Runs the same TEST A/B/C/D as    *
                                    ;* DAC.ASC (RCX card type I, 2x     *
                                    ;* MCP4822), but via FN_CORE2/3/4/  *
                                    ;* 63..67 directly using EXTBIO     *
                                    ;* discovery. Works against         *
                                    ;* RCX_INTERFACE in ROM (CALSLT) or *
                                    ;* in mapped RAM (RCXRAM.COM, via   *
                                    ;* the RAM helper) - see CALLFN.    *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 02/11/26                *
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

RCX_FN_INIT:   EQU  2              ; RCX INIT
RCX_FN_ISINIT: EQU  3              ; RCX ISINIT
RCX_FN_DEINIT: EQU  4              ; RCX DEINIT

DAC_FN_INIT:   EQU  63             ; DAC INIT
DAC_FN_SET:    EQU  64             ; DAC SET
DAC_FN_HOLD:   EQU  65             ; DAC HOLD
DAC_FN_CONF:   EQU  66             ; DAC CONF
DAC_FN_GET:    EQU  67             ; DAC GET

UNAPI_DISC_COUNT: EQU 0            ; EXTBIO discovery: count implementations
UNAPI_DISC_ENTRY: EQU 1            ; EXTBIO discovery: slot/address

;---------------------------

START:        LD    DE,MSTART
              CALL  PRSTR

              CALL  PARSEIO
              JP    C,PARAMERR

                                    ; Locate the RAM helper, if any - a
                                    ; mapped-RAM UNAPI target (segment
                                    ; <> FFh) must be called through it

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
              LD    DE,MSEG
              CALL  PRSTR
              LD    A,(SEG)
              CALL  PRHEX2
              LD    DE,MCRLF
              CALL  PRSTR

                                    ;************************************
                                    ;* Setup: RCX ISINIT/DEINIT/INIT,   *
                                    ;* DAC INIT                         *
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

              LD    A,(IOADDR)
              LD    C,A
              LD    A,DAC_FN_INIT
              CALL  CALLFN
              JP    C,SLOTERR

;---------------------------

                                    ;************************************
                                    ;* TEST A: DAC SET + DAC GET with   *
                                    ;* 0/1365/2730/4095 on every channel*
                                    ;************************************

              XOR   A
              LD    (KVAR),A

TESTA_KLOOP:  XOR   A
              LD    (IVAR),A

TESTA_ILOOP:  LD    HL,TABA
              CALL  TABVAL           ; (VAL16) = TABA[IVAR]

              LD    A,DAC_FN_SET
              CALL  DOVAL
              JR    C,TESTA_CALL

              CALL  DOGET
              JR    C,TESTA_CALL

              CALL  CMPVAL
              JR    Z,TESTA_NEXT

              LD    DE,MFAILA
              CALL  PRFAILV
              JR    TESTA_NEXT

TESTA_CALL:   LD    DE,MFAILCALL
              CALL  PRFAILV

TESTA_NEXT:   LD    A,(IVAR)
              INC   A
              LD    (IVAR),A
              CP    4
              JR    C,TESTA_ILOOP

              LD    A,(KVAR)
              INC   A
              LD    (KVAR),A
              CP    4
              JR    C,TESTA_KLOOP

              LD    DE,MDONEA
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST B: DAC HOLD + DAC GET, the  *
                                    ;* HOLD bit must not be read back   *
                                    ;************************************

              XOR   A
              LD    (KVAR),A

TESTB_LOOP:   LD    A,(KVAR)
              LD    (IVAR),A
              LD    HL,TABB
              CALL  TABVAL           ; (VAL16) = TABB[KVAR]

              LD    A,DAC_FN_HOLD
              CALL  DOVAL
              JR    C,TESTB_CALL

              CALL  DOGET
              JR    C,TESTB_CALL

              CALL  CMPVAL
              JR    Z,TESTB_NEXT

              LD    DE,MFAILB
              CALL  PRFAILV
              JR    TESTB_NEXT

TESTB_CALL:   LD    DE,MFAILCALL
              CALL  PRFAILV

TESTB_NEXT:   LD    A,(KVAR)
              INC   A
              LD    (KVAR),A
              CP    4
              JR    C,TESTB_LOOP

              LD    DE,MDONEB
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST C: all 0 V, 1/2/3 V with    *
                                    ;* HOLD, channel 3 with SET = all   *
                                    ;* outputs at once                  *
                                    ;************************************

              XOR   A
              LD    (KVAR),A

TESTC_ZERO:   LD    HL,0
              LD    (VAL16),HL
              LD    A,DAC_FN_SET
              CALL  DOVAL
              CALL  C,INCER
              LD    A,(KVAR)
              INC   A
              LD    (KVAR),A
              CP    4
              JR    C,TESTC_ZERO

              XOR   A
              LD    (KVAR),A

TESTC_HOLD:   LD    A,(KVAR)
              LD    (IVAR),A
              LD    HL,TABC
              CALL  TABVAL           ; (VAL16) = 1000/2000/3000
              LD    A,DAC_FN_HOLD
              CALL  DOVAL
              CALL  C,INCER
              LD    A,(KVAR)
              INC   A
              LD    (KVAR),A
              CP    3
              JR    C,TESTC_HOLD

              LD    DE,MHOLD0V
              CALL  WAITKEY

              LD    A,3
              LD    (KVAR),A
              LD    HL,4000
              LD    (VAL16),HL
              LD    A,DAC_FN_SET
              CALL  DOVAL
              CALL  C,INCER

              LD    DE,MSYNC
              CALL  WAITKEY

;---------------------------

                                    ;************************************
                                    ;* TEST D: channel 0 (1000) gain x1 *
                                    ;* -> 0.5 V, shutdown -> 0 V, back  *
                                    ;* to gain x2                       *
                                    ;************************************

              XOR   A
              LD    (KVAR),A

              XOR   A
              CALL  DOCONF
              CALL  C,INCER
              LD    DE,MGAIN1
              CALL  WAITKEY

              LD    A,3
              CALL  DOCONF
              CALL  C,INCER
              LD    DE,MSHDN
              CALL  WAITKEY

              LD    A,1
              CALL  DOCONF
              CALL  C,INCER
              LD    DE,MGAIN2
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* SUMMARY + RCX DEINIT             *
                                    ;************************************

              LD    A,(ER)
              OR    A
              JR    NZ,SUMMARY_FAIL

              LD    DE,MALLPASS
              CALL  PRSTR
              JR    SUMMARY_DONE

SUMMARY_FAIL: LD    A,(ER)
              CALL  PRDECB
              LD    DE,MCHECKFAIL
              CALL  PRSTR

SUMMARY_DONE: LD    A,RCX_FN_DEINIT
              CALL  CALLFN

              RET

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

SLOTERR:      LD    DE,MSLOTERR
              CALL  PRSTR
              RET

;---------------------------

                                    ;************************************
                                    ;* DOVAL - DAC SET/HOLD(IOADDR,     *
                                    ;* KVAR,VAL16)                      *
                                    ;************************************
                                    ;* INPUT: A = DAC_FN_SET/DAC_FN_HOLD*
                                    ;* OUTPUT: Carry = 1 -> error       *
                                    ;************************************

DOVAL:        PUSH  AF
              LD    A,(IOADDR)
              LD    C,A
              LD    A,(KVAR)
              LD    B,A
              LD    DE,(VAL16)
              POP   AF
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* DOCONF - DAC CONF(IOADDR,KVAR,A) *
                                    ;************************************
                                    ;* INPUT: A = configuration 0..3    *
                                    ;* OUTPUT: Carry = 1 -> error       *
                                    ;************************************

DOCONF:       LD    E,A
              LD    A,(IOADDR)
              LD    C,A
              LD    A,(KVAR)
              LD    B,A
              LD    A,DAC_FN_CONF
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* DOGET - DAC GET(IOADDR,KVAR),    *
                                    ;* result in (GETBUF)               *
                                    ;************************************
                                    ;* OUTPUT: Carry = 1 -> error       *
                                    ;************************************

DOGET:        LD    HL,#0FFFF
              LD    (GETBUF),HL
              LD    A,(IOADDR)
              LD    C,A
              LD    A,(KVAR)
              LD    B,A
              LD    DE,GETBUF
              LD    A,DAC_FN_GET
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* TABVAL - (VAL16) = word table at *
                                    ;* HL, index (IVAR)                 *
                                    ;************************************

TABVAL:       LD    A,(IVAR)
              ADD   A,A
              LD    E,A
              LD    D,0
              ADD   HL,DE
              LD    E,(HL)
              INC   HL
              LD    D,(HL)
              LD    (VAL16),DE
              RET

                                    ;************************************
                                    ;* CMPVAL - Z = 1 if (GETBUF) =     *
                                    ;* (VAL16)                          *
                                    ;************************************

CMPVAL:       LD    HL,(GETBUF)
              LD    DE,(VAL16)
              OR    A
              SBC   HL,DE
              RET

                                    ;************************************
                                    ;* PRFAILV - prints "<DE> ch=K      *
                                    ;* value=V read=R" and counts it    *
                                    ;************************************

PRFAILV:      CALL  PRSTR
              CALL  INCER
              LD    DE,MCH
              CALL  PRSTR
              LD    A,(KVAR)
              CALL  PRDECB
              LD    DE,MVALUE
              CALL  PRSTR
              LD    HL,(VAL16)
              CALL  PRDECW
              LD    DE,MREAD
              CALL  PRSTR
              LD    HL,(GETBUF)
              CALL  PRDECW
              LD    DE,MCRLF
              CALL  PRSTR
              RET

                                    ;************************************
                                    ;* WAITKEY - prints DE, then waits  *
                                    ;* for a key (BDOS 08h, no echo)    *
                                    ;************************************

WAITKEY:      CALL  PRSTR
              LD    DE,MPRESS
              CALL  PRSTR
              LD    C,8
              CALL  BDOS
              LD    DE,MCRLF
              CALL  PRSTR
              RET

                                    ;************************************
                                    ;* INCER - (ER) = (ER)+1            *
                                    ;************************************

INCER:        LD    A,(ER)
              INC   A
              LD    (ER),A
              RET

;---------------------------

                                    ;************************************
                                    ;* PRDECB - byte (0..255) decimal,  *
                                    ;* no leading zeros                 *
                                    ;************************************

PRDECB:       LD    L,A
              LD    H,0

                                    ;************************************
                                    ;* PRDECW - word (0..65535) decimal,*
                                    ;* no leading zeros                 *
                                    ;************************************
                                    ;* INPUT: HL = value                *
                                    ;************************************

PRDECW:       LD    C,0              ; C = 1 after the first digit shown
              LD    DE,10000
              CALL  PW_DIGIT
              LD    DE,1000
              CALL  PW_DIGIT
              LD    DE,100
              CALL  PW_DIGIT
              LD    DE,10
              CALL  PW_DIGIT
              LD    A,L
              ADD   A,'0'
              CALL  PRCHR
              RET

PW_DIGIT:     LD    B,0
PW_DLOOP:     OR    A
              SBC   HL,DE
              JR    C,PW_DDONE
              INC   B
              JR    PW_DLOOP
PW_DDONE:     ADD   HL,DE
              LD    A,B
              OR    C
              RET   Z
              LD    A,B
              ADD   A,'0'
              CALL  PRCHR
              LD    C,1
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..252, stores into IOADDR        *
                                    ;************************************
                                    ;* OUTPUT: (IOADDR) = 0..252,        *
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

PARSEIO_OK:   LD    (IOADDR),A
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
                                    ;* CALLFN - calls FN_CORExx. Two    *
                                    ;* paths depending on where the     *
                                    ;* discovered implementation lives: *
                                    ;* SEG=FFh (ROM, no mapper segment) *
                                    ;* uses plain CALSLT; SEG<>FFh      *
                                    ;* (mapped RAM, e.g. a RCXRAM.COM   *
                                    ;* install) builds IY=slot/segment  *
                                    ;* and jumps through the RAM        *
                                    ;* helper's register-based call     *
                                    ;* entry (HELPERADD+0) instead      *
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
                                    ;* PRSTR - print a string           *
                                    ;************************************
                                    ;* INPUT: DE = ptr to $-terminated  *
                                    ;* string                           *
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

                                    ;************************************
                                    ;* PRCHR - print one character      *
                                    ;************************************
                                    ;* INPUT: A = character             *
                                    ;************************************

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

                                    ;************************************
                                    ;* PRHEX2 - byte as 2 hex digits    *
                                    ;************************************
                                    ;* INPUT: A = byte                  *
                                    ;************************************

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

IDSTR:        DEFM  "RCX_INTERFACE",0
IDSTRLEN:     EQU   $-IDSTR

TABA:         DEFW  0,1365,2730,4095
TABB:         DEFW  4095,3095,2095,1095
TABC:         DEFW  1000,2000,3000

MSTART:       DEFB  12
              DEFM  "DAC (ASM) - UNAPI/EXTBIOS test for",13,10
              DEFM  "RCX card type I (2x MCP4822).",13,10
              DEFM  "Runs TEST A/B/C/D via",13,10
              DEFM  "FN_CORE63..67 directly.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
MUSAGE:       DEFM  "Usage: DAC /IO:<0..252>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MSEG:         DEFM  " seg=","$"
MCRLF:        DEFM  13,10,"$"

MFAILA:       DEFM  "FAIL A","$"
MFAILB:       DEFM  "FAIL B","$"
MFAILCALL:    DEFM  "FAIL call (Carry)","$"
MCH:          DEFM  " ch=","$"
MVALUE:       DEFM  " value=","$"
MREAD:        DEFM  " read=","$"
MDONEA:       DEFM  "Test A (SET/GET readback) done",13,10,"$"
MDONEB:       DEFM  "Test B (HOLD/GET readback) done",13,10,13,10,"$"

MHOLD0V:      DEFM  "X101.2-5 must still be 0 V",13,10
              DEFM  "(HOLD).","$"
MSYNC:        DEFM  "X101.2-5 now 1.000/2.000/",13,10
              DEFM  "3.000/4.000 V.","$"
MGAIN1:       DEFM  "Gain x1: X101.2 must be 0.500 V.","$"
MSHDN:        DEFM  "Shutdown: X101.2 must be 0 V.","$"
MGAIN2:       DEFM  "Gain x2: X101.2 must be 1.000 V",13,10
              DEFM  "again.",13,10,13,10,"$"
MPRESS:       DEFM  13,10,"Press a key.","$"

MALLPASS:     DEFM  "ALL READBACK CHECKS PASSED",13,10,"$"
MCHECKFAIL:   DEFM  " CHECK(S) FAILED",13,10,"$"

MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MSLOTERR:     DEFM  "DAC INIT failed (Carry).",13,10,"$"

IOADDR:       DEFB  0
FINDTOK_NDL:  DEFW  0

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0
ER:           DEFB  0

KVAR:         DEFB  0
IVAR:         DEFB  0
VAL16:        DEFW  0
GETBUF:       DEFW  0

WORKAREA:     DEFS  #0330
