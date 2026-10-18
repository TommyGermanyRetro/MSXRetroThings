                                    ;************************************
                                    ;* DKCOPY.ASM                       *
                                    ;************************************
                                    ;* Copy code of the MSX cartridge   *
                                    ;* port of Donkey Kong (Buggy       *
                                    ;* Software, SVI-318/328). Placed   *
                                    ;* at #C000 by the ROM init code    *
                                    ;* (DKROM.ASM), which also fills in *
                                    ;* the slot settings below. Copies  *
                                    ;* ROM banks 1-3 (ASCII16) to the   *
                                    ;* RAM at #0000, #4000 and #8000,   *
                                    ;* the state after the three SVI    *
                                    ;* loaders, and starts the game at  *
                                    ;* #3745 with RAM in all pages.     *
                                    ;* Mapper registers are written     *
                                    ;* only while page 1 holds the ROM. *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 11/10/26                *
                                    ;************************************

              ORG   #0C000

;---------------------------

                                    ;************************************
                                    ;* Constants                        *
                                    ;************************************

BANK1:        EQU   #06000          ; ASCII16 bank register, #4000
BANK2:        EQU   #07000          ; ASCII16 bank register, #8000
PAGELEN:      EQU   #04000
GAMESTART:    EQU   #03745          ; game entry after the SVI loaders
GAMESTACK:    EQU   #0E000          ; stack set by the SVI loader

;---------------------------

                                    ;************************************
                                    ;* Entry point and parameters set   *
                                    ;* by the ROM init code             *
                                    ;************************************

COPY_E:       JP    COPY            ; #C000

SLOTX:        DEFB  0               ; page 0 RAM, pages 1-2 ROM
SUBX:         DEFB  0
SLOTY:        DEFB  0               ; pages 0-1 RAM, page 2 ROM
SUBY:         DEFB  0
SLOTZ:        DEFB  0               ; pages 0 and 2 RAM, page 1 ROM
SUBZ:         DEFB  0
SLOTF:        DEFB  0               ; all pages RAM
SUBF:         DEFB  0
EXPAND:       DEFB  0               ; bit 7: RAM slot is expanded

;---------------------------

                                    ;************************************
                                    ;* COPY                             *
                                    ;************************************
                                    ;* Runs from page 3 with interrupts *
                                    ;* disabled.                        *
                                    ;************************************

COPY:         LD    SP,GAMESTACK
              LD    HL,SLOTX
              CALL  SETSLOT
              LD    A,1
              LD    (BANK2),A
              LD    HL,#08000
              LD    DE,#00000
              CALL  COPYPAGE        ; bank 1 -> #0000
              LD    A,2
              LD    (BANK2),A
              LD    HL,SLOTY
              CALL  SETSLOT
              LD    HL,#08000
              LD    DE,#04000
              CALL  COPYPAGE        ; bank 2 -> #4000
              LD    HL,SLOTX
              CALL  SETSLOT
              LD    A,3
              LD    (BANK1),A
              LD    HL,SLOTZ
              CALL  SETSLOT
              LD    HL,#04000
              LD    DE,#08000
              CALL  COPYPAGE        ; bank 3 -> #8000
              LD    HL,SLOTF
              CALL  SETSLOT
              LD    A,#0D0          ; VDP R1 as set by the SVI loader
              OUT   (#099),A
              LD    A,#081
              OUT   (#099),A
              LD    A,15            ; PSG R15: joystick port 1
              OUT   (#0A0),A
              LD    A,#0F
              OUT   (#0A1),A
              JP    GAMESTART

COPYPAGE:     LD    BC,PAGELEN
              LDIR
              RET

                                    ;************************************
                                    ;* SETSLOT: switches to a slot      *
                                    ;* setting; the secondary register  *
                                    ;* first, page 3 stays in the RAM   *
                                    ;* slot                             *
                                    ;************************************
                                    ;* INPUT: HL = primary slot value,  *
                                    ;*        HL+1 = secondary value    *
                                    ;************************************

SETSLOT:      LD    A,(EXPAND)
              RLCA
              JR    NC,SETSLOT1
              INC   HL
              LD    A,(HL)
              DEC   HL
              LD    (#0FFFF),A
SETSLOT1:     LD    A,(HL)
              OUT   (#0A8),A
              RET
