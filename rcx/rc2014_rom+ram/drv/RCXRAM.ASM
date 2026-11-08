                                    ;************************************
                                    ;* RCXRAM.COM                       *
                                    ;************************************
                                    ;* RAM-installable UNAPI driver for *
                                    ;* RCX_INTERFACE - full Kartentyp A *
                                    ;* (I2C/RTC), B (Z80 PIO), C (82C55 *
                                    ;* PPI), D (82C54 timer), E (CAN/   *
                                    ;* SJA1000), F (SPI), G (ADS1220)   *
                                    ;* and I (MCP4822), functions       *
                                    ;* 2,3,4,5-15,16-22,23-32,33-42,    *
                                    ;* 43-56,57-62,63-67.               *
                                    ;************************************
                                    ;* Installation code adapted from   *
                                    ;* Konamiman's unapi-ram.asm        *
                                    ;* (MSX-UNAPI-specification repo).  *
                                    ;* Driver routines                  *
                                    ;* (RCXINIT/GETRCXSLT/SETRCXSLT/    *
                                    ;* IOEN/IODIS/FN_CORE2-15/16-22/    *
                                    ;* 23-32/33-42/43-56/57-67/CANOUT/  *
                                    ;* CANINP/I2CBCTRL/I2CPCTRL)        *
                                    ;* are taken from RCXROM.ASM, ORG   *
                                    ;* #04000 in both cases so the      *
                                    ;* logic ports unchanged. Needs the *
                                    ;* UNAPI RAM helper (ramhelpr.com)  *
                                    ;* installed first. Assembled with  *
                                    ;* pasmo.                           *
                                    ;************************************
                                    ;* Deliberate differences from the  *
                                    ;* ROM driver, see DRV/README.md:   *
                                    ;* 1) GETRAM/SETRAM no longer       *
                                    ;* read/write SLTWRK - a mapped-RAM *
                                    ;* segment can be shared by         *
                                    ;* unrelated code, so the ROM's     *
                                    ;* one-slot-one-owner assumption    *
                                    ;* does not hold. The work area is  *
                                    ;* a fixed address inside this      *
                                    ;* driver's own segment instead.    *
                                    ;* 2) Error paths that used to jump *
                                    ;* into BASIC's error trap          *
                                    ;* (CORE58ERR, CORETIMERR/          *
                                    ;* CORETIMERR1, COREDACERR,         *
                                    ;* SETRCXSLT's                      *
                                    ;* FRERCXERR) now just return Carry *
                                    ;* set - there is no BASIC          *
                                    ;* interpreter to jump into.        *
                                    ;* 3) HCANINT (Kartentyp E's real   *
                                    ;* hardware interrupt handler)      *
                                    ;* lives at a fixed page-3 address  *
                                    ;* (R_CAN_HCANPTR/HCANMSG/HCANINT), *
                                    ;* not inside WORKAREA - WORKAREA   *
                                    ;* is only mapped while this        *
                                    ;* driver's own CALLFN-dispatched   *
                                    ;* code runs, but a real interrupt  *
                                    ;* fires asynchronously. See        *
                                    ;* DRV/README.md.                   *
                                    ;************************************
                                    ;* VERSION: 02/11/26                *
                                    ;************************************

;*******************
;***  CONSTANTS  ***
;*******************

_TERM0:  EQU     #000
_STROUT: EQU     #009

ENASLT: EQU     #0024
EXTBIO: EQU     #0FFCA
ARG:    EQU     #0F847

API_V_P:        EQU     1
API_V_S:        EQU     0
ROM_V_P:        EQU     1
ROM_V_S:        EQU     0

MAX_FN:         EQU     68              ; highest implemented function number
MAX_IMPFN:      EQU     0

;---------------------------

;***************************
;***  INSTALLATION CODE  ***
;***************************

        ORG     #0100

        LD      DE,WELCOME_S
        LD      C,_STROUT
        CALL    5

                                    ;--- Back up the segment body to a
                                    ; low, always-valid buffer, BEFORE
                                    ; touching page 1 at all. pasmo (no
                                    ; phase/virtual-org support) emits a
                                    ; real forward-ORG gap for "ORG
                                    ; #04000" below, so SEG_CODE (the
                                    ; label right before that ORG) is
                                    ; NOT an embedded low copy of the
                                    ; body - it just sits in the zero
                                    ; padding. The .COM loader itself
                                    ; puts the real body at address
                                    ; #04000 as part of loading this
                                    ; program (confirmed via hex dump:
                                    ; real DO_EXTBIO bytes sit at file
                                    ; offset #03F00, not at SEG_CODE's
                                    ; offset). Copy it out here, while
                                    ; page 1 still holds our own
                                    ; original mapping, so it survives
                                    ; later page 1 switches.

        LD      HL,#04000
        LD      DE,SEG_CODE_BACKUP
        LD      BC,SEG_CODE_END-SEG_CODE_START
        LDIR

        ;--- Locate the RAM helper, terminate with error if not installed

        LD      DE,#02222
        LD      HL,0
        LD      A,#0FF
        CALL    EXTBIO
        LD      A,H
        OR      L
        JR      NZ,HELPER_OK

        LD      DE,NOHELPER_S
        LD      C,_STROUT
        CALL    5
        LD      C,_TERM0
        JP      5
HELPER_OK:
        LD      (HELPER_ADD),HL
        LD      (MAPTAB_ADD),BC

        ;--- Check if we are already installed (RCX_INTERFACE may already
        ;    be provided by the ROM cartridge, or by an earlier install)

        LD      HL,UNAPI_ID-SEG_CODE_START+SEG_CODE_BACKUP
        LD      DE,ARG
        LD      BC,UNAPI_ID_END-UNAPI_ID
        LDIR

        LD      DE,#02222
        XOR     A
        LD      B,0
        CALL    EXTBIO
        LD      A,B
        OR      A
        JR      Z,NOT_INST

IMPL_LOOP:      PUSH    AF

        LD      DE,#02222
        CALL    EXTBIO
        LD      (ALLOC_SLOT),A
        LD      A,B
        LD      (ALLOC_SEG),A
        LD      (IMPLEM_ENTRY),HL

        LD      A,H
        AND     #080
        JR      NZ,NEXT_IMP

        LD      A,(ALLOC_SLOT)
        LD      IYH,A
        LD      A,(ALLOC_SEG)
        LD      IYL,A
        LD      IX,(IMPLEM_ENTRY)
        LD      HL,(HELPER_ADD)
        XOR     A
        CALL    CALL_HL         ; returns HL=name address

        LD      A,(ALLOC_SEG)
        LD      B,A
        LD      DE,APIINFO-SEG_CODE_START+SEG_CODE_BACKUP
        LD      IX,(HELPER_ADD)
        INC     IX
        INC     IX
        INC     IX              ; IX=helper routine to read from segment
NAME_LOOP:      LD      A,(ALLOC_SLOT)
        PUSH    BC
        PUSH    DE
        PUSH    HL
        PUSH    IX
        CALL    CALL_IX
        POP     IX
        POP     HL
        POP     DE
        POP     BC
        LD      C,A
        LD      A,(DE)
        CP      C
        JR      NZ,NEXT_IMP
        OR      A
        INC     HL
        INC     DE
        JR      NZ,NAME_LOOP

        LD      DE,ALINST_S
        LD      C,_STROUT
        CALL    5
        LD      C,_TERM0
        JP      5

NEXT_IMP:       POP     AF
        DEC     A
        JR      NZ,IMPL_LOOP

NOT_INST:

        ;--- Obtain the mapper support routines table, if available

        XOR     A
        LD      DE,#00402
        CALL    EXTBIO
        OR      A
        JR      NZ,ALLOC_DOS2

        ;--- DOS 1: use the last segment on the primary mapper

        LD      A,2
        LD      (MAPTAB_ENTRY_SIZE),A

        LD      HL,(MAPTAB_ADD)
        LD      B,(HL)
        INC     HL
        LD      A,(HL)
        JR      ALLOC_OK

ALLOC_DOS2:
        LD      A,B
        LD      (PRIM_SLOT),A
        LD      DE,ALL_SEG
        LD      BC,15*3
        LDIR

        LD      DE,#00401
        CALL    EXTBIO
        LD      (MAPTAB_ADD),HL

        LD      A,8
        LD      (MAPTAB_ENTRY_SIZE),A

        LD      A,(PRIM_SLOT)
        OR      #020
        LD      B,A
        LD      A,1             ; system segment
        CALL    ALL_SEG
        JR      NC,ALLOC_OK

        LD      DE,NOFREE_S
        LD      C,_STROUT
        CALL    5
        LD      C,_TERM0
        JP      5

ALLOC_OK:
        LD      (ALLOC_SEG),A
        LD      A,B
        LD      (ALLOC_SLOT),A

        ;--- Switch segment, copy code, and setup data

        CALL    GET_P1
        LD      (P1_SEG),A

        LD      A,(ALLOC_SLOT)
        LD      H,#040
        CALL    ENASLT
        LD      A,(ALLOC_SEG)
        CALL    PUT_P1

        LD      HL,#04000
        LD      DE,#04001
        LD      BC,#04000-1
        LD      (HL),0
        LDIR

        LD      HL,SEG_CODE_BACKUP
        LD      DE,#04000
        LD      BC,SEG_CODE_END-SEG_CODE_START
        LDIR

        LD      HL,(ALLOC_SLOT)
        LD      (MY_SLOT),HL
        LD      HL,(HELPER_ADD)
        LD      (MY_HELPER),HL

        ;--- Backup and patch the EXTBIO hook

        LD      HL,EXTBIO
        LD      DE,OLD_EXTBIO
        LD      BC,5
        LDIR

        DI
        LD      A,#0CD          ; code for "CALL"
        LD      (EXTBIO),A
        LD      HL,(HELPER_ADD)
        LD      BC,6
        ADD     HL,BC           ; HL points to segment call routine
        LD      (EXTBIO+1),HL

        LD      HL,(MAPTAB_ADD)
        LD      A,(ALLOC_SLOT)
        LD      BC,(MAPTAB_ENTRY_SIZE)
        LD      B,0
        LD      D,A
        LD      E,0
SRCHMAP:
        LD      A,(HL)
        CP      D
        JR      Z,MAPFND
        ADD     HL,BC
        INC     E
        JR      SRCHMAP
MAPFND:
        LD      A,E
        RRCA
        RRCA
        AND     #0C0
        LD      (EXTBIO+3),A

        LD      A,(ALLOC_SEG)
        LD      (EXTBIO+4),A
        EI

        LD      A,(PRIM_SLOT)
        LD      H,#040
        CALL    ENASLT
        LD      A,(P1_SEG)
        CALL    PUT_P1

        LD      DE,OK_S
        LD      C,_STROUT
        CALL    5

        LD      C,_TERM0
        JP      5

CALL_IX:        JP      (IX)
CALL_HL:        JP      (HL)

;---------------------------

;----------------------------------------------------
;--- Data and strings for the installation code
;----------------------------------------------------

PRIM_SLOT:      DEFB    0
P1_SEG: DEFB    0
ALLOC_SLOT:     DEFB    0
ALLOC_SEG:      DEFB    0
HELPER_ADD:     DEFW    0
MAPTAB_ADD:     DEFW    0
MAPTAB_ENTRY_SIZE: DEFB 0
IMPLEM_ENTRY:   DEFW    0

                                    ;--- Low, always-valid copy of the
                                    ; segment body - see the comment at
                                    ; the top of the installation code.
                                    ; Fixed size (pasmo can't forward-
                                    ; reference SEG_CODE_END here) -
                                    ; comfortably larger than the
                                    ; current ~5.6 KB body (Kartentyp A,
                                    ; B, C, D, E, F, G).

SEG_CODE_BACKUP_SIZE: EQU      #03000
SEG_CODE_BACKUP: DEFS   SEG_CODE_BACKUP_SIZE

        ;--- Mapper support routines, used at install time only

ALL_SEG:        DEFS    3
FRE_SEG:        DEFS    3
RD_SEG: DEFS    3
WR_SEG: DEFS    3
CAL_SEG:        DEFS    3
CALLS:  DEFS    3
PUT_PH: DEFS    3
GET_PH: DEFS    3
PUT_P0: DEFS    3
GET_P0: DEFS    3
PUT_P1: JP      _PUT_P1
GET_P1: LD      A,2
        RET
PUT_P2: DEFS    3
GET_P2: DEFS    3
PUT_P3: DEFS    3
_PUT_P1:
        LD      (GET_P1+1),A
        OUT     (#0FD),A
        RET

WELCOME_S:
        DEFM    "RCXRAM - RAM-installable RCX_INTERFACE UNAPI",13,10
        DEFM    "driver (RCX INIT/ISINIT/DEINIT/TABLE, full",13,10
        DEFM    "Kartentyp A/I2C+RTC, B/Z80 PIO, C/82C55 PPI,",13,10
        DEFM    "D/82C54, E/CAN (SJA1000), F/SPI and G/ADS1220",13,10
        DEFM    "- functions 2,3,4,5-15,16-22,23-32,33-42,",13,10
        DEFM    "43-56,57-62).",13,10
        DEFM    13,10,"$"

NOHELPER_S:
        DEFM    "*** ERROR: No UNAPI RAM helper is installed",13,10
        DEFM    "    (run ramhelpr.com first)",13,10,"$"

NOFREE_S:
        DEFM    "*** ERROR: Could not allocate any RAM segment",13,10,"$"

OK_S:   DEFM    "Installed. Run a UNAPI-aware .COM (e.g. RTCLCD/",13,10
        DEFM    "RTCLCDI/8254/8254I/SJA1000/ADS1220) to use it.",13,10,"$"

ALINST_S:
        DEFM    "*** RCX_INTERFACE is already installed (ROM",13,10
        DEFM    "    cartridge or an earlier RAM install).",13,10,"$"

;---------------------------

;*********************************************
;***  CODE TO BE INSTALLED ON RAM SEGMENT  ***
;*********************************************

        ORG     #04000
SEG_CODE_START:

;===============================
;===  EXTBIO hook execution  ===
;===============================

                                    ; Note: this code starts exactly at
                                    ; address #04000, same as
                                    ; RCXROM.ASM's ORG - DO_EXTBIO/
                                    ; UNAPI_ENTRY below are unchanged
                                    ; from the ROM driver

DO_EXTBIO:
        PUSH    HL
        PUSH    BC
        PUSH    AF
        LD      A,D
        CP      #008            ; Device 8 = RS-232C?
        JP      Z,DO_EXT8
        CP      #022
        JR      NZ,JUMP_OLD
        CP      E
        JR      NZ,JUMP_OLD

        ; Check API ID

        LD      HL,UNAPI_ID
        LD      DE,ARG
LOOP1:  LD      A,(DE)
        CALL    TOUPPER
        CP      (HL)
        JR      NZ,JUMP_OLD2
        INC     HL
        INC     DE
        OR      A
        JR      NZ,LOOP1

        ; A=255: jump to old hook

        POP     AF
        PUSH    AF
        INC     A
        JR      Z,JUMP_OLD2

        ; A=0: B=B+1 and jump to old hook

        POP     AF
        POP     BC
        OR      A
        JR      NZ,DO_EXTBIO2
        INC     B
        POP     HL
        LD      DE,#02222
        JP      OLD_EXTBIO
DO_EXTBIO2:

        ; A=1: return A=slot, B=segment, HL=UNAPI entry address

        DEC     A
        JR      NZ,DO_EXTBIO3
        POP     HL
        LD      A,(MY_SEG)
        LD      B,A
        LD      A,(MY_SLOT)
        LD      HL,UNAPI_ENTRY
        LD      DE,#02222
        RET

        ; A>1: A=A-1, and jump to old hook

DO_EXTBIO3:
        POP     HL
        LD      DE,#02222
        JP      OLD_EXTBIO

JUMP_OLD2:
        LD      DE,#02222
JUMP_OLD:                           ; assumes "push hl,bc,af" done
        POP     AF
        POP     BC
        POP     HL

                                    ; old EXTBIO hook contents is here
                                    ; (setup at installation time)

OLD_EXTBIO:
        DEFS    5

;====================================
;===  Functions entry point code  ===
;====================================

UNAPI_ENTRY:
        PUSH    HL
        PUSH    AF
        LD      HL,FN_TABLE
        BIT     7,A
        JR      NZ,UNDEFINED

        CP      MAX_FN
        JR      Z,OK_FNUM
        JR      NC,UNDEFINED

OK_FNUM:
        ADD     A,A
        PUSH    DE
        LD      E,A
        LD      D,0
        ADD     HL,DE
        POP     DE

        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A

        POP     AF
        EX      (SP),HL
        RET

UNDEFINED:
        POP     AF
        POP     HL
        RET

;===================================
;===  Functions addresses table  ===
;===================================

                                    ; 0 and 2-62 as in RCXROM; 1 (help
                                    ; screen of CALL RCX ?) is UNDEFINED

FN_TABLE:
        DEFW    FN_INFO         ; 0  mandatory: API/implementation info
        DEFW    UNDEFINED       ; 1
        DEFW    FN_CORE2        ; 2  RCX INIT
        DEFW    FN_CORE3        ; 3  RCX ISINIT
        DEFW    FN_CORE4        ; 4  RCX DEINIT
        DEFW    FN_CORE5        ; 5  RCX TABLE
        DEFW    FN_CORE6        ; 6  I2C INIT
        DEFW    FN_CORE7        ; 7  I2C WR
        DEFW    FN_CORE8        ; 8  I2C WRB
        DEFW    FN_CORE9        ; 9  I2C RD
        DEFW    FN_CORE10       ; 10 I2C RDB
        DEFW    FN_CORE11       ; 11 I2C RESET
        DEFW    FN_CORE12       ; 12 SET TIME
        DEFW    FN_CORE13       ; 13 GET TIME
        DEFW    FN_CORE14       ; 14 SET DATE
        DEFW    FN_CORE15       ; 15 GET DATE
        DEFW    FN_CORE16       ; 16 PIO INIT
        DEFW    FN_CORE17       ; 17 PIO A RD
        DEFW    FN_CORE18       ; 18 PIO A WR
        DEFW    FN_CORE19       ; 19 PIO B RD
        DEFW    FN_CORE20       ; 20 PIO B WR
        DEFW    FN_CORE21       ; 21 PIO A CTRL
        DEFW    FN_CORE22       ; 22 PIO B CTRL
        DEFW    FN_CORE23       ; 23 PPI INIT
        DEFW    FN_CORE24       ; 24 PPI A RD
        DEFW    FN_CORE25       ; 25 PPI A WR
        DEFW    FN_CORE26       ; 26 PPI B RD
        DEFW    FN_CORE27       ; 27 PPI B WR
        DEFW    FN_CORE28       ; 28 PPI C RD
        DEFW    FN_CORE29       ; 29 PPI C WR
        DEFW    FN_CORE30       ; 30 PPI CTRL
        DEFW    FN_CORE31       ; 31 PPI SET
        DEFW    FN_CORE32       ; 32 PPI RESET
        DEFW    FN_CORE33       ; 33 TIMER INIT
        DEFW    FN_CORE34       ; 34 TIMER 0 RD
        DEFW    FN_CORE35       ; 35 TIMER 0 WR
        DEFW    FN_CORE36       ; 36 TIMER 1 RD
        DEFW    FN_CORE37       ; 37 TIMER 1 WR
        DEFW    FN_CORE38       ; 38 TIMER 2 RD
        DEFW    FN_CORE39       ; 39 TIMER 2 WR
        DEFW    FN_CORE40       ; 40 TIMER CTRL
        DEFW    FN_CORE41       ; 41 TIMER SET GATE
        DEFW    FN_CORE42       ; 42 TIMER RES GATE
        DEFW    FN_CORE43       ; 43 CAN INIT
        DEFW    FN_CORE44       ; 44 CAN WR
        DEFW    FN_CORE45       ; 45 CAN RD
        DEFW    FN_CORE46       ; 46 CAN CHECK
        DEFW    FN_CORE47       ; 47 CAN RX
        DEFW    FN_CORE48       ; 48 CAN GET ADDR
        DEFW    FN_CORE49       ; 49 CAN TX
        DEFW    FN_CORE50       ; 50 SPI INIT
        DEFW    FN_CORE51       ; 51 SPI WR
        DEFW    FN_CORE52       ; 52 SPI WRB
        DEFW    FN_CORE53       ; 53 SPI RD
        DEFW    FN_CORE54       ; 54 SPI RDB
        DEFW    FN_CORE55       ; 55 SPI RDH
        DEFW    FN_CORE56       ; 56 SPI MODE
        DEFW    FN_CORE57       ; 57 ADS INIT
        DEFW    FN_CORE58       ; 58 ADS MODE
        DEFW    FN_CORE59       ; 59 ADS CMD
        DEFW    FN_CORE60       ; 60 ADS STATUS
        DEFW    FN_CORE61       ; 61 ADS READ
        DEFW    FN_CORE62       ; 62 ADS GET
        DEFW    FN_CORE63       ; 63 DAC INIT
        DEFW    FN_CORE64       ; 64 DAC SET
        DEFW    FN_CORE65       ; 65 DAC HOLD
        DEFW    FN_CORE66       ; 66 DAC CONF
        DEFW    FN_CORE67       ; 67 DAC GET
        DEFW    FN_CORE68       ; 68 COM INIT

;========================================================
;===  RCXROM.ASM routines, ported unchanged unless   ===
;===  noted otherwise (see header comment)             ===
;========================================================

                                    ;************************************
                                    ;* RCX card types / IO addresses    *
                                    ;* - copied from RCXROM.ASM         *
                                    ;************************************

WTEC:         EQU  #0C0
WTECINV:      EQU  #03F
IOENDIS:      EQU  #040
ALE:          EQU  #046
SWIO:         EQU  #047

SINIT:        EQU  #0AA
SDEL:         EQU  #055
SADS:         EQU  'G'
SI2C:         EQU  'A'         ; I2C basic card with PCF8584+PCF8583
SPIO:         EQU  'B'         ; Z80 PIO card
SPPI:         EQU  'C'         ; 82C55 PPI card
SCNT:         EQU  'D'         ; 82C54 TIMER card
SCAN:         EQU  'E'         ; SJA1000 card (only once allowed)
SSPI:         EQU  'F'         ; SPI master card
SDAC:         EQU  'I'         ; MCP4822 D/A board
SCOM:         EQU  'H'         ; SC725 SIO/CTC card as COM port

X_CD:         EQU  #010
X_FREE:       EQU  #010
X_CDSTEP:     EQU  #02 + X_FREE
X_CANCNT:     EQU  #020        ; Max count of CAN msg
X_HFLAGS:     EQU  #00000
X_HGENPURP:   EQU  #00001      ; Relative address 16 bit for gen purp
X_HCANPTR:    EQU  #00003      ; Relative address CAN msg count
X_HCANMSG:    EQU  #00005      ; Relative address CAN msg buffer
X_HCARD:      EQU  #001A5
X_HINT:       EQU  #002C5      ; Relative address CAN INT (HCANINT)
X_HBUF:       EQU  #002CA      ; Relative address fill ptr msg buffer
X_HCNT:       EQU  #002CD      ; Relative address fill ptr msg cnt
X_HCIO:       EQU  #002D2      ; Relative address fill CAN IO

                                    ;************************************
                                    ;* FN_INFO - mandatory routine 0,   *
                                    ;* unchanged pattern from           *
                                    ;* RCXROM.ASM / Konamiman's RAM     *
                                    ;* reference. Required: the         *
                                    ;* installer's own duplicate-install*
                                    ;* check calls this on every        *
                                    ;* already-installed implementation.*
                                    ;************************************

FN_INFO:      LD    BC,256*ROM_V_P+ROM_V_S
              LD    DE,256*API_V_P+API_V_S
              LD    HL,APIINFO
              XOR   A
              RET

                                    ;************************************
                                    ;* GETRAM/GETRAMIX - deliberately   *
                                    ;* different from RCXROM.ASM: the   *
                                    ;* work area is a fixed address in  *
                                    ;* this segment (WORKAREA below),   *
                                    ;* not a caller-allocated block     *
                                    ;* tracked via SLTWRK. See header.  *
                                    ;************************************

GETRAM:       LD    HL,WORKAREA
              RET

GETRAMIX:     CALL  GETRAM
              ADD   HL,DE
              RET

                                    ;************************************
                                    ;* RCXINIT - unchanged from         *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

RCXINIT:      PUSH  HL
              PUSH  DE
              LD    DE,X_HFLAGS
              CALL  GETRAMIX
              LD    A,(HL)
              CP    #0AA
              POP   DE
              POP   HL
              RET

                                    ;************************************
                                    ;* SETRCXSLT - unchanged from       *
                                    ;* RCXROM.ASM, EXCEPT FRERCXERR:    *
                                    ;* returns Carry set instead of     *
                                    ;* jumping into BASIC's error trap  *
                                    ;* (no BASIC interpreter here)      *
                                    ;************************************

SETRCXSLT:    PUSH  HL
              PUSH  DE
              PUSH  BC

              LD    DE,X_HCARD
              CALL  GETRAMIX

              LD    DE,X_CDSTEP

              LD    B,X_CD
FRERCXLOOP:   LD    A,(HL)
              CP    0
              JP    Z,FRERCXSLOT

              INC   HL

              LD    A,(HL)
              CP    C
              JP    NZ,FRERCXIO

              JP    FRERCXERR

FRERCXIO:     DEC   HL

              ADD   HL,DE

              DJNZ  FRERCXLOOP

              POP   BC
              POP   DE
              POP   HL

              SCF
              CCF

              RET

FRERCXSLOT:   PUSH  HL
              POP   IX
              INC   IX
              INC   IX

              POP   BC
              LD    (HL),B
              INC   HL
              LD    (HL),C

              POP   DE
              POP   HL

              SCF

              RET

FRERCXERR:    POP   BC
              POP   DE
              POP   HL

              SCF

              RET

                                    ;************************************
                                    ;* GETRCXSLT - unchanged from       *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

GETRCXSLT:    PUSH  HL
              PUSH  DE
              PUSH  BC

              LD    DE,X_HCARD
              CALL  GETRAMIX

              POP   DE

              LD    B,X_CD

RCXLOOP:      LD    A,(HL)
              CP    D
              JP    NZ,RCXLOOP1
              INC   HL
              LD    A,(HL)
              DEC   HL
              CP    E
              JP    Z,RCXSLOTOK

RCXLOOP1:     PUSH  DE
              LD    DE,X_CDSTEP
              ADD   HL,DE
              POP   DE

              DJNZ  RCXLOOP

              PUSH  DE
              POP   BC
              POP   DE
              POP   HL

              SCF
              CCF

              RET

RCXSLOTOK:    PUSH  HL
              POP   IX
              INC   IX
              INC   IX

              PUSH  DE
              POP   BC
              POP   DE
              POP   HL

              SCF

              RET

                                    ;************************************
                                    ;* IOEN/IODIS - unchanged from      *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

IOEN:         PUSH  AF
              LD    A,WTEC
              OUT   (IOENDIS),A
              POP   AF
              RET

IODIS:        PUSH  AF
              LD    A,0
              OUT   (IOENDIS),A
              POP   AF
              RET

;---------------------------

                                    ;************************************
                                    ;* FN_CORE2 / CORE2 - RCX INIT      *
                                    ;* Adapted: no longer needs HL from *
                                    ;* the caller (still accepted for   *
                                    ;* UNAPI-caller compatibility, but  *
                                    ;* ignored) and no longer calls     *
                                    ;* SETRAM - the work area always    *
                                    ;* exists at a fixed address.       *
                                    ;************************************

FN_CORE2:     PUSH  BC
              PUSH  DE

FN_CORE2XIO:  IN    A,(IOENDIS)
              LD    B,A
              LD    A,WTEC
              OUT   (IOENDIS),A
              IN    A,(IOENDIS)
              CP    WTECINV
              LD    A,B
              OUT   (IOENDIS),A

              JP    Z,FN_CORE2_ISI

              POP   DE
              POP   BC

              JP    FN_CORE2_ERR

FN_CORE2_ISI: LD    DE,X_HFLAGS
              CALL  GETRAMIX
              LD    A,(HL)
              POP   DE
              POP   BC
              CP    SDEL
              JP    Z,CORE2
              CP    SINIT
              JP    NZ,CORE2

FN_CORE2_ERR: SCF
              RET

CORE2:        PUSH  HL
              PUSH  DE

              CALL  GETRAM
              LD    BC,WORKTEMPLATE_END-WORKTEMPLATE
              EX    DE,HL
              LD    HL,WORKTEMPLATE
              LDIR

              LD    DE,X_HFLAGS
              CALL  GETRAMIX
              LD    (HL),SINIT
              XOR   A
              LD    (R_COM_BASE+X_HCOMFLG),A ; COM not active

              POP   DE
              POP   HL
              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE3 - RCX ISINIT, unchanged *
                                    ;* from RCXROM.ASM                  *
                                    ;************************************

FN_CORE3:     PUSH  AF
              INC   DE
              LD    A,0
              LD    (DE),A
              DEC   DE

              CALL  RCXINIT
              JP    Z,FN_CORE3ISI

              LD    A,0
              LD    (DE),A

              POP   AF

              RET

FN_CORE3ISI:  LD    A,1
              LD    (DE),A

              POP   AF

              RET

                                    ;************************************
                                    ;* FN_CORE4 / CORE4 - RCX DEINIT,   *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE4:     CALL  RCXINIT
              JP    Z,CORE4
              SCF
              RET

CORE4:        CALL  COMOFF          ; Close COM channels, XIO interrupt off

              DI

              PUSH  HL
              PUSH  DE

              LD    DE,X_HFLAGS
              CALL  GETRAMIX
              LD    (HL),SDEL

              POP   DE
              POP   HL

              EI

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE5 / CORE5 - RCX TABLE,    *
                                    ;* unchanged from RCXROM.ASM. Not   *
                                    ;* card-type-specific (generic      *
                                    ;* infra), sits between RCX DEINIT  *
                                    ;* and Kartentyp A in RCXROM.ASM    *
                                    ;* too.                             *
                                    ;************************************

FN_CORE5:     CALL  RCXINIT
              JP    Z,CORE5
              SCF
              RET

CORE5:        PUSH  HL
              PUSH  DE

              LD    DE,X_HCARD
              CALL  GETRAMIX

              POP   DE

              LD    A,L
              LD    (DE),A
              INC   DE
              LD    A,H
              LD    (DE),A

              POP   HL

              OR    A
              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp A - RC2014 card with  ===
                                    ;===  PCF8584 I2C controller and      ===
                                    ;===  PCF8583 RTC. FN_CORE6-15,       ===
                                    ;===  unchanged from RCXROM.ASM      ===
                                    ;===  (B_COREnn BASIC handlers not    ===
                                    ;===  ported - UNAPI-only driver).    ===
                                    ;========================================

                                    ;************************************
                                    ;* FN_CORE6 / CORE6 - I2C INIT,     *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE6:     CALL  RCXINIT
              JP    Z,FN_CORE6SLT
              SCF
              RET

FN_CORE6SLT:  PUSH  BC
              LD    B,SI2C
              PUSH  IX
              CALL  SETRCXSLT
              POP   IX
              POP   BC
              JP    C,CORE6
              SCF
              RET

CORE6:        PUSH  HL

              CALL  IOEN

              LD    A,C
              LD    D,A
              ADD   A,#01
              LD    E,A

              LD    A,E
              OUT   (ALE),A
              LD    A,#080
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              LD    A,#055
              OUT   (SWIO),A

              LD    A,E
              OUT   (ALE),A
              LD    A,#0A0
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              LD    A,#018
              OUT   (SWIO),A

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C1
              OUT   (SWIO),A

              CALL  IODIS

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE7 / CORE7 - I2C WR,       *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE7:     CALL  RCXINIT
              JP    Z,FN_CORE7SLT
              SCF
              RET

FN_CORE7SLT:  PUSH  BC
              LD    B,SI2C
              LD    C,E
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   BC
              JP    C,CORE7
              SCF
              RET

CORE7:        PUSH  HL

              DI

              LD    A,E

              LD    D,A
              ADD   A,#01
              LD    E,A

              CALL  IOEN

              CALL  I2CBCTRL

              LD    A,D
              OUT   (ALE),A
              LD    A,C
              OUT   (SWIO),A

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C5
              OUT   (SWIO),A

              CALL  I2CPCTRL

              BIT   3,A

              JR    NZ,CORE7STOP

              LD    A,D
              OUT   (ALE),A
              LD    A,B
              OUT   (SWIO),A

              CALL  I2CPCTRL

CORE7STOP:    LD    A,E
              OUT   (ALE),A
              LD    A,#0C3
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE8 / CORE8 - I2C WRB,      *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE8:     CALL  RCXINIT
              JP    Z,FN_CORE8SLT
              SCF
              RET

FN_CORE8SLT:  LD    A,H
              PUSH  BC
              LD    B,SI2C
              LD    C,A
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              LD    A,C
              POP   BC
              JP    C,CORE8
              SCF
              RET

CORE8:        PUSH  HL

              EX    DE,HL

              DI

              LD    D,A
              ADD   A,#01
              LD    E,A

              CALL  IOEN

              CALL  I2CBCTRL

              LD    A,D
              OUT   (ALE),A
              LD    A,C
              OUT   (SWIO),A

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C5
              OUT   (SWIO),A

CORE8LOOP:    CALL  I2CPCTRL

              BIT   3,A

              JR    NZ,CORE8STOP

              LD    A,B
              CP    #00
              JR    Z,CORE8STOP
              SUB   #01
              LD    B,A

              LD    A,D
              OUT   (ALE),A
              LD    A,(HL)
              OUT   (SWIO),A

              INC   HL
              INC   HL

              JR    CORE8LOOP

CORE8STOP:    LD    A,E
              OUT   (ALE),A
              LD    A,#0C3
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE9 / CORE9 - I2C RD,       *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE9:     CALL  RCXINIT
              JP    Z,FN_CORE9SLT
              SCF
              RET

FN_CORE9SLT:  PUSH  BC
              LD    B,SI2C
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   BC
              JP    C,CORE9
              SCF
              RET

CORE9:        PUSH  HL

              EX    DE,HL

              DI

              LD    A,C

              LD    D,A
              ADD   A,#01
              LD    E,A

              CALL  IOEN

              LD    A,D
              OUT   (ALE),A
              LD    A,B
              ADD   A,#01
              OUT   (SWIO),A

              CALL  I2CBCTRL

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C5
              OUT   (SWIO),A

              CALL  I2CPCTRL

              BIT   3,A

              JR    NZ,CORE9STOP

              LD    A,E
              OUT   (ALE),A
              LD    A,#040
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              IN    A,(SWIO)

              CALL  I2CPCTRL

CORE9STOP:    LD    A,E
              OUT   (ALE),A
              LD    A,#0C3
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              IN    A,(SWIO)

              LD    (HL),A
              INC   HL
              LD    A,#00
              LD    (HL),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE10 / CORE10 - I2C RDB,    *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE10:    CALL  RCXINIT
              JP    Z,FN_CORE10SLT
              SCF
              RET

FN_CORE10SLT: LD    A,H
              PUSH  BC
              LD    B,SI2C
              LD    C,A
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              LD    A,C
              POP   BC
              JP    C,CORE10
              SCF
              RET

CORE10:       PUSH  HL

              PUSH  DE

              EX    DE,HL

              DI

              LD    D,A
              ADD   A,#01
              LD    E,A

              CALL  IOEN

              LD    A,D
              OUT   (ALE),A
              LD    A,C
              ADD   A,#01
              OUT   (SWIO),A

              CALL  I2CBCTRL

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C5
              OUT   (SWIO),A

              LD    C,B

CORE10LOOP:   CALL  I2CPCTRL

              BIT   3,A

              JR    NZ,CORE10STOP

              DEC   B
              JR    Z,CORE10END

              LD    A,D
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A

              LD    A,B
              INC   A
              CP    C
              JR    Z,CORE10LOOP

              INC   HL
              LD    A,#00
              LD    (HL),A
              INC   HL
              JR    CORE10LOOP

CORE10END:    LD    A,E
              OUT   (ALE),A
              LD    A,#040
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A

              LD    A,C
              DEC   A
              JR    Z,CORE10LAST

              INC   HL
              LD    A,#00
              LD    (HL),A
              INC   HL

CORE10LAST:   CALL  I2CPCTRL

CORE10STOP:   LD    A,E
              OUT   (ALE),A
              LD    A,#0C3
              OUT   (SWIO),A

              LD    A,D
              OUT   (ALE),A
              IN    A,(SWIO)

              LD    (HL),A
              INC   HL
              LD    A,#00
              LD    (HL),A

              CALL  IODIS

              EI

              POP   DE

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE11 / CORE11 - I2C RESET,  *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE11:    CALL  RCXINIT
              JP    Z,FN_CORE11SLT
              SCF
              RET

FN_CORE11SLT: PUSH  BC
              LD    B,SI2C
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   BC
              JP    C,CORE11
              SCF
              RET

CORE11:       PUSH  HL

              DI

              LD    A,C
              LD    D,A
              ADD   A,#01
              LD    E,A

              CALL  IOEN

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C2
              OUT   (SWIO),A

              LD    B,1
              CALL  DELAY

              LD    A,E
              OUT   (ALE),A
              LD    A,#0C1
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE12 / CORE12 / CORE12BCD - *
                                    ;* SET TIME, unchanged from         *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

FN_CORE12:    CALL  RCXINIT
              JP    Z,FN_CORE12SLT
              SCF
              RET

FN_CORE12SLT: PUSH  BC
              LD    B,SI2C
              CALL  GETRCXSLT
              POP   BC
              JP    C,CORE12
              SCF
              RET

CORE12:       PUSH  HL

              PUSH  BC
              PUSH  DE

              LD    (IX),#00
              LD    (IX+2),#00
              LD    (IX+4),#00

              CALL  CORE12BCD
              LD    (IX+10),A

              INC   DE

              CALL  CORE12BCD
              LD    (IX+8),A

              INC   DE

              CALL  CORE12BCD
              LD    (IX+6),A

              LD    A,C
              LD    C,#0A0
              LD    B,#06

              PUSH  IX
              POP   DE

              CALL  CORE8

              POP   DE
              POP   BC

              POP   HL

              OR    A
              RET

CORE12BCD:    LD    A,(DE)
              SUB   #030
              SLA   A
              SLA   A
              SLA   A
              SLA   A
              LD    B,A
              INC   DE
              LD    A,(DE)
              SUB   #030
              OR    B
              INC   DE

              RET

                                    ;************************************
                                    ;* FN_CORE13 / CORE13 / CORE13BCD - *
                                    ;* GET TIME, unchanged from         *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

FN_CORE13:    CALL  RCXINIT
              JP    Z,FN_CORE13SLT
              SCF
              RET

FN_CORE13SLT: PUSH  BC
              LD    B,SI2C
              CALL  GETRCXSLT
              POP   BC
              JP    C,CORE13
              SCF
              RET

CORE13:       PUSH  HL

              PUSH  BC
              PUSH  DE

              LD    E,C
              LD    C,#0A0
              LD    B,#02

              CALL  CORE7

              POP   DE
              POP   BC

              PUSH  BC
              PUSH  DE

              LD    A,C
              LD    C,#0A0
              LD    B,#03

              PUSH  IX
              POP   DE

              CALL  CORE10

              PUSH  DE
              POP   IX

              POP   DE
              POP   BC

              LD    A,(IX+4)
              AND   #03F
              CALL  CORE13BCD

              LD    A,':'
              LD    (DE),A
              INC   DE

              LD    A,(IX+2)
              CALL  CORE13BCD

              LD    A,':'
              LD    (DE),A
              INC   DE

              LD    A,(IX)
              CALL  CORE13BCD

              POP   HL

              OR    A
              RET

CORE13BCD:    LD    B,A
              AND   #0F0
              SRA   A
              SRA   A
              SRA   A
              SRA   A
              ADD   A,#030
              LD    (DE),A
              LD    A,B
              AND   #0F
              ADD   A,#030
              INC   DE
              LD    (DE),A
              INC   DE

              RET

                                    ;************************************
                                    ;* FN_CORE14 / CORE14 / CORE14BCD - *
                                    ;* SET DATE, unchanged from         *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

FN_CORE14:    CALL  RCXINIT
              JP    Z,FN_CORE14SLT
              SCF
              RET

FN_CORE14SLT: PUSH  BC
              LD    B,SI2C
              CALL  GETRCXSLT
              POP   BC
              JP    C,CORE14
              SCF
              RET

CORE14:       PUSH  HL

              PUSH  BC
              PUSH  DE

              LD    (IX),#05

              CALL  CORE14BCD
              LD    (IX+2),A

              INC   DE

              CALL  CORE14BCD
              LD    (IX+4),A

              INC   DE

              PUSH  BC
              PUSH  DE

              LD    A,C
              LD    C,#0A0
              LD    B,#03

              PUSH  IX
              POP   DE

              CALL  CORE8

              POP   DE
              POP   BC

              LD    (IX),#010

              CALL  CORE14BCD
              LD    (IX+2),A

              LD    A,C
              LD    C,#0A0
              LD    B,#02

              PUSH  IX
              POP   DE

              CALL  CORE8

              POP   DE
              POP   BC

              POP   HL

              OR    A
              RET

CORE14BCD:    LD    A,(DE)
              SUB   #030
              SLA   A
              SLA   A
              SLA   A
              SLA   A
              LD    B,A
              INC   DE
              LD    A,(DE)
              SUB   #030
              OR    B
              INC   DE

              RET

                                    ;************************************
                                    ;* FN_CORE15 / CORE15 / CORE15BCD - *
                                    ;* GET DATE, unchanged from         *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

FN_CORE15:    CALL  RCXINIT
              JP    Z,FN_CORE15SLT
              SCF
              RET

FN_CORE15SLT: PUSH  BC
              LD    B,SI2C
              CALL  GETRCXSLT
              POP   BC
              JP    C,CORE15
              SCF
              RET

CORE15:       PUSH  HL

              PUSH  DE
              PUSH  BC

              LD    E,C
              LD    C,#0A0
              LD    B,#05

              CALL  CORE7

              POP   BC
              PUSH  BC

              LD    A,C
              LD    C,#0A0
              LD    B,#02

              PUSH  IX
              POP   DE

              CALL  CORE10

              PUSH  DE
              POP   IX

              POP   BC
              POP   DE

              LD    A,(IX)
              AND   #0C0
              LD    B,A

              SRA   B
              SRA   B
              SRA   B
              SRA   B
              SRA   B
              SRA   B

              PUSH  BC

              LD    A,(IX)
              AND   #03F
              CALL  CORE15BCD

              LD    A,'/'
              LD    (DE),A
              INC   DE

              LD    A,(IX+2)
              AND   #01F
              CALL  CORE15BCD

              LD    A,'/'
              LD    (DE),A
              INC   DE

              POP   BC

              PUSH  BC
              PUSH  DE

              LD    E,C
              LD    C,#0A0
              LD    B,#010

              CALL  CORE7

              POP   DE
              POP   BC

              PUSH  BC
              PUSH  DE

              LD    B,#0A0

              PUSH  IX
              POP   DE

              CALL  CORE9

              POP   DE
              POP   BC

              LD    C,B

              LD    A,(IX)
              AND   #0F
              ADD   A,C
              LD    C,A
              SUB   #0A
              JP    NC,CORE15CARRY
              LD    A,(IX)
              AND   #0F0
              OR    C
              CALL  CORE15BCD
              JP    CORE15END

CORE15CARRY:  LD    C,A
              LD    A,(IX)
              AND   #0F0
              ADD   A,#010
              OR    C
              CALL  CORE15BCD

CORE15END:    POP   HL

              OR    A
              RET

CORE15BCD:    LD    B,A
              AND   #0F0
              SRA   A
              SRA   A
              SRA   A
              SRA   A
              ADD   A,#030
              LD    (DE),A
              LD    A,B
              AND   #0F
              ADD   A,#030
              INC   DE
              LD    (DE),A
              INC   DE

              RET

                                    ;************************************
                                    ;* I2CBCTRL / I2CPCTRL - I2C bus     *
                                    ;* busy/PIN-bit polling, unchanged  *
                                    ;* from RCXROM.ASM EXCEPT the       *
                                    ;* timeout path: returns Carry set  *
                                    ;* instead of jumping into BASIC's  *
                                    ;* error trap (no BASIC interpreter *
                                    ;* here). NOTE: unlike CORE58ERR/   *
                                    ;* CORE62ERR, this Carry is NOT     *
                                    ;* checked by CORE7-15 above (they  *
                                    ;* were ported unchanged and never  *
                                    ;* tested this flag even in the ROM *
                                    ;* - there it relied on a full non- *
                                    ;* local jump out of BASIC entirely,*
                                    ;* which has no RAM-driver          *
                                    ;* equivalent). A genuine I2C bus   *
                                    ;* timeout (stuck bus / no chip     *
                                    ;* present) is silently absorbed    *
                                    ;* rather than reported - same      *
                                    ;* class of accepted limitation as  *
                                    ;* the other two adaptations, see   *
                                    ;* DRV/README.md.                   *
                                    ;************************************

I2CBCTRL:     PUSH  BC
              LD    B,#0FF
              LD    A,E
              OUT   (ALE),A
BTESTCTRLL:   IN    A,(SWIO)
              BIT   0,A
              JR    NZ,BTESTOK
              DJNZ  BTESTCTRLL

              CALL  IODIS
              EI
              POP   BC
              SCF
              RET

BTESTOK:      POP   BC
              RET

I2CPCTRL:     PUSH  BC
              LD    B,#0FF
              LD    A,E
              OUT   (ALE),A
PTESTCTRLL:   IN    A,(SWIO)
              BIT   7,A
              JR    Z,PTESTOK
              DJNZ  PTESTCTRLL

              CALL  IODIS
              EI
              POP   BC
              SCF
              RET

PTESTOK:      POP   BC
              RET

                                    ;************************************
                                    ;* DELAY - unchanged from           *
                                    ;* RCXROM.ASM. B = outer loop       *
                                    ;* count, each iteration is a fixed *
                                    ;* #0FFFF-tick busy-wait.           *
                                    ;************************************

DELAY:        PUSH  DE
              PUSH  AF

              LD    A,B
              OR    A
              JR    Z,DELAYSEKEND

DELAYSEKL:    LD    DE,#0FFFF
DELAYMUSL:    DEC   DE
              LD    A,D
              OR    E
              JR    NZ,DELAYMUSL

              DJNZ  DELAYSEKL

DELAYSEKEND:  POP   AF
              POP   DE

              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp B - RC2014 card with  ===
                                    ;===  Z80 PIO. FN_CORE16-22,          ===
                                    ;===  unchanged from RCXROM.ASM.      ===
                                    ;===  B_COREnn BASIC handlers not     ===
                                    ;===  ported - UNAPI-only driver.     ===
                                    ;========================================

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 16                   *
                                    ;************************************
                                    ;* Initialise Z80 PIO               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PIO addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PIO addr                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE16:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE16SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE16SLT: PUSH BC               ; Save BC to stack
              LD B,SPIO             ; Set type SPIO
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE16OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE16OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 17                   *
                                    ;************************************
                                    ;* Get Byte from Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE17:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 18                   *
                                    ;************************************
                                    ;* Write Byte to Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 PIO A WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 1 PIO A WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE18:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 19                   *
                                    ;************************************
                                    ;* Get Byte from Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 PIO B RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 2 PIO B RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE19:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 20                   *
                                    ;************************************
                                    ;* Write Byte to Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 PIO B WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 3 PIO B WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE20:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 21                   *
                                    ;************************************
                                    ;* Write Byte to Z80 PIO CTRL A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 PIO A CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 4 PIO A CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE21:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 22                   *
                                    ;************************************
                                    ;* Write Byte to Z80 PIO CTRL B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 PIO B CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 5 PIO B CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE22:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;************************************
                                    ;* Do activity with Z80 PIO         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_COREPIO:   CALL RCXINIT          ; Module installed?
              JP Z,FN_COREPIOST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_COREPIOST: PUSH BC               ; Save bus address to stack
              LD B,SPIO             ; Set type SPIO
              CALL GETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,COREPIO          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* PIO xxx (ADDR,VALUE)             *
                                    ;************************************
                                    ;* Do activity with Z80 PIO         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COREPIO:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, COREPIOAPR      ; Go to PIO A PORT RD
              CP 1
              JP Z, COREPIOAPW      ; Go to PIO A PORT WR
              CP 2
              JP Z, COREPIOBPR      ; Go to PIO B PORT RD
              CP 3
              JP Z, COREPIOBPW      ; Go to PIO B PORT WR
              CP 4
              JP Z, COREPIOACW      ; Go to PIO A CTRL
              CP 5
              JP Z, COREPIOBCW      ; Go to PIO B CTRL
              JP COREPIOEND

COREPIOAPR:   LD A,C                ; PIO A PORT RD
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              LD A,#00
              LD (HL),A             ; Clear high byte of INTEGER

              JP COREPIOEND

COREPIOAPW:   LD A,C                ; PIO A PORT WR
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOBPR:   LD A,C                ; PIO B PORT RD
              ADD A,#01
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              LD A,#00
              LD (HL),A             ; Clear high byte of INTEGER

              JP COREPIOEND

COREPIOBPW:   LD A,C                ; PIO B PORT WR
              ADD A,#01
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOACW:   LD A,C                ; PIO A CTRL WR
              ADD A,#02
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOBCW:   LD A,C                ; PIO B CTRL WR
              ADD A,#03
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

COREPIOEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp C - RC2014 card with  ===
                                    ;===  82C55 PPI. FN_CORE23-32,        ===
                                    ;===  unchanged from RCXROM.ASM.      ===
                                    ;===  B_COREnn BASIC handlers not     ===
                                    ;===  ported - UNAPI-only driver.     ===
                                    ;========================================

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 23                   *
                                    ;************************************
                                    ;* Initialise 82C55                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PPI addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PPI addr                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE23:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE23SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE23SLT: PUSH BC               ; Save BC to stack
              LD B,SPPI             ; Set type SPPI
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE23OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE23OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 24                   *
                                    ;************************************
                                    ;* Get Byte from 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 PPI A RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 0 PPI A RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE24:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 25                   *
                                    ;************************************
                                    ;* Write Byte to 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 PPI A WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 1 PPI A WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE25:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 26                   *
                                    ;************************************
                                    ;* Read Byte from 82C55 PORT B      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 PPI B RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 2 PPI B RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE26:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 27                   *
                                    ;************************************
                                    ;* Write Byte to 82C55 PORT B       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 PPI B WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 3 PPI B WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE27:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 28                   *
                                    ;************************************
                                    ;* Get Byte from 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 PPI C RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 4 PPI C RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE28:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 29                   *
                                    ;************************************
                                    ;* Write Byte to 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 PPI C WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 5 PPI C WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE29:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 30                   *
                                    ;************************************
                                    ;* Write Byte to 82C55 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 6 PPI CTRL                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 6 PPI CTRL                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE30:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 31                   *
                                    ;************************************
                                    ;* Set Bit PORT C 82C55             *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 7 PPI SET BIT                *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 7 PPI SET BIT                *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE31:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 32                   *
                                    ;************************************
                                    ;* Reset Bit PORT C 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE32:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;************************************
                                    ;* Do activity with 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_COREPPI:   CALL RCXINIT          ; Module installed?
              JP Z,FN_COREPPIST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_COREPPIST: PUSH BC               ; Save bus address to stack
              LD B,SPPI             ; Set type SPPI
              CALL GETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,COREPPI          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* PPI xxx (ADDR,VALUE)             *
                                    ;************************************
                                    ;* Do activity with 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COREPPI:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, COREPPIAPR      ; Go to PPI A PORT RD
              CP 1
              JP Z, COREPPIAPW      ; Go to PPI A PORT WR
              CP 2
              JP Z, COREPPIBPR      ; Go to PPI B PORT RD
              CP 3
              JP Z, COREPPIBPW      ; Go to PPI B PORT WR
              CP 4
              JP Z, COREPPICPR      ; Go to PPI C PORT RD
              CP 5
              JP Z, COREPPICPW      ; Go to PPI C PORT WR
              CP 6
              JP Z, COREPPICW       ; Go to PPI CTRL
              CP 7
              JP Z, COREPPISET      ; Go to PPI SET BIT
              CP 8
              JP Z, COREPPIRES      ; Go to PPI RESET BIT

              JP COREPPIEND

COREPPIAPR:   LD A,C                ; PPI A PORT RD
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              LD A,#00
              LD (HL),A             ; Clear high byte of INTEGER

              JP COREPPIEND

COREPPIAPW:   LD A,C                ; PPI A PORT WR
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPIBPR:   LD A,C                ; PPI B PORT RD
              ADD A,#01
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              LD A,#00
              LD (HL),A             ; Clear high byte of INTEGER

              JP COREPPIEND

COREPPIBPW:   LD A,C                ; PPI B PORT WR
              ADD A,#01
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPICPR:   LD A,C                ; PPI C PORT RD
              ADD A,#02
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              LD A,#00
              LD (HL),A             ; Clear high byte of INTEGER

              JP COREPPIEND

COREPPICPW:   LD A,C                ; PPI C PORT WR
              ADD A,#02
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPICW:    LD A,C                ; PPI CTRL
              ADD A,#03
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPISET:   LD A,C                ; PPI SET BIT
              ADD A,#03
              OUT (ALE),A
              LD A,D
              CP #08
              JP NC,COREPPIERR      ; Value > 7? only 0..7 allowed
              SLA A
              ADD A,#01             ; Calculate control word
              OUT (SWIO),A          ; Set Bit according to varptr(VALUE)

              JP COREPPIEND

COREPPIRES:   LD A,C                ; PPI RESET BIT
              ADD A,#03
              OUT (ALE),A
              LD A,D
              CP #08
              JP NC,COREPPIERR      ; Value > 7? only 0..7 allowed
              SLA A                 ; Calculate control word
              OUT (SWIO),A          ; Reset Bit according to varptr(VALUE)

COREPPIEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET

                                    ;************************************
                                    ;* COREPPIERR - PPI SET/RESET bit    *
                                    ;* index out of range (0..7). No     *
                                    ;* BASIC error trap here (UNAPI-only *
                                    ;* driver) - returns Carry set,      *
                                    ;* same Carry-instead-of-BASIC-      *
                                    ;* error-trap adaptation as          *
                                    ;* CORE58ERR/CORETIMERR (see header  *
                                    ;* note).                            *
                                    ;************************************

COREPPIERR:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF                   ; Set Carry Flag for ERROR
              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp D - RC2014 card with  ===
                                    ;===  82C54 timer. FN_CORE33-42,      ===
                                    ;===  unchanged from RCXROM.ASM      ===
                                    ;===  EXCEPT CORETIMERR/CORETIMERR1   ===
                                    ;===  (see header note - return Carry ===
                                    ;===  set instead of jumping into     ===
                                    ;===  BASIC's error trap, same        ===
                                    ;===  adaptation as CORE58ERR).       ===
                                    ;===  B_COREnn BASIC handlers not     ===
                                    ;===  ported - UNAPI-only driver.     ===
                                    ;========================================

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 33                   *
                                    ;************************************
                                    ;* Initialise 82C54                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = TIMER addr                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = TIMER addr                   *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE33:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE33SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE33SLT: PUSH BC               ; Save BC to stack
              LD B,SCNT             ; Set type SCNT
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE33OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE33OK:  DI

              CALL IOEN             ; Switched IO enabled

              LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#030
              OUT (SWIO),A          ; TIMER 0 MODE 0
              LD A,#070
              OUT (SWIO),A          ; TIMER 1 MODE 0
              LD A,#0B0
              OUT (SWIO),A          ; TIMER 2 MODE 0
              LD A,C
              ADD A,#04
              OUT (ALE),A
              LD A,#00
              OUT (SWIO),A          ; CLEAR GATE

              CALL IODIS            ; Switched IO disabled

              EI
              CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 34                   *
                                    ;************************************
                                    ;* Get Byte from 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 TIMER 0 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 0 TIMER 0 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE34:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 35                   *
                                    ;************************************
                                    ;* Write Byte to 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 TIMER 0 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 1 TIMER 0 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE35:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 36                   *
                                    ;************************************
                                    ;* Read Byte from 82C54 TIMER 1     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 TIMER 1 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 2 TIMER 1 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE36:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 37                   *
                                    ;************************************
                                    ;* Write Byte to 82C54 TIMER 1      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 TIMER 1 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 3 TIMER 1 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE37:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 38                   *
                                    ;************************************
                                    ;* Get Byte from 82C54 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 TIMER 2 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 4 TIMER 2 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE38:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 39                   *
                                    ;************************************
                                    ;* Write Byte to 82C55 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 TIMER 2 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 5 TIMER 2 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE39:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 40                   *
                                    ;************************************
                                    ;* Write Byte to 82C54 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 6 TIMER CTRL                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 6 TIMER CTRL                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE40:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 41                   *
                                    ;************************************
                                    ;* Set Bit for GATE 0..2            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 7 TIMER SET GATE             *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 7 TIMER SET GATE             *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE41:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 42                   *
                                    ;************************************
                                    ;* Reset Bit for GATE 0..2          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 8 TIMER RESET GATE           *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 8 TIMER RESET GATE           *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE42:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;************************************
                                    ;* Do activity with 82C54           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* DE = INT if WR                   *
                                    ;************************************
                                    ;* OUTPUT                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* DE = INT if WR                   *
                                    ;* C-Flag not set = ok              *
                                    ;* C-Flag set = error               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORETIM:   CALL RCXINIT          ; Module installed?
              JP Z,FN_CORETIMST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_CORETIMST: PUSH BC               ; Save bus address to stack
              LD B,SCNT             ; Set type SCNT
              CALL GETRCXSLT        ; Initialize card with BC
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORETIM          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* TIMER xxx (ADDR,VALUE)           *
                                    ;************************************
                                    ;* Do activity with 82C54           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;* IX = RAM address                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORETIM:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, CORETIMAPR      ; Go to TIMER 0 RD
              CP 1
              JP Z, CORETIMAPW      ; Go to TIMER 0 WR
              CP 2
              JP Z, CORETIMBPR      ; Go to TIMER 1 RD
              CP 3
              JP Z, CORETIMBPW      ; Go to TIMER 1 WR
              CP 4
              JP Z, CORETIMCPR      ; Go to TIMER 2 RD
              CP 5
              JP Z, CORETIMCPW      ; Go to TIMER 2 WR
              CP 6
              JP Z, CORETIMCW       ; Go to TIMER CTRL
              CP 7
              JP Z, CORETIMSET      ; Go to TIMER SET GATE
              CP 8
              JP Z, CORETIMRES      ; Go to TIMER RES GATE

              JP CORETIMEND

CORETIMAPR:   LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#00
              OUT (SWIO),A          ; Latch Counter 0
              LD A,C
              OUT (ALE),A           ; TIMER 0 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMAPW:   LD A,C                ; TIMER 0 WR
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMBPR:   LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#04
              OUT (SWIO),A          ; Latch Counter 1
              LD A,C
              ADD A,#01
              OUT (ALE),A           ; TIMER 1 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMBPW:   LD A,C                ; TIMER 1 WR
              ADD A,#01
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMCPR:   LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#08
              OUT (SWIO),A          ; Latch Counter 2
              LD A,C
              ADD A,#02
              OUT (ALE),A           ; TIMER 2 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMCPW:   LD A,C                ; TIMER 2 WR
              ADD A,#02
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMCW:    LD A,C                ; TIMER CTRL
              ADD A,#03
              OUT (ALE),A

              LD A,D
              CP 0
              JP NZ,CORETIMERR1
              LD A,E

              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMSET:   LD A,C                ; TIMER SET GATE
              ADD A,#04
              OUT (ALE),A
              LD A,D
              CP 0
              JP NZ,CORETIMERR
              LD A,E
              CP #08
              JP NC,CORETIMERR      ; Value > 7? only 0..7 allowed
              LD D,A                ; Save Value in C
              LD A,(IX)             ; Get old GATE
              OR D                  ; Set Bits in GATE
              LD (IX),A             ; Save new GATE
              OUT (SWIO),A          ; Set Bit according to varptr(VALUE)

              JP CORETIMEND

CORETIMRES:   LD A,C                ; TIMER RESET GATE
              ADD A,#04
              OUT (ALE),A
              LD A,D
              CP 0
              JP NZ,CORETIMERR
              LD A,E
              CP #08
              JP NC,CORETIMERR      ; Value > 7? only 0..7 allowed
              CPL
              AND #07
              LD D,A                ; Save Value in C
              LD A,(IX)             ; Get Bits from RAM
              AND D                 ; Reset Bits in GATE
              LD (IX),A             ; Save new GATE
              OUT (SWIO),A          ; Reset Bit according to varptr(VALUE)

CORETIMEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET

                                    ;************************************
                                    ;* CORETIMERR/CORETIMERR1 - Adapted: *
                                    ;* return Carry set instead of the   *
                                    ;* BASIC PROMPT/CHPUT/B_ERRTYPE      *
                                    ;* sequence - no BASIC interpreter   *
                                    ;* here, same adaptation as          *
                                    ;* CORE58ERR/CORE62ERR.              *
                                    ;************************************

CORETIMERR:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF
              RET

CORETIMERR1:  CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF
              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp E - RC2014 card with  ===
                                    ;===  SJA1000 CAN controller.         ===
                                    ;===  FN_CORE43-49, unchanged from    ===
                                    ;===  RCXROM.ASM EXCEPT GETRAM (see  ===
                                    ;===  header) and HCANINT (below,     ===
                                    ;===  now embedded in WORKTEMPLATE/   ===
                                    ;===  WORKAREA instead of X_ROM).     ===
                                    ;===  B_COREnn BASIC handlers not     ===
                                    ;===  ported - UNAPI-only driver.     ===
                                    ;========================================

                                    ; RAM addresses for registers

CANCTRLREG:   EQU #0000             ; Controll register
CANCMDREG:    EQU #0001             ; Command register
CANSTAREG:    EQU #0002             ; Status register
CANINTREG:    EQU #0003             ; Interrupt register
CANIERREG:    EQU #0004             ; Interrupt mask register
CANBUST0REG:  EQU #0006             ; Bustiming 0 register
CANBUST1REG:  EQU #0007             ; Bustiming 1 register
CANOUTCTREG:  EQU #0008             ; Output control register
CANTXBUFREG:  EQU #0010             ; FIFO TX
CANRXBUFREG:  EQU #0010             ; FIFO RX
CANRMC:       EQU #001D             ; Messages to read
CANACCEPTCREG:EQU #0010             ; ACC code, only in reset mode 
CANACCEPTMREG:EQU #0014             ; ACC mask, only in reset mode
CANCLKDIVREG: EQU #001F             ; Clock divider register

                                    ; Bits in CANCTRLREG

CANRES:       EQU #0001             ; Reset Request
CANACCFMODE:  EQU #0008             ; Acceptfilter Mode

                                    ; Bits in CANCMDREG

CANTXREQ:     EQU #0001             ; Transmission request
CANABTX:      EQU #0002             ; Abbort pending TX
CANRELRXBUF:  EQU #0004             ; Release RX Buffer

                                    ; Bits in CANSTAREG

CANTXFREE:    EQU #0004             ; Transmit buffer released

                                    ; Bits in CANIERREG

CANRIE:       EQU #0001             ; receive interrupt  
CANEPIE:      EQU #0020             ; error passive interrupt
CANALIE:      EQU #0040             ; arbitration lost interrupt
CANBEIE:      EQU #0080             ; bus error interrupt

                                    ; Bits in CANOUTCTREG

CANTX1FLOAT:  EQU #0000             ; configured as float
CANTX0PSHPULL:EQU #0018             ; configured as push/pull
CANNORMMODE:  EQU #0002             ; Normal Mode

                                    ;************************************
                                    ;* R_CAN_HCANPTR/HCANMSG/HCANINT -  *
                                    ;* FIXED page-3 addresses (always   *
                                    ;* mapped), NOT part of WORKAREA -  *
                                    ;* HCANINT is a real hardware       *
                                    ;* interrupt handler (registered    *
                                    ;* via XIO SET ADDR), invoked       *
                                    ;* asynchronously whenever the      *
                                    ;* SJA1000 raises an interrupt,     *
                                    ;* regardless of what is currently  *
                                    ;* mapped into page 1. WORKAREA is  *
                                    ;* only guaranteed mapped while     *
                                    ;* RCXRAM's own dispatched code     *
                                    ;* (via CALLFN) is executing - safe *
                                    ;* for CORE2-15/43-49/57-62, NOT    *
                                    ;* safe for an async interrupt      *
                                    ;* handler. Chosen range avoids     *
                                    ;* #0C000-0C52F (XWORKAREA/         *
                                    ;* XBOUNDFLAG/RCXWORKAREA, used by  *
                                    ;* every transient RCX/XIO test     *
                                    ;* .COM) and #0C600-0CCC8ish        *
                                    ;* (RTCLCDP.MAC's resident TSR      *
                                    ;* block, if installed).            *
                                    ;************************************

R_CAN_HCANPTR: EQU #0CD00           ; 2 bytes
R_CAN_HCANMSG: EQU #0CD02           ; 416 bytes (#1A0)
R_CAN_HCANINT: EQU #0CEA2           ; the interrupt routine itself

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 43                   *
                                    ;************************************ 
                                    ;* Initialise SJA1000               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE43:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE43SLT     ; Installed? Continue with Slot command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 already installed?
              
FN_CORE43SLT: PUSH BC               ; Save BC to stack

              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,FN_CORE43NF     ; JP if slot not found

              POP BC                ; Get BC from stack

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 present?

FN_CORE43NF:  PUSH DE               ; Save varptr(DE) to stack

              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG 
              LD E,CANRES           ; CAN RESET BIT

FN_CORE4301:  CALL CANOUT 
              CALL CANINP           ; A contains Value
              AND #01F              ; Mask 0b00011111
              CP CANRES
              JP Z,FN_CORE4302      ; Wait until SJA1000 is in RESET MODE
              DJNZ FN_CORE4301

              POP DE                ; Get varptr(DE) back from stack
              POP BC                ; Get IO addr back from stack (pushed by FN_CORE43SLT)

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 set to RCX table

FN_CORE4302:  POP DE                ; Get varptr(DE) back from stack
              POP BC                ; Get IO addr back from stack (pushed by FN_CORE43SLT)

              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,CORE43           ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

                                    ;************************************ 
                                    ;* CAN INIT(ADDR,ACC)               *
                                    ;************************************ 
                                    ;* Initialise SJA1000               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************

CORE43:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH IX               ; Save IX to stack

              PUSH DE
              POP IX                ; Save varptr(ACC) to IX

                                    ; SJA in RESET MODE
              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG 
              LD E,CANRES           ; CAN RESET BIT

CORE43S01:    CALL CANOUT 
              CALL CANINP           ; A contains value
              AND #01F              ; Mask 0b00011111, fits for all modes
              CP CANRES
              JP Z,CORE43S02        ; Wait until SJA1000 is in RESET MODE
              DJNZ CORE43S01
    
              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF                   ; Set CARRY flag for ERROR
              RET                       

                                    ; PeliCAN-Mode, Bypass, Clockout off

CORE43S02:    LD D,CANCLKDIVREG     ; CLKDIVIDER REG
              LD E,#0C8
              CALL CANOUT

                                    ; https://kvaser.com/support/calculators/bit-timing-calculator/
                                    ; Bustiming 250 kHz, 24 MHz einstellen
                                    ; T1         12  
                                    ; T2         4
                                    ; BTQ        16
                                    ; SP%        75
                                    ; SJW        4
                                    ; BIT RATE   250
                                    ; ERR%       0

              LD D,CANBUST0REG      ; BUSTIMING0 REG
              LD E,#0082
              CALL CANOUT

              LD D,CANBUST1REG      ; BUSTIMING0 REG
              LD E,#003A
              CALL CANOUT

                                    ; Acceptancecode and -mask

              LD D,CANACCEPTCREG    ; ACCEPTCODE REG
              LD E,(IX)             
              CALL CANOUT           ; ACR.0   ID28...ID21    #0F0
              INC D
              LD E,(IX+2)
              CALL CANOUT           ; ACR.1   ID20...ID13    #0B7
              INC D
              LD E,(IX+4)
              CALL CANOUT           ; ACR.2   ID12...ID5
              INC D
              LD E,(IX+6)
              CALL CANOUT           ; ACR.3   ID4...ID0, RTR, 3 bits n.c.

              LD D,CANACCEPTMREG    ; ACCEPTMASK REG
              LD E,(IX+8)             
              CALL CANOUT           ; AMR.0   ID28...ID21    #0F0
              INC D
              LD E,(IX+10)
              CALL CANOUT           ; AMR.1   ID20...ID13    #007
              INC D
              LD E,(IX+12)
              CALL CANOUT           ; AMR.2   ID12...ID5
              INC D
              LD E,(IX+14)
              CALL CANOUT           ; AMR.3   ID4...ID0, RTR, 3 bits n.c.

                                    ; Outputcontrol

              LD D,CANOUTCTREG      ; OUTPUTCTRL REG
              LD E,CANTX1FLOAT+CANTX0PSHPULL+CANNORMMODE
              CALL CANOUT

                                    ; Receive-Interrupt enable

              LD D,CANIERREG        ; IER REG
              LD E,CANRIE+CANEPIE+CANBEIE+CANALIE
              CALL CANOUT
      
                                    ; Set SJA to Wait Mode

              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG
              LD E,CANACCFMODE   

CORE43S03:    CALL CANOUT 
              CALL CANINP           ; IX for dummy read, A contains value
              AND #01F              ; Mask 0b00011111 fits for PELICAN
              CP CANACCFMODE
              JP Z,CORE43S04        ; Wait until SJA1000 is in ACCEPT MODE
              DJNZ CORE43S03

              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF                   ; Set CARRY flag for ERROR
              RET  

                                    ; Prepare INT routine - copy the
                                    ; resident R_CAN_HCANINT_T template
                                    ; (see below) into the fixed page-3
                                    ; R_CAN_HCANINT, zero the page-3
                                    ; R_CAN_HCANPTR ring pointers, then
                                    ; patch the copy's IO-address operand
                                    ; (R_CAN_HCAN003_T) - HL/DE operands
                                    ; are already correct at assembly
                                    ; time (R_CAN_HCANMSG/R_CAN_HCANPTR
                                    ; are fixed constants, not WORKAREA-
                                    ; relative)

CORE43S04:    PUSH  BC
              PUSH  DE
              LD    HL,R_CAN_HCANINT_T
              LD    DE,R_CAN_HCANINT
              LD    BC,R_CAN_HCANINT_T_END-R_CAN_HCANINT_T
              LDIR

              XOR   A
              LD    (R_CAN_HCANPTR),A
              LD    (R_CAN_HCANPTR+1),A

              POP   DE
              POP   BC
              LD    A,C             ; C contains IO addr
              LD    (R_CAN_HCANINT+R_CAN_HCIO_OFF),A

                                    ; Clear RX FIFO Buffer

CORE43S05:    LD D,CANCMDREG        ; COMMAND REG
              LD E,CANRELRXBUF

              CALL CANOUT

                                    ; Still new msg?

              LD D,CANRMC           ; RMC REG
              CALL CANINP

              OR A
              JP NZ,CORE43S05       ; Jump if more new msg

              LD D,CANCMDREG        ; COMMAND REG
              LD E,CANABTX          ; Clear TX Transmission

              CALL CANOUT

              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET

                                    ;************************************
                                    ;* R_CAN_HCANINT_T - template for   *
                                    ;* the real CAN interrupt routine,  *
                                    ;* LDIR-copied to the fixed page-3  *
                                    ;* R_CAN_HCANINT by CORE43S04       *
                                    ;* (CAN INIT). Otherwise unchanged  *
                                    ;* from RCXROM.ASM's HCANINT,       *
                                    ;* except HCAN001/HCAN002 (the      *
                                    ;* HL/DE operands) are now fixed    *
                                    ;* compile-time constants           *
                                    ;* (R_CAN_HCANMSG/R_CAN_HCANPTR)    *
                                    ;* instead of runtime-patched       *
                                    ;* values - only HCAN003 (the IO    *
                                    ;* address, a per-install runtime   *
                                    ;* value) still needs patching.     *
                                    ;* All internal jumps are relative  *
                                    ;* (JR/DJNZ), so the LDIR copy      *
                                    ;* needs no further fixups.         *
                                    ;************************************

R_CAN_HCANINT_T:
              LD    A,WTEC
              OUT   (IOENDIS),A

R_CAN_HCAN001_T:
              LD    HL,R_CAN_HCANMSG

R_CAN_HCAN002_T:
              LD    DE,R_CAN_HCANPTR

              LD    A,(DE)
              LD    B,A

R_CAN_HCAN003_T:
              LD    C,0

              ADD   A,#01
              CP    X_CANCNT
              JR    NZ,R_CAN_HCANINTL_T
              LD    A,#00

R_CAN_HCANINTL_T:
              LD    (DE),A

              LD    DE,#00D

              SCF
              CCF

              SBC   HL,DE

R_CAN_HCANLOOP1_T:
              ADD   HL,DE
              DJNZ  R_CAN_HCANLOOP1_T

              PUSH  HL
              POP   IX

              LD    E,C
              INC   E

              LD    A,C
              OUT   (ALE),A
              LD    A,CANINTREG
              OUT   (SWIO),A
              LD    A,E
              OUT   (ALE),A
              IN    A,(SWIO)

              LD    D,CANRXBUFREG
              LD    B,#0D

R_CAN_HCANINTL1_T:
              LD    A,C
              OUT   (ALE),A
              LD    A,D
              OUT   (SWIO),A
              LD    A,E
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (IX),A

              INC   IX
              INC   D

              DJNZ  R_CAN_HCANINTL1_T

              LD    A,C
              OUT   (ALE),A
              LD    A,CANCMDREG
              OUT   (SWIO),A
              LD    A,E
              OUT   (ALE),A
              LD    A,CANRELRXBUF
              OUT   (SWIO),A

              LD    A,C
              OUT   (ALE),A
              LD    A,CANRMC
              OUT   (SWIO),A
              LD    A,E
              OUT   (ALE),A
              IN    A,(SWIO)
              OR    A
              JR    NZ,R_CAN_HCAN001_T

              LD    A,0
              OUT   (IOENDIS),A

              RET

R_CAN_HCANINT_T_END:

R_CAN_HCIO_OFF: EQU R_CAN_HCAN003_T-R_CAN_HCANINT_T+1

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 44                   *
                                    ;************************************ 
                                    ;* Transfer Data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE44:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE44SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE44SLT: PUSH BC               ; Save BC to stack
              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CANOUT           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN WR(ADDR,REG,DATA)            *                            
                                    ;************************************
                                    ;* Transfer Data to SJA1000         *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CANOUT:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH DE               ; Save DE to stack
 
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,D                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,E                ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000
              
              CALL IODIS            ; Switched IO disabled

              EI

              POP DE                ; Get DE from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 45                   *
                                    ;************************************ 
                                    ;* Read data from SJA1000           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE45:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE45SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE45SLT: PUSH BC               ; Save BC to stack
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed? 
                                    ; IX contains RAM-address
              POP BC                ; Get BC from stack
              JP C,CANINP           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN RD(ADDR,REG,DATA)            *
                                    ;************************************
                                    ;* Read data from SJA1000           *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* A = CAN DATA                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CANINP:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH DE               ; Save DE to stack
 
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,D                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)           ; Get Data from SJA1000
                            
              CALL IODIS            ; Switched IO disabled

              EI

              POP DE                ; Get DE from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET                                        

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 46                   *
                                    ;************************************ 
                                    ;* Checks for new msg to read       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN addr                     *
                                    ;* A = RMC                          *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE46:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE46SLT     ; Installed? Continue with Slot command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE46SLT: PUSH BC               ; Save BC to stack
              PUSH DE               ; Save DE to stack
              
              LD B,SCAN             ; Set type SCAN

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,FN_CORE46F       ; JP if slot found

              POP DE                ; Get DE from stack
              POP BC                ; Get BC from stack

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE46F:   LD HL,R_CAN_HCANPTR   ; fixed page-3 address, see there
              LD A,(HL)             ; A contains incoming msg pointer
              LD B,A                ; B contains incoming msg pointer
              INC HL
              LD A,(HL)             ; A contains read msg pointer
              CP B
              JR Z,FN_CORE46NONE    ; no new incoming msg

              LD A,#01
              JR FN_CORE46DONE

FN_CORE46NONE: LD A,#00

FN_CORE46DONE: POP DE               ; Get DE back from stack
              POP BC                ; Get BC back from stack

              OR A                  ; Clear Carry flag, A holds result

              RET

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 47                   *
                                    ;************************************ 
                                    ;* Read RX data from SJA1000        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE47:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE47SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE47SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CORE47           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN RX(ADDR,DATA)                *
                                    ;************************************
                                    ;* Read RX data from SJA1000        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* (DE) = RX DATA                   *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CORE47:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH BC               ; Save BC to stack
 
              PUSH DE               ; DE points to varptr(RX DATA)

              DI

              LD HL,R_CAN_HCANPTR   ; fixed page-3 address, see there
              INC HL
              LD A,(HL)
              LD B,A                ; B contains next msg to read

              LD HL,R_CAN_HCANMSG   ; fixed page-3 address, see there

              LD DE,#0D

              SCF
              CCF

              SBC HL,DE

CORE47LOOP:   ADD HL,DE
              DJNZ CORE47LOOP       ; HL points to next msg to be written

              POP DE                ; DE points to varptr(RX DATA)

              LD B,#0D              ; Set loop for 13 bytes

CORE47LOOP1:  LD A,(HL)
              LD (DE),A             ; Save result in (DE)
              INC DE             
              LD A,0
              LD (DE),A             ; Set higher Byte (DE+1) to 0
              INC DE                ; Increase DE-Pointer to new buffer
              INC HL                ; Increase RX FIFO place
              DJNZ CORE47LOOP1
   
              LD HL,R_CAN_HCANPTR   ; fixed page-3 address, see there
              INC HL
              LD A,(HL)
              ADD A,#01
              CP X_CANCNT           ; Max count reached? Set 0
              JP NZ,CORE47_01
              LD A,#00
CORE47_01:    LD (HL),A             ; Save read msg count

              EI


              POP BC                ; Get BC back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET                                        

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 48                   *
                                    ;************************************ 
                                    ;* Get addr of INT routine          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = INTADDR                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************

FN_CORE48:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE48SLT     ; Installed? Continue with slt command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE48SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE48ADR     ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE48ADR: LD DE,R_CAN_HCANINT   ; fixed page-3 address, see there

              OR A                  ; Clear CARRY flag
              RET

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 49                   *
                                    ;************************************ 
                                    ;* Write TX data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE49:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE49SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE49SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CORE49           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN TX(ADDR,DATA)                *
                                    ;************************************
                                    ;* Write TX data to SJA1000         *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************ 
              
CORE49:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH BC               ; Save BC to stack

              DI

              CALL IOEN             ; Switched IO enabled

                                    ; Clear Interrupts

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANINTREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)

                                    ; Wait till TX buffer is free

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANSTAREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
CORE49L0:     IN A,(SWIO)
              AND CANTXFREE
              JP Z,CORE49L0         ; Wait for released TX buffer

              LD H,CANTXBUFREG
              LD B,#0D              ; Set loop for 13 bytes

CORE49L1:     LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,H                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,(DE)             ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000

              INC DE                ; Increase write buffer index 
              INC DE                ; Increase write buffer index 
              
              INC H                 ; Increase TX FIFO place
                            
              DJNZ CORE49L1         ; Do loop

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANCMDREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,CANTXREQ         ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000

              CALL IODIS            ; Switched IO disabled

              EI

              POP BC                ; Get BC back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
                      
              OR A                  ; Clear Carry Flag to end routine handler
              RET

;---------------------------

                                    ;========================================
                                    ;===  Kartentyp F - RC2014 card with  ===
                                    ;===  ATMEGA8 4x SPI master.          ===
                                    ;===  FN_CORE50-56, unchanged from    ===
                                    ;===  RCXROM.ASM (see GETRAM header  ===
                                    ;===  note - no card-specific         ===
                                    ;===  adaptation needed here).        ===
                                    ;===  B_COREnn BASIC handlers not     ===
                                    ;===  ported - UNAPI-only driver.     ===
                                    ;========================================

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 50                   *
                                    ;************************************ 
                                    ;* Initialise SPI                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE50:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE50SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE50SLT: PUSH BC               ; Save BC to stack
              LD B,SSPI             ; Set type SSPI 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE50OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE50OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 51                   *
                                    ;************************************ 
                                    ;* Write data to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE51:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE51SLT     ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE51SLT: PUSH BC               ; Save SPI Bus address & I2C DATA to stack
              LD B,SSPI             ; Set type SSPI 
              LD C,E
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C CHIP & I2C DATA from stack
              JP C,CORE51           ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI WR(ADDR,BUS,DATA)            *
                                    ;************************************ 
                                    ;* Write data to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE51:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,E                ; Set active Bus
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Send data
              ADD A,#03             
              OUT (ALE),A
              LD A,B     
              OUT (SWIO),A

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A
  
  
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 52                   *
                                    ;************************************ 
                                    ;* Write block data to SPI          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                     *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE52:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE52SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE52SLT: PUSH BC               ; Save SPI BUS & LEN to stack
              LD B,SSPI             ; Set type SSPI
              LD C,H                ; Set SPI ADDR
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI BUS & LEN from stack
              LD A,H                ; Get SPI ADDR
              JP C,CORE52           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI WRB(BUS,ADDR,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Write block data to SPI          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE52:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,A                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Send data
              ADD A,#03             
              OUT (ALE),A

CORE52LOOP:   LD A,(HL)
              INC HL
              INC HL     
              OUT (SWIO),A

              DJNZ CORE52LOOP

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A     


              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 53                   *
                                    ;************************************ 
                                    ;* Read data from SPI               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE53:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE53SLT     ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE53SLT: PUSH BC
              LD B,SSPI             ; Set type SPI 
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC
              JP C,CORE53           ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RD(ADDR,BUS,DATA)            *
                                    ;************************************ 
                                    ;* Read data from SPI               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = SPI DATA                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE53:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(SPI DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,C                ; E contains SPI ADDR
              LD A,E                ; Set active Bus
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,B     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Read data
              ADD A,#03             
              OUT (ALE),A
              IN A,(SWIO)
              LD (HL),A
              LD A,#00
              INC HL
              LD (HL),A

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 54                   *
                                    ;************************************ 
                                    ;* Read block data from SPI         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE54:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE54SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE54SLT: PUSH BC               ; Save SPI BUS & LEN to stack
              LD B,SSPI             ; Set type SSPI
              LD C,H                ; Set SPI ADDR
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI BUS & LEN from stack
              LD A,H                ; Get SPI ADDR
              JP C,CORE54           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RDB(ADDR,BUS,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Read block data from SPI         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE54:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,A                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Read data
              ADD A,#03             
              OUT (ALE),A

CORE54LOOP:   IN A,(SWIO)
              LD (HL),A
              INC HL
              LD A,#00
              LD (HL),A
              INC HL     

              DJNZ CORE54LOOP

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A   
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 55                   *
                                    ;************************************ 
                                    ;* Read block data from SPI after   *
                                    ;* Header                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* H = SPI LEN HEADER               *
                                    ;* L = SPI LEN DATA                 *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* (IX) = SPI LEN HEADER            *
                                    ;* (IX+1) = SPI LEN DATA            *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE55:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE55SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE55SLT: LD A,C                ; Get SPI ADDR
              PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              LD A,H                ; SPI LEN HEADER
              LD (IX),A
              LD A,L                ; SPI LEN DATA
              LD (IX+1),A

              POP BC                ; Get SPI ADDR & BUS from stack
              JP C,CORE55           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RDH(ADDR,BUS,LEN,LEN,DATA)   *
                                    ;************************************ 
                                    ;* Read block data from SPI after   *
                                    ;* Header                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* (IX) = SPI LEN HEADER            *
                                    ;* (IX+1) = SPI LEN DATA            *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE55:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(SPI DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,C                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,B     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD B,(IX)             ; SPI LEN HEADER

              LD A,E                ; Write Header
              ADD A,#03             
              OUT (ALE),A

CORE55LOOP:   LD A,(HL)
              INC HL
              INC HL     
              OUT (SWIO),A

              DJNZ CORE55LOOP

              LD B,(IX+1)           ; SPI LEN HEADER

CORE55LOOPR:  IN A,(SWIO)
              LD (HL),A
              INC HL
              LD A,#00
              LD (HL),A
              INC HL

              DJNZ CORE55LOOPR

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A   

              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 56                   *
                                    ;************************************ 
                                    ;* Write MODE to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE56:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE56SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE56SLT: PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI ADDR & BUS from stack
              JP C,CORE56           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI MODE(ADDR,BUS,MOD,FQZ)       *
                                    ;************************************ 
                                    ;* Write MODE to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE56:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains SPI ADDR
              OUT (ALE),A

              LD A,D                ; Prepare SPI MODE
              SLA A
              SLA A
              SLA A
              SLA A
              LD D,A

              LD A,E                ; Prepare SPI FREQUENCY
              SLA A
              SLA A
              SLA A
              SLA A
              SLA A
              SLA A
              LD E,A

              LD A,B                ; Prepare SPI BUS
              OR D                  ; Prepare MODE
              OR E                  ; 0bEEDDxxBB
   
              OUT (SWIO),A          ; Send new MODE to SPI

              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET

;---------------------------

                                    ;************************************
                                    ;* FN_CORE57 - ADS INIT, unchanged  *
                                    ;* from RCXROM.ASM (no shared       *
                                    ;* CORE57 body in the original)     *
                                    ;************************************

FN_CORE57:    CALL  RCXINIT
              JP    Z,FN_CORE57SLT
              SCF
              RET

FN_CORE57SLT: PUSH  BC
              LD    B,SADS
              PUSH  IX
              CALL  SETRCXSLT
              POP   IX
              POP   BC
              JP    C,FN_CORE57OK
              SCF
              RET

FN_CORE57OK:  CCF
              RET

                                    ;************************************
                                    ;* FN_CORE58 / CORE58 - ADS MODE.   *
                                    ;* Adapted: CORE58ERR (TYPE out of  *
                                    ;* range) returns Carry set instead *
                                    ;* of the BASIC PROMPT/CHPUT/       *
                                    ;* B_ERRTYPE sequence - no BASIC    *
                                    ;* interpreter here.                *
                                    ;************************************

FN_CORE58:    CALL  RCXINIT
              JP    Z,FN_CORE58SLT
              SCF
              RET

FN_CORE58SLT: PUSH  DE
              LD    B,SADS
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   DE
              JP    C,CORE58
              SCF
              RET

CORE58:       PUSH  HL

              LD    A,E
              CP    #00C
              JP    NC,CORE58ERR

              DI

              CALL  IOEN

              LD    A,C
              OUT   (ALE),A
              LD    A,E
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

CORE58ERR:    POP   HL
              SCF
              RET

                                    ;************************************
                                    ;* FN_CORE59 / CORE59 - ADS CMD,    *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE59:    CALL  RCXINIT
              JP    Z,FN_CORE59SLT
              SCF
              RET

FN_CORE59SLT: PUSH  DE
              LD    B,SADS
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   DE
              JP    C,CORE59
              SCF
              RET

CORE59:       PUSH  HL

              DI

              CALL  IOEN

              LD    A,C
              ADD   A,#01
              OUT   (ALE),A
              LD    A,E
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE60 / CORE60 - ADS STATUS, *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE60:    CALL  RCXINIT
              JP    Z,FN_CORE60SLT
              SCF
              RET

FN_CORE60SLT: PUSH  DE
              LD    B,SADS
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   DE
              JP    C,CORE60
              SCF
              RET

CORE60:       PUSH  HL

              EX    DE,HL

              DI

              CALL  IOEN

              LD    A,C
              ADD   A,#01
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A
              INC   HL
              LD    A,#00
              LD    (HL),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE61 / CORE61 - ADS READ,   *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE61:    CALL  RCXINIT
              JP    Z,FN_CORE61SLT
              SCF
              RET

FN_CORE61SLT: PUSH  DE
              LD    B,SADS
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   DE
              JP    C,CORE61
              SCF
              RET

CORE61:       PUSH  HL

              EX    DE,HL

              DI

              CALL  IOEN

              LD    A,C
              ADD   A,#03
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A
              INC   HL
              LD    A,#00
              LD    (HL),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE62 / CORE62 - ADS GET.    *
                                    ;* Adapted: CORE62ERR (TYPE out of  *
                                    ;* range) returns Carry set instead *
                                    ;* of the BASIC PROMPT/CHPUT/       *
                                    ;* B_ERRTYPE sequence - no BASIC    *
                                    ;* interpreter here.                *
                                    ;************************************

FN_CORE62:    CALL  RCXINIT
              JP    Z,FN_CORE62SLT
              SCF
              RET

FN_CORE62SLT: PUSH  BC
              LD    B,SADS
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   BC
              JP    C,CORE62
              SCF
              RET

CORE62:       PUSH  HL

              LD    A,B

              CP    #00C
              JP    NC,CORE62ERR

              PUSH  DE
              PUSH  BC

              DI

              CALL  IOEN

              LD    A,C
              OUT   (ALE),A
              POP   BC
              LD    A,B
              PUSH  BC
              OUT   (SWIO),A

              CALL  IODIS

              EI

              DI

              CALL  IOEN

              LD    A,C
              ADD   A,#01
              OUT   (ALE),A
              LD    A,#080
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   BC
              LD    A,B
              LD    H,0
              LD    L,A
              LD    DE,ADSCHANMAP
              ADD   HL,DE
              LD    A,(HL)
              LD    B,A

              DI

              CALL  IOEN

              LD    A,C
              ADD   A,#01
              OUT   (ALE),A
              LD    A,B
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   DE
              EX    DE,HL

              DI

              CALL  IOEN

              LD    B,4
CORE62LOOP:   LD    A,C
              ADD   A,#03
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A
              INC   HL
              DJNZ  CORE62LOOP

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

CORE62ERR:    POP   HL
              SCF
              RET

                                    ;************************************
                                    ;* ADS TYPE -> mChannel lookup      *
                                    ;* Mirrors the dMeasType switch in  *
                                    ;* MSXADS1220.ino exactly           *
                                    ;************************************

ADSCHANMAP:   DEFB  0,1,2,3,0,0,0,1,1,2,1,3

;---------------------------

                                    ;************************************
                                    ;* FN_CORE63 - DAC INIT, unchanged  *
                                    ;* from RCXROM.ASM (no shared       *
                                    ;* CORE63 body in the original)     *
                                    ;************************************

FN_CORE63:    CALL  RCXINIT
              JP    Z,FN_CORE63SLT
              SCF
              RET

FN_CORE63SLT: PUSH  BC
              LD    B,SDAC
              PUSH  IX
              CALL  SETRCXSLT
              POP   IX
              POP   BC
              JP    C,FN_CORE63OK
              SCF
              RET

FN_CORE63OK:  CCF
              RET

                                    ;************************************
                                    ;* FN_CORE64 / CORE64 - DAC SET.    *
                                    ;* Adapted: COREDACERRC/V (CHANNEL  *
                                    ;* or VAL out of range) return      *
                                    ;* Carry set instead of the BASIC   *
                                    ;* PROMPT/CHPUT/B_ERRTYPE sequence  *
                                    ;* - no BASIC interpreter here.     *
                                    ;************************************

FN_CORE64:    CALL  FN_COREDAC
              JP    C,CORE64
              SCF
              RET

CORE64:       PUSH  HL

              LD    L,#000

CORE64CHK:    LD    A,B
              CP    #004
              JP    NC,COREDACERR

              LD    A,D
              CP    #010
              JP    NC,COREDACERR

              DI

              CALL  IOEN

              LD    A,C
              OUT   (ALE),A
              LD    A,B
              OUT   (SWIO),A

              LD    A,C
              ADD   A,#02
              OUT   (ALE),A
              LD    A,E
              OUT   (SWIO),A

              LD    A,C
              ADD   A,#03
              OUT   (ALE),A
              LD    A,D
              OR    L
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE65 / CORE65 - DAC HOLD,   *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

FN_CORE65:    CALL  FN_COREDAC
              JP    C,CORE65
              SCF
              RET

CORE65:       PUSH  HL

              LD    L,#040

              JP    CORE64CHK

                                    ;************************************
                                    ;* FN_CORE66 / CORE66 - DAC CONF.   *
                                    ;* Adapted: COREDACERR returns      *
                                    ;* Carry set (see FN_CORE64)        *
                                    ;************************************

FN_CORE66:    CALL  FN_COREDAC
              JP    C,CORE66
              SCF
              RET

CORE66:       PUSH  HL

              LD    A,B
              CP    #004
              JP    NC,COREDACERR

              LD    A,E
              CP    #004
              JP    NC,COREDACERR

              DI

              CALL  IOEN

              LD    A,C
              OUT   (ALE),A
              LD    A,B
              OUT   (SWIO),A

              LD    A,C
              ADD   A,#01
              OUT   (ALE),A
              LD    A,E
              OUT   (SWIO),A

              CALL  IODIS

              EI

              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_CORE67 / CORE67 - DAC GET.    *
                                    ;* Adapted: COREDACERR returns      *
                                    ;* Carry set (see FN_CORE64)        *
                                    ;************************************

FN_CORE67:    CALL  FN_COREDAC
              JP    C,CORE67
              SCF
              RET

CORE67:       PUSH  HL

              LD    A,B
              CP    #004
              JP    NC,COREDACERR

              PUSH  DE
              EX    DE,HL

              DI

              CALL  IOEN

              LD    A,C
              OUT   (ALE),A
              LD    A,B
              OUT   (SWIO),A

              LD    A,C
              ADD   A,#02
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A
              INC   HL

              LD    A,C
              ADD   A,#03
              OUT   (ALE),A
              IN    A,(SWIO)
              LD    (HL),A

              CALL  IODIS

              EI

              POP   DE
              POP   HL

              OR    A
              RET

                                    ;************************************
                                    ;* FN_COREDAC - module installed    *
                                    ;* and card found, unchanged from   *
                                    ;* RCXROM.ASM                       *
                                    ;************************************

FN_COREDAC:   CALL  RCXINIT
              JR    Z,FN_COREDACSLT
              OR    A
              RET

FN_COREDACSLT:
              PUSH  BC
              LD    B,SDAC
              PUSH  IX
              CALL  GETRCXSLT
              POP   IX
              POP   BC
              RET

                                    ;************************************
                                    ;* COREDACERR - replaces RCXROM's   *
                                    ;* COREDACERRC/COREDACERRV/         *
                                    ;* CORE66ERR: Carry set, no BASIC   *
                                    ;************************************

COREDACERR:   POP   HL
              SCF
              RET

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type H                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card SC725 (Z80 SIO/2 + Z80 CTC) as MSX COM port          +++
;+++ RS-232C Extended BIOS (EXTBIO device 8), multi channel type.     +++
;+++ Driver code from RCXROM.ASM without the BASIC parts (device      +++
;+++ COMn:, CALL statements). State, interrupt routine and the        +++
;+++ RS-232C jump table live at fixed page-3 addresses (R_COM_BASE),  +++
;+++ reachable for the interrupt and for CALSLT callers; the table    +++
;+++ entries call the routines in this segment via the RAM helper.    +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

RDSLT:        EQU    #0000C         ; Read byte from slot
WRSLT:        EQU    #00014         ; Write byte to slot
CALSLT:       EQU    #0001C         ; Inter slot call
IDFREQ:       EQU    #0002B         ; ROM ID, bit 7: 0 = 60 Hz, 1 = 50 Hz
BREAKX:       EQU    #000B7         ; Carry if CTRL-STOP pressed
JIFFY:        EQU    #0FC9E         ; Interrupt counter
EXPTBL:       EQU    #0FCC1         ; Slot expanded

                                    ;************************************
                                    ;* Page 3 (fixed, after the CAN     *
                                    ;* area #0CD00-0CF0D)               *
                                    ;************************************

R_COM_BASE:   EQU  #0CF10           ; COM state, X_HCOMxx relative to it
X_HCOMFLG:    EQU  #00000           ; COM flags (bit 0 = active,
                                    ; bit 1 = interrupt registered)
X_HCOMIO:     EQU  #00001           ; COM SIO IO address
X_HCOMCH:     EQU  #00002           ; COM active channel
X_HCOMC0:     EQU  #00003           ; COM channel block 0
X_COMBLK:     EQU  #00014           ; Size of one COM channel block
X_HCOMPAR:    EQU  X_HCOMC0+2*X_COMBLK ; COM INIT parameters
X_HCOMGP:     EQU  X_HCOMPAR+#0E    ; 2 bytes for XIO GET MASK
X_HCOMINT:    EQU  #00040           ; COM interrupt routine
X_HCOMI01:    EQU  X_HCOMINT+HCOM001-HCOMINT+1 ; ISR work area pointer
R_COM_TBL:    EQU  R_COM_BASE+X_HCOMINT+HCOMINTE-HCOMINT ; Jump table
R_COM_IYA:    EQU  R_COM_TBL+COMSIY-COMTBLT+2 ; Slot/segment in the table
R_COM_HLPA:   EQU  R_COM_TBL+COMSHLP-COMTBLT+1 ; RAM helper in the table

                                    ; COM channel block (X_COMBLK bytes)
CB_MODE:      EQU  #00              ; Open mode, 0 = closed
CB_FLAGS:     EQU  #01              ; Bit 0 XON/XOFF, 1 CTS-RTS, 2 LF RX,
                                    ; 3 LF TX, 4 SI/SO, 7 initialised
CB_WR4:       EQU  #02              ; SIO WR4 (clock, stop bits, parity)
CB_WR3:       EQU  #03              ; SIO WR3 (RX bits, RX enable)
CB_WR5:       EQU  #04              ; SIO WR5 (DTR, TX bits, TX enable, RTS)
CB_TC:        EQU  #05              ; CTC time constant
CB_TOUT:      EQU  #06              ; Time out in seconds, 0 = none
CB_BKF:       EQU  #07              ; Backup character valid
CB_BKC:       EQU  #08              ; Backup character
CB_STL:       EQU  #09              ; Status low of last character
CB_STH:       EQU  #0A              ; Status high of last character
CB_FCBL:      EQU  #0B              ; Receive buffer address low
CB_FCBH:      EQU  #0C              ; Receive buffer address high
CB_LEN:       EQU  #0D              ; Receive buffer length in characters
CB_XOFF:      EQU  #0E              ; XOFF received
CB_TSTL:      EQU  #0F              ; Time out start (JIFFY) low
CB_TSTH:      EQU  #10              ; Time out start (JIFFY) high
CB_TLML:      EQU  #11              ; Time out limit in ticks low
CB_TLMH:      EQU  #12              ; Time out limit in ticks high
CB_STATE:     EQU  #13              ; Bit 0 RTS held off, 1 XOFF sent,
                                    ; 2 SO sent, 3 SO received

                                    ; COM receive buffer (OPEN, at FCB);
                                    ; bytes 0, 4, 6, 8 belong to BASIC
FC_PUT:       EQU  #01              ; Write index (interrupt routine)
FC_GET:       EQU  #02              ; Read index (GETCHR)
FC_CNT:       EQU  #03              ; Characters in buffer
FC_LEN:       EQU  #05              ; Buffer length in characters
FC_OVF:       EQU  #07              ; Bit 7 = buffer overflow
FC_DATA:      EQU  #09              ; Character, status (2 bytes each)

                                    ;************************************
                                    ;* COM: address in the page-3 COM   *
                                    ;* state                            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = X_HCOMxx                    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = R_COM_BASE + DE             *
                                    ;************************************

GETCOMX:      LD HL,R_COM_BASE
              ADD HL,DE
              RET

                                    ;************************************
                                    ;* EXTBIO device 8, function 0      *
                                    ;************************************
                                    ;* If COM INIT was done: appends    *
                                    ;* slot of page 3, address of the   *
                                    ;* page-3 jump table (low, high)    *
                                    ;* and 0 at [B:HL], HL = HL+4, then *
                                    ;* old hook                         *
                                    ;************************************

DO_EXT8:      LD A,E
              OR A                  ; Function 0?
              JP NZ,JUMP_OLD
              CALL COMCHK           ; RCX installed, COM active?
              JP NZ,JUMP_OLD

              POP AF
              POP BC                ; B = slot of the table
              POP HL                ; HL = table address
              PUSH AF               ; Save caller's AF
              PUSH DE               ; D = 8, E = 0

              CALL COMP3S
              LD E,A                ; Slot of page 3
              CALL DO_EXT8W
              LD E,R_COM_TBL & #0FF ; Jump table low
              CALL DO_EXT8W
              LD E,R_COM_TBL / 256  ; Jump table high
              CALL DO_EXT8W
              LD E,#000             ; Reserved
              CALL DO_EXT8W
              EI

              POP DE
              POP AF
              PUSH HL
              PUSH BC
              PUSH AF
              JP JUMP_OLD

DO_EXT8W:     PUSH BC
              PUSH DE
              LD A,B
              CALL WRSLT            ; [B:HL] = E
              POP DE
              POP BC
              INC HL
              RET

                                    ;************************************
                                    ;* COM: slot of page 3              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = slot (bit 7 = expanded)      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMP3S:       PUSH BC
              PUSH HL
              IN A,(#0A8)
              RLCA
              RLCA
              AND #003              ; Primary slot of page 3
              LD C,A
              LD B,#000
              LD HL,EXPTBL
              ADD HL,BC
              LD A,(HL)
              AND #080              ; Expanded?
              JR Z,COMP3S1
              LD A,(#0FFFF)         ; Secondary slot register (inverted)
              CPL
              RLCA
              RLCA
              AND #003
              ADD A,A
              ADD A,A
              OR C
              OR #080
              LD C,A
COMP3S1:      LD A,C
              POP HL
              POP BC
              RET

                                    ;************************************
                                    ;* RS-232C jump table template,     *
                                    ;* copied to R_COM_TBL by COM INIT  *
                                    ;************************************
                                    ;* Each entry calls its routine in  *
                                    ;* this segment via the RAM helper  *
                                    ;* (IYh = slot, IYl = segment, IX = *
                                    ;* address; AF, BC, DE, HL passed   *
                                    ;* and returned). Only relative     *
                                    ;* jumps inside the copy            *
                                    ;************************************

COMTBLT:      DEFB DVINFB           ; Device information
              DEFB #01              ; Version 1 = multi channel type
              DEFB #00              ; Reserved
              JP R_COM_TBL+COMS00-COMTBLT ; INIT
              JP R_COM_TBL+COMS01-COMTBLT ; OPEN
              JP R_COM_TBL+COMS02-COMTBLT ; STAT
              JP R_COM_TBL+COMS03-COMTBLT ; GETCHR
              JP R_COM_TBL+COMS04-COMTBLT ; SNDCHR
              JP R_COM_TBL+COMS05-COMTBLT ; CLOSE
              JP R_COM_TBL+COMS06-COMTBLT ; EOF
              JP R_COM_TBL+COMS07-COMTBLT ; LOC
              JP R_COM_TBL+COMS08-COMTBLT ; LOF
              JP R_COM_TBL+COMS09-COMTBLT ; BACKUP
              JP R_COM_TBL+COMS10-COMTBLT ; SNDBRK
              JP R_COM_TBL+COMS11-COMTBLT ; DTR
              JP R_COM_TBL+COMS12-COMTBLT ; SETCHN
              DEFB #0C9,#000,#000   ; No entry
              DEFB #0C9,#000,#000   ; No entry

COMS00:       PUSH IX
              LD IX,COMINI
              JR COMSCALL
COMS01:       PUSH IX
              LD IX,COMOPN
              JR COMSCALL
COMS02:       PUSH IX
              LD IX,COMSTA
              JR COMSCALL
COMS03:       PUSH IX
              LD IX,COMGET
              JR COMSCALL
COMS04:       PUSH IX
              LD IX,COMSND
              JR COMSCALL
COMS05:       PUSH IX
              LD IX,COMCLO
              JR COMSCALL
COMS06:       PUSH IX
              LD IX,COMEOF
              JR COMSCALL
COMS07:       PUSH IX
              LD IX,COMLOC
              JR COMSCALL
COMS08:       PUSH IX
              LD IX,COMLOF
              JR COMSCALL
COMS09:       PUSH IX
              LD IX,COMBCK
              JR COMSCALL
COMS10:       PUSH IX
              LD IX,COMBRK
              JR COMSCALL
COMS11:       PUSH IX
              LD IX,COMDTR
              JR COMSCALL
COMS12:       PUSH IX
              LD IX,COMSCH
COMSCALL:     PUSH IY
COMSIY:       LD IY,0               ; Slot/segment, set by COM INIT
COMSHLP:      CALL 0                ; RAM helper, set by COM INIT
              POP IY
              POP IX
              RET
COMTBLTE:

                                    ;************************************
                                    ;* COM interrupt routine template,  *
                                    ;* copied to R_COM_BASE+X_HCOMINT   *
                                    ;* by COM INIT (from RCXROM.ASM)    *
                                    ;************************************

DVINFB:       EQU    #000           ; Device information byte

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* Routine A = 68                   *
                                    ;************************************
                                    ;* Register SC725 card as COM port  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO IOADDR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SIO IOADDR                   *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE68:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE68SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE68SLT: PUSH BC               ; Save BC to stack
              LD B,SCOM             ; Set type SCOM
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Check card with BC
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,CORE68           ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

                                    ;************************************
                                    ;* COM INIT                         *
                                    ;************************************
                                    ;* Resets CTC channel 0..3 (&H03    *
                                    ;* twice), SIO channel A and B,     *
                                    ;* clears both channel blocks and   *
                                    ;* sets COM active                  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO IOADDR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, CARRY-Flag = 0               *
                                    ;************************************

CORE68:       CALL COMOFF           ; Close open channels of a former INIT

              PUSH HL
              PUSH DE
              PUSH BC

              LD HL,HCOMINT         ; Interrupt routine to page 3
              LD DE,R_COM_BASE+X_HCOMINT
              LD BC,HCOMINTE-HCOMINT
              LDIR
              LD HL,COMTBLT         ; RS-232C jump table to page 3
              LD DE,R_COM_TBL
              LD BC,COMTBLTE-COMTBLT
              LDIR
              LD HL,(MY_HELPER)     ; RAM helper: call routine in segment
              LD (R_COM_HLPA),HL
              LD A,(MY_SEG)         ; IYl = segment
              LD (R_COM_IYA),A
              LD A,(MY_SLOT)        ; IYh = slot
              LD (R_COM_IYA+1),A
              POP BC
              PUSH BC

              LD DE,X_HCOMIO
              CALL GETCOMX         ; [HL] = COM IO address
              LD (HL),C             ; Save SIO IO addr
              INC HL
              LD (HL),#00           ; Channel 0 active
              INC HL                ; [HL] = channel block 0
              LD B,2*X_COMBLK
CORE68CLR:    LD (HL),#00           ; Clear channel blocks
              INC HL
              DJNZ CORE68CLR

              LD A,C
              ADD A,#08
              LD C,A                ; C = CTC channel 0
              LD B,#04              ; 4 CTC channels
CORE68CTC:    LD A,#03              ; Control word, software reset
              CALL COMWR            ; Taken as TC if one was expected
              CALL COMWR
              INC C
              DJNZ CORE68CTC

              POP BC
              PUSH BC               ; C = SIO A ctrl
              LD A,#18              ; Channel reset
              CALL COMWR
              INC C
              INC C                 ; C = SIO B ctrl
              CALL COMWR

              LD DE,X_HCOMFLG
              CALL GETCOMX
              LD (HL),#01           ; COM active

              LD A,#001             ; Defaults for channel 1 and 0
CORE68DF:     PUSH AF
              CALL COMSCH
              LD HL,COMDEFP
              LD DE,R_COM_BASE+X_HCOMPAR
              LD BC,#000D
              LDIR                  ; Defaults to the parameter table
              CALL COMINIP
              POP AF
              SUB #001
              JR NC,CORE68DF        ; Ends with channel 0 active

              POP BC
              POP DE
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag
              RET

                                    ;************************************
                                    ;* COM INIT defaults (as Sony       *
                                    ;* HBI-232 after power on)          *
                                    ;************************************

COMDEFP:      DEFM "8N1XHNNN"       ; 8 bit, no parity, 1 stop bit,
                                    ; XON/XOFF, CTS-RTS
              DEFW 1200             ; Receive baud
              DEFW 1200             ; Send baud
              DEFB 0                ; No time out

;---------------------------

                                    ;************************************
                                    ;* COM: write switched IO           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = port, A = value              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* Interrupts enabled at the end    *
                                    ;************************************

COMWR:        PUSH AF               ; Save value
              DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A           ; Set port
              POP AF
              OUT (SWIO),A          ; Write value
              CALL IODIS            ; Switched IO disabled
              EI
              RET

                                    ;************************************
                                    ;* COM: read switched IO            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = port                         *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = value                        *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F, interrupts enabled at the end *
                                    ;************************************

COMRD:        DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A           ; Set port
              IN A,(SWIO)           ; Read value
              CALL IODIS            ; Switched IO disabled
              EI
              RET

                                    ;************************************
                                    ;* COM: write SIO register          *
                                    ;************************************
                                    ;* Pointer and value without        *
                                    ;* interrupt in between             *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO ctrl port                *
                                    ;* B = register 1..7                *
                                    ;* A = value                        *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* Interrupts enabled at the end    *
                                    ;************************************

COMWRR:       PUSH AF               ; Save value
              DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A           ; Set port
              LD A,B
              OUT (SWIO),A          ; Register pointer
              POP AF
              OUT (SWIO),A          ; Register value
              CALL IODIS            ; Switched IO disabled
              EI
              RET

                                    ;************************************
                                    ;* COM: check RCX and COM active    *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* ZERO-Flag = 1 -> active          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMCHK:       CALL RCXINIT          ; ZERO-Flag = 1 -> RCX installed
              RET NZ
              PUSH HL
              PUSH DE
              LD DE,X_HCOMFLG
              CALL GETCOMX
              LD A,(HL)
              POP DE
              POP HL
              AND #01               ; Bit 0 = COM active
              XOR #01               ; ZERO-Flag = 1 -> active
              RET

                                    ;************************************
                                    ;* COM: context of active channel   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* IX = channel block               *
                                    ;* C = SIO ctrl port                *
                                    ;* B = CTC port                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL                           *
                                    ;************************************

COMCTX:       PUSH DE
              LD DE,X_HCOMIO
              CALL GETCOMX         ; [HL] = IO addr, [HL+1] = channel
              LD A,(HL)             ; SIO IO addr
              INC HL
              LD E,(HL)             ; E = active channel
              ADD A,E
              LD B,A                ; B = IO addr + channel
              ADD A,E
              LD C,A                ; C = SIO ctrl port
              LD A,B
              ADD A,#08
              LD B,A                ; B = CTC port
              INC HL                ; [HL] = channel block 0
              LD A,E
              OR A
              JR Z,COMCTX1
              LD DE,X_COMBLK
              ADD HL,DE             ; [HL] = channel block 1
COMCTX1:      PUSH HL
              POP IX                ; IX = channel block
              POP DE
              RET

;---------------------------

                                    ;************************************
                                    ;* Data length 5..8 to SIO WR3      *
                                    ;* bits 7-6 (WR5 bits 6-5 = >> 1)   *
                                    ;************************************

COMBITS:      DEFB #000,#080,#040,#0C0

                                    ;************************************
                                    ;* Baud rate table: baud, CTC time  *
                                    ;* constant (0 = 256), SIO WR4      *
                                    ;* clock bits (#040 x16, #0C0 x64)  *
                                    ;* CTC input = CLK / 2              *
                                    ;* (3579545 Hz / 2)                 *
                                    ;************************************

COMBAUD:      DEFW 110
              DEFB 254,#0C0
              DEFW 300
              DEFB 93,#0C0
              DEFW 600
              DEFB 186,#040
              DEFW 1200
              DEFB 93,#040
              DEFW 1800
              DEFB 62,#040
              DEFW 2000
              DEFB 56,#040
              DEFW 2400
              DEFB 47,#040
              DEFW 3600
              DEFB 31,#040
              DEFW 4800
              DEFB 23,#040
              DEFW 7200
              DEFB 16,#040
              DEFW 9600
              DEFB 12,#040
              DEFW 19200
              DEFB 6,#040
              DEFW 0                ; End of table

;---------------------------

                                    ;************************************
                                    ;* RS-232C INIT                     *
                                    ;************************************
                                    ;* Initialize the active channel    *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* HL = parameter table             *
                                    ;* B = slot of parameter table      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> illegal        *
                                    ;* parameters                       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMINI:       PUSH HL
              PUSH DE
              PUSH BC
              PUSH IX
              CALL COMCHK           ; RCX installed, COM active?
              JP NZ,COMINIER

              PUSH HL               ; Source of parameter table
              PUSH BC               ; B = slot
              LD DE,X_HCOMPAR
              CALL GETCOMX
              EX DE,HL              ; DE = copy in work area
              POP BC
              POP HL
              LD C,#0E              ; 14 bytes
COMINIRD:     PUSH BC
              PUSH DE
              LD A,B
              CALL RDSLT            ; A = byte from slot B
              POP DE
              POP BC
              LD (DE),A
              INC HL
              INC DE
              DEC C
              JR NZ,COMINIRD
              EI
              JR COMINIC

                                    ;************************************
                                    ;* RS-232C INIT from the copy in    *
                                    ;* the work area (X_HCOMPAR)        *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> illegal        *
                                    ;* parameters                       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMINIP:      PUSH HL
              PUSH DE
              PUSH BC
              PUSH IX
              CALL COMCHK           ; RCX installed, COM active?
              JP NZ,COMINIER

COMINIC:      CALL COMCTX           ; IX = block, C = SIO, B = CTC
              PUSH BC               ; Save ports
              LD DE,X_HCOMPAR
              CALL GETCOMX         ; [HL] = parameter copy

              LD A,(HL)             ; Data length 5..8
              SUB '5'
              CP #04
              JP NC,COMINIEP
              LD E,A
              LD D,#00
              PUSH HL
              LD HL,COMBITS
              ADD HL,DE
              LD A,(HL)             ; WR3 bits 7-6
              POP HL
              LD E,A
              OR #001               ; RX enable
              LD (IX+CB_WR3),A
              LD A,E
              SRL A                 ; WR5 bits 6-5
              OR #088               ; DTR on, TX enable, RTS off
              LD (IX+CB_WR5),A

              INC HL                ; Parity
              LD A,(HL)
              LD E,#000             ; No parity
              CP 'N'
              JR Z,COMINIPA
              CP 'I'                ; Ignore parity
              JR Z,COMINIPA
              LD E,#003             ; Even parity
              CP 'E'
              JR Z,COMINIPA
              LD E,#001             ; Odd parity
              CP 'O'
              JP NZ,COMINIEP

COMINIPA:     INC HL                ; Stop bits 1 / 1.5 / 2
              LD A,(HL)
              SUB '0'
              CP #01
              JP C,COMINIEP
              CP #04
              JP NC,COMINIEP
              ADD A,A
              ADD A,A               ; WR4 bits 3-2
              OR E
              LD (IX+CB_WR4),A      ; Stop bits and parity

              LD D,#000             ; Flags
              INC HL                ; XON/XOFF control
              LD A,(HL)
              CP 'X'
              JR NZ,COMINIF1
              SET 0,D
              JR COMINIG1
COMINIF1:     CP 'N'
              JP NZ,COMINIEP
COMINIG1:
              INC HL                ; CTS-RTS handshake
              LD A,(HL)
              CP 'H'
              JR NZ,COMINIF2
              SET 1,D
              JR COMINIG2
COMINIF2:     CP 'N'
              JP NZ,COMINIEP
COMINIG2:
              INC HL                ; Auto LF for receive
              LD A,(HL)
              CP 'A'
              JR NZ,COMINIF3
              SET 2,D
              JR COMINIG3
COMINIF3:     CP 'N'
              JP NZ,COMINIEP
COMINIG3:
              INC HL                ; Auto LF for send
              LD A,(HL)
              CP 'A'
              JR NZ,COMINIF4
              SET 3,D
              JR COMINIG4
COMINIF4:     CP 'N'
              JP NZ,COMINIEP
COMINIG4:
              INC HL                ; SI/SO control
              LD A,(HL)
              CP 'S'
              JR NZ,COMINIF5
              SET 4,D
              JR COMINIG5
COMINIF5:     CP 'N'
              JP NZ,COMINIEP
COMINIG5:
              LD A,D
              OR #080               ; Channel initialised
              LD (IX+CB_FLAGS),A

              INC HL                ; Receiver baud rate
              LD E,(HL)
              INC HL
              LD D,(HL)
              INC HL                ; Transmitter baud rate
              LD A,(HL)
              CP E
              JP NZ,COMINIEP        ; Only one baud clock per channel
              INC HL
              LD A,(HL)
              CP D
              JP NZ,COMINIEP
              INC HL                ; Time out
              LD A,(HL)
              LD (IX+CB_TOUT),A

              BIT 7,D               ; Negative: value to CTC directly
              JR Z,COMINIBT
              XOR A
              SUB E
              LD (IX+CB_TC),A
              LD A,#040             ; x16
              JR COMINIBC

COMINIBT:     LD HL,COMBAUD
COMINIBL:     LD C,(HL)
              INC HL
              LD B,(HL)
              INC HL
              LD A,B
              OR C
              JP Z,COMINIEP         ; Baud rate not in table
              LD A,C
              CP E
              JR NZ,COMINIBN
              LD A,B
              CP D
              JR NZ,COMINIBN
              LD A,(HL)
              LD (IX+CB_TC),A
              INC HL
              LD A,(HL)             ; WR4 clock bits
              JR COMINIBC
COMINIBN:     INC HL
              INC HL
              JR COMINIBL

COMINIBC:     OR (IX+CB_WR4)
              LD (IX+CB_WR4),A

              POP BC                ; C = SIO ctrl, B = CTC
              PUSH BC
              LD C,B
              LD A,#057             ; Counter, rising edge, TC follows
              CALL COMWR
              LD A,(IX+CB_TC)
              CALL COMWR
              POP BC
              PUSH BC
              LD A,#018             ; Channel reset
              CALL COMWR
              LD B,#004
              LD A,(IX+CB_WR4)
              CALL COMWRR
              LD B,#003
              LD A,(IX+CB_WR3)
              CALL COMWRR
              LD B,#005
              LD A,(IX+CB_WR5)
              CALL COMWRR
              LD B,#001             ; No interrupts
              XOR A
              CALL COMWRR
              LD DE,X_HCOMCH
              CALL GETCOMX
              LD A,(HL)
              OR A
              JR Z,COMINIOK
              LD B,#002             ; Channel B: vector 8
              LD A,#008
              CALL COMWRR
COMINIOK:     POP BC
              POP IX
              POP BC
              POP DE
              POP HL
              OR A                  ; Clear Carry Flag
              RET

COMINIEP:     POP BC                ; Remove ports
COMINIER:     POP IX
              POP BC
              POP DE
              POP HL
              SCF                   ; Set CARRY flag for ERROR
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C STAT                     *
                                    ;************************************
                                    ;* Read status of the active        *
                                    ;* channel                          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = status (bit 7 CTS, 2 break, *
                                    ;* 0 DCD, 15..11 errors of the      *
                                    ;* last character read)             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

COMSTA:       PUSH AF
              PUSH BC
              PUSH DE
              PUSH IX
              LD HL,#0000
              CALL COMCHK
              JR NZ,COMSTAE
              CALL COMCTX
              CALL COMRR0           ; A = RR0, current state
              LD E,#000
              BIT 5,A               ; CTS
              JR Z,COMSTA1
              SET 7,E
COMSTA1:      BIT 3,A               ; DCD
              JR Z,COMSTA2
              SET 0,E
COMSTA2:      BIT 7,A               ; Break
              JR Z,COMSTA3
              SET 2,E
COMSTA3:      LD A,(IX+CB_STL)
              OR E
              LD L,A
              LD H,(IX+CB_STH)
COMSTAE:      POP IX
              POP DE
              POP BC
              POP AF
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C SNDCHR                   *
                                    ;************************************
                                    ;* Send a character on the active   *
                                    ;* channel. SI/SO: SO (&H0E) before *
                                    ;* characters with bit 7, SI (&H0F) *
                                    ;* back, bit 7 removed. Auto LF:    *
                                    ;* LF after CR                      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = character                    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> CTRL-STOP      *
                                    ;* ZERO-Flag = 1 -> time out        *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F                                *
                                    ;************************************

COMSND:       PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              PUSH AF               ; Character
              CALL COMCHK
              JR NZ,COMSNDTO
              CALL COMCTX           ; IX = block, C = SIO ctrl
              POP AF
              PUSH AF
              LD D,A                ; D = character

              BIT 4,(IX+CB_FLAGS)   ; SI/SO?
              JR Z,COMSNDC
              BIT 7,D
              JR Z,COMSNDI
              BIT 2,(IX+CB_STATE)   ; SO already sent?
              JR NZ,COMSND7
              LD A,#00E             ; SO
              CALL COMSN1
              JR C,COMSNDE
              JR Z,COMSNDE
              SET 2,(IX+CB_STATE)
COMSND7:      RES 7,D
              JR COMSNDC
COMSNDI:      BIT 2,(IX+CB_STATE)   ; SO active?
              JR Z,COMSNDC
              LD A,#00F             ; SI
              CALL COMSN1
              JR C,COMSNDE
              JR Z,COMSNDE
              RES 2,(IX+CB_STATE)

COMSNDC:      LD A,D
              CALL COMSN1           ; Character
              JR C,COMSNDE
              JR Z,COMSNDE
              LD A,D
              CP #00D               ; CR?
              JR NZ,COMSNDOK
              BIT 3,(IX+CB_FLAGS)   ; Auto LF send?
              JR Z,COMSNDOK
              LD A,#00A             ; LF
              CALL COMSN1
              JR COMSNDE

COMSNDOK:     LD A,#001
              OR A                  ; ZERO-Flag = 0, CARRY-Flag = 0
              JR COMSNDE

COMSNDTO:     XOR A                 ; ZERO-Flag = 1, CARRY-Flag = 0

COMSNDE:      POP BC                ; B = character
              LD A,B                ; Flags stay
              POP IX
              POP HL
              POP DE
              POP BC
              RET

                                    ;************************************
                                    ;* COM: send one character          *
                                    ;************************************
                                    ;* Waits for TX empty, CTS (CTS-RTS *
                                    ;* handshake) and XON (XON/XOFF;    *
                                    ;* not for XON/XOFF themselves),    *
                                    ;* CTRL-STOP, time out (INIT)       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = character                    *
                                    ;* IX = block, C = SIO ctrl         *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> CTRL-STOP      *
                                    ;* ZERO-Flag = 1 -> time out        *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F, B, E, HL                      *
                                    ;************************************

COMSN1:       PUSH AF               ; Character
              PUSH DE
              CALL COMTOS           ; Start time out
              POP DE

COMSN1L:      CALL COMBRKX          ; CTRL-STOP?
              JR C,COMSN1B
              CALL COMRR0           ; A = RR0, current state
              LD E,A
              BIT 1,(IX+CB_FLAGS)   ; CTS-RTS handshake?
              JR Z,COMSN1X
              BIT 5,E               ; CTS?
              JR Z,COMSN1W
COMSN1X:      BIT 0,(IX+CB_FLAGS)   ; XON/XOFF?
              JR Z,COMSN1G
              POP AF
              PUSH AF
              CP #011               ; XON and XOFF always
              JR Z,COMSN1G
              CP #013
              JR Z,COMSN1G
              LD A,(IX+CB_XOFF)
              OR A                  ; XOFF received?
              JR NZ,COMSN1W
COMSN1G:      POP AF
              PUSH AF
              CALL COMPUT           ; Written?
              JR NZ,COMSN1OK
COMSN1W:      PUSH DE
              CALL COMTOC           ; Time out reached?
              POP DE
              JR C,COMSN1L          ; No? Wait

              POP AF                ; Time out
              LD B,A
              XOR A                 ; ZERO-Flag = 1, CARRY-Flag = 0
              LD A,B
              RET

COMSN1B:      POP AF                ; CTRL-STOP
              LD B,A
              LD A,#001
              OR A                  ; ZERO-Flag = 0
              SCF                   ; CARRY-Flag = 1
              LD A,B
              RET

COMSN1OK:     POP AF
              LD B,A
              LD A,#001
              OR A                  ; ZERO-Flag = 0, CARRY-Flag = 0
              LD A,B
              RET

                                    ;************************************
                                    ;* COM: write character if TX empty *
                                    ;************************************
                                    ;* Test and write without interrupt *
                                    ;* in between (the interrupt        *
                                    ;* routine may send XOFF)           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = character, C = SIO ctrl      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* ZERO-Flag = 0 -> written         *
                                    ;* ZERO-Flag = 1 -> TX busy         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F, interrupts enabled at the end *
                                    ;************************************

COMPUT:       PUSH BC
              LD B,A                ; B = character
              DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A
              IN A,(SWIO)           ; RR0
              AND #004              ; TX buffer empty?
              JR Z,COMPUTE          ; No? ZERO-Flag = 1
              LD A,C
              INC A
              OUT (ALE),A
              LD A,B
              OUT (SWIO),A          ; Character
              LD A,#001
              OR A                  ; ZERO-Flag = 0
COMPUTE:      CALL IODIS            ; Switched IO disabled
              EI
              LD A,B
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C DTR                      *
                                    ;************************************
                                    ;* Turn DTR of the active channel   *
                                    ;* on or off                        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = 0 off, otherwise on          *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F                                *
                                    ;************************************

COMDTR:       PUSH AF
              PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              LD D,A
              CALL COMCHK
              JR NZ,COMDTRE
              CALL COMCTX
              LD A,(IX+CB_WR5)
              RES 7,A               ; DTR off
              INC D
              DEC D
              JR Z,COMDTR1
              SET 7,A               ; DTR on
COMDTR1:      LD (IX+CB_WR5),A
              XOR A
              CALL COMW5            ; RTS stays off if held
COMDTRE:      POP IX
              POP HL
              POP DE
              POP BC
              POP AF
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C SETCHN                   *
                                    ;************************************
                                    ;* Set the active channel           *
                                    ;* (0 = SIO A, 1 = SIO B)           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = channel                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> channel not    *
                                    ;* in the cartridge                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC                           *
                                    ;************************************

COMSCH:       CP #002
              CCF                   ; CARRY-Flag = 1 if A >= 2
              RET C
              PUSH HL
              PUSH DE
              PUSH AF
              CALL COMCHK
              JR NZ,COMSCHE
              LD DE,X_HCOMCH
              CALL GETCOMX
              POP AF
              LD (HL),A
              POP DE
              POP HL
              OR A                  ; Clear Carry Flag
              RET
COMSCHE:      POP AF
              POP DE
              POP HL
              SCF                   ; Set CARRY flag for ERROR
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C OPEN                     *
                                    ;************************************
                                    ;* Open the active channel: receive *
                                    ;* buffer at FCB (9 bytes control + *
                                    ;* 2 bytes per character), register *
                                    ;* the interrupt routine at XIO,    *
                                    ;* receive interrupt on, RTS on     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* HL = FCB (>= &H8000)             *
                                    ;* C = buffer length 32..254        *
                                    ;* E = mode 1 input, 2 output,      *
                                    ;* 4 raw input/output               *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> error          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMOPN:       PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              PUSH IY
              PUSH HL
              POP IY                ; IY = FCB
              CALL COMCHK           ; RCX installed, COM active?
              JP NZ,COMOPNE
              BIT 7,H               ; FCB >= &H8000?
              JP Z,COMOPNE
              LD A,C
              CP 32                 ; Length 32..254?
              JP C,COMOPNE
              CP 255
              JP NC,COMOPNE
              LD A,E
              CP #001               ; Input
              JR Z,COMOPNM
              CP #002               ; Output
              JR Z,COMOPNM
              CP #004               ; Raw input/output
              JP NZ,COMOPNE
COMOPNM:      LD D,C                ; D = length, E = mode
              CALL COMCTX           ; IX = block, C = SIO ctrl
              BIT 7,(IX+CB_FLAGS)   ; Channel initialised?
              JP Z,COMOPNE
              LD A,(IX+CB_MODE)
              OR A                  ; Already open?
              JP NZ,COMOPNE

              PUSH IY
              POP HL
              LD (IX+CB_FCBL),L
              LD (IX+CB_FCBH),H
              LD (IY+FC_PUT),#000   ; Clear buffer control (the
              LD (IY+FC_GET),#000   ; other FCB bytes belong to BASIC)
              LD (IY+FC_CNT),#000
              LD (IY+FC_OVF),#000
              LD (IY+FC_LEN),D
              LD (IX+CB_LEN),D
              LD (IX+CB_BKF),#000
              LD (IX+CB_STL),#000
              LD (IX+CB_STH),#000
              LD (IX+CB_XOFF),#000
              LD (IX+CB_STATE),#000

              PUSH BC
              PUSH DE
              CALL COMXON           ; Register interrupt at XIO
              POP DE
              POP BC
              JP C,COMOPNE

              CALL COMFLU           ; Discard old characters
              LD (IX+CB_MODE),E     ; Channel open

              PUSH BC
              LD DE,X_HCOMIO
              CALL GETCOMX
              LD A,(HL)
              ADD A,#02
              LD C,A                ; C = SIO B ctrl
              LD B,#002             ; Vector 8 -> XIO Z80 channel 4
              LD A,#008
              CALL COMWRR
              POP BC

              LD B,#001             ; Interrupt on all received
              LD A,#018             ; characters
              CALL COMWRR
              LD A,(IX+CB_WR5)
              SET 1,A               ; RTS on
              LD (IX+CB_WR5),A
              LD B,#005
              CALL COMWRR
              OR A                  ; Clear Carry Flag
              JR COMOPNR

COMOPNE:      SCF                   ; Set CARRY flag for ERROR
COMOPNR:      POP IY
              POP IX
              POP HL
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C GETCHR                   *
                                    ;************************************
                                    ;* Get a character of the active    *
                                    ;* channel: backed up character     *
                                    ;* first, then receive buffer.      *
                                    ;* Waits for a character with       *
                                    ;* CTRL-STOP and time out (INIT)    *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = character                    *
                                    ;* SIGN-Flag = 1 -> error (STAT:    *
                                    ;* overflow, framing, overrun,      *
                                    ;* parity, time out, CTRL-STOP)     *
                                    ;* CARRY-Flag = 1 -> EOF (&H1A) in  *
                                    ;* input mode                       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMGET:       PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              PUSH IY
              CALL COMCHK           ; RCX installed, COM active?
              JP NZ,COMGETN
              CALL COMCTX           ; IX = block
              LD A,(IX+CB_MODE)
              OR A                  ; Channel open?
              JP Z,COMGETN
              LD L,(IX+CB_FCBL)
              LD H,(IX+CB_FCBH)
              PUSH HL
              POP IY                ; IY = receive buffer
              CALL COMTOS           ; Start time out

COMGETL:      DI
              LD A,(IX+CB_BKF)
              OR A                  ; Backed up character?
              JR Z,COMGET1
              LD (IX+CB_BKF),#000
              LD D,(IX+CB_BKC)
              LD E,#000             ; No error
              JR COMGETS

COMGET1:      LD A,(IY+FC_CNT)
              OR A                  ; Character in buffer?
              JR NZ,COMGET2
              EI                    ; Receive interrupt must run
              CALL COMBRKX          ; CTRL-STOP?
              JR C,COMGETB
              CALL COMTOC           ; Time out reached?
              JR C,COMGETL          ; No? Wait
              LD E,#040             ; Status bit 14: time out
              JR COMGETX
COMGETB:      LD E,#004             ; Status bit 10: CTRL-STOP
COMGETX:      LD (IX+CB_STH),E
              LD HL,#0080           ; A = 0, SIGN-Flag = 1
              JR COMGETF

COMGET2:      LD L,(IY+FC_GET)
              LD H,#000
              ADD HL,HL
              LD DE,FC_DATA
              ADD HL,DE
              PUSH IY
              POP DE
              ADD HL,DE             ; HL = FCB + 9 + 2 * read index
              LD D,(HL)             ; D = character
              INC HL
              LD E,(HL)             ; E = status
              LD A,(IY+FC_GET)
              INC A
              CP (IY+FC_LEN)
              JR C,COMGET3
              XOR A                 ; Ring index back to 0
COMGET3:      LD (IY+FC_GET),A
              DEC (IY+FC_CNT)
              LD A,(IY+FC_OVF)
              OR E                  ; Status bit 15: overflow
              LD E,A
              LD (IY+FC_OVF),#000
              CALL COMFLW           ; RTS on / XON if space again

COMGETS:      EI
              LD (IX+CB_STH),E      ; Status of this character
              LD L,#000             ; Flags
              LD A,E
              OR A
              JR Z,COMGET4
              LD L,#080             ; SIGN-Flag = 1 -> error
COMGET4:      LD A,(IX+CB_MODE)
              CP #001               ; Input mode?
              JR NZ,COMGET5
              LD A,D
              CP #01A               ; EOF?
              JR NZ,COMGET5
              SET 0,L               ; CARRY-Flag = 1 -> EOF
COMGET5:      LD H,D                ; H = character
              JR COMGETF

COMGETN:      LD HL,#0080           ; A = 0, SIGN-Flag = 1

COMGETF:      PUSH HL
              POP AF                ; A = character, F = flags
              POP IY
              POP IX
              POP HL
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C CLOSE                    *
                                    ;************************************
                                    ;* Close the active channel: EOF    *
                                    ;* in output mode, receive          *
                                    ;* interrupt off, RTS off; after    *
                                    ;* the last channel the interrupt   *
                                    ;* is removed from XIO              *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 0                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMCLO:       PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              CALL COMCHK           ; RCX installed, COM active?
              JR NZ,COMCLOR
              CALL COMCTX           ; IX = block, C = SIO ctrl
              LD A,(IX+CB_MODE)
              OR A                  ; Channel open?
              JR Z,COMCLOR
              CP #002               ; Output mode?
              JR NZ,COMCLO1
              LD A,#01A
              CALL COMSND           ; Send EOF
COMCLO1:      CALL COMCLS1          ; Interrupt off, RTS off, closed
              CALL COMANY           ; Other channel open?
              CALL Z,COMXOF         ; No? Remove interrupt from XIO
COMCLOR:      POP IX
              POP HL
              POP DE
              POP BC
              OR A                  ; Clear Carry Flag
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C EOF                      *
                                    ;************************************
                                    ;* Is the next character EOF?       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = -1, CARRY-Flag = 1 -> EOF   *
                                    ;* HL = 0, CARRY-Flag = 0 -> no EOF *
                                    ;* or no character                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMEOF:       PUSH BC
              PUSH DE
              PUSH IX
              CALL COMCUR           ; Channel open?
              JR Z,COMEOFN
              LD A,(IX+CB_BKF)
              OR A                  ; Backed up character?
              LD A,(IX+CB_BKC)
              JR NZ,COMEOF1
              DI
              LD A,(HL)             ; Characters in buffer
              OR A
              JR Z,COMEOFE
              LD L,(IX+CB_FCBL)
              LD H,(IX+CB_FCBH)
              PUSH HL
              LD DE,FC_GET
              ADD HL,DE
              LD E,(HL)             ; Read index
              LD D,#000
              EX DE,HL
              ADD HL,HL
              LD BC,FC_DATA
              ADD HL,BC
              POP DE
              ADD HL,DE             ; HL = FCB + 9 + 2 * read index
              LD A,(HL)             ; Next character
              EI
COMEOF1:      CP #01A               ; EOF?
              JR NZ,COMEOFN
              LD HL,#0FFFF
              SCF                   ; Set CARRY flag for EOF
              JR COMEOFR
COMEOFE:      EI
COMEOFN:      LD HL,#0000
              OR A                  ; Clear Carry Flag
COMEOFR:      POP IX
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C LOC                      *
                                    ;************************************
                                    ;* Characters in the receive buffer *
                                    ;* including a backed up character  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = number of characters        *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMLOC:       PUSH BC
              PUSH DE
              PUSH IX
              CALL COMCUR           ; Channel open?
              LD A,(HL)             ; Characters in buffer
              LD HL,#0000
              JR Z,COMLOCR
              LD L,A
              LD A,(IX+CB_BKF)
              OR A                  ; Backed up character?
              JR Z,COMLOCR
              INC HL
COMLOCR:      POP IX
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C LOF                      *
                                    ;************************************
                                    ;* Free space in the receive buffer *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = free characters             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMLOF:       PUSH BC
              PUSH DE
              PUSH IX
              CALL COMCUR           ; Channel open?
              LD HL,#0000
              JR Z,COMLOFR
              LD A,(IX+CB_LEN)
              SUB D                 ; Length - characters in buffer
              LD L,A
COMLOFR:      POP IX
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C BACKUP                   *
                                    ;************************************
                                    ;* Back up a character; GETCHR      *
                                    ;* returns it first. A former one   *
                                    ;* is lost                          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = character                    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F                                *
                                    ;************************************

COMBCK:       PUSH AF
              PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              LD D,C                ; D = character
              CALL COMCHK           ; RCX installed, COM active?
              JR NZ,COMBCKE
              CALL COMCTX           ; IX = block
              LD (IX+CB_BKC),D
              LD (IX+CB_BKF),#001
COMBCKE:      POP IX
              POP HL
              POP DE
              POP BC
              POP AF
              RET

;---------------------------

                                    ;************************************
                                    ;* RS-232C SNDBRK                   *
                                    ;************************************
                                    ;* Send break (WR5 bit 4) for DE    *
                                    ;* character times: the transmitter *
                                    ;* keeps shifting dummy characters  *
                                    ;* while TXD is held on space       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = number of break characters  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> CTRL-STOP      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE                           *
                                    ;************************************

COMBRK:       PUSH BC
              PUSH HL
              PUSH IX
              CALL COMCHK           ; RCX installed, COM active?
              JR NZ,COMBRKOK
              CALL COMCTX           ; IX = block, C = SIO ctrl
              LD A,#010             ; Send break on
              CALL COMW5

COMBRKL:      LD A,D
              OR E                  ; All break characters?
              JR Z,COMBRKS
              CALL COMBRKX          ; CTRL-STOP?
              JR C,COMBRKB
              XOR A                 ; Dummy character
              CALL COMPUT
              JR Z,COMBRKL          ; TX busy? Wait
              DEC DE
              JR COMBRKL

COMBRKS:      CALL COMBRKX          ; CTRL-STOP?
              JR C,COMBRKB
              CALL COMRR1           ; A = RR1
              RRCA                  ; All sent?
              JR NC,COMBRKS

              XOR A                 ; Send break off
              CALL COMW5
COMBRKOK:     OR A                  ; Clear Carry Flag
              JR COMBRKE

COMBRKB:      XOR A                 ; Send break off
              CALL COMW5
              SCF                   ; Set CARRY flag for CTRL-STOP
COMBRKE:      POP IX
              POP HL
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* COM: write WR5                   *
                                    ;************************************
                                    ;* CB_WR5 OR A; RTS stays off while *
                                    ;* held by CTS-RTS handshake        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = additional bits (#010 break) *
                                    ;* IX = block, C = SIO ctrl         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, interrupts enabled at the end*
                                    ;************************************

COMW5:        PUSH BC
              OR (IX+CB_WR5)
              BIT 0,(IX+CB_STATE)   ; RTS held off?
              JR Z,COMW51
              RES 1,A
COMW51:       LD B,#005
              CALL COMWRR
              POP BC
              RET

                                    ;************************************
                                    ;* COM: read current RR0            *
                                    ;************************************
                                    ;* The SIO latches CTS, DCD and     *
                                    ;* break in RR0 after a change;     *
                                    ;* WR0 &H10 (reset external/status  *
                                    ;* interrupts) first gives the      *
                                    ;* current state                    *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO ctrl                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = RR0                          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F, interrupts enabled at the end *
                                    ;************************************

COMRR0:       DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A
              LD A,#010             ; Reset external/status interrupts
              OUT (SWIO),A
              IN A,(SWIO)           ; RR0
              CALL IODIS            ; Switched IO disabled
              EI
              RET

                                    ;************************************
                                    ;* COM: read RR1                    *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO ctrl                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = RR1 (bit 0 all sent)         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* F, interrupts enabled at the end *
                                    ;************************************

COMRR1:       DI
              CALL IOEN             ; Switched IO enabled
              LD A,C
              OUT (ALE),A
              LD A,#001
              OUT (SWIO),A          ; Pointer RR1
              IN A,(SWIO)           ; RR1
              CALL IODIS            ; Switched IO disabled
              EI
              RET

                                    ;************************************
                                    ;* COM: receive flow control back   *
                                    ;************************************
                                    ;* After GETCHR: with 16 or more    *
                                    ;* free characters RTS on again     *
                                    ;* (CTS-RTS) and XON (XON/XOFF), if *
                                    ;* the interrupt routine turned     *
                                    ;* them off                         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* IX = block, IY = FCB,            *
                                    ;* C = SIO ctrl                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, interrupts enabled if a      *
                                    ;* signal was changed               *
                                    ;************************************

COMFLW:       LD A,(IY+FC_LEN)
              SUB (IY+FC_CNT)
              CP #010               ; 16 or more free?
              RET C
              BIT 0,(IX+CB_STATE)   ; RTS held off?
              JR Z,COMFLW1
              RES 0,(IX+CB_STATE)
              XOR A
              CALL COMW5            ; RTS on
COMFLW1:      BIT 1,(IX+CB_STATE)   ; XOFF sent?
              RET Z
              RES 1,(IX+CB_STATE)
COMFLW2:      LD A,#011             ; XON
              CALL COMPUT
              JR Z,COMFLW2          ; TX busy? Wait
              RET

;---------------------------

                                    ;************************************
                                    ;* COM: active channel open?        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* ZERO-Flag = 1 -> not open        *
                                    ;* ZERO-Flag = 0 -> IX = block,     *
                                    ;* HL = FCB + FC_CNT, D = count     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC, D, HL, IX                *
                                    ;************************************

COMCUR:       LD HL,COMCURZ         ; [HL] = 0 if not open
              CALL COMCHK           ; RCX installed, COM active?
              JR NZ,COMCURN
              CALL COMCTX           ; IX = block
              LD L,(IX+CB_FCBL)
              LD H,(IX+CB_FCBH)
              LD A,(IX+CB_MODE)
              LD BC,FC_CNT
              ADD HL,BC             ; [HL] = characters in buffer
              LD D,(HL)
              OR A                  ; ZERO-Flag = 1 -> not open
              RET NZ
              LD HL,COMCURZ
              RET
COMCURN:      XOR A                 ; ZERO-Flag = 1
              RET

COMCURZ:      DEFB #000

                                    ;************************************
                                    ;* COM: discard received characters *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SIO ctrl                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, B                            *
                                    ;************************************

COMFLU:       LD B,#004             ; SIO FIFO 3 characters
COMFLUL:      CALL COMRD            ; RR0
              RRCA                  ; Character available?
              JR NC,COMFLUE
              INC C
              CALL COMRD            ; Read and discard
              DEC C
              DJNZ COMFLUL
COMFLUE:      LD A,#030             ; Error reset
              JP COMWR

                                    ;************************************
                                    ;* COM: close one channel without   *
                                    ;* EOF: receive interrupt off, RTS  *
                                    ;* off, mode = closed               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* IX = block, C = SIO ctrl         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, B                            *
                                    ;************************************

COMCLS1:      LD B,#001             ; No interrupts
              XOR A
              CALL COMWRR
              LD A,(IX+CB_WR5)
              RES 1,A               ; RTS off
              LD (IX+CB_WR5),A
              LD B,#005
              CALL COMWRR
              LD (IX+CB_MODE),#000  ; Closed
              LD (IX+CB_STATE),#000
              RET

                                    ;************************************
                                    ;* COM: any channel open?           *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* ZERO-Flag = 1 -> none            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, HL                       *
                                    ;************************************

COMANY:       LD DE,X_HCOMC0+CB_MODE
              CALL GETCOMX
              LD A,(HL)
              LD DE,X_COMBLK
              ADD HL,DE
              OR (HL)
              RET

                                    ;************************************
                                    ;* COM: close all channels and      *
                                    ;* remove the interrupt from XIO    *
                                    ;* (RCX DEINIT, COM INIT)           *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

COMOFF:       PUSH AF
              PUSH BC
              PUSH DE
              PUSH HL
              PUSH IX
              CALL COMCHK           ; RCX installed, COM active?
              JR NZ,COMOFFE
              LD DE,X_HCOMCH
              CALL GETCOMX
              LD A,(HL)
              PUSH AF               ; Save active channel
              XOR A
COMOFFL:      LD (HL),A             ; Channel 0, 1
              PUSH AF
              PUSH HL
              CALL COMCTX           ; IX = block, C = SIO ctrl
              LD A,(IX+CB_MODE)
              OR A
              CALL NZ,COMCLS1       ; Open? Close
              POP HL
              POP AF
              INC A
              CP #002
              JR C,COMOFFL
              POP AF
              LD (HL),A             ; Restore active channel
              CALL COMXOF           ; Remove interrupt from XIO
COMOFFE:      POP IX
              POP HL
              POP DE
              POP BC
              POP AF
              RET

                                    ;************************************
                                    ;* COM: start time out              *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* IX = block (CB_TOUT seconds)     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, B, DE, HL                    *
                                    ;************************************

COMTOS:       LD E,(IX+CB_TOUT)     ; Time out limit in ticks
              LD D,#000
              LD HL,#0000
              LD B,60
              CALL COMFREQ          ; A = IDFREQ
              BIT 7,A               ; 50 Hz?
              JR Z,COMTOSM
              LD B,50
COMTOSM:      ADD HL,DE
              DJNZ COMTOSM
              LD (IX+CB_TLML),L
              LD (IX+CB_TLMH),H
              LD HL,(JIFFY)
              LD (IX+CB_TSTL),L
              LD (IX+CB_TSTH),H
              RET

                                    ;************************************
                                    ;* COM: check time out              *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* IX = block                       *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> go on waiting  *
                                    ;* CARRY-Flag = 0 -> time out       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, HL                       *
                                    ;************************************

COMTOC:       LD A,(IX+CB_TOUT)
              OR A                  ; No time out?
              SCF
              RET Z
              LD HL,(JIFFY)
              LD E,(IX+CB_TSTL)
              LD D,(IX+CB_TSTH)
              OR A
              SBC HL,DE             ; Elapsed ticks
              LD E,(IX+CB_TLML)
              LD D,(IX+CB_TLMH)
              OR A
              SBC HL,DE             ; CARRY-Flag = 1 -> elapsed < limit
              RET

                                    ;************************************
                                    ;* COM: CTRL-STOP via the BIOS      *
                                    ;************************************
                                    ;* BREAKX in the main ROM by        *
                                    ;* CALSLT: page 0 is RAM under      *
                                    ;* MSX-DOS. Interrupts enabled at   *
                                    ;* the end (receive interrupt)      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> CTRL-STOP      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMBRKX:      PUSH IX
              PUSH IY
              LD IX,BREAKX
              LD IY,(EXPTBL-1)      ; IYh = slot of the main ROM
              CALL CALSLT
              EI
              POP IY
              POP IX
              RET

                                    ;************************************
                                    ;* COM: IDFREQ via the BIOS         *
                                    ;************************************
                                    ;* Reads IDFREQ from the main ROM   *
                                    ;* by RDSLT (page 0 is RAM under    *
                                    ;* MSX-DOS). Interrupts enabled at  *
                                    ;* the end                          *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = IDFREQ (bit 7 = 50 Hz)       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COMFREQ:      PUSH BC
              PUSH DE
              PUSH HL
              LD A,(EXPTBL)         ; Slot of the main ROM
              LD HL,IDFREQ
              CALL RDSLT
              EI
              POP HL
              POP DE
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* COM: register the interrupt      *
                                    ;* routine at XIO (once)            *
                                    ;************************************
                                    ;* Sets the work area pointer in    *
                                    ;* the routine, XIO SET ADDR        *
                                    ;* (channel 12, page 3), GET MASK,  *
                                    ;* SET MASK with bit 12             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> XIO missing    *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC, DE, HL                   *
                                    ;************************************

COMXON:       LD DE,X_HCOMFLG
              CALL GETCOMX
              LD A,(HL)
              AND #002              ; Already registered?
              RET NZ                ; Yes? CARRY-Flag = 0

              LD DE,X_HCOMIO
              CALL GETCOMX
              PUSH HL
              LD DE,X_HCOMI01
              CALL GETCOMX         ; [HL] = pointer in the routine
              POP DE
              LD (HL),E
              INC HL
              LD (HL),D             ; Routine points to HCOMIO

              LD DE,X_HCOMINT
              CALL GETCOMX
              EX DE,HL              ; DE = interrupt routine
              LD BC,#0CFF           ; Z80 channel 12, page 3
              LD A,#007             ; XIO SET ADDR
              CALL COMXIO
              RET C

              LD DE,X_HCOMGP
              CALL GETCOMX
              EX DE,HL              ; DE = 2 bytes for the mask
              PUSH DE
              LD A,#00A             ; XIO GET MASK
              CALL COMXIO
              POP HL
              RET C
              LD E,(HL)             ; PIC mask
              INC HL
              LD A,(HL)
              OR #010               ; Z80 channel 12
              LD D,A
              LD A,#008             ; XIO SET MASK
              CALL COMXIO
              RET C

              LD DE,X_HCOMFLG
              CALL GETCOMX
              SET 1,(HL)            ; Registered
              OR A                  ; Clear Carry Flag
              RET

                                    ;************************************
                                    ;* COM: remove the interrupt        *
                                    ;* routine from XIO                 *
                                    ;************************************
                                    ;* XIO GET MASK, SET MASK without   *
                                    ;* bit 12, SET ADDR(12,0)           *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC, DE, HL                   *
                                    ;************************************

COMXOF:       LD DE,X_HCOMFLG
              CALL GETCOMX
              LD A,(HL)
              AND #002              ; Registered?
              RET Z
              RES 1,(HL)

              LD DE,X_HCOMGP
              CALL GETCOMX
              EX DE,HL              ; DE = 2 bytes for the mask
              PUSH DE
              LD A,#00A             ; XIO GET MASK
              CALL COMXIO
              POP HL
              JR C,COMXOFA
              LD E,(HL)             ; PIC mask
              INC HL
              LD A,(HL)
              AND #0EF              ; Without Z80 channel 12
              LD D,A
              LD A,#008             ; XIO SET MASK
              CALL COMXIO
COMXOFA:      LD DE,#0000           ; Standard routine of XIO
              LD BC,#0CFF           ; Z80 channel 12
              LD A,#007             ; XIO SET ADDR
              CALL COMXIO
              OR A
              RET

                                    ;************************************
                                    ;* COM: call XIO UNAPI function     *
                                    ;************************************
                                    ;* Looks up "XIO_IO_EXPANDER" via   *
                                    ;* EXTBIO and calls the entry with  *
                                    ;* CALSLT                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = function                     *
                                    ;* BC, DE = parameters              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> error or XIO   *
                                    ;* not installed                    *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC, DE                       *
                                    ;************************************

COMXIO:       PUSH HL
              PUSH IX
              PUSH IY
              PUSH AF               ; Function
              PUSH BC
              PUSH DE
              LD HL,COMXIOID
              LD DE,ARG
              LD BC,#0010           ; Name with 0
              LDIR
              LD HL,#0000
              LD DE,#02222          ; UNAPI
              LD A,#001             ; First implementation
              CALL EXTBIO           ; A = slot, HL = entry
              LD C,A
              LD A,H
              OR L
              JR Z,COMXIOE          ; Not found
              PUSH HL
              POP IX                ; IX = entry
              LD H,C
              LD L,#000
              PUSH HL
              POP IY                ; IYH = slot
              POP DE
              POP BC
              POP AF                ; A = function
              CALL CALSLT
              JR COMXIOR
COMXIOE:      POP DE
              POP BC
              POP AF
              SCF                   ; Set CARRY flag for ERROR
COMXIOR:      POP IY
              POP IX
              POP HL
              RET

COMXIOID:     DEFM "XIO_IO_EXPANDER",0

                                    ;************************************
                                    ;* COM interrupt routine            *
                                    ;************************************
                                    ;* Called by XIO (Z80 channel 4,    *
                                    ;* SIO vector 8). For SIO A and B:  *
                                    ;* if the channel is open, all      *
                                    ;* received characters with error   *
                                    ;* bits (RR1) into the ring buffer  *
                                    ;* at FCB; WR0 &H30 error reset.    *
                                    ;* Then WR0 A &H38 (return from     *
                                    ;* interrupt, the SIO does not see  *
                                    ;* the RETI through XIO) and RETI.  *
                                    ;* Only relative jumps: runs as     *
                                    ;* copy in the work area            *
                                    ;************************************

HCOMINT:      LD A,WTEC
              OUT (IOENDIS),A

HCOM001:      LD HL,0               ; Will be filled by OPEN
                                    ; HL points to HCOMIO in work area
              LD C,(HL)             ; C = SIO A ctrl
              INC HL
              INC HL                ; HL = channel block 0
              LD B,#02              ; 2 channels

HCOMICH:      PUSH BC
              PUSH HL
              PUSH HL
              POP IX                ; IX = channel block
              LD A,(IX+CB_MODE)
              OR A
              JR Z,HCOMINX          ; Channel closed
              LD L,(IX+CB_FCBL)
              LD H,(IX+CB_FCBH)
              PUSH HL
              POP IY                ; IY = receive buffer (FCB)
              JR HCOMIRX

HCOMINX:      POP HL
              LD DE,X_COMBLK
              ADD HL,DE             ; HL = next channel block
              POP BC
              INC C
              INC C                 ; C = next SIO ctrl
              DJNZ HCOMICH

              DEC C
              DEC C
              DEC C
              DEC C                 ; C = SIO A ctrl
              LD A,C
              OUT (ALE),A
              LD A,#038             ; Return from interrupt
              OUT (SWIO),A

              XOR A
              OUT (IOENDIS),A

              RETI


HCOMIRX:      LD A,C
              OUT (ALE),A           ; SIO ctrl
              IN A,(SWIO)           ; RR0
              RRCA                  ; RX character available?
              JR NC,HCOMINX         ; No? Next channel
              LD A,#01
              OUT (SWIO),A          ; Pointer RR1
              IN A,(SWIO)           ; RR1
              RRCA                  ; Framing, overrun, parity error
              AND #038              ; to status bits 5, 4, 3
              LD E,A                ; E = status
              LD A,C
              INC A
              OUT (ALE),A           ; SIO data
              IN A,(SWIO)
              LD D,A                ; D = character
              LD A,C
              OUT (ALE),A           ; SIO ctrl
              LD A,#030             ; Error reset
              OUT (SWIO),A

              BIT 0,(IX+CB_FLAGS)   ; XON/XOFF?
              JR Z,HCOMISI
              LD A,D
              CP #013               ; XOFF: stop sending
              JR NZ,HCOMIXN
              LD (IX+CB_XOFF),#001
              JR HCOMIRX
HCOMIXN:      CP #011               ; XON: send again
              JR NZ,HCOMISI
              LD (IX+CB_XOFF),#000
              JR HCOMIRX

HCOMISI:      BIT 4,(IX+CB_FLAGS)   ; SI/SO?
              JR Z,HCOMIBF
              LD A,D
              CP #00E               ; SO: bit 7 on
              JR NZ,HCOMISO
              SET 3,(IX+CB_STATE)
              JR HCOMIRX
HCOMISO:      CP #00F               ; SI: bit 7 off
              JR NZ,HCOMIS7
              RES 3,(IX+CB_STATE)
HCOMIRH:      JR HCOMIRX
HCOMIS7:      BIT 3,(IX+CB_STATE)
              JR Z,HCOMIBF
              SET 7,D

HCOMIBF:      LD A,(IY+FC_CNT)
              CP (IY+FC_LEN)        ; Buffer full?
              JR C,HCOMIST
              SET 7,(IY+FC_OVF)     ; Yes? Character lost
              JR HCOMIFL

HCOMIST:      INC (IY+FC_CNT)
              PUSH BC
              LD L,(IY+FC_PUT)
              LD H,#00
              ADD HL,HL
              LD BC,FC_DATA
              ADD HL,BC
              PUSH IY
              POP BC
              ADD HL,BC             ; HL = FCB + 9 + 2 * write index
              LD (HL),D             ; Character
              INC HL
              LD (HL),E             ; Status
              POP BC
              LD A,(IY+FC_PUT)
              INC A
              CP (IY+FC_LEN)
              JR C,HCOMIPT
              XOR A                 ; Ring index back to 0
HCOMIPT:      LD (IY+FC_PUT),A

              LD A,D
              CP #00D               ; CR?
              JR NZ,HCOMIFL
              BIT 2,(IX+CB_FLAGS)   ; Auto LF receive?
              JR Z,HCOMIFL
              LD DE,#00A00          ; LF without error
              JR HCOMIBF
HCOMIRK:      JR HCOMIRH            ; Next character

HCOMIFL:      LD A,(IY+FC_LEN)      ; Flow control
              SUB (IY+FC_CNT)
              CP #010               ; Less than 16 free?
              JR NC,HCOMIRK         ; No? Next character
              BIT 1,(IX+CB_FLAGS)   ; CTS-RTS?
              JR Z,HCOMIXF
              BIT 0,(IX+CB_STATE)   ; RTS already off?
              JR NZ,HCOMIXF
              SET 0,(IX+CB_STATE)
              LD A,C
              OUT (ALE),A           ; SIO ctrl
              LD A,#005
              OUT (SWIO),A          ; Pointer WR5
              LD A,(IX+CB_WR5)
              AND #0FD              ; RTS off
              OUT (SWIO),A
HCOMIXF:      BIT 0,(IX+CB_FLAGS)   ; XON/XOFF?
              JR Z,HCOMIRK
              BIT 1,(IX+CB_STATE)   ; XOFF already sent?
              JR NZ,HCOMIRK
              LD A,C
              OUT (ALE),A           ; SIO ctrl
              IN A,(SWIO)           ; RR0
              AND #004              ; TX buffer empty?
              JR Z,HCOMIRK          ; No? Next received character
              SET 1,(IX+CB_STATE)
              LD A,C
              INC A
              OUT (ALE),A           ; SIO data
              LD A,#013             ; XOFF
              OUT (SWIO),A
              JR HCOMIRK            ; Next character


HCOMINTE:

;---------------------------

;---------------------------

                                    ;************************************
                                    ;* Auxiliary routines               *
                                    ;************************************

TOUPPER:      CP    'a'
              RET   C
              CP    'z'+1
              RET   NC
              AND   #0DF
              RET

;---------------------------

                                    ;============================
                                    ;===  UNAPI related data  ===
                                    ;============================

MY_SLOT:      DEFB  0
MY_SEG:       DEFB  0
MY_HELPER:    DEFW  0              ; RAM helper: call routine in segment

UNAPI_ID:     DEFM  "RCX_INTERFACE",0
UNAPI_ID_END:

APIINFO:      DEFM  "RCX Interface UNAPI RAM implementation (Kartentyp A-I)",0

                                    ;************************************
                                    ;* WORKTEMPLATE - pristine copy of  *
                                    ;* the work area, copied to         *
                                    ;* WORKAREA by CORE2 on every RCX   *
                                    ;* INIT, exactly like RCXROM.ASM's  *
                                    ;* X_ROM template. Same HFLAGS..    *
                                    ;* HENDE layout/size as RCXROM.ASM's*
                                    ;* X_WRK, so X_HFLAGS/X_HCARD stay  *
                                    ;* correct.                          *
                                    ;*                                  *
                                    ;* NOTE: the HCANPTR/HCANMSG/       *
                                    ;* HCANINT fields below (at X_HCANPTR/*
                                    ;* X_HCANMSG/X_HINT, matching        *
                                    ;* RCXROM.ASM's byte layout) are    *
                                    ;* VESTIGIAL/unused dead weight -    *
                                    ;* they live in WORKAREA, which is  *
                                    ;* only mapped into page 1 while    *
                                    ;* RCXRAM's own CALLFN-dispatched   *
                                    ;* code runs. A real CAN hardware   *
                                    ;* interrupt fires asynchronously,  *
                                    ;* regardless of what is currently  *
                                    ;* mapped - a handler here would be *
                                    ;* unreachable/wrong most of the    *
                                    ;* time. The REAL, live CAN         *
                                    ;* interrupt state now lives at the *
                                    ;* fixed, always-mapped page-3      *
                                    ;* R_CAN_HCANPTR/R_CAN_HCANMSG/     *
                                    ;* R_CAN_HCANINT (see the Kartentyp *
                                    ;* E section above) - kept in sync  *
                                    ;* with a diagnosed, real hardware  *
                                    ;* bug (CAN RX never worked with    *
                                    ;* this WORKAREA-based approach).   *
                                    ;************************************

WORKTEMPLATE:

HFLAGS:       DEFB  #00              ; RCX init flag
HGENPURP:     DEFW  #00              ; 16 bit free memory
HCANPTR:      DEFW  #00              ; CAN Msg Count, incoming / read
HCANMSG:      DEFS  #1A0,#00         ; CAN Msg Buffer, 32 messages x 13
                                    ; Bytes
HCARD:        DEFS  X_CD*X_CDSTEP,#00 ; Usage of slot (type/IO)

                                    ;************************************
                                    ;* HCANINT - CAN interrupt routine, *
                                    ;* unchanged from RCXROM.ASM        *
                                    ;************************************

HCANINT:      LD    A,WTEC
              OUT   (IOENDIS),A

HCAN001:      LD    HL,0             ; Will be filled when RXA is init
                                    ; HL points to CAN msg buffer start

HCAN002:      LD    DE,0             ; Will be filled when RXA is init
                                    ; DE points to CAN msg count

              LD    A,(DE)           ; Get CAN msg count
              LD    B,A              ; B contains CAN msg count

HCAN003:      LD    C,0              ; Will be filled when RXA is init
                                    ; C contains IO address

              ADD   A,#01            ; Increase cnt of msg
              CP    X_CANCNT
              JR    NZ,HCANINTL
              LD    A,#00            ; If max cnt set ring index to 0

HCANINTL:     LD    (DE),A           ; Save new CAN msg count

              LD    DE,#00D          ; 13 Bytes to be read

              SCF
              CCF                    ; Clear CARRY flag

              SBC   HL,DE            ; Prepare index

HCANLOOP1:    ADD   HL,DE            ; Move pointer to next buffer to be
                                    ; written
              DJNZ  HCANLOOP1        ; Do loop

              PUSH  HL               ; HL points to next buffer to be
                                    ; written
              POP   IX               ; IX points to active buffer index

              LD    E,C
              INC   E

              LD    A,C              ; C contains IO addr
              OUT   (ALE),A          ; Set ALE to SJA1000
              LD    A,CANINTREG      ; D contains SJA1000 Register addr
              OUT   (SWIO),A         ; Write ALE to SJA1000
              LD    A,E
              OUT   (ALE),A          ; Set Data transfer to SJA1000
              IN    A,(SWIO)         ; Get Data from SJA1000

              LD    D,CANRXBUFREG
              LD    B,#0D            ; Set loop for 13 bytes

HCANINTL1:    LD    A,C              ; C contains IO addr
              OUT   (ALE),A          ; Set ALE to SJA1000
              LD    A,D              ; D contains SJA1000 Register addr
              OUT   (SWIO),A         ; Write ALE to SJA1000
              LD    A,E
              OUT   (ALE),A          ; Set Data transfer to SJA1000
              IN    A,(SWIO)         ; Get Data from SJA1000
              LD    (IX),A           ; Save result in (IX)

              INC   IX               ; Increase read buffer index
              INC   D                ; Increase RX FIFO place

              DJNZ  HCANINTL1        ; Do loop

              LD    A,C              ; C contains IO addr
              OUT   (ALE),A          ; Set ALE to SJA1000
              LD    A,CANCMDREG      ; D contains SJA1000 Register addr
              OUT   (SWIO),A         ; Write ALE to SJA1000
              LD    A,E
              OUT   (ALE),A          ; Set Data transfer to SJA1000
              LD    A,CANRELRXBUF    ; E contains value
              OUT   (SWIO),A         ; Release RX Interrupt

                                    ; Still new msg?

              LD    A,C              ; C contains IO addr
              OUT   (ALE),A          ; Set ALE to SJA1000
              LD    A,CANRMC         ; RMC
              OUT   (SWIO),A         ; Write ALE to SJA1000
              LD    A,E
              OUT   (ALE),A          ; Set Data transfer to SJA1000
              IN    A,(SWIO)         ; Any more new messages?
              OR    A
              JR    NZ,HCAN001       ; Jump if more new msg
                                    ; Otherwise the interrupt will not
                                    ; go back to edge

              LD    A,0
              OUT   (IOENDIS),A

              RET

HENDE:        DEFB  #00

WORKTEMPLATE_END:

WORKAREA:     DEFS  WORKTEMPLATE_END-WORKTEMPLATE,0

SEG_CODE_END:
