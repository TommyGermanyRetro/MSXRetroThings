                                    ;************************************
                                    ;* FORMAT.COM                       *
                                    ;************************************
                                    ;* Formats a single-sided disk in   *
                                    ;* drive A: or B: of the SVI-605B   *
                                    ;* for Spectravideo CP/M 2.24:      *
                                    ;* track 0 with 18 FM sectors of    *
                                    ;* 128 bytes, tracks 1-39 with 17   *
                                    ;* MFM sectors of 256 bytes, every  *
                                    ;* sector filled with E5h (empty    *
                                    ;* directory). Track layout and     *
                                    ;* sector interleave as in MFORMAT  *
                                    ;* of Spectravideo CP/M 2.27.       *
                                    ;* Ends with a warm boot from the   *
                                    ;* system disk in drive A:.         *
                                    ;************************************

        ORG     0100H

;---------------------------
                                    ;************************************
                                    ;* CONSTANTS                        *
                                    ;************************************
BDOS:   EQU     0005H
CONIN:  EQU     1                   ; BDOS: console input with echo
CONOUT: EQU     2                   ; BDOS: console output
PRTSTR: EQU     9                   ; BDOS: print string up to '$'

FDCCMD: EQU     30H                 ; FD1793 command / status
FDCDAT: EQU     33H                 ; FD1793 data
FDCCTL: EQU     34H                 ; W: drive select, motors; R: INTRQ bit 7, DRQ bit 6
FDCDEN: EQU     38H                 ; bit 0: 1 = FM, 0 = MFM

SELA:   EQU     0DH                 ; select drive A, both motors on
SELB:   EQU     0EH                 ; select drive B, both motors on
RESTOR: EQU     02H                 ; RESTORE, step rate 20 ms
STEPIN: EQU     52H                 ; STEP IN with track update, 20 ms
WRTRK:  EQU     0F0H                ; WRITE TRACK
TRACKS: EQU     40

BUFFER: EQU     2000H               ; one track as written to the FDC

;---------------------------
                                    ;************************************
                                    ;* MAIN                             *
                                    ;************************************
START:  LD      SP,STACK
        LD      DE,MTITLE
        CALL    PRINT
ASK:    LD      DE,MASK
        CALL    PRINT
        CALL    GETKEY
        CP      0DH
        JP      Z,QUIT
        AND     0DFH                ; upper case
        LD      B,SELA
        CP      'A'
        JR      Z,ASK1
        LD      B,SELB
        CP      'B'
        JR      NZ,ASK
ASK1:   LD      (DRVLET),A
        LD      A,B
        LD      (DRVSEL),A
        LD      DE,MINS1
        CALL    PRINT
        LD      A,(DRVLET)
        CALL    PUTCH
        LD      DE,MINS2
        CALL    PRINT
        CALL    GETKEY
        CP      03H                 ; CTRL-C
        JP      Z,QUIT
        CALL    FORMAT
        CALL    PRINT
        LD      DE,MAGAIN
        CALL    PRINT
        CALL    GETKEY
        AND     0DFH
        CP      'Y'
        JR      Z,ASK
QUIT:   LD      DE,MSYS
        CALL    PRINT
        CALL    GETKEY
        JP      0                   ; warm boot

;---------------------------
                                    ;************************************
                                    ;* FORMAT                           *
                                    ;************************************
                                    ;* Homes the head, then writes the  *
                                    ;* 40 tracks one after the other.   *
                                    ;************************************
                                    ;* OUTPUT: DE = result message      *
                                    ;************************************
FORMAT: LD      A,(DRVSEL)
        OUT     (FDCCTL),A
        LD      A,RESTOR
        OUT     (FDCCMD),A
        CALL    WAITIRQ
        XOR     A
        LD      (TRACK),A
FMTTRK: CALL    SHOWTRK
        LD      A,(TRACK)
        OR      A
        JR      NZ,FMTMFM
        LD      A,1
        OUT     (FDCDEN),A
        CALL    BLDFM
        JR      FMTWR
FMTMFM: XOR     A
        OUT     (FDCDEN),A
        CALL    BLDMFM
FMTWR:  CALL    WRITE
        LD      DE,MNODSK
        BIT     7,A                 ; not ready
        RET     NZ
        LD      DE,MPROT
        BIT     6,A                 ; write protected
        RET     NZ
        LD      DE,MFAIL
        BIT     2,A                 ; lost data
        RET     NZ
        LD      A,(TRACK)
        INC     A
        CP      TRACKS
        JR      NC,FMTEND
        LD      (TRACK),A
        LD      A,STEPIN
        OUT     (FDCCMD),A
        CALL    WAITIRQ
        JR      FMTTRK
FMTEND: LD      A,RESTOR
        OUT     (FDCCMD),A
        CALL    WAITIRQ
        LD      DE,MDONE
        RET

                                    ;************************************
                                    ;* WRITE                            *
                                    ;************************************
                                    ;* WRITE TRACK from BUFFER: a byte  *
                                    ;* on every DRQ until INTRQ.        *
                                    ;************************************
                                    ;* OUTPUT: A = FDC status           *
                                    ;************************************
WRITE:  DI
        LD      HL,BUFFER
        LD      A,WRTRK
        OUT     (FDCCMD),A
WRLOOP: IN      A,(FDCCTL)
        ADD     A,A
        JR      C,WRDONE            ; INTRQ: track written
        JP      P,WRLOOP            ; no DRQ yet
        LD      A,(HL)
        OUT     (FDCDAT),A
        INC     HL
        JR      WRLOOP
WRDONE: EI
        IN      A,(FDCCMD)
        RET

                                    ;************************************
                                    ;* WAITIRQ                          *
                                    ;************************************
                                    ;* Waits for INTRQ of a type I      *
                                    ;* command and clears it.           *
                                    ;************************************
WAITIRQ:
        IN      A,(FDCCTL)
        RLCA
        JR      NC,WAITIRQ
        IN      A,(FDCCMD)
        RET

;---------------------------
                                    ;************************************
                                    ;* TRACK IMAGES                     *
                                    ;************************************
                                    ;* BLDFM: track 0, FM, 18 x 128.    *
                                    ;* BLDMFM: tracks 1-39, MFM,        *
                                    ;* 17 x 256. F5h writes the A1h     *
                                    ;* sync mark, F6h C2h, F7h the CRC. *
                                    ;************************************
BLDFM:  LD      HL,BUFFER
        LD      BC,16*256+0FFH
        CALL    FILL
        LD      BC,6*256+00H
        CALL    FILL
        LD      (HL),0FCH           ; index mark
        INC     HL
        LD      BC,11*256+0FFH
        CALL    FILL
        LD      IX,SKEW
        LD      D,18
BFMSEC: LD      BC,6*256+00H
        CALL    FILL
        LD      E,00H               ; size code 0: 128 bytes
        CALL    IDFLD
        LD      BC,11*256+0FFH
        CALL    FILL
        LD      BC,6*256+00H
        CALL    FILL
        LD      (HL),0FBH           ; data mark
        INC     HL
        LD      BC,128*256+0E5H
        CALL    FILL
        LD      (HL),0F7H
        INC     HL
        LD      BC,10*256+0FFH
        CALL    FILL
        DEC     D
        JR      NZ,BFMSEC
        LD      BC,0*256+0FFH       ; 256 bytes gap to the end
        JP      FILL

BLDMFM: LD      HL,BUFFER
        LD      BC,80*256+4EH
        CALL    FILL
        LD      BC,12*256+00H
        CALL    FILL
        LD      BC,3*256+0F6H
        CALL    FILL
        LD      (HL),0FCH           ; index mark
        INC     HL
        LD      BC,50*256+4EH
        CALL    FILL
        LD      IX,SKEW
        LD      D,17
BMFSEC: LD      BC,12*256+00H
        CALL    FILL
        LD      BC,3*256+0F5H
        CALL    FILL
        LD      E,01H               ; size code 1: 256 bytes
        CALL    IDFLD
        LD      BC,22*256+4EH
        CALL    FILL
        LD      BC,12*256+00H
        CALL    FILL
        LD      BC,3*256+0F5H
        CALL    FILL
        LD      (HL),0FBH           ; data mark
        INC     HL
        LD      BC,0*256+0E5H       ; 256 bytes
        CALL    FILL
        LD      (HL),0F7H
        INC     HL
        LD      BC,34*256+4EH
        CALL    FILL
        DEC     D
        JR      NZ,BMFSEC
        LD      BC,0*256+4EH        ; 2 x 256 bytes gap to the end
        CALL    FILL
        LD      BC,0*256+4EH
        JP      FILL

                                    ;************************************
                                    ;* IDFLD                            *
                                    ;************************************
                                    ;* ID field: FEh, track, side 0,    *
                                    ;* sector from the skew table, size *
                                    ;* code, F7h.                       *
                                    ;************************************
                                    ;* INPUT: HL = buffer, IX = skew    *
                                    ;* table entry, E = size code       *
                                    ;* OUTPUT: HL, IX advanced          *
                                    ;************************************
IDFLD:  LD      (HL),0FEH
        INC     HL
        LD      A,(TRACK)
        LD      (HL),A
        INC     HL
        LD      (HL),00H
        INC     HL
        LD      A,(IX+0)
        LD      (HL),A
        INC     HL
        INC     IX
        LD      (HL),E
        INC     HL
        LD      (HL),0F7H
        INC     HL
        RET

                                    ;************************************
                                    ;* FILL                             *
                                    ;************************************
                                    ;* INPUT: HL = buffer, B = count    *
                                    ;* (0 = 256), C = byte              *
                                    ;* OUTPUT: HL advanced              *
                                    ;************************************
FILL:   LD      (HL),C
        INC     HL
        DJNZ    FILL
        RET

;---------------------------
                                    ;************************************
                                    ;* CONSOLE                          *
                                    ;************************************
SHOWTRK:
        LD      DE,MTRACK
        CALL    PRINT
        LD      A,(TRACK)
        LD      B,'0'-1
SHOWT1: INC     B
        SUB     10
        JR      NC,SHOWT1
        ADD     A,'0'+10
        PUSH    AF
        LD      A,B
        CALL    PUTCH
        POP     AF
        JP      PUTCH

PRINT:  LD      C,PRTSTR
        JP      BDOS

PUTCH:  LD      E,A
        LD      C,CONOUT
        JP      BDOS

GETKEY: LD      C,CONIN
        JP      BDOS

;---------------------------
                                    ;************************************
                                    ;* DATA                             *
                                    ;************************************
SKEW:   DB      1,10,2,11,3,12,4,13,5,14,6,15,7,16,8,17,9,18

MTITLE: DB      'Spectravideo CP/M 2.24 disk formatter',0DH,0AH
        DB      'Single sided: FM track 0, 39 MFM tracks',0DH,0AH,'$'
MASK:   DB      0DH,0AH,'Format disk in drive A or B (RETURN = quit)? $'
MINS1:  DB      0DH,0AH,'Insert the disk in drive $'
MINS2:  DB      ': and press RETURN $'
MTRACK: DB      0DH,'Formatting track $'
MDONE:  DB      0DH,0AH,'Disk formatted.',0DH,0AH,'$'
MNODSK: DB      0DH,0AH,'Error: there is no disk in that drive!',0DH,0AH,'$'
MPROT:  DB      0DH,0AH,'Error: the disk is write protected!',0DH,0AH,'$'
MFAIL:  DB      0DH,0AH,'Error: cannot format this disk!',0DH,0AH,'$'
MAGAIN: DB      'Format another disk (Y/N)? $'
MSYS:   DB      0DH,0AH,'Put the CP/M system disk in drive A: and press RETURN $'

DRVSEL: DB      SELA
DRVLET: DB      'A'
TRACK:  DB      0

        DS      64
STACK:

        END     START
