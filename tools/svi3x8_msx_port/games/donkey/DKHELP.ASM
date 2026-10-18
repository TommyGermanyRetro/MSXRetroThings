                                    ;************************************
                                    ;* DKHELP.ASM                       *
                                    ;************************************
                                    ;* Helper block of the MSX port of  *
                                    ;* Donkey Kong (Buggy Software,     *
                                    ;* SVI-318/328). Part of the game   *
                                    ;* image at #F000, a range the game *
                                    ;* never uses. Provides the SVI     *
                                    ;* keyboard and joystick ports      *
                                    ;* translated to the MSX PPI and    *
                                    ;* PSG. The SVI reads both          *
                                    ;* joysticks from PSG register 14   *
                                    ;* and both triggers from PPI port  *
                                    ;* A; the MSX selects the joystick  *
                                    ;* port with PSG register 15 bit 6. *
                                    ;* Assembled with pasmo.            *
                                    ;************************************
                                    ;* VERSION: 11/10/26                *
                                    ;************************************

              ORG   #0F000

;---------------------------

                                    ;************************************
                                    ;* Fixed entry points called by the *
                                    ;* patched game code                *
                                    ;************************************

SVIROW_E:     JP    SVIROW          ; #F000
SVIJOY_E:     JP    SVIJOY          ; #F003
TRIG4_E:      JP    TRIG4           ; #F006
TRIG5_E:      JP    TRIG5           ; #F009

;---------------------------

                                    ;************************************
                                    ;* SVIJOY                           *
                                    ;************************************
                                    ;* Replaces LD A,14 : OUT (#88),A : *
                                    ;* IN A,(#90), SVI PSG register 14. *
                                    ;************************************
                                    ;* OUTPUT: A bits 0-3 = joystick 1, *
                                    ;*         bits 4-7 = joystick 2,   *
                                    ;*         up/down/left/right,      *
                                    ;*         active low               *
                                    ;************************************

SVIJOY:       PUSH  BC
              LD    A,I
              PUSH  AF
              DI
              LD    B,#4F
              CALL  JOYPORT
              AND   #0F
              RLCA
              RLCA
              RLCA
              RLCA
              LD    C,A             ; C = joystick 2 in bits 4-7
              LD    B,#0F
              CALL  JOYPORT
              AND   #0F
              OR    C
              LD    C,A
              POP   AF
              JP    PO,SVIJOY1
              EI
SVIJOY1:      LD    A,C
              POP   BC
              RET

                                    ;************************************
                                    ;* TRIG4 / TRIG5                    *
                                    ;************************************
                                    ;* Replace IN A,(#98) : BIT 4,A and *
                                    ;* IN A,(#98) : BIT 5,A. SVI PPI    *
                                    ;* port A bit 4 is the trigger of   *
                                    ;* joystick 1, bit 5 that of        *
                                    ;* joystick 2; on MSX it is PSG     *
                                    ;* register 14 bit 4 of the port    *
                                    ;* selected in register 15.         *
                                    ;************************************
                                    ;* OUTPUT: A = SVI port A value     *
                                    ;*         with the trigger bit,    *
                                    ;*         Z flag as after BIT      *
                                    ;************************************

TRIG4:        PUSH  BC
              LD    B,#0F
              CALL  TRIGPORT
              POP   BC
              BIT   4,A
              RET

TRIG5:        PUSH  BC
              LD    B,#4F
              CALL  TRIGPORT
              RLCA                  ; trigger to bit 5
              POP   BC
              BIT   5,A
              RET

                                    ;************************************
                                    ;* TRIGPORT: reads PSG register 14  *
                                    ;* of the port in B with interrupts *
                                    ;* disabled, then selects port 1    *
                                    ;************************************
                                    ;* INPUT:  B = register 15 value    *
                                    ;* OUTPUT: A = register 14          *
                                    ;************************************

TRIGPORT:     LD    A,I
              PUSH  AF
              DI
              CALL  JOYPORT
              LD    C,A
              LD    B,#0F
              LD    A,15
              OUT   (#0A0),A
              LD    A,B
              OUT   (#0A1),A
              LD    A,14
              OUT   (#0A0),A
              POP   AF
              JP    PO,TRIGP1
              EI
TRIGP1:       LD    A,C
              RET

                                    ;************************************
                                    ;* JOYPORT: selects a joystick port *
                                    ;* and reads PSG register 14        *
                                    ;************************************
                                    ;* INPUT:  B = register 15 value    *
                                    ;*         (#0F port 1, #4F port 2) *
                                    ;* OUTPUT: A = register 14; PSG     *
                                    ;*         register 14 selected     *
                                    ;************************************

JOYPORT:      LD    A,15
              OUT   (#0A0),A
              LD    A,B
              OUT   (#0A1),A
              LD    A,14
              OUT   (#0A0),A
              IN    A,(#0A2)
              RET

;---------------------------

                                    ;************************************
                                    ;* SVIROW                           *
                                    ;************************************
                                    ;* Replaces OUT (#96),A : IN A,     *
                                    ;* (#99) of the SVI PPI. Returns    *
                                    ;* the row from ROWDATA while it is *
                                    ;* valid, otherwise builds it with  *
                                    ;* the routine of the SVI row in    *
                                    ;* DKROWS.INC. All rows become      *
                                    ;* invalid every ROWAGE calls, less *
                                    ;* than one frame of the game.      *
                                    ;* Flags and all other registers    *
                                    ;* are preserved.                   *
                                    ;************************************
                                    ;* INPUT:  A = SVI row (0-10)       *
                                    ;* OUTPUT: A = SVI row data, active *
                                    ;*         low; #FF for rows > 10   *
                                    ;************************************

ROWAGE:       EQU   32

SVIROW:       PUSH  BC
              PUSH  DE
              PUSH  HL
              PUSH  AF
              LD    HL,ROWCOUNT
              DEC   (HL)
              JR    NZ,SVIROW1
              LD    (HL),ROWAGE
              LD    HL,ROWVALID
              LD    B,11
SVIROW0:      LD    (HL),0
              INC   HL
              DJNZ  SVIROW0
SVIROW1:      LD    C,#0FF
              CP    11
              JR    NC,ROWDONE
              LD    E,A
              LD    D,0
              LD    HL,ROWVALID
              ADD   HL,DE
              LD    A,(HL)
              OR    A
              JR    Z,ROWMISS
              LD    BC,11
              ADD   HL,BC
              LD    C,(HL)          ; ROWDATA entry
              JR    ROWDONE
ROWMISS:      LD    (HL),1
              PUSH  HL
              LD    HL,ROWTAB
              ADD   HL,DE
              ADD   HL,DE
              LD    A,(HL)
              INC   HL
              LD    H,(HL)
              LD    L,A
              CALL  JPHL
              POP   HL
              LD    DE,11
              ADD   HL,DE
              LD    (HL),C          ; ROWDATA entry
ROWDONE:      POP   AF
              LD    A,C
              POP   HL
              POP   DE
              POP   BC
              RET

JPHL:         JP    (HL)

ROWCOUNT:     DEFB  ROWAGE
ROWVALID:     DEFS  11,0            ; 1 = ROWDATA entry valid
ROWDATA:      DEFS  11,#0FF         ; SVI row data, active low

;---------------------------

                                    ;************************************
                                    ;* Row routines and ROWTAB, written *
                                    ;* by the build from its KEYMAP     *
                                    ;************************************

              INCLUDE "DKROWS.INC"

HELPEND:
