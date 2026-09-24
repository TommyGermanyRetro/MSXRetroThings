                                    ;************************************
                                    ;* ADS1220.COM                      *
                                    ;************************************
                                    ;* Runs the same TEST A/B/C/D/F/G   *
                                    ;* as the BASIC original, but via   *
                                    ;* FN_CORE2/4/57..62 directly using *
                                    ;* EXTBIO discovery. Works against  *
                                    ;* RCX_INTERFACE in ROM (CALSLT) or *
                                    ;* in mapped RAM (RCXRAM.COM, via   *
                                    ;* the RAM helper) - see CALLFN.    *
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

RCX_FN_INIT:   EQU  2              ; RCX INIT
RCX_FN_ISINIT: EQU  3              ; RCX ISINIT
RCX_FN_DEINIT: EQU  4              ; RCX DEINIT

ADS_FN_INIT:   EQU  57             ; ADS INIT
ADS_FN_MODE:   EQU  58             ; ADS MODE
ADS_FN_CMD:    EQU  59             ; ADS CMD
ADS_FN_STATUS: EQU  60             ; ADS STATUS
ADS_FN_READ:   EQU  61             ; ADS READ
ADS_FN_GET:    EQU  62             ; ADS GET

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

                                    ; Locate the RAM helper, if any - a
                                    ; mapped-RAM UNAPI target (segment
                                    ; <> FFh, see below) must be called
                                    ; through it, plain CALSLT only
                                    ; switches the slot and knows
                                    ; nothing about mapper segments. Not
                                    ; an error if absent: a ROM-only
                                    ; system has no helper and doesn't
                                    ; need one, CALLFN below only uses
                                    ; HELPERADD when SEG<>FFh, which
                                    ; can't happen without a RAM driver
                                    ; (and thus a helper) installed.

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
                                    ;* ADS INIT                          *
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

              LD    A,(ADSADDR)
              LD    C,A
              LD    A,ADS_FN_INIT
              CALL  CALLFN
              JP    C,SLOTERR

;---------------------------

                                    ;************************************
                                    ;* TEST A: MODE 0..11, start        *
                                    ;* conversion, select channel, read+*
                                    ;* decode RESULT - ADS1220.ASC       *
                                    ;* line 210-380                     *
                                    ;************************************

              XOR   A
              LD    (TVAR),A

TESTA_LOOP:   CALL  DOMODE

              LD    A,#080
              LD    (CMDVAL),A
              CALL  DOCMD           ; start conversion

              LD    A,(TVAR)
              LD    HL,ADSCHANMAP
              LD    D,0
              LD    E,A
              ADD   HL,DE
              LD    A,(HL)
              LD    (VVAR),A
              LD    (CMDVAL),A
              CALL  DOCMD           ; select channel

              CALL  DOSTATUS

              LD    A,(XVAR)
              LD    HL,VVAR
              CP    (HL)
              JR    NZ,TESTA_FAIL
              LD    A,(WVAR)
              OR    A
              JR    Z,TESTA_OK

TESTA_FAIL:   CALL  INCER
              LD    DE,MFAILA
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

TESTA_OK:     CALL  READ4           ; reads 4 RESULT bytes into BYBUF
              CALL  DECODEFLOAT     ; BYBUF -> FS/FDP/DGARR

                                    ; FA(T) = BYBUF, for TEST G

              LD    A,(TVAR)
              LD    H,0
              LD    L,A
              ADD   HL,HL
              ADD   HL,HL           ; HL = T*4
              LD    DE,FABUF
              ADD   HL,DE
              EX    DE,HL           ; DE = destination
              LD    HL,BYBUF
              LD    BC,4
              LDIR

              LD    A,(TVAR)
              CALL  PRNAME
              LD    DE,MCH
              CALL  PRSTR
              LD    A,(VVAR)
              CALL  PRDECB
              LD    DE,MBY
              CALL  PRSTR
              LD    HL,BYBUF
              LD    B,4
              CALL  PRHEXBUF2
              LD    DE,MS
              CALL  PRSTR
              LD    A,(FS)
              CALL  PRDECB
              LD    DE,MEX
              CALL  PRSTR
              LD    A,(BYBUF)
              AND   #07F
              CALL  PRDECB
              LD    DE,MDP
              CALL  PRSTR
              LD    A,(FDP)
              CALL  PRSDECB
              LD    DE,MDG
              CALL  PRSTR
              LD    HL,DGARR
              LD    B,6
PRDG_LOOP:    LD    A,(HL)
              ADD   A,'0'
              CALL  PRCHR
              INC   HL
              DJNZ  PRDG_LOOP

              LD    DE,MF
              CALL  PRSTR
              CALL  PRFLOAT
              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(TVAR)
              INC   A
              LD    (TVAR),A
              CP    12
              JP    C,TESTA_LOOP

              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST B: CMD cmd0 (channel select *
                                    ;* 0..3) readback - ADS1220.ASC      *
                                    ;* 410-470                          *
                                    ;************************************

              XOR   A
              LD    (TVAR),A

TESTB_LOOP:   LD    A,(TVAR)
              LD    (CMDVAL),A
              CALL  DOCMD
              CALL  DOSTATUS

              LD    DE,MCMD0
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MRCHAN
              CALL  PRSTR
              LD    A,(XVAR)
              CALL  PRDECB
              LD    DE,MMCNT
              CALL  PRSTR
              LD    A,(WVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(XVAR)
              LD    HL,TVAR
              CP    (HL)
              JR    NZ,TESTB_FAIL
              LD    A,(WVAR)
              OR    A
              JR    Z,TESTB_NEXT

TESTB_FAIL:   CALL  INCER
              LD    DE,MFAILB
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

TESTB_NEXT:   LD    A,(TVAR)
              INC   A
              LD    (TVAR),A
              CP    4
              JP    C,TESTB_LOOP

              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST C: CMD cmd1 (set mCount)    *
                                    ;* readback - ADS1220.ASC 510-580    *
                                    ;************************************

              XOR   A
              LD    (TVAR),A

TESTC_LOOP:   LD    A,(TVAR)
              ADD   A,A
              ADD   A,A             ; T*4
              OR    #040
              LD    (CMDVAL),A
              CALL  DOCMD
              CALL  DOSTATUS

              LD    DE,MCMD1
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MMCNT
              CALL  PRSTR
              LD    A,(WVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(WVAR)
              LD    HL,TVAR
              CP    (HL)
              JR    Z,TESTC_NEXT

              CALL  INCER
              LD    DE,MFAILC
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

TESTC_NEXT:   LD    A,(TVAR)
              INC   A
              LD    (TVAR),A
              CP    4
              JP    C,TESTC_LOOP

              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST D: CMD cmd3 (reserved) must *
                                    ;* not change state - ADS1220.ASC    *
                                    ;* 610-670                          *
                                    ;************************************

              LD    A,1
              LD    (CMDVAL),A
              CALL  DOCMD
              LD    A,#0C0
              LD    (CMDVAL),A
              CALL  DOCMD
              CALL  DOSTATUS

              LD    DE,MCMD3
              CALL  PRSTR
              LD    A,(XVAR)
              CALL  PRDECB
              LD    DE,MMCNT
              CALL  PRSTR
              LD    A,(WVAR)
              CALL  PRDECB
              LD    DE,MEXPECT10
              CALL  PRSTR

              LD    A,(XVAR)
              CP    1
              JR    NZ,TESTD_FAIL
              LD    A,(WVAR)
              OR    A
              JR    Z,TESTD_DONE

TESTD_FAIL:   CALL  INCER
              LD    DE,MFAILD
              CALL  PRSTR

TESTD_DONE:   LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST F: RESULT auto-increment +  *
                                    ;* wrap x2 - ADS1220.ASC 810-890     *
                                    ;************************************

              LD    A,2
              LD    (CMDVAL),A
              CALL  DOCMD

              XOR   A
              LD    (KVAR),A

TESTF_LOOP:   CALL  DOREAD          ; reads 1 byte -> READBYTE
              CALL  DOSTATUS        ; W = mCount after the read

              LD    DE,MREADK
              CALL  PRSTR
              LD    A,(KVAR)
              CALL  PRDECB
              LD    DE,MBYTE
              CALL  PRSTR
              LD    A,(READBYTE)
              CALL  PRHEXV
              LD    DE,MMCNTAFTER
              CALL  PRSTR
              LD    A,(WVAR)
              CALL  PRDECB
              LD    DE,MEXPECT
              CALL  PRSTR
              LD    A,(KVAR)
              AND   #03
              INC   A
              AND   #03
              LD    (PVAR),A
              CALL  PRDECB
              LD    DE,MCLOSEPAR
              CALL  PRSTR
              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(WVAR)
              LD    HL,PVAR
              CP    (HL)
              JR    Z,TESTF_NEXT

              CALL  INCER
              LD    DE,MFAILF
              CALL  PRSTR
              LD    A,(KVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

TESTF_NEXT:   LD    A,(KVAR)
              INC   A
              LD    (KVAR),A
              CP    8
              JP    C,TESTF_LOOP

              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* TEST G: ADS GET vs TEST A - path *
                                    ;* 1 (individual commands, above) vs*
                                    ;* path 2 (convenience command),    *
                                    ;* tolerance instead of exact       *
                                    ;* comparison - ADS1220.ASC 920-970  *
                                    ;************************************

              XOR   A
              LD    (TVAR),A

TESTG_LOOP:   LD    A,(ADSADDR)
              LD    C,A
              LD    A,(TVAR)
              LD    B,A
              LD    DE,BYBUF
              LD    A,ADS_FN_GET
              CALL  CALLFN

              CALL  DECODEFLOAT     ; BYBUF (=GET result) -> FS/FDP/DGARR
              CALL  TOFIXED10       ; HL = GET value*10 (approximated)
              LD    (GVAL10),HL

              LD    A,(TVAR)
              CALL  PRNAME
              LD    DE,MGET
              CALL  PRSTR
              CALL  PRFLOAT

              LD    DE,MTESTAEQ
              CALL  PRSTR

                                    ; load and decode FA(T)
                                    ; (FABUF+T*4 -> BYBUF, reverse
                                    ; direction from the store in
                                    ; TEST A)

              LD    A,(TVAR)
              LD    H,0
              LD    L,A
              ADD   HL,HL
              ADD   HL,HL
              LD    DE,FABUF
              ADD   HL,DE           ; HL = FABUF+T*4 (source)
              LD    DE,BYBUF        ; DE = BYBUF (destination)
              LD    BC,4
              LDIR

              CALL  DECODEFLOAT
              CALL  PRFLOAT
              LD    DE,MCLOSEPAR
              CALL  PRSTR
              LD    DE,MCRLF
              CALL  PRSTR

              CALL  TOFIXED10       ; HL = TestA value*10

                                    ; D10 = |GVAL10 - HL|

              LD    DE,(GVAL10)
              OR    A
              SBC   HL,DE
              JP    P,TESTG_ABSOK
              XOR   A
              SUB   L
              LD    L,A
              LD    A,0
              SBC   A,H
              LD    H,A

TESTG_ABSOK:  LD    DE,20           ; tolerance 2.0 -> *10 = 20
              OR    A
              SBC   HL,DE
              JP    M,TESTG_NEXT    ; |diff|*10 < 20 -> diff < 2.0 -> OK

              CALL  INCER
              LD    DE,MFAILG
              CALL  PRSTR
              LD    A,(TVAR)
              CALL  PRDECB
              LD    DE,MCRLF
              CALL  PRSTR

TESTG_NEXT:   LD    A,(TVAR)
              INC   A
              LD    (TVAR),A
              CP    12
              JP    C,TESTG_LOOP

              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* SUMMARY + RCX DEINIT - ADS1220.ASC*
                                    ;* 1060-1070                        *
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
                                    ;* DOMODE - ADS MODE(ADSADDR,TVAR)  *
                                    ;************************************

DOMODE:       LD    A,(ADSADDR)
              LD    C,A
              LD    A,(TVAR)
              LD    E,A
              LD    A,ADS_FN_MODE
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* DOCMD - ADS CMD(ADSADDR,CMDVAL)  *
                                    ;************************************

DOCMD:        LD    A,(ADSADDR)
              LD    C,A
              LD    A,(CMDVAL)
              LD    E,A
              LD    A,ADS_FN_CMD
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* DOSTATUS - ADS STATUS(ADSADDR),  *
                                    ;* splits into (XVAR)=rChannel      *
                                    ;* 0..3, (WVAR)=mCount 0..3         *
                                    ;************************************

DOSTATUS:     LD    A,(ADSADDR)
              LD    C,A
              LD    DE,STATBUF
              LD    A,ADS_FN_STATUS
              CALL  CALLFN
              LD    A,(STATBUF)
              LD    (RVAR),A
              AND   #03
              LD    (XVAR),A
              LD    A,(RVAR)
              SRL   A
              SRL   A
              AND   #03
              LD    (WVAR),A
              RET

                                    ;************************************
                                    ;* DOREAD - ADS READ(ADSADDR),      *
                                    ;* result in (READBYTE)             *
                                    ;************************************

DOREAD:       LD    A,(ADSADDR)
              LD    C,A
              LD    DE,BYTEBUF
              LD    A,ADS_FN_READ
              CALL  CALLFN
              LD    A,(BYTEBUF)
              LD    (READBYTE),A
              RET

                                    ;************************************
                                    ;* READ4 - 4x ADS READ(ADSADDR) in  *
                                    ;* BYBUF                            *
                                    ;************************************

READ4:        LD    HL,BYBUF
              LD    B,4

READ4_LOOP:   PUSH  HL
              PUSH  BC
              CALL  DOREAD
              POP   BC
              POP   HL
              LD    A,(READBYTE)
              LD    (HL),A
              INC   HL
              DJNZ  READ4_LOOP
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
                                    ;* DECODEFLOAT - splits BYBUF (4-   *
                                    ;* byte MSX SNG float) into FS(0/1),*
                                    ;* FDP(signed), DGARR(6 digits)     *
                                    ;************************************

DECODEFLOAT:  LD    A,(BYBUF)
              LD    B,A
              AND   #080
              JR    Z,DF_POS
              LD    A,1
              JR    DF_SETS
DF_POS:       XOR   A
DF_SETS:      LD    (FS),A

              LD    A,B
              AND   #07F
              SUB   #040
              LD    (FDP),A

              LD    A,(BYBUF+1)
              LD    B,A
              AND   #0F0
              RRCA
              RRCA
              RRCA
              RRCA
              LD    (DGARR+0),A
              LD    A,B
              AND   #0F
              LD    (DGARR+1),A

              LD    A,(BYBUF+2)
              LD    B,A
              AND   #0F0
              RRCA
              RRCA
              RRCA
              RRCA
              LD    (DGARR+2),A
              LD    A,B
              AND   #0F
              LD    (DGARR+3),A

              LD    A,(BYBUF+3)
              LD    B,A
              AND   #0F0
              RRCA
              RRCA
              RRCA
              RRCA
              LD    (DGARR+4),A
              LD    A,B
              AND   #0F
              LD    (DGARR+5),A

              RET

                                    ;************************************
                                    ;* PRFLOAT - prints FS/FDP/DGARR as *
                                    ;* a decimal number (e.g. "784.788")*
                                    ;************************************

PRFLOAT:      LD    A,(FS)
              OR    A
              JR    Z,PF_NOSIGN
              LD    A,'-'
              CALL  PRCHR
PF_NOSIGN:
              LD    A,(FDP)
              BIT   7,A
              JR    Z,PF_POS

                                    ; DP<0: "0." + |DP| zeros + 6 digits

              LD    A,'0'
              CALL  PRCHR
              LD    A,'.'
              CALL  PRCHR
              LD    A,(FDP)
              NEG
              LD    B,A
PF_NLOOP:     LD    A,B
              OR    A
              JR    Z,PF_ALLDIGITS
              LD    A,'0'
              CALL  PRCHR
              DEC   B
              JR    PF_NLOOP
PF_ALLDIGITS: LD    HL,DGARR
              LD    B,6
PF_DLOOP1:    LD    A,(HL)
              ADD   A,'0'
              CALL  PRCHR
              INC   HL
              DJNZ  PF_DLOOP1
              RET

PF_POS:                             ; DP>=0
              LD    HL,DGARR
              LD    A,(FDP)
              CP    6
              JR    NC,PF_BIGDP

              LD    B,A
              LD    C,0
PF_BEFORE:    LD    A,B
              OR    A
              JR    Z,PF_DOT
              LD    A,(HL)
              ADD   A,'0'
              CALL  PRCHR
              INC   HL
              DEC   B
              INC   C
              JR    PF_BEFORE
PF_DOT:       LD    A,'.'
              CALL  PRCHR
              LD    A,6
              SUB   C
              LD    B,A
PF_AFTER:     LD    A,B
              OR    A
              RET   Z
              LD    A,(HL)
              ADD   A,'0'
              CALL  PRCHR
              INC   HL
              DEC   B
              JR    PF_AFTER

PF_BIGDP:                           ; DP>=6: all 6 digits, then zeros
              LD    B,6
PF_BIGLOOP:   LD    A,(HL)
              ADD   A,'0'
              CALL  PRCHR
              INC   HL
              DJNZ  PF_BIGLOOP
              LD    A,(FDP)
              SUB   6
              LD    B,A
PF_BIGZ:      LD    A,B
              OR    A
              RET   Z
              LD    A,'0'
              CALL  PRCHR
              DEC   B
              JR    PF_BIGZ

                                    ;************************************
                                    ;* MULADD10 - HL = HL*10 + A        *
                                    ;************************************

MULADD10:     LD    E,A
              LD    D,0
              PUSH  DE
              LD    D,H
              LD    E,L
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,DE
              ADD   HL,HL
              POP   DE
              ADD   HL,DE
              RET

                                    ;************************************
                                    ;* DIV10 - HL = HL/10 (unsigned,    *
                                    ;* remainder discarded)             *
                                    ;************************************

DIV10:        LD    DE,0
DIV10_LP:     LD    BC,10
              OR    A
              SBC   HL,BC
              JR    C,DIV10_DONE
              INC   DE
              JR    DIV10_LP
DIV10_DONE:   ADD   HL,BC
              EX    DE,HL
              RET

                                    ;************************************
                                    ;* TOFIXED10 - converts FS/FDP/DGARR*
                                    ;* (first 4 digits, approximated)   *
                                    ;* into HL = signed value*10 (for   *
                                    ;* the tolerance comparison, not    *
                                    ;* for display)                     *
                                    ;************************************

TOFIXED10:    LD    HL,0
              LD    A,(DGARR+0)
              CALL  MULADD10
              LD    A,(DGARR+1)
              CALL  MULADD10
              LD    A,(DGARR+2)
              CALL  MULADD10
              LD    A,(DGARR+3)
              CALL  MULADD10        ; HL = 4-digit number 0..9999

              LD    A,(FDP)
              SUB   3
              OR    A
              JR    Z,TF_SIGN
              JP    P,TF_MUL

              NEG
              LD    B,A
TF_DIVLOOP:   PUSH  BC
              CALL  DIV10
              POP   BC
              DJNZ  TF_DIVLOOP
              JR    TF_SIGN

TF_MUL:       LD    B,A
TF_MULLOOP:   PUSH  BC
              LD    D,H
              LD    E,L
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,DE
              ADD   HL,HL
              POP   BC
              DJNZ  TF_MULLOOP

TF_SIGN:      LD    A,(FS)
              OR    A
              JR    Z,TF_DONE
              XOR   A
              SUB   L
              LD    L,A
              LD    A,0
              SBC   A,H
              LD    H,A
TF_DONE:      RET

;---------------------------

                                    ;************************************
                                    ;* PRNAME - prints NM$(A)           *
                                    ;************************************

PRNAME:       PUSH  HL
              PUSH  DE
              LD    H,0
              LD    L,A
              ADD   HL,HL
              LD    DE,NMTAB
              ADD   HL,DE
              LD    E,(HL)
              INC   HL
              LD    D,(HL)
              CALL  PRSTR
              POP   DE
              POP   HL
              RET

                                    ;************************************
                                    ;* PRDECB - byte (0..255) decimal,  *
                                    ;* no leading zeros                 *
                                    ;************************************

PRDECB:       PUSH  BC
              LD    C,0
              LD    B,100
              CALL  PD_DIGIT
              LD    B,10
              CALL  PD_DIGIT
              ADD   A,'0'
              CALL  PRCHR
              POP   BC
              RET

PD_DIGIT:     PUSH  DE
              LD    D,0
PD_DLOOP:     CP    B
              JR    C,PD_DDONE
              SUB   B
              INC   D
              JR    PD_DLOOP
PD_DDONE:     PUSH  AF
              LD    A,D
              OR    A
              JR    NZ,PD_SHOW
              LD    A,C
              OR    A
              JR    Z,PD_SKIP
PD_SHOW:      LD    A,D
              ADD   A,'0'
              CALL  PRCHR
              LD    A,1
              LD    C,A
PD_SKIP:      POP   AF
              POP   DE
              RET

                                    ;************************************
                                    ;* PRSDECB - signed byte decimal    *
                                    ;* (for DP)                         *
                                    ;************************************

PRSDECB:      BIT   7,A
              JR    Z,PRDECB
              PUSH  AF
              LD    A,'-'
              CALL  PRCHR
              POP   AF
              NEG
              JP    PRDECB

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
                                    ;* PRHEXBUF2 - buffer as 2-digit    *
                                    ;* hex bytes, space-separated       *
                                    ;************************************
                                    ;* INPUT: HL=ptr, B=count           *
                                    ;************************************

PRHEXBUF2:    PUSH  BC
              PUSH  HL
              LD    A,(HL)
              CALL  PRHEX2
              LD    A,' '
              CALL  PRCHR
              POP   HL
              POP   BC
              INC   HL
              DJNZ  PRHEXBUF2
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..252, stores into ADSADDR       *
                                    ;************************************
                                    ;* OUTPUT: (ADSADDR) = 0..252,       *
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

PARSEIO_OK:   LD    (ADSADDR),A
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
                                    ;* uses plain CALSLT as before;     *
                                    ;* SEG<>FFh (mapped RAM, e.g. a     *
                                    ;* RCXRAM.COM install) builds       *
                                    ;* IY=slot/segment and jumps through*
                                    ;* the RAM helper's register-based  *
                                    ;* call entry (HELPERADD+0) instead,*
                                    ;* since CALSLT alone would switch  *
                                    ;* only the slot and land on        *
                                    ;* whatever segment happens to      *
                                    ;* already be mapped there.         *
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

ADSCHANMAP:   DEFB  0,1,2,3,0,0,0,1,1,2,1,3

NMTAB:        DEFW  NM0,NM1,NM2,NM3,NM4,NM5,NM6,NM7,NM8,NM9,NM10,NM11
NM0:          DEFM  "SE AIN0-GND","$"
NM1:          DEFM  "SE AIN1-GND","$"
NM2:          DEFM  "SE AIN2-GND","$"
NM3:          DEFM  "SE AIN3-GND","$"
NM4:          DEFM  "DE AIN0-AIN1","$"
NM5:          DEFM  "DE AIN0-AIN2","$"
NM6:          DEFM  "DE AIN0-AIN3","$"
NM7:          DEFM  "DE AIN1-AIN2","$"
NM8:          DEFM  "DE AIN1-AIN3","$"
NM9:          DEFM  "DE AIN2-AIN3","$"
NM10:         DEFM  "DE AIN1-AIN0","$"
NM11:         DEFM  "DE AIN3-AIN2","$"

MSTART40:     DEFB  12
              DEFM  "ADS1220 - UNAPI/EXTBIOS driver",13,10
              DEFM  "for RCX card type G (ADS1220).",13,10
              DEFM  "Runs TEST A/B/C/D/F/G via",13,10
              DEFM  "FN_CORE57..62 directly.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "ADS1220 - UNAPI/EXTBIOS port of ADS1220.ASC",13,10
              DEFM  "(card type G, ADS1220). Runs the same TEST A/B/",13,10
              DEFM  "C/D/F/G, but via FN_CORE57..62 directly instead",13,10
              DEFM  "of BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
MUSAGE:       DEFM  "Usage: ADS1220 /IO:<0..252>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MCRLF:        DEFM  13,10,"$"

MCH:          DEFM  " ch=","$"
MBY:          DEFM  " by=","$"
MS:           DEFM  " S=","$"
MEX:          DEFM  " EX=","$"
MDP:          DEFM  " DP=","$"
MDG:          DEFM  " DG=","$"
MF:           DEFM  " F=","$"
MFAILA:       DEFM  "FAIL A cmd readback type ","$"

MCMD0:        DEFM  "cmd0 ch=","$"
MRCHAN:       DEFM  " -> rChannel=","$"
MMCNT:        DEFM  " mCount=","$"
MFAILB:       DEFM  "FAIL B ch ","$"

MCMD1:        DEFM  "cmd1 cnt=","$"
MFAILC:       DEFM  "FAIL C cnt ","$"

MCMD3:        DEFM  "cmd3 (reserved) -> rChannel=","$"
MEXPECT10:    DEFM  " (expect 1 0)","$"
MFAILD:       DEFM  "FAIL D state changed","$"

MREADK:       DEFM  "read ","$"
MBYTE:        DEFM  " byte=","$"
MMCNTAFTER:   DEFM  " mCount-after=","$"
MEXPECT:      DEFM  " (expect ","$"
MCLOSEPAR:    DEFM  ")","$"
MFAILF:       DEFM  "FAIL F wrap read ","$"

MGET:         DEFM  " GET=","$"
MTESTAEQ:     DEFM  " (TestA=","$"
MFAILG:       DEFM  "FAIL G tolerance type ","$"

MALLPASS:     DEFM  "ALL BIT-LEVEL CHECKS PASSED",13,10,"$"
MCHECKFAIL:   DEFM  " CHECK(S) FAILED",13,10,"$"

MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MSLOTERR:     DEFM  "ADS INIT failed - no card at ADSADDR.",13,10,"$"

ADSADDR:      DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0
ER:           DEFB  0

TVAR:         DEFB  0
KVAR:         DEFB  0
VVAR:         DEFB  0
XVAR:         DEFB  0
WVAR:         DEFB  0
PVAR:         DEFB  0
RVAR:         DEFB  0
CMDVAL:       DEFB  0
READBYTE:     DEFB  0

STATBUF:      DEFW  0
BYTEBUF:      DEFW  0
BYBUF:        DEFS  4
FABUF:        DEFS  48

FS:           DEFB  0
FDP:          DEFB  0
DGARR:        DEFS  6
GVAL10:       DEFW  0

WORKAREA:     DEFS  #0330

              END   START
