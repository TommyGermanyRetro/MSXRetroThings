                                    ;************************************
                                    ;* A8COPY.ASM                       *
                                    ;************************************
                                    ;* Copy code of the MSX cartridge   *
                                    ;* port of Alien 8 (SVI-318/328).   *
                                    ;* Placed at #E000 by the ROM init  *
                                    ;* code (A8ROM.ASM), which also     *
                                    ;* fills in the slot settings and   *
                                    ;* the ROM addresses below. The     *
                                    ;* game needs RAM at the addresses  *
                                    ;* of the ROM, so every part goes   *
                                    ;* through a buffer in the RAM of   *
                                    ;* page 0: slot setting A (pages 0  *
                                    ;* and 3 RAM, pages 1 and 2 ROM)    *
                                    ;* to read, setting B (all RAM) to  *
                                    ;* write. Starts the game with RAM  *
                                    ;* in all pages; the MSX BIOS is    *
                                    ;* not used afterwards.             *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 11/10/26                *
                                    ;************************************

              ORG   #0E000

;---------------------------

                                    ;************************************
                                    ;* Constants                        *
                                    ;************************************

HELPADR:      EQU   #00200          ; helper block final address
BUFFER:       EQU   #00400          ; transfer buffer in page 0
BUFLEN:       EQU   #03B00          ; #0400-#3EFF
GAMEDST:      EQU   #03F00          ; game start address on the SVI
GAMESTART:    EQU   #04000          ; game entry after both SVI loaders
GAMESTACK:    EQU   #0F200          ; game stack, set again by the game
ZEROEND:      EQU   #0C000          ; game image ends with zeros

              INCLUDE "A8ROM.INC"   ; HELPLEN, GAMELEN, written by the build

ZERODST:      EQU   GAMEDST + GAMELEN

;---------------------------

                                    ;************************************
                                    ;* Entry points and parameters set  *
                                    ;* by the ROM init code             *
                                    ;************************************

COPY_E:       JP    COPY            ; #E000

SLOTA:        DEFB  0               ; primary slot register, setting A
SUBA:         DEFB  0               ; secondary register, setting A
SLOTB:        DEFB  0               ; primary slot register, setting B
SUBB:         DEFB  0               ; secondary register, setting B
EXPAND:       DEFB  0               ; bit 7: RAM slot is expanded
HELPROM:      DEFW  0               ; helper block in the ROM
GAMEROM:      DEFW  0               ; game image in the ROM

;---------------------------

                                    ;************************************
                                    ;* COPY                             *
                                    ;************************************
                                    ;* Runs from page 3 with interrupts *
                                    ;* disabled.                        *
                                    ;************************************

COPY:         LD    SP,GAMESTACK
              CALL  SETA
              LD    HL,(HELPROM)
              LD    DE,HELPADR
              LD    BC,HELPLEN
              LDIR
              LD    HL,(GAMEROM)
              LD    DE,GAMEDST
              LD    BC,GAMELEN
COPYPART:     PUSH  BC              ; remaining length
              LD    A,B
              CP    HIGH BUFLEN
              JR    C,COPYLAST
              LD    BC,BUFLEN
COPYLAST:     PUSH  BC              ; part length
              CALL  SETA
              PUSH  DE
              LD    DE,BUFFER
              LDIR                  ; ROM -> buffer, HL advances
              POP   DE
              POP   BC
              PUSH  HL
              CALL  SETB
              LD    HL,BUFFER
              PUSH  BC
              LDIR                  ; buffer -> game, DE advances
              POP   BC
              POP   HL
              EX    (SP),HL         ; HL = remaining length
              AND   A
              SBC   HL,BC
              LD    B,H
              LD    C,L
              POP   HL
              LD    A,B
              OR    C
              JR    NZ,COPYPART
              LD    HL,ZERODST
              LD    DE,ZERODST + 1
              LD    BC,ZEROEND - ZERODST - 1
              LD    (HL),0
              LDIR
              LD    A,15            ; PSG R15: joystick port 1
              OUT   (#0A0),A
              LD    A,#0F
              OUT   (#0A1),A
              JP    GAMESTART

                                    ;************************************
                                    ;* SETA / SETB: switch to slot      *
                                    ;* setting A or B; the secondary    *
                                    ;* register first, page 3 stays in  *
                                    ;* the RAM slot                     *
                                    ;************************************

SETA:         LD    A,(EXPAND)
              RLCA
              JR    NC,SETA1
              LD    A,(SUBA)
              LD    (#0FFFF),A
SETA1:        LD    A,(SLOTA)
              OUT   (#0A8),A
              RET

SETB:         LD    A,(EXPAND)
              RLCA
              JR    NC,SETB1
              LD    A,(SUBB)
              LD    (#0FFFF),A
SETB1:        LD    A,(SLOTB)
              OUT   (#0A8),A
              RET
