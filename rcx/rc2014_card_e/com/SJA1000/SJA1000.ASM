                                    ;************************************
                                    ;* SJA1000.COM                      *
                                    ;************************************
                                    ;* UNAPI/EXTBIOS driver for the     *
                                    ;* SJA1000 CAN card. Uses two       *
                                    ;* separate UNAPI drivers           *
                                    ;* (RCX_INTERFACE for the CAN       *
                                    ;* functions, XIO_IO_EXPANDER for   *
                                    ;* the interrupt), each with its    *
                                    ;* own discovery/dispatch pair      *
                                    ;* (CALLFN/CALLFNXIO). Registers    *
                                    ;* RCXROM's own CAN interrupt       *
                                    ;* service routine (returned by     *
                                    ;* CAN GET ADDR) via XIO SET ADDR - *
                                    ;* see the note at RCXWORKAREA      *
                                    ;* below. CAN is not implemented in *
                                    ;* RCXRAM.COM, so this program only *
                                    ;* works against a real RCX ROM     *
                                    ;* card.                            *
                                    ;* Usage: SJA1000 /IO:x /INT:y      *
                                    ;* (x=IO address 0..254, y=PIC INT  *
                                    ;* channel 0..7), read from the     *
                                    ;* MSX-DOS command tail and parsed  *
                                    ;* by PARSEIO/PARSEINT/FINDTOK/     *
                                    ;* PARSEDEC.                        *
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
                                    ;* (position in FN_TABLE)           *
                                    ;************************************

RCX_FN_INIT:    EQU  2             ; RCX INIT
RCX_FN_ISINIT:  EQU  3             ; RCX ISINIT
RCX_FN_DEINIT:  EQU  4             ; RCX DEINIT

CAN_FN_INIT:    EQU  43            ; CAN INIT
CAN_FN_CHECK:   EQU  46            ; CAN CHECK
CAN_FN_RX:      EQU  47            ; CAN RX
CAN_FN_GETADDR: EQU  48            ; CAN GET ADDR
CAN_FN_TX:      EQU  49            ; CAN TX

XIO_FN_INIT:    EQU  4             ; XIO INIT
XIO_FN_ISINIT:  EQU  5             ; XIO ISINIT
XIO_FN_DEINIT:  EQU  6             ; XIO DEINIT
XIO_FN_SETADDR: EQU  7             ; XIO SET ADDR
XIO_FN_SETMASK: EQU  8             ; XIO SET MASK

UNAPI_DISC_COUNT: EQU 0
UNAPI_DISC_ENTRY: EQU 1

CMDTAIL:      EQU   #0080         ; MSX-DOS command tail: length byte,
                                    ; followed by the raw text at
                                    ; CMDTAIL+1

ROWRX:        EQU   8             ; screen row of the CAN RX display

;---------------------------

                                    ;************************************
                                    ;* Start                            *
                                    ;************************************

                                    ;************************************
                                    ;* Screen width: reads the currently *
                                    ;* active column count (LINLEN),     *
                                    ;* never changes it - 40-column      *
                                    ;* layout below the 41-column        *
                                    ;* threshold, 80-column layout at    *
                                    ;* or above it                       *
                                    ;************************************

START:        LD    A,(LINLEN)
              CP    41
              LD    A,40
              JR    C,SETWIDTH
              LD    A,80

SETWIDTH:     LD    (SCRCOLS),A

                                    ; Row layout below ROWRX depends on
                                    ; how many rows the CAN RX hex dump
                                    ; needs: 1 row at 80 columns, 2 rows
                                    ; at 40 columns (13 bytes plus the
                                    ; "RX: " prefix do not fit on a
                                    ; single 40-column line)

              LD    A,(SCRCOLS)
              CP    80
              LD    A,1
              JR    Z,SETROWS
              LD    A,2

SETROWS:      LD    (RXSPAN),A
              ADD   A,ROWRX+2
              LD    (ROWADDR),A
              INC   A
              LD    (ROWDIR),A
              INC   A
              LD    (ROWCLR),A
              ADD   A,4
              LD    (ROWH1),A
              INC   A
              LD    (ROWH2),A

              LD    A,(SCRCOLS)
              CP    80
              LD    DE,MSTART80
              JR    Z,PRINTSTART
              LD    DE,MSTART40
PRINTSTART:   CALL  PRSTR

              CALL  PARSEIO
              JP    C,PARAMERR
              CALL  PARSEINT
              JP    C,PARAMERR

                                    ; Locate the RAM helper, if any -
                                    ; shared by both UNAPI dispatch
                                    ; paths below

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
                                    ;* (before RCX)                     *
                                    ;************************************
                                    ; ISINIT writes its flag at (DE+1),
                                    ; not (DE)

              LD    DE,XISIBUF
              LD    A,XIO_FN_ISINIT
              CALL  CALLFNXIO
              LD    A,(XISIBUF+1)
              OR    A
              JR    Z,XSETUP_INIT

              LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

XSETUP_INIT:  LD    HL,XWORKAREA
              LD    DE,XBOUNDFLAG
              LD    A,XIO_FN_INIT
              CALL  CALLFNXIO
              JP    C,XINITERR

                                    ;************************************
                                    ;* Setup: RCX ISINIT/DEINIT/INIT     *
                                    ;************************************
                                    ; RCXWORKAREA lives at a fixed page-3
                                    ; address (see the comment there) -
                                    ; RCX INIT stores the passed-in HL
                                    ; pointer verbatim into SLTWRK
                                    ; (SETRAM); every later RCX routine
                                    ; (including the CAN ISR itself,
                                    ; running inside a real hardware
                                    ; interrupt!) retrieves that same
                                    ; address again via GETRAM.

              LD    DE,ISIBUF
              LD    A,RCX_FN_ISINIT
              CALL  CALLFN
              LD    A,(ISIBUF)
              CP    1
              JR    NZ,SETUP_INIT

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

SETUP_INIT:   LD    HL,RCXWORKAREA
              LD    A,RCX_FN_INIT
              CALL  CALLFN
              JP    C,INITERR

;---------------------------

                                    ;************************************
                                    ;* CAN INIT(CANADDR,ACC)             *
                                    ;************************************

              LD    A,(CANADDR)
              LD    C,A
              LD    DE,ACCBUF
              LD    A,CAN_FN_INIT
              CALL  CALLFN
              JP    C,CANINITERR

                                    ;************************************
                                    ;* CAN GET ADDR + XIO SET ADDR/MASK  *
                                    ;************************************
                                    ; DE=INTADDR (a real address, not a
                                    ; varptr!) points into RCXWORKAREA,
                                    ; i.e. page 3 - hence SLOT=#0FF
                                    ; (page-3 call, no slot switch
                                    ; needed).

              LD    A,(CANADDR)
              LD    C,A
              LD    A,CAN_FN_GETADDR
              CALL  CALLFN
              JP    C,CANINITERR

              LD    A,(CANCHAN)
              LD    B,A
              LD    C,#0FF
              LD    A,XIO_FN_SETADDR
              CALL  CALLFNXIO
              JR    C,SETADDRERR

              LD    A,(MASKVAL)
              LD    E,A
              LD    D,0
              LD    A,XIO_FN_SETMASK
              CALL  CALLFNXIO
              JR    C,SETMASKERR

                                    ; The loop-control hints below, and
                                    ; the RX/TX screens in DORX/
                                    ; DOTX_ADDR/DOTX_DIR, position the
                                    ; cursor via PRLOC and then print
                                    ; their message text with a separate
                                    ; plain PRSTR call. Embedding the
                                    ; positioning escape bytes inside
                                    ; the same PRSTR string does not
                                    ; position the cursor correctly.

              LD    A,(ROWH1)
              LD    B,A
              LD    C,0
              CALL  PRLOC
              LD    A,(SCRCOLS)
              CP    80
              LD    DE,MLOC180
              JR    Z,PRINTHINT1
              LD    DE,MLOC140
PRINTHINT1:   CALL  PRSTR
              LD    A,(ROWH2)
              LD    B,A
              LD    C,0
              CALL  PRLOC
              LD    DE,MLOC2
              CALL  PRSTR
              JR    MAINLOOP

SETADDRERR:   LD    DE,MSETADDRERR
              CALL  PRSTR
              JR    CLEANUP

SETMASKERR:   LD    DE,MSETMASKERR
              CALL  PRSTR
              JR    CLEANUP

;---------------------------

                                    ;************************************
                                    ;* Main loop - CAN CHECK, on a new   *
                                    ;* message CAN RX + print, key 1 =   *
                                    ;* send, ESC = exit                 *
                                    ;************************************
                                    ; Key poll uses BDOS function 6
                                    ; (Direct Console I/O) with E=FFh -
                                    ; the non-blocking, non-echoing
                                    ; "return immediately" form (Z set
                                    ; if no key waiting, else A=key
                                    ; code).

MAINLOOP:     LD    A,(CANADDR)
              LD    C,A
              LD    A,CAN_FN_CHECK
              CALL  CALLFN
              OR    A
              JR    Z,MAINLOOP_KEY

              CALL  DORX

MAINLOOP_KEY: LD    E,#0FF
              LD    C,6
              CALL  BDOS
              JR    Z,MAINLOOP

              CP    27             ; ESC
              JR    Z,CLEANUP
              CP    '1'
              JR    NZ,MAINLOOP

              CALL  DOTX_ADDR
              JR    MAINLOOP

;---------------------------

                                    ;************************************
                                    ;* Cleanup                          *
                                    ;************************************

CLEANUP:      LD    A,XIO_FN_DEINIT
              CALL  CALLFNXIO

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

              LD    A,12             ; CLS (Ctrl-L)
              CALL  PRCHR

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

CANINITERR:   LD    DE,MCANINITERR
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
                                    ;* DORX - CAN RX(CANADDR,RXBUF),     *
                                    ;* prints "RX: " + 13 hex bytes, on  *
                                    ;* one line at 80 columns or split   *
                                    ;* across two lines at 40 columns    *
                                    ;* (RXSPAN rows, set at startup)     *
                                    ;************************************

DORX:         LD    A,(CANADDR)
              LD    C,A
              LD    DE,RXBUF
              LD    A,CAN_FN_RX
              CALL  CALLFN

              LD    B,ROWRX
              LD    C,0
              CALL  PRLOC
              LD    DE,MRX
              CALL  PRSTR

              LD    HL,RXBUF
              LD    A,(RXSPAN)
              CP    1
              LD    B,13
              JR    Z,DORX_PRINT

              LD    B,7             ; 40 columns: 7 bytes fit next to
                                    ; "RX: ", the rest go on the next
                                    ; row
DORX_PRINT:   CALL  DORX_BYTES
              LD    A,(RXSPAN)
              CP    1
              JR    Z,DORX_DONE

              LD    B,ROWRX+1
              LD    C,0
              CALL  PRLOC
              LD    B,6
              CALL  DORX_BYTES

DORX_DONE:    RET

                                    ;************************************
                                    ;* DORX_BYTES - prints B hex bytes   *
                                    ;* (low byte of each word at HL,     *
                                    ;* HL+=2 per byte) separated by      *
                                    ;* spaces                            *
                                    ;************************************

DORX_BYTES:   LD    A,(HL)
              CALL  PRHEX2
              LD    A,' '
              CALL  PRCHR
              INC   HL
              INC   HL              ; RXBUF elements are words, only
                                    ; the low byte holds data
              DJNZ  DORX_BYTES
              RET

                                    ;************************************
                                    ;* DOTX - asks for a CAN target      *
                                    ;* address (0x3000..0x33FF, 4 hex    *
                                    ;* digits) and a direction           *
                                    ;* (0=Red/1=Green), each with its    *
                                    ;* own bounds check and retry, fills *
                                    ;* TXBUF, sends twice (G10=1, then   *
                                    ;* G10=0). The blank/clear PRLOC     *
                                    ;* calls use column 0 (where BDOS's  *
                                    ;* buffered-line INPUT always leaves *
                                    ;* the cursor after Enter, i.e.      *
                                    ;* where a bare error PRSTR lands),  *
                                    ;* the prompt PRLOC calls use        *
                                    ;* column 1 (the indent the prompts  *
                                    ;* are actually shown at) - clearing *
                                    ;* at column 1 would leave the error *
                                    ;* text's first character behind.   *
                                    ;************************************

DOTX_ADDR:    LD    A,(ROWADDR)
              LD    B,A
              LD    C,0
              CALL  PRLOC
              LD    DE,MBLANK
              CALL  PRSTR
              LD    A,(ROWADDR)
              LD    B,A
              LD    C,1
              CALL  PRLOC

              LD    DE,MADDRPROMPT
              CALL  PRSTR

              LD    A,4
              LD    (INBUF),A
              LD    DE,INBUF
              LD    C,#0A
              CALL  BDOS

              LD    A,(INBUF+1)
              CP    4
              JR    NZ,DOTX_ADDR_BAD

              LD    HL,0
              LD    IX,INBUF+2
              LD    B,4

DOTX_ADDR_DIGLOOP: LD A,(IX+0)
              CALL  HEXVAL
              JR    C,DOTX_ADDR_BAD

              LD    D,0
              LD    E,A
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,HL
              ADD   HL,HL           ; HL = HL*16
              ADD   HL,DE           ; HL = HL*16 + digit

              INC   IX
              DJNZ  DOTX_ADDR_DIGLOOP

                                    ; bounds check: 0x3000 <= HL <= 0x33FF
                                    ; (i.e. HL >= 0x3000 AND HL < 0x3400) -
                                    ; each SBC/ADD pair leaves HL restored
                                    ; to its original value afterwards

              LD    DE,#03000
              OR    A
              SBC   HL,DE
              JR    C,DOTX_ADDR_BAD
              ADD   HL,DE

              LD    DE,#03400
              OR    A
              SBC   HL,DE
              JR    NC,DOTX_ADDR_BAD
              ADD   HL,DE

              LD    A,H             ; G(7) = high byte
              LD    (TXBUF+14),A
              LD    A,L
              AND   #0F             ; G(8) = low nibble
              LD    (TXBUF+16),A

              JR    DOTX_DIR

DOTX_ADDR_BAD: LD    DE,MBADTXADDR
              CALL  PRSTR
              JR    DOTX_ADDR

DOTX_DIR:     LD    A,(ROWDIR)
              LD    B,A
              LD    C,0
              CALL  PRLOC
              LD    DE,MBLANK
              CALL  PRSTR
              LD    A,(ROWDIR)
              LD    B,A
              LD    C,1
              CALL  PRLOC

              LD    DE,MDIRPROMPT
              CALL  PRSTR

              LD    A,1
              LD    (INBUF),A
              LD    DE,INBUF
              LD    C,#0A
              CALL  BDOS

              LD    A,(INBUF+1)
              OR    A
              JR    Z,DOTX_DIR_BAD

              LD    A,(INBUF+2)
              CALL  HEXVAL
              JR    C,DOTX_DIR_BAD
              CP    2
              JR    NC,DOTX_DIR_BAD  ; only 0 or 1 allowed

              LD    (TXBUF+18),A    ; G(9)

              LD    A,(ROWCLR)
              LD    B,A
              LD    C,0
              CALL  PRLOC
              LD    DE,MBLANK
              CALL  PRSTR

              LD    A,1
              LD    (TXBUF+20),A    ; G(10) = 1

              LD    A,(CANADDR)
              LD    C,A
              LD    DE,TXBUF
              LD    A,CAN_FN_TX
              CALL  CALLFN

              XOR   A
              LD    (TXBUF+20),A    ; G(10) = 0

              LD    A,(CANADDR)
              LD    C,A
              LD    DE,TXBUF
              LD    A,CAN_FN_TX
              CALL  CALLFN

              RET

DOTX_DIR_BAD: LD    DE,MBADTXDIR
              CALL  PRSTR
              JR    DOTX_DIR

                                    ;************************************
                                    ;* HEXVAL - A=ASCII character ->     *
                                    ;* A=value (0..15), Carry=1 if not   *
                                    ;* a valid hex digit                 *
                                    ;************************************

HEXVAL:       CP    '0'
              JR    C,HEXVAL_BAD
              CP    '9'+1
              JR    NC,HEXVAL_UC
              SUB   '0'
              OR    A
              RET

HEXVAL_UC:    CP    'A'
              JR    C,HEXVAL_BAD
              CP    'F'+1
              JR    NC,HEXVAL_LC
              SUB   'A'-10
              OR    A
              RET

HEXVAL_LC:    CP    'a'
              JR    C,HEXVAL_BAD
              CP    'f'+1
              JR    NC,HEXVAL_BAD
              SUB   'a'-10
              OR    A
              RET

HEXVAL_BAD:   SCF
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..254, stores into CANADDR       *
                                    ;************************************
                                    ;* OUTPUT: (CANADDR) = 0..254,       *
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
              CP    255             ; 0..254 valid
              JR    C,PARSEIO_OK

              SCF
              RET

PARSEIO_OK:   LD    (CANADDR),A
              OR    A
              RET

                                    ;************************************
                                    ;* PARSEINT - finds "/INT:" in the   *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..7, stores CANCHAN and          *
                                    ;* precomputes MASKVAL = 2^CANCHAN   *
                                    ;************************************
                                    ;* OUTPUT: (CANCHAN) = 0..7,         *
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

PARSEINT_OK:  LD    (CANCHAN),A

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
                                    ;* CALLFN - calls RCX's FN_CORExx.   *
                                    ;* SEG=FFh (ROM) uses CALSLT         *
                                    ;* directly, SEG<>FFh (mapped RAM)   *
                                    ;* jumps through the RAM helper      *
                                    ;* instead.                          *
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
                                    ;* CALLFNXIO - calls XIO's           *
                                    ;* FN_CORExx. Identical dispatch to  *
                                    ;* CALLFN, but against XSLOT/XSEG/   *
                                    ;* XENTRY (XIO_IO_EXPANDER's OWN     *
                                    ;* discovery result) instead of      *
                                    ;* SLOT/SEG/ENTRY (RCX_INTERFACE) -  *
                                    ;* two independent UNAPI drivers,    *
                                    ;* each with its own dispatch        *
                                    ;* target.                           *
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

IDSTR:        DEFM  "RCX_INTERFACE",0
IDSTRLEN:     EQU   $-IDSTR

XIDSTR:       DEFM  "XIO_IO_EXPANDER",0
XIDSTRLEN:    EQU   $-XIDSTR

                                    ; Acceptance code & mask. CAN INIT
                                    ; only reads the even offsets
                                    ; (0,2,4,...14) - the odd offsets
                                    ; (high bytes) are never read.

ACCBUF:       DEFB  #0F0,0
              DEFB  #0B7,0
              DEFB  #0FF,0
              DEFB  #0FF,0
              DEFB  #0F0,0
              DEFB  #007,0
              DEFB  #0FF,0
              DEFB  #0FF,0

                                    ; TX buffer - 13 words. G(0..6) are
                                    ; fixed (Maerklin CS3 message
                                    ; header), G(7..9) are computed from
                                    ; the input in DOTX, G(10) is
                                    ; toggled there between 1 and 0,
                                    ; G(11)/G(12) stay 0.

TXBUF:        DEFB  #086,0          ; G(0)
              DEFB  0,0             ; G(1)
              DEFB  #0B1,0          ; G(2)
              DEFB  #0DA,0          ; G(3)
              DEFB  #0E8,0          ; G(4)
              DEFB  0,0             ; G(5)
              DEFB  0,0             ; G(6)
              DEFB  0,0             ; G(7)
              DEFB  0,0             ; G(8)
              DEFB  0,0             ; G(9)
              DEFB  0,0             ; G(10)
              DEFB  0,0             ; G(11)
              DEFB  0,0             ; G(12)

                                    ; RX buffer - 13 words. CAN RX
                                    ; fills both bytes of every element
                                    ; itself (low byte = data, high
                                    ; byte = 0), no pre-fill needed.

RXBUF:        DEFS  26

MSTART40:     DEFB  12
              DEFM  "SJA1000 - UNAPI/EXTBIOS driver",13,10
              DEFM  "for RCX card type E (CAN).",13,10
              DEFM  "CAN messages arrive via a",13,10
              DEFM  "hardware interrupt (RCX ISR).",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "SJA1000 - UNAPI/EXTBIOS port of SJA1000.ASC",13,10
              DEFM  "(card type E, CAN). CAN messages are received",13,10
              DEFM  "via interrupt (RCX's own ISR registered through",13,10
              DEFM  "XIO SET ADDR).",13,10
              DEFM  13,10,"$"
PIOTOK:       DEFM  "/IO:",0
PINTTOK:      DEFM  "/INT:",0
MUSAGE:       DEFM  "Usage: SJA1000 /IO:<0..254> /INT:<0..7>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MXFOUND:      DEFM  "XIO_IO_EXPANDER found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MLOC140:      DEFM  "Key '1' sends a command",13,10,"$"
MLOC180:      DEFM  "Key '1' starts the switching command sequence",13,10,"$"
MLOC2:        DEFM  "ESC exits the programme",13,10,"$"
MRX:          DEFM  "RX: ","$"
MBLANK:       DEFM  "                             ","$"
MADDRPROMPT:  DEFM  "ADDR (3000...33ff): ","$"
MBADTXADDR:   DEFM  "Please enter 3000..33FF.",13,10,"$"
MDIRPROMPT:   DEFM  "DIR (0=Red,1=Green): ","$"
MBADTXDIR:    DEFM  "Please enter 0 or 1.",13,10,"$"
MSETADDRERR:  DEFM  "XIO SET ADDR failed (Carry).",13,10,"$"
MSETMASKERR:  DEFM  "XIO SET MASK failed (Carry).",13,10,"$"
MDONE:        DEFM  "Done.",13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"
MCANINITERR:  DEFM  "CAN INIT failed - no SJA1000 found at",13,10
              DEFM  "CANADDR, or already installed.",13,10,"$"
MXNOTFOUND:   DEFM  "XIO_IO_EXPANDER not found.",13,10,"$"
MXINITERR:    DEFM  "XIO INIT failed (Carry).",13,10,"$"

INBUF:        DEFB  6,0
              DEFS  6

CANADDR:      DEFB  0
CANCHAN:      DEFB  0
MASKVAL:      DEFB  0
MASKTAB:      DEFB  1,2,4,8,16,32,64,128
FINDTOK_NDL:  DEFW  0

SCRCOLS:      DEFB  0             ; 40 or 80
RXSPAN:       DEFB  0             ; rows used by the CAN RX hex dump
ROWADDR:      DEFB  0             ; screen row of the ADDR prompt
ROWDIR:       DEFB  0             ; screen row of the DIR prompt
ROWCLR:       DEFB  0             ; screen row cleared before CAN TX
ROWH1:        DEFB  0             ; screen row of the first hint line
ROWH2:        DEFB  0             ; screen row of the second hint line

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

XSLOT:        DEFB  0
XSEG:         DEFB  0
XENTRY:       DEFW  0
XISIBUF:      DEFW  0

                                    ; XWORKAREA/XBOUNDFLAG/RCXWORKAREA
                                    ; must live at a fixed page-3
                                    ; address, not in this .COM's own
                                    ; image (page 0/1): during a real
                                    ; hardware interrupt the MSX BIOS
                                    ; forces page 0 to system ROM, and
                                    ; only page 2 gets slot-switched to
                                    ; reach mapped RAM. CAN INIT writes
                                    ; a ready-made interrupt routine
                                    ; directly into RCXWORKAREA, which
                                    ; then gets registered as the
                                    ; interrupt handler via XIO SET
                                    ; ADDR and reads/writes that same
                                    ; buffer again on every interrupt -
                                    ; if it lived in page 0, those
                                    ; accesses would silently target
                                    ; system ROM instead.

XWORKAREA:    EQU   #0C000         ; 97 bytes, #0C000-#0C060
XBOUNDFLAG:   EQU   #0C100         ; 2 bytes (mandatory binding for XIO
                                    ; INIT, written on every PIC
                                    ; interrupt even with a registered
                                    ; custom handler)
RCXWORKAREA:  EQU   #0C200         ; 816 bytes (#0330), #0C200-#0C52F

              END   START
