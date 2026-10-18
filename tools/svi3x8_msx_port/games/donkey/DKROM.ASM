                                    ;************************************
                                    ;* DKROM.ASM                        *
                                    ;************************************
                                    ;* Bank 0 of the 64 KiB ASCII16     *
                                    ;* cartridge ROM of the MSX port of *
                                    ;* Donkey Kong (Buggy Software,     *
                                    ;* SVI-318/328). Banks 1-3 hold the *
                                    ;* patched game parts for #0000,    *
                                    ;* #4000 and #8000. The init code   *
                                    ;* enables the ROM in page 2, works *
                                    ;* out the slot settings for the    *
                                    ;* copy code (DKCOPY.ASM), places   *
                                    ;* it at #C000 and runs it.         *
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
COPYADR:      EQU   #0C000          ; copy code address in page 3
COPYPAR:      EQU   COPYADR + 3     ; its parameter block

              INCLUDE "DKROM.INC"   ; COPYLEN, written by the build

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
                                    ;* slot scan with bank 0 in page 1. *
                                    ;* Does not return.                 *
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
              LD    D,A             ; D = slots with the ROM in pages 1-2
              RLCA
              RLCA
              AND   #03
              LD    E,A             ; E = primary slot of page 3 (RAM)
              LD    C,#03
              CALL  MIXSLOT
              LD    (IX+0),A        ; X: page 0 RAM
              LD    C,#0F
              CALL  MIXSLOT
              LD    (IX+2),A        ; Y: pages 0-1 RAM
              LD    C,#33
              CALL  MIXSLOT
              LD    (IX+4),A        ; Z: pages 0 and 2 RAM
              LD    C,#0FF
              CALL  MIXSLOT
              LD    (IX+6),A        ; F: all RAM
              LD    HL,EXPTBL
              LD    B,0
              LD    C,E
              ADD   HL,BC
              LD    A,(HL)
              LD    (IX+8),A        ; bit 7: RAM slot expanded
              LD    A,(#0FFFF)
              CPL
              LD    D,A             ; D = secondary slot register
              RLCA
              RLCA
              AND   #03
              LD    E,A             ; E = secondary slot of page 3
              LD    C,#03
              CALL  MIXSLOT
              LD    (IX+1),A
              LD    C,#0F
              CALL  MIXSLOT
              LD    (IX+3),A
              LD    C,#33
              CALL  MIXSLOT
              LD    (IX+5),A
              LD    C,#0FF
              CALL  MIXSLOT
              LD    (IX+7),A
              JP    COPYADR

                                    ;************************************
                                    ;* MIXSLOT: puts a 2-bit slot       *
                                    ;* number into the page fields      *
                                    ;* selected by a mask               *
                                    ;************************************
                                    ;* INPUT:  D = slot register value, *
                                    ;*         E = slot 0-3, C = mask   *
                                    ;* OUTPUT: A = (D AND NOT C) OR     *
                                    ;*         (E * #55 AND C)          *
                                    ;************************************

MIXSLOT:      LD    A,E
              RLCA
              RLCA
              OR    E
              LD    B,A
              RLCA
              RLCA
              RLCA
              RLCA
              OR    B               ; A = E * #55
              AND   C
              LD    B,A
              LD    A,C
              CPL
              AND   D
              OR    B
              RET

;---------------------------

COPYBIN:      INCBIN "DKCOPY.BIN"

ROMEND:
