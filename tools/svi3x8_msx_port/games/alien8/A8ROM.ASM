                                    ;************************************
                                    ;* A8ROM.ASM                        *
                                    ;************************************
                                    ;* 32 KiB cartridge ROM (#4000-     *
                                    ;* #BFFF, no mapper) of the MSX     *
                                    ;* port of Alien 8 (SVI-318/328).   *
                                    ;* The init code enables the ROM in *
                                    ;* page 2, works out the slot       *
                                    ;* settings for the copy code       *
                                    ;* (A8COPY.ASM), places it at #E000 *
                                    ;* and runs it. Holds the copy code,*
                                    ;* the page-0 helper block          *
                                    ;* (A8HELP.ASM) and the patched     *
                                    ;* game image #3F00-#B349 written   *
                                    ;* by the build (A8GAME.BIN).       *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 11/10/26                *
                                    ;************************************

              ORG   #04000

;---------------------------

                                    ;************************************
                                    ;* Constants                        *
                                    ;************************************

ENASLT:       EQU   #00024          ; BIOS: enable slot H in page of HL
RSLREG:       EQU   #00138          ; BIOS: read primary slot register
EXPTBL:       EQU   #0FCC1          ; MSX slot expansion flags
SLTTBL:       EQU   #0FCC5          ; secondary slot registers
COPYADR:      EQU   #0E000          ; copy code address in page 3
COPYPAR:      EQU   COPYADR + 3     ; its parameter block

              INCLUDE "A8ROM.INC"   ; HELPLEN, GAMELEN, COPYLEN

;---------------------------

                                    ;************************************
                                    ;* Cartridge header                 *
                                    ;************************************

              DEFB  "AB"
              DEFW  INIT
              DEFW  0, 0, 0
              DEFS  6,0

;---------------------------

                                    ;************************************
                                    ;* INIT                             *
                                    ;************************************
                                    ;* Called by the BIOS during the    *
                                    ;* slot scan with the ROM in page   *
                                    ;* 1. Does not return.              *
                                    ;************************************

INIT:         DI
              CALL  RSLREG
              RRCA
              RRCA
              AND   #03
              LD    C,A             ; C = primary slot of page 1
              LD    B,0
              LD    HL,EXPTBL
              ADD   HL,BC
              LD    A,(HL)
              AND   #80
              OR    C
              LD    C,A
              INC   HL
              INC   HL
              INC   HL
              INC   HL              ; HL = SLTTBL entry
              LD    A,(HL)
              AND   #0C             ; secondary slot of page 1
              OR    C               ; A = slot ID of the ROM
              LD    H,#80
              CALL  ENASLT          ; page 2: ROM
              DI
              LD    HL,COPYBIN
              LD    DE,COPYADR
              LD    BC,COPYLEN
              LDIR
              LD    IX,COPYPAR
              IN    A,(#0A8)
              LD    B,A
              RLCA
              RLCA
              AND   #03
              LD    E,A             ; E = primary slot of page 3 (RAM)
              LD    A,B
              AND   #0FC
              OR    E
              LD    (IX+0),A        ; setting A: page 0 RAM, 1-2 ROM
              LD    A,E
              CALL  ALL4
              LD    (IX+2),A        ; setting B: all RAM
              LD    HL,EXPTBL
              LD    D,0
              ADD   HL,DE
              LD    A,(HL)
              LD    (IX+4),A        ; bit 7: RAM slot expanded
              LD    A,(#0FFFF)
              CPL
              LD    B,A             ; B = secondary slot register
              RLCA
              RLCA
              AND   #03
              LD    E,A             ; E = secondary slot of page 3
              LD    A,B
              AND   #0FC
              OR    E
              LD    (IX+1),A        ; setting A: page 0 RAM sub-slot
              LD    A,E
              CALL  ALL4
              LD    (IX+3),A        ; setting B: all RAM sub-slot
              LD    HL,HELPBIN
              LD    (IX+5),L
              LD    (IX+6),H
              LD    HL,GAMEBIN
              LD    (IX+7),L
              LD    (IX+8),H
              JP    COPYADR

                                    ;************************************
                                    ;* ALL4: copies a 2-bit slot number *
                                    ;* into all four page fields        *
                                    ;************************************
                                    ;* INPUT:  A = slot 0-3             *
                                    ;* OUTPUT: A = slot * #55           *
                                    ;************************************

ALL4:         LD    B,A
              RLCA
              RLCA
              OR    B
              LD    B,A
              RLCA
              RLCA
              RLCA
              RLCA
              OR    B
              RET

;---------------------------

COPYBIN:      INCBIN "A8COPY.BIN"
HELPBIN:      INCBIN "A8HELP.BIN"
GAMEBIN:      INCBIN "A8GAME.BIN"

ROMEND:
