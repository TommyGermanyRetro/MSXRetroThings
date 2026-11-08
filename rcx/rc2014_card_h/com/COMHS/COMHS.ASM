                                    ;************************************
                                    ;* COMHS.COM                        *
                                    ;************************************
                                    ;* SC725 (card H) COM test with     *
                                    ;* CTS-RTS handshake under MSX-DOS. *
                                    ;* Same test as BASIC COMHS.ASC:    *
                                    ;* for every channel in CHANS at    *
                                    ;* 9600 baud characters are sent    *
                                    ;* without reading until RTS stops  *
                                    ;* the sender (time out), the       *
                                    ;* buffer is read and checked (no   *
                                    ;* overflow), then the rest of 200  *
                                    ;* characters is sent and read back.*
                                    ;* Loopback TXD-RXD and RTS-CTS on  *
                                    ;* the tested port.                 *
                                    ;* Uses XIO_IO_EXPANDER and         *
                                    ;* RCX_INTERFACE via UNAPI (ROM or  *
                                    ;* RCXRAM) and the RS-232C Extended *
                                    ;* BIOS (EXTBIO device 8) of the    *
                                    ;* RCX COM driver.                  *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 07/11/26                *
                                    ;************************************

              ORG   #0100

;---------------------------

                                    ;************************************
                                    ;* MSX-DOS and BIOS constants       *
                                    ;************************************

BDOS:         EQU   #00005
EXTBIO:       EQU   #0FFCA
CALSLT:       EQU   #0001C
ARG:          EQU   #0F847
EXPTBL:       EQU   #0FCC1

;---------------------------

                                    ;************************************
                                    ;* UNAPI function numbers           *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT
RCX_FN_COMINIT: EQU  68            ; COM INIT

XIO_FN_INIT:    EQU  4             ; XIO INIT
XIO_FN_ISINIT:  EQU  5             ; XIO ISINIT
XIO_FN_DEINIT:  EQU  6             ; XIO DEINIT

                                    ;************************************
                                    ;* RS-232C jump table entries       *
                                    ;************************************

RS_INIT:      EQU   0
RS_OPEN:      EQU   1
RS_STAT:      EQU   2
RS_GETCHR:    EQU   3
RS_SNDCHR:    EQU   4
RS_CLOSE:     EQU   5
RS_LOC:       EQU   7
RS_SETCHN:    EQU   12

COMIOADDR:    EQU   #080           ; SC725 SIO base address
BUFLEN:       EQU   123            ; Receive buffer in characters

;---------------------------

START:        LD    DE,MSTART
              CALL  PRSTR

              CALL  SETUP           ; XIO, RCX, COM INIT, EXTBIO dev 8
              JP    C,EXIT

              LD    A,(CHANS)
              LD    (CHCNT),A
              LD    HL,CHANS+1
              LD    (CHPTR),HL

CHLOOP:       LD    HL,(CHPTR)
              LD    A,(HL)
              LD    (CHAN),A
              INC   HL
              LD    (CHPTR),HL
              LD    HL,9600
              LD    (BAUD),HL
              CALL  HSTEST

              LD    A,(CHCNT)
              DEC   A
              LD    (CHCNT),A
              JR    NZ,CHLOOP

              CALL  SUMMARY

EXIT:         CALL  TEARDOWN
              LD    C,0
              JP    BDOS

;---------------------------

                                    ;************************************
                                    ;* One channel with handshake       *
                                    ;************************************

HSTEST:       XOR   A
              LD    (ERRS),A
              LD    HL,PARHS        ; "8N1NHNNN", 9600, 9600, 1 s
              CALL  INITCH
              JP    C,HSFAIL

              LD    E,0             ; E = characters sent
HSSEND:       CALL  HSCHAR          ; A = character number E
              LD    (RSN),A
              PUSH  DE
              LD    A,RS_SNDCHR
              CALL  CALLRSA
              POP   DE
              JR    Z,HSSTOP        ; Time out: RTS stopped the sender
              CALL  C,ERRINC        ; CTRL-STOP
              INC   E
              LD    A,E
              CP    200
              JR    C,HSSEND
              CALL  ERRINC          ; Never stopped
HSSTOP:       LD    A,E
              LD    (SENT),A

              LD    A,RS_LOC
              CALL  CALLRS
              LD    A,H
              OR    A
              CALL  NZ,ERRINC
              LD    A,L
              LD    (RLOC),A
              LD    HL,SENT
              CP    (HL)            ; LOC = characters sent?
              CALL  NZ,ERRINC
              CP    BUFLEN          ; Stopped before the buffer end?
              CALL  NC,ERRINC

              LD    E,0             ; Read and compare
HSREAD:       LD    A,(RLOC)
              CP    E
              JR    Z,HSREAD1
              PUSH  DE
              LD    A,RS_GETCHR
              CALL  CALLRS
              POP   DE
              CALL  M,ERRINC
              LD    B,A
              CALL  HSCHAR
              CP    B
              CALL  NZ,ERRINC
              INC   E
              JR    HSREAD
HSREAD1:      LD    A,RS_STAT
              CALL  CALLRS
              BIT   7,H             ; Overflow?
              CALL  NZ,ERRINC

              LD    DE,MCOM
              CALL  PRSTR
              LD    A,(CHAN)
              ADD   A,'0'
              CALL  PRCHR
              LD    DE,MSTOP
              CALL  PRSTR
              LD    A,(SENT)
              LD    L,A
              LD    H,0
              CALL  PRDEC
              LD    DE,MLOC
              CALL  PRSTR
              LD    A,(RLOC)
              LD    L,A
              LD    H,0
              CALL  PRDEC
              CALL  RESULT

              XOR   A               ; Send and read the rest
              LD    (ERRS),A
              LD    A,(SENT)
              LD    E,A
HSREST:       LD    A,E
              CP    200
              JR    NC,HSREST1
              CALL  HSCHAR
              LD    (RSN),A
              PUSH  DE
              LD    A,RS_SNDCHR
              CALL  CALLRSA
              POP   DE
              CALL  C,ERRINC
              CALL  Z,ERRINC
              PUSH  DE
              LD    A,RS_GETCHR
              CALL  CALLRS
              POP   DE
              CALL  M,ERRINC
              LD    B,A
              CALL  HSCHAR
              CP    B
              CALL  NZ,ERRINC
              INC   E
              JR    HSREST
HSREST1:      LD    A,RS_CLOSE
              CALL  CALLRS

              LD    DE,MCOM
              CALL  PRSTR
              LD    A,(CHAN)
              ADD   A,'0'
              CALL  PRCHR
              LD    DE,MREST
              CALL  PRSTR
              LD    A,(SENT)
              LD    B,A
              LD    A,200
              SUB   B
              LD    L,A
              LD    H,0
              CALL  PRDEC
              LD    DE,MCHARS
              CALL  PRSTR
              JP    RESULT

HSFAIL:       CALL  ERRINC
              LD    DE,MCOM
              CALL  PRSTR
              LD    A,(CHAN)
              ADD   A,'0'
              CALL  PRCHR
              LD    DE,MINIT
              CALL  PRSTR
              JP    RESULT

                                    ;************************************
                                    ;* HSCHAR - character number E:     *
                                    ;* "A" + E MOD 26                   *
                                    ;************************************
                                    ;* OUTPUT: A = character            *
                                    ;************************************

HSCHAR:       LD    A,E
HSCHAR1:      CP    26
              JR    C,HSCHAR2
              SUB   26
              JR    HSCHAR1
HSCHAR2:      ADD   A,'A'
              RET

;---------------------------

                                    ;************************************
                                    ;* SETCHN, INIT and OPEN            *
                                    ;************************************
                                    ;* INPUT: HL = 8 parameter chars,   *
                                    ;* then time out byte; BAUD, CHAN   *
                                    ;************************************
                                    ;* OUTPUT: CARRY = 1 -> error       *
                                    ;************************************

INITCH:       LD    DE,PARTBL
              LD    BC,8
              LDIR
              LD    A,(HL)
              LD    (PARTBL+12),A   ; Time out
              LD    HL,(BAUD)
              LD    (PARTBL+8),HL   ; Receive baud
              LD    (PARTBL+10),HL  ; Send baud

              LD    A,(CHAN)
              LD    (RSN),A
              LD    A,RS_SETCHN
              CALL  CALLRSA
              RET   C

              CALL  P3SLOT
              LD    B,A             ; B = slot of PARTBL (page 3)
              LD    HL,PARTBL
              LD    A,RS_INIT
              CALL  CALLRS
              RET   C

              LD    HL,FCB
              LD    C,BUFLEN
              LD    E,4             ; Raw input and output
              LD    A,RS_OPEN
              JP    CALLRS

;---------------------------

                                    ;************************************
                                    ;* Setup: XIO, RCX, COM INIT and    *
                                    ;* the RS-232C jump table           *
                                    ;************************************
                                    ;* OUTPUT: CARRY = 1 -> error       *
                                    ;************************************

SETUP:        LD    DE,#02222
              LD    HL,0
              LD    A,#0FF
              CALL  EXTBIO
              LD    (HELPERADD),HL

              LD    HL,IDSTR
              LD    DE,ARG
              LD    BC,IDSTRLEN
              LDIR
              XOR   A
              LD    B,A
              LD    DE,#02222
              CALL  EXTBIO
              LD    A,B
              OR    A
              LD    DE,MNOTFOUND
              JP    Z,SETERR
              LD    A,1
              LD    DE,#02222
              CALL  EXTBIO
              LD    (SLOT),A
              LD    A,B
              LD    (SEG),A
              LD    (ENTRY),HL

              LD    HL,XIDSTR
              LD    DE,ARG
              LD    BC,XIDSTRLEN
              LDIR
              XOR   A
              LD    B,A
              LD    DE,#02222
              CALL  EXTBIO
              LD    A,B
              OR    A
              LD    DE,MXNOTFOUND
              JP    Z,SETERR
              LD    A,1
              LD    DE,#02222
              CALL  EXTBIO
              LD    (XSLOT),A
              LD    A,B
              LD    (XSEG),A
              LD    (XENTRY),HL

              LD    DE,XISIBUF      ; XIO ISINIT writes at (DE+1)
              LD    A,XIO_FN_ISINIT
              CALL  CALLFNXIO
              LD    A,(XISIBUF+1)
              OR    A
              JR    Z,SETXINIT
              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO
SETXINIT:     LD    HL,XWORKAREA
              LD    DE,INTFLAGS
              LD    A,XIO_FN_INIT
              CALL  CALLFNXIO
              LD    DE,MXINITERR
              JP    C,SETERR

              LD    DE,ISIBUF       ; RCX ISINIT writes at (DE)
              LD    A,RCX_FN_ISINIT
              CALL  CALLFN
              LD    A,(ISIBUF)
              CP    1
              JR    NZ,SETRINIT
              LD    A,RCX_FN_DEINIT
              CALL  CALLFN
SETRINIT:     LD    HL,RCXWORKAREA
              LD    A,RCX_FN_INIT
              CALL  CALLFN
              LD    DE,MINITERR
              JP    C,SETERR

              LD    C,COMIOADDR
              LD    A,RCX_FN_COMINIT
              CALL  CALLFN
              LD    DE,MCOMERR
              JP    C,SETERR

              CALL  P3SLOT          ; EXTBIO device 8 into EXTBUF
              LD    B,A
              LD    HL,EXTBUF
              LD    DE,#00800
              CALL  EXTBIO
              EI
              LD    DE,EXTBUF
              OR    A
              SBC   HL,DE           ; At least one entry?
              LD    DE,MNODEV8
              JR    Z,SETERR
              LD    A,(EXTBUF)
              LD    (TSLOT),A
              LD    HL,(EXTBUF+1)
              LD    (TADDR),HL

              LD    DE,MTABLE
              CALL  PRSTR
              LD    A,(TSLOT)
              CALL  PRHEX2
              LD    A,' '
              CALL  PRCHR
              LD    A,(TADDR+1)
              CALL  PRHEX2
              LD    A,(TADDR)
              CALL  PRHEX2
              LD    DE,MCRLF
              CALL  PRSTR
              OR    A
              RET

SETERR:       CALL  PRSTR
              SCF
              RET

                                    ;************************************
                                    ;* Teardown: RCX DEINIT (closes the *
                                    ;* COM channels) and XIO DEINIT     *
                                    ;************************************

TEARDOWN:     LD    HL,(ENTRY)
              LD    A,H
              OR    L
              JR    Z,TEARDN1       ; RCX not found
              LD    A,RCX_FN_DEINIT
              CALL  CALLFN
TEARDN1:      LD    HL,(XENTRY)
              LD    A,H
              OR    L
              RET   Z               ; XIO not found
              LD    A,XIO_FN_DEINIT
              JP    CALLFNXIO

;---------------------------

                                    ;************************************
                                    ;* Call an RS-232C table entry      *
                                    ;************************************
                                    ;* CALLRS:  A = entry, AF/BC/DE/HL  *
                                    ;*          parameters except A     *
                                    ;* CALLRSA: A = entry, (RSN) = A    *
                                    ;*          parameter               *
                                    ;************************************
                                    ;* OUTPUT: as the entry; interrupts *
                                    ;* enabled                          *
                                    ;************************************

CALLRSA:      PUSH  HL
              PUSH  DE
              CALL  RSADDR
              POP   DE
              POP   HL
              LD    A,(RSN)
              JR    CALLRS1

CALLRS:       PUSH  HL
              PUSH  DE
              CALL  RSADDR
              POP   DE
              POP   HL

CALLRS1:      PUSH  HL
              LD    HL,(RSIX)
              PUSH  HL
              POP   IX
              LD    HL,(TSLOT-1)    ; H = slot
              LD    L,0
              PUSH  HL
              POP   IY
              POP   HL
              CALL  CALSLT
              EI
              RET

RSADDR:       LD    E,A             ; IX = table + 3 + 3 * entry
              ADD   A,A
              ADD   A,E
              ADD   A,3
              LD    E,A
              LD    D,0
              LD    HL,(TADDR)
              ADD   HL,DE
              LD    (RSIX),HL
              RET

                                    ;************************************
                                    ;* CALLFN / CALLFNXIO - UNAPI call  *
                                    ;* of RCX_INTERFACE / XIO_IO_       *
                                    ;* EXPANDER (ROM via CALSLT, RAM    *
                                    ;* via the RAM helper)              *
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
              JP    CALSLT

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
              JP    CALSLT

                                    ;************************************
                                    ;* P3SLOT - slot of page 3          *
                                    ;************************************
                                    ;* OUTPUT: A = slot (bit 7 =        *
                                    ;* expanded)                        *
                                    ;************************************

