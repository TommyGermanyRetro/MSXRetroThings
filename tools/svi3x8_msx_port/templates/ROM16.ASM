                                    ;************************************
                                    ;* ROM16.ASM                        *
                                    ;************************************
                                    ;* 16 KiB cartridge image of an MSX *
                                    ;* port of an SVI-318/328 game that *
                                    ;* runs at #C000. The ROM startup   *
                                    ;* copies the game image to RAM at  *
                                    ;* #C000 and runs the same page-0   *
                                    ;* helper as the CAS build.         *
                                    ;* Assembled with pasmo.            *
                                    ;************************************

              ORG   #04000
              DEFW  #04241          ; MSX cartridge signature "AB"
              DEFW  INIT            ; initialization entry
              DEFW  0,0,0,0,0,0
              DEFS  #04010-$,0

GAMEDST:      EQU   #0C000
GAMELEN:      EQU   #03800
FINADR:       EQU   #0FA80
HELPTMP:      EQU   #0FB00
HELPADR:      EQU   #00038
EXPTBL:       EQU   #0FCC1

              INCLUDE "HELP.INC"

INIT:         DI
              LD    SP,FINADR
              IN    A,(#0A8)
              RLCA
              RLCA
              AND   #03
              LD    E,A
              LD    HL,EXPTBL
              LD    D,0
              ADD   HL,DE
              LD    D,(HL)
              PUSH  DE
              LD    HL,GAMEIMAGE
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

FINISH:       LD    A,E
              CALL  FIN_ALL - FINISH + FINADR
              LD    C,A
              BIT   7,D
              JR    Z,FIN_PRI
              LD    A,(#0FFFF)
              CPL
              AND   #0C0
              RLCA
              RLCA
              CALL  FIN_ALL - FINISH + FINADR
              LD    (#0FFFF),A
FIN_PRI:      LD    A,C
              OUT   (#0A8),A
              LD    HL,HELPTMP
              LD    DE,HELPADR
              LD    BC,HELPLEN
              LDIR
              LD    A,15
              OUT   (#0A0),A
              LD    A,#0F
              OUT   (#0A1),A
              JP    GAMEDST

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

GAMEIMAGE:
              INCBIN "GAME.BIN"
HELPBIN:
              INCBIN "HELP.BIN"
HELPEND:
