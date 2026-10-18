                                    ;************************************
                                    ;* BOOT.ASM                         *
                                    ;************************************
                                    ;* Boot code of an MSX port of an   *
                                    ;* SVI-318/328 game that runs at    *
                                    ;* #C000. Loaded by BLOAD"CAS:",R   *
                                    ;* together with the patched game   *
                                    ;* image (#8000-#B7FF) and the      *
                                    ;* page-0 helper block (HELP.BIN,   *
                                    ;* appended behind this code).      *
                                    ;* Copies the game to #C000,        *
                                    ;* switches all four pages to the   *
                                    ;* RAM slot of page 3, installs the *
                                    ;* helper block at #0038 and starts *
                                    ;* the game. The MSX BIOS is not    *
                                    ;* used afterwards. Assembled with  *
                                    ;* pasmo.                           *
                                    ;************************************

              ORG   #0B800

;---------------------------

                                    ;************************************
                                    ;* Constants                        *
                                    ;************************************

GAMESRC:      EQU   #08000          ; game image as loaded
GAMEDST:      EQU   #0C000          ; game address on the SVI
GAMELEN:      EQU   #03800          ; #C000-#F7FF
FINADR:       EQU   #0FA80          ; FINISH runs here (page 3)
HELPTMP:      EQU   #0FB00          ; helper block parked in page 3
HELPADR:      EQU   #00038          ; helper block final address
EXPTBL:       EQU   #0FCC1          ; MSX slot expansion flags

              INCLUDE "HELP.INC"    ; HELPLEN, written by the build

;---------------------------

                                    ;************************************
                                    ;* BOOT                             *
                                    ;************************************
                                    ;* Runs with the BIOS in page 0.    *
                                    ;* Reads the page-3 slot and its    *
                                    ;* expansion flag before the copy   *
                                    ;* overwrites the system area.      *
                                    ;************************************

BOOT:         DI
              LD    SP,FINADR
              IN    A,(#0A8)
              RLCA
              RLCA
              AND   #03
              LD    E,A             ; E = primary slot of page 3
              LD    HL,EXPTBL
              LD    D,0
              ADD   HL,DE
              LD    D,(HL)          ; D bit 7 = slot expanded
              PUSH  DE
              LD    HL,GAMESRC
              LD    DE,GAMEDST
              LD    BC,GAMELEN
              LDIR
              LD    HL,HELPBIN
              LD    DE,HELPTMP
              LD    BC,HELPLEN
              LDIR
              LD    HL,FINISH
              LD    DE,FINADR
              LD    BC,FINLEN
              LDIR
              POP   DE
              JP    FINADR

                                    ;************************************
                                    ;* FINISH                           *
                                    ;************************************
                                    ;* Position independent, executed   *
                                    ;* at FINADR in page 3 so that the  *
                                    ;* slot switch cannot remove the    *
                                    ;* running code.                    *
                                    ;************************************
                                    ;* INPUT: E = primary slot, D bit 7 *
                                    ;*        = slot expanded           *
                                    ;************************************

FINISH:       LD    A,E
              CALL  FIN_ALL - FINISH + FINADR
              LD    C,A             ; C = primary slot register value
              BIT   7,D
              JR    Z,FIN_PRI
              LD    A,(#0FFFF)
              CPL
              AND   #0C0
              RLCA
              RLCA                  ; A = secondary slot of page 3
              CALL  FIN_ALL - FINISH + FINADR
              LD    (#0FFFF),A
FIN_PRI:      LD    A,C
              OUT   (#0A8),A
              LD    HL,HELPTMP
              LD    DE,HELPADR
              LD    BC,HELPLEN
              LDIR
              LD    A,15            ; PSG R15: joystick port 1
              OUT   (#0A0),A
              LD    A,#0F
              OUT   (#0A1),A
              JP    GAMEDST

                                    ;************************************
                                    ;* FIN_ALL: copies a 2-bit slot     *
                                    ;* number into all four page fields *
                                    ;************************************
                                    ;* INPUT:  A = slot 0-3             *
                                    ;* OUTPUT: A = slot * #55           *
                                    ;************************************

FIN_ALL:      LD    B,A
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

FINEND:
FINLEN:       EQU   FINEND - FINISH

HELPBIN:                            ; HELP.BIN follows here