P3SLOT:       PUSH  BC
              PUSH  HL
              IN    A,(#0A8)
              RLCA
              RLCA
              AND   #003
              LD    C,A
              LD    B,0
              LD    HL,EXPTBL
              ADD   HL,BC
              LD    A,(HL)
              AND   #080
              JR    Z,P3SLOT1
              LD    A,(#0FFFF)
              CPL
              RLCA
              RLCA
              AND   #003
              ADD   A,A
              ADD   A,A
              OR    C
              OR    #080
              LD    C,A
P3SLOT1:      LD    A,C
              POP   HL
              POP   BC
              RET

;---------------------------

                                    ;************************************
                                    ;* Result and summary               *
                                    ;************************************

ERRINC:       PUSH  AF
              LD    A,(ERRS)
              INC   A
              JR    Z,ERRINC1       ; Keep 255
              LD    (ERRS),A
ERRINC1:      POP   AF
              RET

RESULT:       LD    A,(TESTS)
              INC   A
              LD    (TESTS),A
              LD    A,(ERRS)
              OR    A
              JR    NZ,RESFAIL
              LD    A,(PASSES)
              INC   A
              LD    (PASSES),A
              LD    DE,MPASS
              JP    PRSTR
RESFAIL:      LD    DE,MFAIL
              CALL  PRSTR
              LD    A,(ERRS)
              LD    L,A
              LD    H,0
              CALL  PRDEC
              LD    DE,MCRLF
              JP    PRSTR

SUMMARY:      LD    DE,MCRLF
              CALL  PRSTR
              LD    A,(PASSES)
              LD    L,A
              LD    H,0
              CALL  PRDEC
              LD    DE,MOF
              CALL  PRSTR
              LD    A,(TESTS)
              LD    L,A
              LD    H,0
              CALL  PRDEC
              LD    DE,MPASSES
              JP    PRSTR

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
                                    ;* PRDEC - HL unsigned decimal,     *
                                    ;* no leading zeros                 *
                                    ;************************************

PRDEC:        LD    B,0             ; Digit printed
              LD    DE,10000
              CALL  PRDIG
              LD    DE,1000
              CALL  PRDIG
              LD    DE,100
              CALL  PRDIG
              LD    DE,10
              CALL  PRDIG
              LD    A,L
              ADD   A,'0'
              JP    PRCHR
PRDIG:        LD    A,'0'-1
PRDIG1:       INC   A
              OR    A
              SBC   HL,DE
              JR    NC,PRDIG1
              ADD   HL,DE
              CP    '0'
              JR    NZ,PRDIG2
              INC   B
              DEC   B
              RET   Z               ; Leading zero
PRDIG2:       LD    B,1
              JP    PRCHR

;---------------------------

                                    ;************************************
                                    ;* Data                             *
                                    ;************************************

                                    ; Channels to test: count, then
                                    ; 0 = port A, 1 = port B

CHANS:        DEFB  1,1

PARHS:        DEFM  "8N1NHNNN"      ; CTS-RTS handshake
              DEFB  1               ; Time out in seconds

IDSTR:        DEFM  "RCX_INTERFACE",0
IDSTRLEN:     EQU   $-IDSTR
XIDSTR:       DEFM  "XIO_IO_EXPANDER",0
XIDSTRLEN:    EQU   $-XIDSTR

MSTART:       DEFM  "SC725 COM test, CTS-RTS",13,10,"$"
MTABLE:       DEFM  "RS-232C table ","$"
MCOM:         DEFM  "COM","$"
MCOLON:       DEFM  ": ","$"
MSTOP:        DEFM  " stop after ","$"
MLOC:         DEFM  " LOC ","$"
MREST:        DEFM  " rest ","$"
MCHARS:       DEFM  " chars","$"
MINIT:        DEFM  " INIT/OPEN","$"
MPASS:        DEFM  " PASS",13,10,"$"
MFAIL:        DEFM  " FAIL ","$"
MOF:          DEFM  " of ","$"
MPASSES:      DEFM  " PASS",13,10,"$"
MCRLF:        DEFM  13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MCOMERR:      DEFM  "COM INIT failed (Carry).",13,10,"$"
MNODEV8:      DEFM  "No RS-232C (EXTBIO device 8).",13,10,"$"

SLOT:         DEFB  0
SEG:          DEFB  0
ENTRY:        DEFW  0
HELPERADD:    DEFW  0
ISIBUF:       DEFW  0
XSLOT:        DEFB  0
XSEG:         DEFB  0
XENTRY:       DEFW  0
XISIBUF:      DEFW  0

              DEFB  0               ; (TSLOT-1) for LD HL,(TSLOT-1)
TSLOT:        DEFB  0
TADDR:        DEFW  0
RSIX:         DEFW  0
RSN:          DEFB  0

CHAN:         DEFB  0
CHCNT:        DEFB  0
CHPTR:        DEFW  0
BAUD:         DEFW  0
ERRS:         DEFB  0
SENT:         DEFB  0
RLOC:         DEFB  0
TESTS:        DEFB  0
PASSES:       DEFB  0

                                    ; Work areas, parameter table and
                                    ; receive buffer must not be in the
                                    ; .COM image (page 0/1): page 0 is
                                    ; the BIOS ROM while the interrupt
                                    ; routine runs; the receive buffer
                                    ; must be at #8000 or higher (RS-
                                    ; 232C specification)

XWORKAREA:    EQU   #0C000          ; XIO work area
INTFLAGS:     EQU   #0C100          ; XIO interrupt variable (2 bytes)
RCXWORKAREA:  EQU   #0C200          ; RCX work area (ROM: #496 bytes)
PARTBL:       EQU   #0C6A0          ; RS-232C INIT parameters (13 bytes)
EXTBUF:       EQU   #0C6B0          ; EXTBIO device 8 entries
FCB:          EQU   #09000          ; Receive buffer (9 + 2 * 123)

              END   START
