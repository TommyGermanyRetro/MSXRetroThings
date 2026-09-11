                                    ;************************************
                                    ;* RTCLCD.COM                       *
                                    ;************************************
                                    ;* Reads/shows the PCF8583 RTC      *
                                    ;* date/time, optionally sets them, *
                                    ;* then writes both to the SEED I2C *
                                    ;* LCD, via FN_CORE2/4/6/8/12..15   *
                                    ;* directly through EXTBIO          *
                                    ;* discovery - no BASIC interpreter *
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

I2C_FN_INIT:    EQU  6             ; I2C INIT
I2C_FN_WRB:     EQU  8             ; I2C WRB

RTC_FN_SETTIME: EQU  12            ; SET TIME
RTC_FN_GETTIME: EQU  13            ; GET TIME
RTC_FN_SETDATE: EQU  14            ; SET DATE
RTC_FN_GETDATE: EQU  15            ; GET DATE

UNAPI_DISC_COUNT: EQU 0
UNAPI_DISC_ENTRY: EQU 1

LCDCHIP:      EQU   #07C           ; PCF8584 slave address of the LCD

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
                                    ;* I2C INIT                         *
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

              LD    A,(I2CADDR)
              LD    C,A
              LD    A,I2C_FN_INIT
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* Read + show current date/time    *
                                    ;************************************

                                    ; GET TIME/GET DATE write straight
                                    ; into an 8-byte buffer (ASCII
                                    ; digits + separators) - unlike the
                                    ; BASIC wrapper, the UNAPI routines
                                    ; don't go through EVALSTR, so there
                                    ; is no pre-existing-string-length
                                    ; requirement here, just a plain
                                    ; buffer

              LD    A,(I2CADDR)
              LD    C,A
              LD    DE,TIMEBUF
              LD    A,RTC_FN_GETTIME
              CALL  CALLFN

              LD    A,(I2CADDR)
              LD    C,A
              LD    DE,DATEBUF
              LD    A,RTC_FN_GETDATE
              CALL  CALLFN

              LD    DE,MDATE
              CALL  PRSTR
              LD    HL,DATEBUF
              LD    B,8
              CALL  PRBUF
              LD    DE,MCRLF
              CALL  PRSTR

              LD    DE,MTIME
              CALL  PRSTR
              LD    HL,TIMEBUF
              LD    B,8
              CALL  PRBUF
              LD    DE,MCRLF
              CALL  PRSTR

;---------------------------

                                    ;************************************
                                    ;* Optionally change date/time      *
                                    ;************************************

              LD    DE,MASKDATE
              CALL  PRSTR
              CALL  ASKYESNO
              JR    NZ,ASKTIME

              CALL  ASKNEWDATE
              LD    HL,LINEBUF
              LD    DE,DATEBUF
              LD    BC,8
              LDIR

              LD    A,(I2CADDR)
              LD    C,A
              LD    DE,DATEBUF
              LD    A,RTC_FN_SETDATE
              CALL  CALLFN

ASKTIME:      LD    DE,MASKTIME
              CALL  PRSTR
              CALL  ASKYESNO
              JR    NZ,DOLCD

              CALL  ASKNEWTIME
              LD    HL,LINEBUF
              LD    DE,TIMEBUF
              LD    BC,8
              LDIR

              LD    A,(I2CADDR)
              LD    C,A
              LD    DE,TIMEBUF
              LD    A,RTC_FN_SETTIME
              CALL  CALLFN

;---------------------------

                                    ;************************************
                                    ;* SEED I2C LCD init sequence,      *
                                    ;* then date on line 1 / time on    *
                                    ;* line 2                            *
                                    ;************************************

DOLCD:        LD    B,4
LCDINITLOOP:  PUSH  BC
              LD    HL,FUNCSET
              LD    B,2
              CALL  DOWRB
              POP   BC
              DJNZ  LCDINITLOOP

              LD    HL,CLRDISP
              LD    B,2
              CALL  DOWRB

              LD    HL,DISPON
              LD    B,2
              CALL  DOWRB

              LD    HL,LINE1ADDR
              LD    B,2
              CALL  DOWRB

              LD    HL,DATEBUF
              LD    DE,DATEOUT_DATA
              CALL  WIDEN8

              LD    HL,DATEOUT
              LD    B,9
              CALL  DOWRB

              LD    HL,LINE2ADDR
              LD    B,2
              CALL  DOWRB

              LD    HL,TIMEBUF
              LD    DE,TIMEOUT_DATA
              CALL  WIDEN8

              LD    HL,TIMEOUT
              LD    B,9
              CALL  DOWRB

              LD    DE,MDONE
              CALL  PRSTR

              LD    A,RCX_FN_DEINIT
              CALL  CALLFN

              LD    DE,MDEINIT
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
                                    ;* DOWRB - I2C WRB(LCDCHIP,B,HL) at *
                                    ;* the I2C card IO address asked    *
                                    ;* for at start - caller presets    *
                                    ;* B=length, HL=data pointer        *
                                    ;************************************

DOWRB:        EX    DE,HL           ; DE = data pointer (FN_CORE8's
                                    ; documented DE=varptr(I2C DATA))
              LD    A,(I2CADDR)
              LD    H,A
              LD    C,LCDCHIP
              LD    A,I2C_FN_WRB
              CALL  CALLFN
              RET

                                    ;************************************
                                    ;* WIDEN8 - copies 8 raw bytes from *
                                    ;* HL into 8 words at DE (low byte =*
                                    ;* the char, high byte = 0) - CORE8 *
                                    ;* (unchanged from RCXROM.ASM)      *
                                    ;* steps its data pointer by 2 per  *
                                    ;* element (only the low byte is    *
                                    ;* ever sent), matching how         *
                                    ;* RTCLCD.ASC's BASIC integer      *
                                    ;* array B() works - NOT a plain    *
                                    ;* byte-packed buffer               *
                                    ;************************************

WIDEN8:       LD    B,8
WIDEN8LP:     LD    A,(HL)
              LD    (DE),A
              INC   HL
              INC   DE
              XOR   A
              LD    (DE),A
              INC   DE
              DJNZ  WIDEN8LP
              RET

;---------------------------

                                    ;************************************
                                    ;* PARSEIO - finds "/IO:" in the     *
                                    ;* MSX-DOS command tail, parses the  *
                                    ;* decimal digits after it, checks   *
                                    ;* 0..254, stores into I2CADDR       *
                                    ;************************************
                                    ;* OUTPUT: (I2CADDR) = 0..254,       *
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

PARSEIO_OK:   LD    (I2CADDR),A
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

                                    ;************************************
                                    ;* ASKYESNO - reads exactly 1 char, *
                                    ;* sets Z for Y/y, NZ for N/n,      *
                                    ;* re-prompts on anything else      *
                                    ;************************************

ASKYESNO:     LD    A,1
              LD    (INBUF),A
              LD    DE,INBUF
              LD    C,#0A
              CALL  BDOS

              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(INBUF+1)
              CP    1
              JR    NZ,ASKYESNO_BAD

              LD    A,(INBUF+2)
              CP    'Y'
              JR    Z,ASKYESNO_YES
              CP    'y'
              JR    Z,ASKYESNO_YES
              CP    'N'
              JR    Z,ASKYESNO_NO
              CP    'n'
              JR    Z,ASKYESNO_NO

ASKYESNO_BAD: LD    DE,MBADYESNO
              CALL  PRSTR
              JR    ASKYESNO

ASKYESNO_NO:  OR    1               ; clear Z (guaranteed non-zero)
              RET

ASKYESNO_YES: XOR   A               ; set Z
              RET

                                    ;************************************
                                    ;* ASK8CHARS - reads a line into    *
                                    ;* LINEBUF, re-prompts until at     *
                                    ;* least 8 characters were typed    *
                                    ;************************************

ASK8CHARS:    LD    A,20
              LD    (INBUF),A
              LD    DE,INBUF
              LD    C,#0A
              CALL  BDOS

              LD    DE,MCRLF
              CALL  PRSTR

              LD    A,(INBUF+1)
              CP    8
              JR    C,ASK8CHARS_BAD

              LD    HL,INBUF+2
              LD    DE,LINEBUF
              LD    BC,8
              LDIR
              RET

ASK8CHARS_BAD: LD    DE,MBAD8
              CALL  PRSTR
              JR    ASK8CHARS

                                    ;************************************
                                    ;* ASKNEWDATE - reads into LINEBUF  *
                                    ;* via ASK8CHARS, then validates    *
                                    ;* format DD/MM/YY (DD=01..31,      *
                                    ;* MM=01..12, YY=any 2 digits) -    *
                                    ;* re-prompts on any violation      *
                                    ;************************************

ASKNEWDATE:   LD    DE,MNEWDATE
              CALL  PRSTR

ASKNEWDATE_RETRY:
              CALL  ASK8CHARS

              LD    A,(LINEBUF+2)
              CP    '/'
              JR    NZ,ASKNEWDATE_BAD
              LD    A,(LINEBUF+5)
              CP    '/'
              JR    NZ,ASKNEWDATE_BAD

              LD    HL,LINEBUF
              CALL  DIGIT2
              JR    C,ASKNEWDATE_BAD
              CP    1
              JR    C,ASKNEWDATE_BAD
              CP    32
              JR    NC,ASKNEWDATE_BAD

              LD    HL,LINEBUF+3
              CALL  DIGIT2
              JR    C,ASKNEWDATE_BAD
              CP    1
              JR    C,ASKNEWDATE_BAD
              CP    13
              JR    NC,ASKNEWDATE_BAD

              LD    HL,LINEBUF+6
              CALL  DIGIT2
              JR    C,ASKNEWDATE_BAD

              RET

ASKNEWDATE_BAD: LD    DE,MBADDATEFMT
              CALL  PRSTR
              JR    ASKNEWDATE_RETRY

                                    ;************************************
                                    ;* ASKNEWTIME - reads into LINEBUF  *
                                    ;* via ASK8CHARS, then validates    *
                                    ;* format HH:MM:SS (HH=00..23,      *
                                    ;* MM/SS=00..59) - re-prompts on    *
                                    ;* any violation                    *
                                    ;************************************

ASKNEWTIME:   LD    DE,MNEWTIME
              CALL  PRSTR

ASKNEWTIME_RETRY:
              CALL  ASK8CHARS

              LD    A,(LINEBUF+2)
              CP    ':'
              JR    NZ,ASKNEWTIME_BAD
              LD    A,(LINEBUF+5)
              CP    ':'
              JR    NZ,ASKNEWTIME_BAD

              LD    HL,LINEBUF
              CALL  DIGIT2
              JR    C,ASKNEWTIME_BAD
              CP    24
              JR    NC,ASKNEWTIME_BAD

              LD    HL,LINEBUF+3
              CALL  DIGIT2
              JR    C,ASKNEWTIME_BAD
              CP    60
              JR    NC,ASKNEWTIME_BAD

              LD    HL,LINEBUF+6
              CALL  DIGIT2
              JR    C,ASKNEWTIME_BAD
              CP    60
              JR    NC,ASKNEWTIME_BAD

              RET

ASKNEWTIME_BAD: LD    DE,MBADTIMEFMT
              CALL  PRSTR
              JR    ASKNEWTIME_RETRY

                                    ;************************************
                                    ;* DIGIT2 - HL points at 2 ASCII    *
                                    ;* digit chars, returns A=value     *
                                    ;* (0..99), Carry set if either     *
                                    ;* char is not '0'..'9'             *
                                    ;************************************

DIGIT2:       LD    A,(HL)
              CP    '0'
              JR    C,DIGIT2_BAD
              CP    '9'+1
              JR    NC,DIGIT2_BAD
              SUB   '0'
              LD    B,A
              INC   HL
              LD    A,(HL)
              CP    '0'
              JR    C,DIGIT2_BAD
              CP    '9'+1
              JR    NC,DIGIT2_BAD
              SUB   '0'
              LD    C,A
              LD    A,B
              ADD   A,A
              ADD   A,A
              ADD   A,B
              ADD   A,A
              ADD   A,C
              OR    A
              RET

DIGIT2_BAD:   SCF
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

                                    ;************************************
                                    ;* PRBUF - prints B raw bytes from  *
                                    ;* HL as characters (not $-         *
                                    ;* terminated - GET TIME/DATE fill  *
                                    ;* a fixed-length ASCII buffer, no  *
                                    ;* terminator)                      *
                                    ;************************************

PRBUF:        PUSH  BC
              PUSH  HL
              LD    A,(HL)
              CALL  PRCHR
              POP   HL
              POP   BC
              INC   HL
              DJNZ  PRBUF
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

IDSTR:        DEFM  "RCX_INTERFACE",0
IDSTRLEN:     EQU   $-IDSTR

                                    ; HD44780 command buffers - each
                                    ; element is 1 WORD (only the low
                                    ; byte is ever sent, the high byte
                                    ; is never read) - FN_CORE8/CORE8
                                    ; is unchanged from RCXROM.ASM and
                                    ; steps its data pointer by 2 per
                                    ; element, exactly matching how
                                    ; RTCLCD.ASC's BASIC integer array
                                    ; B() works. A byte-packed buffer
                                    ; here would make CORE8 read past
                                    ; the end of every buffer.

FUNCSET:      DEFW  #080,#028       ; command reg, function-set 8-bit
CLRDISP:      DEFW  #080,#001       ; command reg, clear display
DISPON:       DEFW  #080,#00F       ; command reg, display/cursor/blink on
LINE1ADDR:    DEFW  #080,#080       ; command reg, DDRAM address 0x00
LINE2ADDR:    DEFW  #080,#0C0       ; command reg, DDRAM address 0x40

DATEOUT:      DEFW  #040            ; data reg, followed by 8 widened
DATEOUT_DATA: DEFS  16              ; words filled from DATEBUF (see
                                    ; WIDEN8/DOLCD)

TIMEOUT:      DEFW  #040            ; data reg, followed by 8 widened
TIMEOUT_DATA: DEFS  16              ; words filled from TIMEBUF (see
                                    ; WIDEN8/DOLCD)

MSTART40:     DEFB  12
              DEFM  "RTCLCD - UNAPI/EXTBIOS driver",13,10
              DEFM  "for RCX card type A (I2C+RTC).",13,10
              DEFM  "Reads/sets RTC, writes to the",13,10
              DEFM  "SEED I2C LCD.",13,10
              DEFM  13,10,"$"

MSTART80:     DEFB  12
              DEFM  "RTCLCD - UNAPI/EXTBIOS port of RTCLCD.ASC",13,10
              DEFM  "(card type A, I2C+RTC). Reads/shows the RTC,",13,10
              DEFM  "optionally sets it, then writes to the SEED LCD -",13,10
              DEFM  "via FN_CORE2/4/6/8/12..15 directly, no BASIC CALL.",13,10
              DEFM  13,10,"$"

PIOTOK:       DEFM  "/IO:",0
MUSAGE:       DEFM  "Usage: RTCLCD /IO:<0..254>",13,10,"$"
MFOUND:       DEFM  "RCX_INTERFACE found, slot=","$"
MCRLF:        DEFM  13,10,"$"
MDATE:        DEFM  "Current date: ","$"
MTIME:        DEFM  "Current time: ","$"
MASKDATE:     DEFM  "Change date (Y/N)? ","$"
MASKTIME:     DEFM  "Change time (Y/N)? ","$"
MNEWDATE:     DEFM  "New date (DD/MM/YY, 8 characters): ","$"
MNEWTIME:     DEFM  "New time (HH:MM:SS, 8 characters): ","$"
MBAD8:        DEFM  "Please enter exactly 8 characters.",13,10,"$"
MBADYESNO:    DEFM  "Please enter Y or N.",13,10,"$"
MBADDATEFMT:  DEFM  "Invalid date - format DD/MM/YY,",13,10
              DEFM  "DD=01..31, MM=01..12.",13,10,"$"
MBADTIMEFMT:  DEFM  "Invalid time - format HH:MM:SS,",13,10
              DEFM  "HH=00..23, MM/SS=00..59.",13,10,"$"
MDONE:        DEFM  "LCD sequence sent.",13,10,"$"
MDEINIT:      DEFM  "Done.",13,10,"$"
MNOTFOUND:    DEFM  "RCX_INTERFACE not found.",13,10,"$"
MINITERR:     DEFM  "RCX INIT failed (Carry).",13,10,"$"

I2CADDR:      DEFB  0
SCRCOLS:      DEFB  0             ; 40 or 80
FINDTOK_NDL:  DEFW  0
INBUF:        DEFB  20,0
              DEFS  20
LINEBUF:      DEFS  8

SLOT:         DEFB  0
SEG:          DEFB  0
HELPERADD:    DEFW  0
ENTRY:        DEFW  0
ISIBUF:       DEFW  0

TIMEBUF:      DEFS  8
DATEBUF:      DEFS  8

WORKAREA:     DEFS  #0330

              END   START
