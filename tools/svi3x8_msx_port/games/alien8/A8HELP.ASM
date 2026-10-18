                                    ;************************************
                                    ;* A8HELP.ASM                       *
                                    ;************************************
                                    ;* Page-0 helper block of the MSX   *
                                    ;* port of Alien 8 (SVI-318/328).   *
                                    ;* Runs from RAM at #0200 after the *
                                    ;* boot code has switched all four  *
                                    ;* pages to the RAM slot of page 3. *
                                    ;* Provides the SVI keyboard ports  *
                                    ;* translated to the MSX PPI. The   *
                                    ;* game reads PPI port C back, so   *
                                    ;* its value is kept in PORTC.      *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 11/10/26                *
                                    ;************************************

              ORG   #0200

;---------------------------

                                    ;************************************
                                    ;* Fixed entry points called by the *
                                    ;* patched game code                *
                                    ;************************************

KEYR_E:       JP    KEYR            ; #0200
KEYC_E:       JP    KEYC            ; #0203
KEYS_E:       JP    KEYS            ; #0206

PORTC:        DEFB  0               ; SVI PPI port C as last written

;---------------------------

                                    ;************************************
                                    ;* KEYR                             *
                                    ;************************************
                                    ;* Replaces the game routine #3F50: *
                                    ;* OUT (#96),A : IN A,(#99) : RET.  *
                                    ;************************************
                                    ;* INPUT:  A = SVI port C value,    *
                                    ;*         bits 0-3 = row           *
                                    ;* OUTPUT: A = SVI row data, active *
                                    ;*         low                      *
                                    ;************************************

KEYR:         LD    (PORTC),A
              AND   #0F
              JP    SVIROW

                                    ;************************************
                                    ;* KEYC                             *
                                    ;************************************
                                    ;* Replaces the game routine #3F80: *
                                    ;* OUT (#96),A, then IN A,(#99);    *
                                    ;* with port C bits 0-4 = #15 the   *
                                    ;* row data gets bit 7 set. Flags   *
                                    ;* as after CP #15 of the original. *
                                    ;************************************
                                    ;* INPUT:  A = SVI port C value     *
                                    ;* OUTPUT: A = SVI row data, active *
                                    ;*         low                      *
                                    ;************************************

KEYC:         LD    (PORTC),A
              PUSH  HL
              AND   #0F
              CALL  SVIROW
              LD    H,A
              LD    A,(PORTC)
              AND   #1F
              CP    #15
              LD    A,H
              JR    NZ,KEYC1
              SET   7,A
KEYC1:        POP   HL
              RET

                                    ;************************************
                                    ;* KEYS                             *
                                    ;************************************
                                    ;* Replaces LD C,A : IN A,(#96) :   *
                                    ;* AND #F0 : OR C : CALL #3F80 at   *
                                    ;* #5503; port C is read back from  *
                                    ;* PORTC.                           *
                                    ;************************************
                                    ;* INPUT:  A = SVI row (0-10)       *
                                    ;* OUTPUT: A = SVI row data, C =    *
                                    ;*         row                      *
                                    ;************************************

KEYS:         LD    C,A
              LD    A,(PORTC)
              AND   #F0
              OR    C
              JP    KEYC

;---------------------------

                                    ;************************************
                                    ;* SVIROW                           *
                                    ;************************************
                                    ;* Builds the SVI keyboard row from *
                                    ;* the MSX matrix via KEYMAP. Flags *
                                    ;* and all other registers are      *
                                    ;* preserved.                       *
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
