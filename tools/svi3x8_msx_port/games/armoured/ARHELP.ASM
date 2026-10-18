                                    ;************************************
                                    ;* ARHELP.ASM                       *
                                    ;************************************
                                    ;* Page-0 helper block of the MSX   *
                                    ;* port of Armoured Assault         *
                                    ;* (SVI-318/328).                   *
                                    ;* Runs from RAM at #0038 after the *
                                    ;* boot code has switched all four  *
                                    ;* pages to the RAM slot of page 3. *
                                    ;* Provides the SVI ROM interrupt   *
                                    ;* entry the game expects, and the  *
                                    ;* SVI keyboard and both SVI        *
                                    ;* joysticks translated to the MSX  *
                                    ;* PPI and PSG.                     *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 09/10/26                *
                                    ;************************************

              ORG   #0038

;---------------------------

                                    ;************************************
                                    ;* Interrupt entry                  *
                                    ;************************************
                                    ;* Same frame as the SVI-328 ROM    *
                                    ;* (#0038 -> #3CC2): HL, DE, BC, AF *
                                    ;* pushed, then the game hook is    *
                                    ;* called. The game handler at      *
                                    ;* #C085 pops the return address    *
                                    ;* and the four registers itself    *
                                    ;* and ends with EI/RETI.           *
                                    ;************************************

GAMEISR:      EQU   #0C085
PASTORE:      EQU   #0F8C5          ; game variable for SVI PPI port A

ISR:          PUSH  HL
              PUSH  DE
              PUSH  BC
              PUSH  AF
              CALL  GAMEISR

              DEFS  #0040-$,0

                                    ;************************************
                                    ;* Fixed entry points called by the *
                                    ;* patched game code                *
                                    ;************************************

SVIROW_E:     JP    SVIROW          ; #0040
SVITRIG_E:    JP    SVITRIG         ; #0043
SVIR14_E:     JP    SVIR14          ; #0046
SVIPA_E:      JP    SVIPA           ; #0049

;---------------------------

                                    ;************************************
                                    ;* SVIROW                           *
                                    ;************************************
                                    ;* Replaces OUT (#96),A : IN A,     *
                                    ;* (#99) of the SVI PPI. Builds the *
                                    ;* SVI keyboard row from the MSX    *
                                    ;* matrix via KEYMAP. Flags and all *
                                    ;* other registers are preserved.   *
                                    ;************************************
                                    ;* INPUT:  A = SVI row (0-10)       *
                                    ;* OUTPUT: A = SVI row data, active *
                                    ;*         low; #FF for rows > 10   *
                                    ;************************************

SVIROW:       PUSH  BC
              PUSH  DE
              PUSH  HL
              PUSH  AF
              LD    C,#0FF
              CP    11
              JR    NC,ROWEND
              ADD   A,A
              ADD   A,A
              ADD   A,A
              ADD   A,A
              LD    E,A
              LD    D,0
              LD    HL,KEYMAP
              ADD   HL,DE
              LD    B,1
              LD    E,8
ROWBIT:       LD    A,(HL)
              INC   HL
              CP    #0FF
              JR    Z,ROWSKIP
              LD    D,A
              IN    A,(#0AA)
              AND   #0F0
              OR    D
              OUT   (#0AA),A
              IN    A,(#0A9)
              AND   (HL)
              JR    NZ,ROWSKIP
              LD    A,B
              CPL
              AND   C
              LD    C,A
ROWSKIP:      INC   HL
              RLC   B
              DEC   E
              JR    NZ,ROWBIT
ROWEND:       POP   AF
              LD    A,C
              POP   HL
              POP   DE
              POP   BC
              RET

                                    ;************************************
                                    ;* SVITRIG                          *
                                    ;************************************
                                    ;* Replaces IN A,(#98) : BIT 4,A.   *
                                    ;* SVI PPI port A bit 4 is the fire *
                                    ;* button of joystick 1; on MSX it  *
                                    ;* is PSG register 14 bit 4 with    *
                                    ;* port 1 selected (R15 bit 6 = 0,  *
                                    ;* set by the boot code).           *
                                    ;************************************
                                    ;* OUTPUT: A bit 4 = trigger,       *
                                    ;*         0 = pressed; Z flag as   *
                                    ;*         after BIT 4,A            *
                                    ;************************************

SVITRIG:      LD    A,14
              OUT   (#0A0),A
              IN    A,(#0A2)
              BIT   4,A
              RET

                                    ;************************************
                                    ;* SVIR14                           *
                                    ;************************************
                                    ;* Replaces LD A,14 : OUT (#88),A : *
                                    ;* IN A,(#90). The SVI reads both   *
                                    ;* joysticks at once from PSG       *
                                    ;* register 14 (joystick 1 in bits  *
                                    ;* 0-3, joystick 2 in bits 4-7); on *
                                    ;* MSX each port is selected with   *
                                    ;* R15 bit 6. Ends with port 1      *
                                    ;* selected and R14 latched, as on  *
                                    ;* the SVI.                         *
                                    ;************************************
                                    ;* OUTPUT: A = SVI R14 value,       *
                                    ;*         active low               *
                                    ;************************************

SVIR14:       PUSH  BC
              CALL  JOYPORT2
              AND   #0F
              RLCA
              RLCA
              RLCA
              RLCA
              LD    B,A
              CALL  JOYPORT1
              AND   #0F
              OR    B
              POP   BC
              RET

                                    ;************************************
                                    ;* SVIPA                            *
                                    ;************************************
                                    ;* Replaces IN A,(#98) : LD (#F8C5) *
                                    ;* ,A. SVI PPI port A holds the     *
                                    ;* fire buttons, bit 4 joystick 1,  *
                                    ;* bit 5 joystick 2; on MSX both    *
                                    ;* are R14 bit 4 of the selected    *
                                    ;* port. The patch has no room for  *
                                    ;* the store, so it is done here.   *
                                    ;************************************
                                    ;* OUTPUT: A = (PASTORE) = SVI port *
                                    ;*         A, buttons active low    *
                                    ;************************************

SVIPA:        PUSH  BC
              CALL  JOYPORT2
              AND   #10
              RLCA
              LD    B,A
              CALL  JOYPORT1
              AND   #10
              OR    B
              LD    (PASTORE),A
              POP   BC
              RET

                                    ;************************************
                                    ;* JOYPORT1/JOYPORT2: select an MSX *
                                    ;* joystick port and read PSG R14   *
                                    ;************************************
                                    ;* OUTPUT: A = R14 of the port      *
                                    ;************************************

JOYPORT1:     LD    A,#0F
              JR    JOYREAD
JOYPORT2:     LD    A,#4F
JOYREAD:      PUSH  AF
              LD    A,15
              OUT   (#0A0),A
              POP   AF
              OUT   (#0A1),A
              LD    A,14
              OUT   (#0A0),A
              IN    A,(#0A2)
              RET

;---------------------------

                                    ;************************************
                                    ;* KEYMAP                           *
                                    ;************************************
                                    ;* One pair per SVI key, rows 0-10, *
                                    ;* bits 0-7: MSX row, MSX bit mask. *
                                    ;* #FF = no MSX equivalent. SVI     *
                                    ;* matrix as in openMSX Keyboard.cc *
                                    ;* (getSVIMapping), MSX matrix      *
                                    ;* international layout.            *
                                    ;************************************

KEYMAP:
                                    ; SVI row 0: 0 1 2 3 4 5 6 7
              DEFB  0,#01, 0,#02, 0,#04, 0,#08, 0,#10, 0,#20, 0,#40, 0,#80
                                    ; SVI row 1: 8 9 : ' , = . /
              DEFB  1,#01, 1,#02, 1,#80, 2,#01, 2,#04, 1,#08, 2,#08, 2,#10
                                    ; SVI row 2: - A B C D E F G
              DEFB  1,#04, 2,#40, 2,#80, 3,#01, 3,#02, 3,#04, 3,#08, 3,#10
                                    ; SVI row 3: H I J K L M N O
              DEFB  3,#20, 3,#40, 3,#80, 4,#01, 4,#02, 4,#04, 4,#08, 4,#10
                                    ; SVI row 4: P Q R S T U V W
              DEFB  4,#20, 4,#40, 4,#80, 5,#01, 5,#02, 5,#04, 5,#08, 5,#10
                                    ; SVI row 5: X Y Z [ \ ] BS UP
              DEFB  5,#20, 5,#40, 5,#80, 1,#20, 1,#10, 1,#40, 7,#20, 8,#20
                                    ; SVI row 6: SHIFT CTRL LGRAPH RGRAPH
                                    ;            ESC STOP ENTER LEFT
              DEFB  6,#01, 6,#02, 6,#04, 6,#10, 7,#04, 7,#10, 7,#80, 8,#10
                                    ; SVI row 7: F1 F2 F3 F4 F5 CLS INS DOWN
              DEFB  6,#20, 6,#40, 6,#80, 7,#01, 7,#02, 8,#02, 8,#04, 8,#40
                                    ; SVI row 8: SPACE TAB DEL CAPS SELECT
                                    ;            PRINT - RIGHT
              DEFB  8,#01, 7,#08, 8,#08, 6,#08, 7,#40, #FF,0, #FF,0, 8,#80
                                    ; SVI row 9: keypad 0-7
              DEFB  9,#08, 9,#10, 9,#20, 9,#40, 9,#80, 10,#01, 10,#02, 10,#04
                                    ; SVI row 10: keypad 8 9 + - * / . ,
              DEFB  10,#08, 10,#10, 9,#02, 10,#20, 9,#01, 9,#04, 10,#80, 10,#40

HELPEND:
