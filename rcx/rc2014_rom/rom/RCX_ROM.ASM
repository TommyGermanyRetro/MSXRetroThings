
                                    ;************************************
                                    ;* MSX-RC2014-Interface             * 
                                    ;************************************
                                    ;* (c) 2023 by Dr.-Ing. Th. Wiesner *
                                    ;* Based on the UNAPI ROM standard  *
                                    ;* by KONAMIMAN 2019                *
                                    ;* and CALL statement posted on     *
                                    ;* https://www.msx.org              *
                                    ;************************************
 
              ORG    #04000

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Declarations                                                     +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 
                                    ;************************************
                                    ;* MSX-Bios Calls and Hooks         *
                                    ;************************************

CHGMOD:       EQU    #0005F         ; Change screen mode
CHGET:        EQU    #0009F         ; Get char from screen
CHPUT:        EQU    #000A2         ; Put char on screen
CLS:          EQU    #000C3         ; Clear screen
CALBAS:       EQU    #00159         ; Far call BASIC
ERRHAND:      EQU    #0406F         ; BASIC routine ERROR Handling
CHRGTR:       EQU    #04666         ; BASIC routine get Char from BASIC LINE
FRMEVL:       EQU    #04C64         ; BASIC routine get expression from LINE
GETBYT:       EQU    #0521C         ; BASIC routine get 1-byte-integer
FRMQNT:       EQU    #0542F         ; BASIC routine get 2-byte-integer
PTRGET:       EQU    #05EA4         ; BASIC routine get variable ptr
CHKALF:       EQU    #064A7         ; BASIC routine get flag C if variable 
FRESTR:       EQU    #067D0         ; BASIC routine clear string
VALTYP:       EQU    #0F663         ; BASIC type of parameter
MEMSIZ:       EQU    #0F672         ; Stringarea
STKTOP:       EQU    #0F674         ; Stacktop
SAVSTK:       EQU    #0F6B1         ; Pointer to valid stack bottom
DAC:          EQU    #0F7F6         ; DAC evaluated Value
USRVAR:       EQU    #0F7F8         ; BASIC parameter
ARG:          EQU    #0F847         ; Value used to be calculate with DAC
MAXFIL:       EQU    #0F85F         ; Maximum file number
FILTAB:       EQU    #0F860         ; Pointer to file pointer table
NULBUF:       EQU    #0F862         ; Pointer to buffer #0
HOKVLD:       EQU    #0FB20         ; Bit 0 = presence of an extended Bios
BOTTOM:       EQU    #0FC48         ; Pointer to bottom of RAM
HIMEM:        EQU    #0FC4A         ; Address of machine code area
SCRMOD:       EQU    #0FCAF         ; Active screen mode
OLDSCR:       EQU    #0FCB0         ; The screen mode before change
EXPTBL:       EQU    #0FCC1         ; Slot expanded
SLTWRK:       EQU    #0FD09         ; Slot working area
PROCNM:       EQU    #0FD89         ; Contents the instruction name or device name
HKEYI:        EQU    #0FD9A         ; Hook zur Interruptverabeitung
HERRO:        EQU    #0FFB1         ; Error handling
EXTBIO:       EQU    #0FFCA         ; Call to external BIOS
SUBFLG:       EQU    #0F6A5         ; Array identifier
CURLIN:       EQU    #0F41C         ; Current execution line number of Basic

;---------------------------

                                    ;************************************
                                    ;* IO addresses of peripherie       *
                                    ;************************************

WTEC:         EQU    #0C0           ; Assembly ID
WTECINV:      EQU    #03F           ; Complement of ID
IOENDIS:      EQU    #040           ; Switched IO En/Dis
ALE:          EQU    #046           ; IO-Address set
SWIO:         EQU    #047           ; IO-Data read/write

;---------------------------

                                    ;************************************
                                    ;* RCX card types                   *
                                    ;************************************

SINIT:        EQU    #0AA           ; Is set when RCX is installed
SDEL:         EQU    #055           ; Is set when RCX was uninstalled

SI2C:         EQU    'A'            ; I2C basic card with PCF8584+PFC8583

SPIO:         EQU    'B'            ; Z80 PIO card

SPPI:         EQU    'C'            ; 82C55 PPI card

SCNT:         EQU    'D'            ; 82C54 TIMER card

SCAN:         EQU    'E'            ; SJA1000 card (only once allowed)

SSPI:         EQU    'F'            ; SPI master card
                                                                                                    
SSIO:         EQU    'G'            ; Z80 SIO card

SCTC:         EQU    'H'            ; Z80 CTC card

                                    
;---------------------------                                                 

                                    ;************************************
                                    ;* Alias for work area              *
                                    ;* X-ROM points to prepared work    *
                                    ;* area in ROM which will be copied *
                                    ;* to reserved RAM in page 3 slot 0 *
                                    ;************************************

X_CD:         EQU  #010             ; Max count of cards
X_FREE:       EQU  #010             ; Free Memory each card
X_CDSTEP:     EQU  #02 + X_FREE     ; Index step in RCX table
X_CANCNT:     EQU  #020             ; Max count of CAN msg
X_ROM:        EQU  #07C00           ; Start address work area in ROM
X_HFLAGS:     EQU  #00000           ; Relative address in Main RAM Page 3
X_HGENPURP:   EQU  #00001           ; Relative address 16 bit for gen purp
X_HCANPTR:    EQU  #00003           ; Relative address CAN msg count
X_HCANMSG:    EQU  #00005           ; Relative address CAN msg buffer
X_HCARD:      EQU  #001A5           ; Relative address Table usage slots
X_HINT:       EQU  #002C5           ; Relative address CAN INT
X_HBUF:       EQU  #002CA           ; Relative address fill ptr msg buffer
X_HCNT:       EQU  #002CD           ; Relative address fill ptr msg cnt
X_HCIO:       EQU  #002D2           ; Relative address fill CAN IO
X_HENDE:      EQU  #00331           ; End of table

                                    ; The RCX memory is build up as followed
                                    ;
                                    ; ADDR     Usage
                                    ;
                                    ; +0       Shows init status
                                    ;          #0AA = initialised
                                    ;          #055 = uninstalled
                                    ; +1       General purpose 16 bit
                                    ; +3       Ptr incoming CAN Msg
                                    ; +4       Ptr last read CAN Msg
                                    ; +5       32x13 Bytes ring CAN buffer
                                    ; +421     Card Type No. 1
                                    ; +422     IO address 0..255
                                    ; +423     Free Memory
                                    ; ...
                                    ; +20      Free Memory
                                    ; +21      Card Type No. 2
                                    ; ... 
                                    ; +708     End of RCX Table
                                    ; +709     Begin interupt routine
                                    ; ...
                                    ; +832     End of RCX memory

;---------------------------

                                    ;************************************
                                    ;* ROM-file header                  *
                                    ;************************************
 
              DEFW    #4241,INITHAND,CALLHAND,0,0,0,0,0

;---------------------------

                                    ;************************************
                                    ;* API version and                  *
                                    ;* implementation version           *
                                    ;************************************

                                    ; Specification identifier 
                                    ; (up to 15 chars and zero terminated)

UNAPI_ID:     DEFM  "RCX_INTERFACE",0

                                    ; Implementation identifier 
                                    ; (up to 63 chars and zero terminated)

APIINFO:      DEFM  "RCX Interface UNAPI ROM",0

API_V_P:      EQU  1
API_V_S:      EQU  0
ROM_V_P:      EQU  1
ROM_V_S:      EQU  0

;---------------------------

                                    ;************************************
                                    ;* Maximum number of available      *
                                    ;* standard and implementation-     *
                                    ;* specific function numbers        *
                                    ;************************************

MAX_FN:       EQU  56               ; Must be 0 to 127

MAX_IMPFN:    EQU  0                ; Must be either zero (if no 
                                    ; implementation-specific functions 
                                    ; available), or 128 to 254

;---------------------------

                                    ;************************************
                                    ;* BASIC                            *
                                    ;* List of available instructions   *
                                    ;* (as ASCIIZ) and execute address  *
                                    ;* (as word)                        *
                                    ;************************************

B_CMDS:
              DEFM "RCX ?",0        ; Call RCX HELP
              DEFW B_CORE1

              DEFM "RCX INIT",0     ; Call RCX INIT
              DEFW B_CORE2

              DEFM "RCX ISINIT",0   ; Call RCX ISINIT
              DEFW B_CORE3

              DEFM "RCX DEINIT",0   ; Call RCX DEINIT
              DEFW B_CORE4

              DEFM "RCX TABLE",0    ; Call RCX TABLE
              DEFW B_CORE5

              DEFM "I2C INIT",0     ; Call I2C INIT
              DEFW B_CORE6

              DEFM "I2C WR",0       ; Call I2C WRITE
              DEFW B_CORE7

              DEFM "I2C WRB",0      ; Call I2C WRITE Block
              DEFW B_CORE8

              DEFM "I2C RD",0       ; Call I2C READ
              DEFW B_CORE9

              DEFM "I2C RDB",0      ; Call I2C READ BLOCK
              DEFW B_CORE10

              DEFM "I2C RESET",0    ; Call I2C RESET
              DEFW B_CORE11

              DEFM "SET TIME",0     ; Call SET TIME
              DEFW B_CORE12

              DEFM "GET TIME",0     ; Call GET TIME
              DEFW B_CORE13

              DEFM "SET DATE",0     ; Call SET DATE
              DEFW B_CORE14

              DEFM "GET DATE",0     ; Call GET DATE
              DEFW B_CORE15

              DEFM "PIO INIT",0     ; Call PIO INIT
              DEFW B_CORE16

              DEFM "PIO A RD",0     ; Call PIO A RD
              DEFW B_CORE17

              DEFM "PIO A WR",0     ; Call PIO A WR
              DEFW B_CORE18

              DEFM "PIO B RD",0     ; Call PIO B RD
              DEFW B_CORE19

              DEFM "PIO B WR",0     ; Call PIO B WR
              DEFW B_CORE20

              DEFM "PIO A CTRL",0   ; Call PIO A CTRL
              DEFW B_CORE21
                                    
              DEFM "PIO B CTRL",0   ; Call PIO B CTRL
              DEFW B_CORE22

              DEFM "PPI INIT",0     ; Call PPI INIT
              DEFW B_CORE23

              DEFM "PPI A RD",0     ; Call PPI A RD
              DEFW B_CORE24

              DEFM "PPI A WR",0     ; Call PPI A WR
              DEFW B_CORE25

              DEFM "PPI B RD",0     ; Call PPI B RD
              DEFW B_CORE26

              DEFM "PPI B WR",0     ; Call PPI B WR
              DEFW B_CORE27

              DEFM "PPI C RD",0     ; Call PPI C RD
              DEFW B_CORE28

              DEFM "PPI C WR",0     ; Call PPI C WR
              DEFW B_CORE29
                                    
              DEFM "PPI CTRL",0     ; Call PPI CTRL
              DEFW B_CORE30

              DEFM "PPI SET",0      ; Call PPI SET
              DEFW B_CORE31

              DEFM "PPI RESET",0    ; Call PPI RESET
              DEFW B_CORE32

              DEFM "TIMER INIT",0   ; Call 8254 Init
              DEFW B_CORE33

              DEFM "TIMER 0 RD",0   ; Call 8254 0 RD
              DEFW B_CORE34

              DEFM "TIMER 0 WR",0   ; Call 8254 0 WR
              DEFW B_CORE35

              DEFM "TIMER 1 RD",0   ; Call 8254 1 RD
              DEFW B_CORE36

              DEFM "TIMER 1 WR",0   ; Call 8254 1 WR
              DEFW B_CORE37

              DEFM "TIMER 2 RD",0   ; Call 8254 2 RD
              DEFW B_CORE38

              DEFM "TIMER 2 WR",0   ; Call 8254 2 WR
              DEFW B_CORE39
                                    
              DEFM "TIMER CTRL",0   ; Call 8254 CTRL
              DEFW B_CORE40

              DEFM "TIMER SET GATE",0   ; Call 8254 intern GATE set
              DEFW B_CORE41

              DEFM "TIMER RES GATE",0   ; Call 8254 intern GATE reset
              DEFW B_CORE42

              DEFM "CAN INIT",0     ; Call SJA1000 INIT
              DEFW B_CORE43

              DEFM "CAN WR",0       ; Call SJA1000 WR
              DEFW B_CORE44

              DEFM "CAN RD",0       ; Call SJA1000 RD
              DEFW B_CORE45

              DEFM "CAN CHECK",0    ; Call SJA1000 CHECK
              DEFW B_CORE46

              DEFM "CAN RX",0       ; Call SJA1000 RX
              DEFW B_CORE47

              DEFM "CAN GET ADDR",0 ; Call SJA1000 get Init routine
              DEFW B_CORE48

              DEFM "CAN TX",0       ; Call SJA1000 TX
              DEFW B_CORE49

              DEFM "SPI INIT",0     ; Call SPI INIT
              DEFW B_CORE50

              DEFM "SPI WR",0       ; Call SPI WR
              DEFW B_CORE51

              DEFM "SPI WRB",0      ; Call SPI WRB
              DEFW B_CORE52

              DEFM "SPI RD",0       ; Call SPI RD
              DEFW B_CORE53

              DEFM "SPI RDB",0      ; Call SPI RDB
              DEFW B_CORE54

              DEFM "SPI RDH",0      ; Call SPI RDH
              DEFW B_CORE55

              DEFM "SPI MODE",0     ; Call SPI MODE
              DEFW B_CORE56
                                                                                                 
              DEFB 0                ; No more instructions

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* FUNCTIONS ADDRESSES TABLE        *
                                    ;************************************

                                    ; TODO: Adjust for the routines 
                                    ; of your implementation

FN_TABLE:
              DEFW  FN_INFO         ; Call RCX INFO

              DEFW  FN_CORE1        ; Call RCX HELP

              DEFW  FN_CORE2        ; Call RCX INIT

              DEFW  FN_CORE3        ; Call RCX ISINIT

              DEFW  FN_CORE4        ; Call RCX DEINIT

              DEFW  FN_CORE5        ; Call RCX TABLE

              DEFW  FN_CORE6        ; Call I2C INIT

              DEFW  FN_CORE7        ; Call I2C WRITE

              DEFW  FN_CORE8        ; Call I2C WRITE BLOCK

              DEFW  FN_CORE9        ; Call I2C READ

              DEFW  FN_CORE10       ; Call I2C READ BLOCK

              DEFW  FN_CORE11       ; Call I2C RESET

              DEFW  FN_CORE12       ; Call SET TIME

              DEFW  FN_CORE13       ; Call GET TIME

              DEFW  FN_CORE14       ; Call SET DATE

              DEFW  FN_CORE15       ; Call GET DATE

              DEFW  FN_CORE16       ; Call PIO INIT

              DEFW  FN_CORE17       ; Call PIO A RD

              DEFW  FN_CORE18       ; Call PIO A WR

              DEFW  FN_CORE19       ; Call PIO B RD

              DEFW  FN_CORE20       ; Call PIO B WR

              DEFW  FN_CORE21       ; Call PIO A CTRL

              DEFW  FN_CORE22       ; Call PIO B CTRL

              DEFW  FN_CORE23       ; Call PPI INIT

              DEFW  FN_CORE24       ; Call PPI A RD

              DEFW  FN_CORE25       ; Call PPI A WR

              DEFW  FN_CORE26       ; Call PPI B RD

              DEFW  FN_CORE27       ; Call PPI B WR

              DEFW  FN_CORE28       ; Call PPI C RD

              DEFW  FN_CORE29       ; Call PPI C WR

              DEFW  FN_CORE30       ; Call PPI CTRL

              DEFW  FN_CORE31       ; Call PPI SET

              DEFW  FN_CORE32       ; Call PPI RESET

              DEFW  FN_CORE33       ; Call TIMER INIT

              DEFW  FN_CORE34       ; Call TIMER 0 RD

              DEFW  FN_CORE35       ; Call TIMER 0 WR

              DEFW  FN_CORE36       ; Call TIMER 1 RD

              DEFW  FN_CORE37       ; Call TIMER 1 WR

              DEFW  FN_CORE38       ; Call TIMER 2 RD

              DEFW  FN_CORE39       ; Call TIMER 2 WR

              DEFW  FN_CORE40       ; Call TIMER CTRL

              DEFW  FN_CORE41       ; Call TIMER SET GATE

              DEFW  FN_CORE42       ; Call TIMER RES GATE

              DEFW  FN_CORE43       ; Call CAN INIT

              DEFW  FN_CORE44       ; Call CAN WR

              DEFW  FN_CORE45       ; Call CAN RD

              DEFW  FN_CORE46       ; Call CAN CHECK

              DEFW  FN_CORE47       ; Call CAN RX

              DEFW  FN_CORE48       ; Call CAN GET ADDR

              DEFW  FN_CORE49       ; Call CAN TX

              DEFW  FN_CORE50       ; Call SPI INIT

              DEFW  FN_CORE51       ; Call SPI WR

              DEFW  FN_CORE52       ; Call SPI WRB

              DEFW  FN_CORE53       ; Call SPI RD

              DEFW  FN_CORE54       ; Call SPI RDB

              DEFW  FN_CORE55       ; Call SPI RDH

              DEFW  FN_CORE56       ; Call SPI MODE


                                    ; Implementation-specific routines 
                                    ; addresses table

              if MAX_IMPFN >= 128

              IMPFN_TABLE:

              DEFW  FN_DUMMY        ; FN_128

              endif

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ MSX start up routine                                             +++
;+++ Cartridge Header functions from KONAMIMAN und msx.org            +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* General INIT                     *
                                    ;* Handler when MSX starts          *
                                    ;************************************

INITHAND:     LD A,(HOKVLD)         ; Initialize EXTBIO hook if necessary
              BIT 0,A
              JR NZ,OK_INIEXTB

              LD HL,EXTBIO
              LD DE,EXTBIO+1
              LD BC,4
              LD (HL),#0C9          ; code for RET
              LDIR

              OR 1
              LD (HOKVLD),A

OK_INIEXTB:   CALL GETSLT           ; Save previous EXTBIO hook
              CALL GETWRK
              EX DE,HL
              LD HL,EXTBIO
              LD BC,5
              LDIR

              DI                    ; Patch EXTBIO hook
              
              LD A,#0F7             ; code for "RST 30h"
              LD (EXTBIO),A
              CALL GETSLT
              LD (EXTBIO+1),A
              LD HL,DO_EXTBIO
              LD (EXTBIO+2),HL

              EI

ROM_INIT:     LD A,#00              ; Activate Screen 0
              CALL CHGMOD
              CALL CLS              ; CLS
              LD HL,PSTART          ; Prompt Welcome Message
              CALL PROMPT

              CALL GETSLT           ; Prompt Slot-ID
              LD B,A
              AND #03
              ADD A,#30
              CALL CHPUT
              LD A,'-'
              CALL CHPUT
              LD A,B
              AND #0C
              SRL A
              SRL A
              ADD A,#30
              CALL CHPUT

              LD HL,PCRLF           ; Prompt CRLF
              CALL PROMPT

              LD B,#0A              ; Wait 10s
              CALL DELAY
              CALL CLS              ; CLS
              LD A,(OLDSCR)         ; Activate previous Screen
              CALL CHGMOD

              RET      

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ BASIC entry call                                                 +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* General BASIC                    *
                                    ;* CALL-instruction handler         *
                                    ;* Based on the MSX.ORG Examples    *
                                    ;* by NYYRIKKI, zPasi, et al.       *
                                    ;************************************
 
CALLHAND:     PUSH HL               ; Save BASIC Line-Pointer
              
              LD HL,B_CMDS          ; Table with "_" instructions

CHKCMD:       LD DE,PROCNM
LOOP:         LD A,(DE)
              CP (HL)
              JR NZ,TONEXTCMD       ; Not equal
              INC DE
              INC HL
              AND A
              JR NZ,LOOP            ; No end of instruction name, go checking
              LD E,(HL)
              INC HL
              LD D,(HL)
              POP HL                ; Get BASIC Line-Pointer back

              CALL GETPREVCHAR
              
              PUSH DE               ; Save routine address for execution
              AND A                 ; Clear CARRY to sign valid CALL 
              RET                   ; Return to main routine
 
TONEXTCMD:    LD C,#0FF
              XOR A
              CPIR                  ; Skip to end of instruction name
              INC HL
              INC HL                ; Skip address
              CP (HL)
              JR NZ,CHKCMD          ; Not end of table, go checking

              POP HL                ; Get BASIC Line-Pointer back
              SCF                   ; Set CARRY to sign invalid CALL
              RET                   ; Return to main routine

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ EXTBIOS entry call                                               +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* General EXTBIOS                  *
                                    ;* CALL-instruction handler         *
                                    ;* Based on UNAPI Examples          *
                                    ;* by Konamiman                     *
                                    ;************************************

                                    ;************************************
                                    ;* EXTBIO HOOK EXECUTION            *
                                    ;************************************

DO_EXTBIO:
              PUSH HL
              PUSH BC
              PUSH AF
              LD A,D
              CP #022
              JR NZ,JUMP_OLD
              CP E
              JR NZ,JUMP_OLD

              ; Check API ID

              LD HL,UNAPI_ID
              LD DE,ARG
LOOP1:        LD A,(DE)
              CALL  TOUPPER
              CP (HL)
              JR NZ,JUMP_OLD2
              INC HL
              INC DE
              OR A
              JR NZ,LOOP1

              ; A=255: Jump to old hook

              POP AF
              PUSH AF
              INC A
              JR Z,JUMP_OLD2

              ; A=0: B=B+1 and jump to old hook

              CALL  GETSLT
              CALL  GETWRK
              POP AF
              POP BC
              OR A
              JR NZ,DO_EXTBIO2
              INC B
              EX (SP),HL
              LD  DE,#02222
              RET

DO_EXTBIO2:

              ; A=1: Return A=Slot, B=Segment, HL=UNAPI entry address

              DEC A
              JR NZ,DO_EXTBIO3
              POP HL
              CALL  GETSLT
              LD B,#0FF
              LD HL,UNAPI_ENTRY
              LD DE,#02222
              RET

              ; A>1: A=A-1, and jump to old hook

DO_EXTBIO3:  
              ; A=A-1 already done
              
              EX (SP),Hl
              LD DE,#02222
              RET

              ; Jump here to execute old EXTBIO code

JUMP_OLD2:
              LD DE,#02222

JUMP_OLD:     ; Assumes "push hl,bc,af" done
              
              PUSH DE
              CALL  GETSLT
              CALL  GETWRK
              POP DE
              POP AF
              POP BC
              EX (SP),HL
              RET

;---------------------------

                                    ;************************************
                                    ;* FUNCTIONS ENTRY POINT CODE       *
                                    ;************************************

UNAPI_ENTRY:  PUSH HL
              PUSH AF
              LD HL,FN_TABLE
              BIT 7,A

              if MAX_IMPFN >= 128

              JR Z,IS_STANDARD
              LD HL,IMPFN_TABLE
              AND  01111111b
              CP  MAX_IMPFN-128
              JR Z,OK_FNUM
              JR NC,UNDEFINED
              
              IS_STANDARD:

              else

              JR NZ,UNDEFINED

              endif

              CP  MAX_FN
              JR Z,OK_FNUM
              JR NC,UNDEFINED

OK_FNUM:
              ADD A,A
              PUSH DE
              LD E,A
              LD D,0
              ADD HL,DE
              POP DE

              LD A,(HL)
              INC HL
              LD H,(HL)
              LD L,A
              POP AF
              EX (SP),HL
              RET

              ; Undefined function: return with registers unmodified

UNDEFINED:
              POP AF
              POP HL
              RET

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Declaration of ROM functions                                     +++
;+++ 1. BASIC entry (B_xxx)                                           +++
;+++ 2. EXTBIOS entry (FN_xxx)                                        +++
;+++ 3. CORE equal for both entries                                   +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Mandatory routine A = 0          *
                                    ;************************************ 
                                    ;* Gives info to UNAPI back to      *
                                    ;* main routine                     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HL = Descriptive string for this *
                                    ;* implementation, on this slot,    *
                                    ;* zero terminated                  *
                                    ;* DE = API version supported, D.E  *
                                    ;* BC = This implementation version,*
                                    ;* B.C.                             *
                                    ;* A  = 0 and Cy = 0                *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC, DE, HL                   *
                                    ;************************************

FN_INFO:      LD BC,256*ROM_V_P+ROM_V_S
              LD DE,256*API_V_P+API_V_S
              LD HL,APIINFO
              XOR A                 ; Clear CARRY Flag
              RET                   ; Go back to main routine

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RCX main administrative functions                                +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL RCX ?                       *
                                    ;************************************ 
                                    ;* Direct Jump to CORE command      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

B_CORE1:      JP CORE1              ; Continue with CORE command

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 1                    *
                                    ;************************************ 
                                    ;* Direct Jump to CORE command      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************


FN_CORE1:     JP CORE1              ; Continue with CORE command


                                    ;************************************ 
                                    ;* HELP Desk                        *
                                    ;************************************ 
                                    ;* Shows the HELP Desk for Commands *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE1:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)

CORE1HM:      CALL CLEARSCR

              LD HL,PHELP 
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM1
              CP 'P'
              JR Z,CORE1HM
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM            ; Invalid char. Go back to beginning

CORE1HM1:     CALL CLEARSCR

              LD HL,PHELPA1
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM2
              CP 'P'
              JR Z,CORE1HM
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM1           ; Invalid char. Go back to beginning

CORE1HM2:     CALL CLEARSCR

              LD HL,PHELPA2
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM3
              CP 'P'
              JR Z,CORE1HM1
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM2           ; Invalid char. Go back to beginning

CORE1HM3:     CALL CLEARSCR

              LD HL,PHELPB1
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM4
              CP 'P'
              JR Z,CORE1HM2
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM3           ; Invalid char. Go back to beginning

CORE1HM4:     CALL CLEARSCR

              LD HL,PHELPB2
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM5
              CP 'P'
              JR Z,CORE1HM3
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM4           ; Invalid char. Go back to beginning

CORE1HM5:     CALL CLEARSCR

              LD HL,PHELPC1
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM6
              CP 'P'
              JR Z,CORE1HM4
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM5           ; Invalid char. Go back to beginning

CORE1HM6:     CALL CLEARSCR

              LD HL,PHELPC2
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM7
              CP 'P'
              JR Z,CORE1HM5
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM6           ; Invalid char. Go back to beginning

CORE1HM7:     CALL CLEARSCR

              LD HL,PHELPD1
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM8
              CP 'P'
              JR Z,CORE1HM6
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM7           ; Invalid char. Go back to beginning

CORE1HM8:     CALL CLEARSCR

              LD HL,PHELPD2
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM9
              CP 'P'
              JR Z,CORE1HM7
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM8           ; Invalid char. Go back to beginning

CORE1HM9:     CALL CLEARSCR

              LD HL,PHELPE1
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM10
              CP 'P'
              JR Z,CORE1HM8
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM9           ; Invalid char. Go back to beginning

CORE1HM10:    CALL CLEARSCR

              LD HL,PHELPE2
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1END
              CP 'P'
              JR Z,CORE1HM9
              CP 'Z'
              JP Z,CORE1END
              JR CORE1HM10           ; Invalid char. Go back to beginning

CORE1END:     CALL CLEARSCR

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine


;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL RCX INIT                    *
                                    ;************************************ 
                                    ;* Initialise RCX                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE2:      PUSH BC
              PUSH HL
              PUSH DE

B_CORE2XIO:   IN A,(IOENDIS)        ; Get old #40
              LD B,A
              LD A,WTEC             ; Check if [XIO] is present
              OUT (IOENDIS),A
              IN A,(IOENDIS)
              CP WTECINV
              LD A,B
              OUT (IOENDIS),A       ; Set old #40
              
              JP NZ,B_CORE2XIOERR   ; Not present, error

B_CORE2WA:    CALL GETRAM           ; Work area already prepared?
              LD B,H
              LD A,L
              OR B
              JP Z,B_CORE2RAM       ; No? Start reservation RAM

B_CORE2INIT:  LD DE,X_HFLAGS
              CALL GETRAMIX
              LD A,(HL)
              CP SDEL               ; Check if it was installed before
              JP Z,B_CORE2RAM1      ; Yes? Then go to clean RAM
              CP SINIT              ; Module installed if A = SINIT
              JP NZ,B_CORE2RAM      ; No? Continue with Work area

B_CORE2ISI:   LD HL,PISINIT         ; Yes? Prompt message 
              CALL PROMPT           ; that module is already installed
              POP DE                ; Clean SP
              POP HL                ; Get BASIC Line-Pointer
              POP BC                ; Clean SP
              RET                   ; Back to BASIC programme 

B_CORE2RAM:   LD HL,HENDE-X_ROM     ; Prepare working area  
              CALL ALLOC
              JP C,B_CORE2MEMERR    ; Carry Flag set = Error
              CALL SETRAM
              CALL DEFILE 
              
B_CORE2RAM1:  POP DE                ; Clean SP
              POP HL                ; Get BASIC Line-Pointer
              POP BC                ; Clean SP               

              JP CORE2              ; Continue with CORE command

B_CORE2MEMERR:LD HL,PMEMERR         ; Yes? Prompt message 
              CALL PROMPT           ; that not enough memory is left
              JP B_CORE2ERREND

B_CORE2XIOERR:LD HL,PERRINITY       ; Prompt message 
              CALL PROMPT           ; that module [XIO] is not present
              
B_CORE2ERREND:POP DE                ; Clean SP
              POP HL                ; Get BASIC Line-Pointer
              POP BC                ; Clean SP
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 2                    *
                                    ;************************************ 
                                    ;* Initialise RCX                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* HL = RAM base address            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (SLTWORK+6/7) = RAM base address *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE2:     PUSH BC
              PUSH DE

FN_CORE2XIO:  IN A,(IOENDIS)        ; Get old #40
              LD B,A
              LD A,WTEC             ; Check if [XIO] is present
              OUT (IOENDIS),A
              IN A,(IOENDIS)
              CP WTECINV
              LD A,B
              OUT (IOENDIS),A       ; Set old #40

              JP Z,FN_CORE2_WA      ; Yes? Go on

              POP DE                ; Clear stack position DE
              POP BC                ; Get IO addr back from stack

              JP FN_CORE2_ERR       ; No? Return Error

FN_CORE2_WA:  PUSH HL

              CALL GETRAM           ; Work area already prepared?
              LD B,H
              LD A,L
              OR B
              
              POP HL
              
              JP NZ,FN_CORE2_ISI    ; Yes? Check if is already init

FN_CORE2_INI: CALL SETRAM           ; Save RAM base address in slot work

FN_CORE2_ISI: LD DE,X_HFLAGS
              CALL GETRAMIX
              LD A,(HL)
              POP DE                ; Clear stack position DE
              POP BC                ; Get IO addr back from stack
              CP SDEL               ; Module installed before if A = SDEL
              JP Z,CORE2            ; Yes? Continue with CORE command
              CP SINIT              ; Module installed if A = SINIT
              JP NZ,CORE2           ; No? Continue with CORE command

FN_CORE2_ERR: SCF                   ; Set Carry Flag for ERROR 
              RET                   ; Go back to main routine


                                    ;************************************ 
                                    ;* RCX INIT                         *
                                    ;************************************
                                    ;* Initialise RCX                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* (SLTWORK+6/7) = RAM base address *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* HIOFLAGS = INIT flags            *
                                    ;************************************
                                    ;* BASIC-RETURN:                    *
                                    ;************************************
                                    ;* (X_HFLAGS) Bit FINIT set         *
                                    ;************************************ 

CORE2:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
              PUSH DE               ; Save DE to stack

              CALL GETRAM           ; Obtain RAM base address
              LD BC,HENDE-HFLAGS    ; Prepare working area 
              EX DE,HL
              LD HL,X_ROM
              LDIR    
               
              LD DE,X_HFLAGS
              CALL GETRAMIX         
              LD (HL),SINIT         ; Sign RCX as installed     

CORE2END:     POP DE                ; Get DE back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine


;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL RCX ISINIT(ISI)             *
                                    ;************************************ 
                                    ;* Checks, if working area is       *
                                    ;* reserved, working area is pre-   *
                                    ;* pared and module is init         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* Varptr(ISI) = ISI                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = Varptr(ISI)                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, BC, IX                   *
                                    ;************************************

B_CORE3:      CALL CHKCHAR
              DEFB "("              ; Check for (

              PUSH BC
              LD B,0
              CALL EVALVARINT       ; Test variable, DE contains varptr
              POP BC

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              INC DE
              LD A,0
              LD (DE),A
              DEC DE                ; DE points to varptr

              CALL RCXINIT
              JP Z,B_CORE3ISI       ; Yes? Set true No? Set false

              LD A,0
              LD (DE),A             ; Not installed? Set ISI = 0 

              OR A
              RET                   ; Go back to main routine

B_CORE3ISI:   LD A,1                ; Installed? Set ISI = 1 
              LD (DE),A

              OR A
              RET                   ; Go back to main routine  
              

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 3                    *
                                    ;************************************ 
                                    ;* Checks, if working area is       *
                                    ;* reserved, working area is pre-   *
                                    ;* pared and module is init         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = Varptr(ISI)                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (X_HVARPTR) = Varptr(ISI)        *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE3:     PUSH AF
              INC DE
              LD A,0
              LD (DE),A
              DEC DE

              CALL RCXINIT
              JP Z,FN_CORE3ISI      ; Yes? Set true No? Set false
    
              LD A,0
              LD (DE),A             ; Not installed? Set ISI = 0 

              POP AF

              RET                   ; Go back to main routine

FN_CORE3ISI:  LD A,1                ; Installed? Set ISI = 1 
              LD (DE),A

              POP AF

              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL RCX DEINIT                  *
                                    ;************************************ 
                                    ;* Clears Work Area                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
              
B_CORE4:      CALL RCXINIT          ; Module installed?
              JP Z,CORE4            ; Yes? Deinit No? Prompt error
                                                                      
B_CORE4NOISI: CALL MODERR           ; Prompt error no RCX
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 4                    *
                                    ;************************************
                                    ;* Clears Work Area                 *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE4:     CALL RCXINIT          ; Module installed?
              JP Z,CORE4            ; Installed? Continue with CORE command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* RCX DEINIT                       *
                                    ;************************************
                                    ;* Clears Work Area                 *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              * 
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (X_HFLAGS) = DEL set             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
                                    
CORE4:        DI                    ; Disable INT

              PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
              PUSH DE

CORE4RAMOFF:  LD DE,X_HFLAGS
              CALL GETRAMIX         
              LD (HL),SDEL          ; Sign RCX as deinstalled               
    
              POP DE
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)

              EI                    ; Enable INT
    
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine


;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL RCX TABLE                   *
                                    ;************************************ 
                                    ;* Returns RCX table of cards       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: TABLE ADDR                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = varptr(TABLE ADDR)          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
              
B_CORE5:      CALL RCXINIT          ; Module installed?
              JP NZ,B_CORE5NOISI    ; No? Prompt error

              CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              JP CORE5

                                                              
B_CORE5NOISI: CALL MODERR           ; Prompt error no RCX
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 5                    *
                                    ;************************************
                                    ;* Returns RCX table of cards       *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(TABLE ADDR)          *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* DE = varptr(TABLE ADDR)          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE5:     CALL RCXINIT          ; Module installed?
              JP Z,CORE5 
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* CALL RCX TABLE                   *
                                    ;************************************
                                    ;* Returns RCX table of cards       *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(TABLE ADDR)          * 
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE)+(DE+1) = TABLE ADDR         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************
                                    
CORE5:        PUSH HL               ; Save BASIC Pointer

              PUSH DE               ; Save DE to stack

              LD DE,X_HCARD
              CALL GETRAMIX         ; HL contains tab index types card 0

              POP DE                ; Get DE back from stack

              LD A,L
              LD (DE),A
              INC DE
              LD A,H
              LD (DE),A             ; Save table address to varptr
         
              POP HL                ; Get BASIC Pointer back
    
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;--------------------------- 

                                    ;************************************
                                    ;* Module installed?                *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* Z-Flag set = Yes                 *
                                    ;* Z-Flag not set = No              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

RCXINIT:      PUSH HL               ; Save BASIC pointer to stack
              PUSH DE               ; Save varptr to stack
              LD DE,X_HFLAGS
              CALL GETRAMIX
              LD A,(HL)
              CP #0AA               ; Module installed if A = 0xaa
              POP DE                ; Get varptr from stack
              POP HL                ; Get BASIC pointer from stack

              RET

;--------------------------- 

                                    ;************************************
                                    ;* Test if card type was already    *
                                    ;* installed for use once only      *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* B = Slot card type               *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C-Flag set = Yes                 *
                                    ;* C-Flag not set = No              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

RCXONCE:      PUSH HL               ; Save HL to stack
              PUSH DE               ; Save DE to stack
              PUSH BC               ; Save BC to stack
                                                      
              LD DE,X_HCARD
              CALL GETRAMIX         ; Get tab with slot card types
                                    ; HL contains tab address

              LD DE,X_CDSTEP        ; Account of Bytes each card

              LD C,B                ; C contains card type
              LD B,X_CD             ; Account of cards
RCXONCELOOP:  LD A,(HL)
              CP C                  ; Card used? Then go on
              JP Z,RCXONCEUSED

              ADD HL,DE             ; HL points to next slot index tab
              
              DJNZ RCXONCELOOP

              POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              SCF
              CCF                   ; Clear C-Flag = no usage found

              RET 

RCXONCEUSED:  POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              SCF                   ; Set C-Flag = usage found

              RET           

;--------------------------- 

                                    ;************************************
                                    ;* Set card at free RCX slot        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* B = Slot card type               *
                                    ;* C = Slot card I/O address        *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* IX = Free Slot Tab Address       *
                                    ;* C-Flag not set = no free slot    *
                                    ;* C-Flag set = free slot found     * 
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

SETRCXSLT:    PUSH HL               ; Save HL to stack
              PUSH DE               ; Save DE to stack
              PUSH BC               ; Save BC to stack
                                                      
              LD DE,X_HCARD
              CALL GETRAMIX         ; Get tab with slot card types
                                    ; HL contains tab address

              LD DE,X_CDSTEP        ; Account of Bytes each card

              LD B,X_CD             ; Account of cards
FRERCXLOOP:   LD A,(HL)
              CP 0                  ; Slot not used? Then go on
              JP Z,FRERCXSLOT

              INC HL

              LD A,(HL)             ; Get IO address of used card
              CP C                  ; Check if already in use
              JP NZ,FRERCXIO

              JP FRERCXERR          ; If already in use prompt ERROR

FRERCXIO:     DEC HL

              ADD HL,DE             ; HL points to next slot index tab
              
              DJNZ FRERCXLOOP

              POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              SCF
              CCF                   ; Clear C-Flag = no free slot found

              RET 

FRERCXSLOT:   PUSH HL
              POP IX
              INC IX
              INC IX                ; IX contains pointer to free card mem

              POP BC                ; Get BC back from stack
              LD (HL),B             ; Save type
              INC HL
              LD (HL),C             ; Save I/O-address

              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack
  
              SCF                   ; Set C-Flag = free slot found

              RET                   ; Go back to calling routine

FRERCXERR:    LD HL,PERRADDR        ; IO address already in use
              CALL PROMPT
             
              POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS


;--------------------------- 

                                    ;************************************
                                    ;* Get RCX slot Mem                 *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* B = Slot card type               *
                                    ;* C = Slot card I/O address        *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* IX = Slot Tab Address            *
                                    ;* B = Slot card type               *
                                    ;* C = Slot card I/O address        *
                                    ;* C-Flag not set = no slot found   *
                                    ;* C-Flag set = slot found          * 
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

GETRCXSLT:    PUSH HL               ; Save HL to stack
              PUSH DE               ; Save DE to stack
              PUSH BC               ; Save BC to stack  

              LD DE,X_HCARD
              CALL GETRAMIX         ; Get tab with slot card types
                                    ; HL contains tab index types card 0
 
              POP DE                ; DE contains slot type and address

              LD B,X_CD             ; Account of cards

RCXLOOP:      LD A,(HL)             ; HL points to type
              CP D                  ; Compare type
              JP NZ, RCXLOOP1       ; Prepare next tab index types if not found
              INC HL
              LD A,(HL)             ; HL points to I/O address
              DEC HL
              CP E                  ; Compare I/O address
              JP Z,RCXSLOTOK        ; Type & I/O address found

RCXLOOP1:     PUSH DE
              LD DE,X_CDSTEP        ; Account of Bytes each card
              ADD HL,DE             ; HL points to next tab index types
              POP DE

              DJNZ RCXLOOP

              PUSH DE
              POP BC                ; Get original BC back
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              SCF
              CCF                   ; Clear C-Flag = no slot found

              RET

RCXSLOTOK:    PUSH HL
              POP IX
              INC IX
              INC IX                ; IX contains pointer to free card mem

              PUSH DE
              POP BC                ; Get original BC back
              POP DE                ; Get DE back from stack
              POP HL                ; Get HL back from stack

              SCF                   ; Set C-Flag = slot found

              RET                   ; Go back to calling routine

;---------------------------

                                    ;************************************
                                    ;* Switched IO enable               *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************


IOEN:         PUSH AF
              LD A,WTEC
              OUT (IOENDIS),A
              POP AF
              RET

;---------------------------

                                    ;************************************
                                    ;* Switched IO disable              *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

IODIS:        PUSH AF
              LD A,0
              OUT (IOENDIS),A
              POP AF
              RET     


;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type A                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with PCF8584 I2C-Controller and PCF8583 RTC          +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C INIT(ADDR)              *
                                    ;************************************ 
                                    ;* Initialise PCF8584               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: PCF8584 addr              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PCF8584 addr                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE6:      CALL RCXINIT          ; Module installed?
              JP Z,B_CORE6START     ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE6START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr from stack 

              LD B,SI2C             ; Set type SI2C 
              
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,SLTFREEERR      ; No free slot found

              JP CORE6              ; Continue with CORE command


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 6                    *
                                    ;************************************ 
                                    ;* Direct Jump to CORE command      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PCF8584 addr                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PCF8584 addr                 *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE6:     CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE6SLT      ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE6SLT:  PUSH BC
              LD B,SI2C             ; Set type SI2C 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC
              JP C,CORE6            ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* PCF8584 Initialize               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PCF8584 addr                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* BASIC-RETURN:                    *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************ 

CORE6:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write stack with HL dummy
            
              CALL IOEN             ; Enable Module

              LD A,C                ; Get IO adress of PCF8584
              LD D,A                ; Set A0 = 0
              ADD A,#01
              LD E,A                ; Set A0 = 1
   
              LD A,E                ; Loads byte 80H into register S1'
              OUT (ALE),A           ; i.e. next byte will be loaded into register S0'
              LD A,#080             ; (own address register); serial interface off.
              OUT (SWIO),A
                                   
              LD A,D                ; Loads byte 55H into register S0'
              OUT (ALE),A           ; effective own address becomes AAH
              LD A,#055             
              OUT (SWIO),A
                                    
              LD A,E                ; Loads byte A0H into register S1, i.e. next byte
              OUT (ALE),A           ; will be loaded into the clock control register S2.
              LD A,#0A0             
              OUT (SWIO),A
                                    
              LD A,D                ; Loads byte 18H into register S2;
              OUT (ALE),A           ; system clock is 8 MHz; SCL = 90 kHz
              LD A,#018             
              OUT (SWIO),A
 
              LD A,E                ; Loads byte C1H into register S1; register enable
              OUT (ALE),A           ; serial interface, set I2C-bus into idle mode
              LD A,#0C1             ; SDA and SCL are HIGH. The next write or read
              OUT (SWIO),A          ; operation will be to/from data transfer register
                                    ; S0 if A0 = LOW.
     
              CALL IODIS            ; Disable Module

CORE6END:     POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C WR(BUS,CHIP,DATA)       *
                                    ;************************************ 
                                    ;* Write data to PCF8584            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: I2C CHIP                  *
                                    ;* PARA3: I2C DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* E = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE7:      CALL RCXINIT          ; Module installed?
              JP Z,B_CORE7START     ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE7START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR

              LD E,A                
              PUSH DE               ; Save Bus IO address on stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1
              CALL EVALBYTEVAR
              
              LD C,A                ; Save IO address
              PUSH BC               ; Save IO address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,2
              CALL EVALBYTEVAR

              POP BC                
              LD B,A                ; BC contains IO address and data
                            
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP DE                ; DE contains Bus IO address

              PUSH BC               ; Save I2C address and data to stack
              LD B,SI2C             ; Set type SI2C
              LD C,E 
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C address and data from stack

              JP C,CORE7            ; JP if slot found

              JP SLTERR             ; No suitable slot found



                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 7                    *
                                    ;************************************ 
                                    ;* Write data to PCF8584            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* E = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE7:     CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE7SLT      ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE7SLT:  PUSH BC               ; Save I2C CHIP & I2C DATA to stack
              LD B,SI2C             ; Set type SI2C 
              LD C,E
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C CHIP & I2C DATA from stack
              JP C,CORE7            ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* I2C WR(BUS,CHIP,DATA)            *
                                    ;************************************ 
                                    ;* Write data to PCF8584            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE7:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              LD A,E                ; Get IO adress of PCF8584
              
              LD D,A                ; A0 = 0
              ADD A,#01
              LD E,A                ; A0 = 1

              CALL IOEN             ; Switched IO enabled

              CALL I2CBCTRL         ; Test, if I2C is busy

              LD A,D                ; I2C slave address "Write"
              OUT (ALE),A
              LD A,C                ; I2C slave addr
              OUT (SWIO),A
        
              LD A,E                ; Send START on I2C
              OUT (ALE),A
              LD A,#0C5
              OUT (SWIO),A

              CALL I2CPCTRL         ; Test PIN Bit
                                    ; A contains bit field

              BIT 3,A               ; Test LRB Bit

              JR NZ,CORE7STOP       ; 0 = Slave Acknowledged
                                    ; 1 = No slave present, stop output

              LD A,D                
              OUT (ALE),A
              LD A,B                ; Get byte from buffer
              OUT (SWIO),A          ; Send data on I2C

              CALL I2CPCTRL         ; Test PIN Bit

CORE7STOP:    LD A,E                ; Yes? Send STOP on I2C
              OUT (ALE),A
              LD A,#0C3
              OUT (SWIO),A

              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C WRB(BUS,CHIP,LEN,DATA)  *
                                    ;************************************ 
                                    ;* Write block data to PCF8584      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: I2C CHIP                  *
                                    ;* PARA3: I2C LEN                   *
                                    ;* PARA4: I2C DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE8:      CALL RCXINIT          ; Module installed?
              JP Z,B_CORE8START     ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE8START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              PUSH AF               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              LD C,A                ; Save IO address
              PUSH BC               ; Save IO address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
               
              POP BC                
              LD B,A                ; Save I2C Len
                                    ; BC contains I2C address & I2C len 
              PUSH BC               ; Save I2C address & I2C len on stack
  
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get I2C address & I2C len on stack
              POP AF                ; Get BUS address from stack

              PUSH BC               ; Save I2C address and data to stack
              LD B,SI2C             ; Set type SI2C
              LD C,A                ; Set BUS address
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              LD A,C                ; Get BUS address back
              POP BC                ; Get I2C address and data from stack
              JP C,CORE8            ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 8                    *
                                    ;************************************ 
                                    ;* Write block data to PCF8584      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE8:     CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE8SLT      ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE8SLT:  LD A,H                ; Get BUS address
              PUSH BC               ; Save I2C address and data to stack
              LD B,SI2C             ; Set type SI2C
              LD C,A                ; Set BUS address
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C address and data from stack
              LD A,C                ; Get BUS address back
              JP C,CORE8            ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* I2C WRB(BUS,CHIP,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Write block data to PCF8584      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE8:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              LD D,A                ; A0 = 0
              ADD A,#01
              LD E,A                ; A0 = 1

              CALL IOEN             ; Switched IO enabled

              CALL I2CBCTRL         ; Test, if I2C is busy

              LD A,D                ; I2C slave address "Write"
              OUT (ALE),A
              LD A,C                ; I2C slave addr
              OUT (SWIO),A
        
              LD A,E                ; Send START on I2C
              OUT (ALE),A
              LD A,#0C5
              OUT (SWIO),A

CORE8LOOP:    CALL I2CPCTRL         ; Test PIN Bit
                                    ; A contains bit field

              BIT 3,A               ; Test LRB Bit

              JR NZ,CORE8STOP       ; 0 = Slave Acknowledged
                                    ; 1 = No slave present, stop output

              LD A,B                ; Buffer complete transmitted?
              CP #00                ; then stop output
              JR Z,CORE8STOP
              SUB #01               ; B = B - 1
              LD B,A
                                    
              LD A,D                
              OUT (ALE),A
              LD A,(HL)             ; Get byte from buffer
              OUT (SWIO),A          ; Send data on I2C

              INC HL                ; Increment data pointer
                                    ; Only lower byte of INT is send
              INC HL                ; Increment data pointer
                                    ; HL points to next INT in buffer
  
              JR CORE8LOOP          ; Loop for next data

CORE8STOP:    LD A,E                ; Yes? Send STOP on I2C
              OUT (ALE),A
              LD A,#0C3
              OUT (SWIO),A

              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C RD(BUS,CHIP,DATA)       *
                                    ;************************************ 
                                    ;* Read data from PCF8584           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: I2C CHIP                  *
                                    ;* PARA3: I2C DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* B = I2C CHIP                     *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE9:      CALL RCXINIT          ; Module installed?
              JP Z,B_CORE9START     ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE9START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR

              LD C,A                ; C contains bus address
              PUSH BC               ; Save bus address on stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1
              CALL EVALBYTEVAR
              
              POP BC                ; Get bus and chip address from stack
              LD B,A                ; B contains chip address
              PUSH BC               ; Save bus and chip address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,2                ; Third PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                            
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; BC contains Bus IO and chip address

              PUSH BC               ; Save bus and chip address to stack
              LD B,SI2C             ; Set type SI2C
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get bus and chip address from stack

              JP C,CORE9            ; JP if slot found

              JP SLTERR             ; No suitable slot found



                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 9                    *
                                    ;************************************ 
                                    ;* Read data from PCF8584           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* B = I2C CHIP                     *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* B = I2C CHIP                     *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE9:     CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE9SLT      ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE9SLT:  PUSH BC
              LD B,SI2C             ; Set type SI2C 
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC
              JP C,CORE9            ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* I2C RD(BUS,CHIP,DATA)            *
                                    ;************************************ 
                                    ;* Read data from PCF8584           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* B = I2C CHIP                     *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = I2C DATA                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE9:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              LD A,C                ; Get IO address of PCF8584
              
              LD D,A                ; A0 = 0
              ADD A,#01
              LD E,A                ; A0 = 1

              CALL IOEN             ; Switched IO enabled

              LD A,D                ; I2C slave address 
              OUT (ALE),A
              LD A,B                ; I2C slave addr
              ADD A,#01             ; I2C slave addr read
              OUT (SWIO),A
        
              CALL I2CBCTRL         ; Test, if I2C is busy
        
              LD A,E                ; Send START on I2C
              OUT (ALE),A
              LD A,#0C5
              OUT (SWIO),A

              CALL I2CPCTRL         ; Test PIN Bit
                                    ; A contains bit field

              BIT 3,A               ; Test LRB Bit

              JR NZ,CORE9STOP       ; 0 = Slave Acknowledged
                                    ; 1 = No slave present, stop output

              LD A,E                ; Set ACK bit S1 to 0 in preparation
              OUT (ALE),A           ; for negative acknowledgement
              LD A,#040
              OUT (SWIO),A              

              LD A,D                ; Read I2C data
              OUT (ALE),A           
              IN A,(SWIO)           ; A contains dummy data
                       
              CALL I2CPCTRL         ; Test PIN Bit

CORE9STOP:    LD A,E                ; Yes? Send STOP on I2C
              OUT (ALE),A
              LD A,#0C3
              OUT (SWIO),A

              LD A,D                ; I2C read
              OUT (ALE),A           
              IN A,(SWIO)   
              
              LD (HL),A             ; Save data to varptr(I2C DATA)
              INC HL
              LD A,#00              ; Set higher byte of buffer to 0
              LD (HL),A

              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C RDB(BUS,CHIP,LEN,DATA)  *
                                    ;************************************ 
                                    ;* Read block data from PCF8584     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: I2C CHIP                  *
                                    ;* PARA3: I2C LEN                   *
                                    ;* PARA4: I2C DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE10:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE10ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE10ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              PUSH AF               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              LD C,A                ; Save IO address
              PUSH BC               ; Save IO address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
               
              POP BC                
              LD B,A                ; Save I2C Len
                                    ; BC contains I2C address & I2C len 
              PUSH BC               ; Save I2C address & I2C len on stack
  
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get I2C address & I2C len on stack
              POP AF                ; Get BUS address from stack

              PUSH BC               ; Save I2C address and data to stack
              LD B,SI2C             ; Set type SI2C
              LD C,A                ; Set BUS address
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              LD A,C                ; Get BUS address back
              POP BC                ; Get I2C address and data from stack
              JP C,CORE10           ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 10                   *
                                    ;************************************ 
                                    ;* Read block data from PCF8584     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE10:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE10SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE10SLT: LD A,H                ; Get BUS address
              PUSH BC               ; Save I2C address and data to stack
              LD B,SI2C             ; Set type SI2C
              LD C,A                ; Set BUS address
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C address and data from stack
              LD A,C                ; Get BUS address back
              JP C,CORE10           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* I2C RDB(BUS,CHIP,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Read block data from PCF8584     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = BUS ADDR                     *
                                    ;* C = I2C CHIP                     *
                                    ;* B = I2C LEN                      *
                                    ;* DE = varptr(I2C DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE10:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH DE               ; Save varptr on stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI
            
              LD D,A                ; A0 = 0
              ADD A,#01
              LD E,A                ; A0 = 1

              CALL IOEN             ; Switched IO enabled

              LD A,D                ; I2C slave address 
              OUT (ALE),A
              LD A,C                ; I2C slave addr
              ADD A,#01             ; I2C slave addr read
              OUT (SWIO),A
        
              CALL I2CBCTRL         ; Test, if I2C is busy
        
              LD A,E                ; Send START on I2C
              OUT (ALE),A
              LD A,#0C5
              OUT (SWIO),A

              LD C,B                ; Save LEN to C

CORE10LOOP:   CALL I2CPCTRL         ; Test PIN Bit
                                    ; A contains bit field

              BIT 3,A               ; Test LRB Bit

              JR NZ,CORE10STOP      ; 0 = Slave Acknowledged
                                    ; 1 = No slave present, stop output

              LD A,D                ; Read I2C data
              OUT (ALE),A           
              IN A,(SWIO)           ; A contains dummy data at first time
              LD (HL),A             ; Set lower byte of buffer
              
              LD A,B                ; First read? Discard data
              SUB C
              JP Z,CORE10GO
                            
              INC HL
              LD A,#00              ; Set higher byte of buffer to 0
              LD (HL),A
              INC HL                ; HL points to next INT in buffer

CORE10GO:     DJNZ CORE10LOOP

CORE10END:    LD A,E                ; Set ACK bit S1 to 0 in preparation
              OUT (ALE),A           ; for negative acknowledgement
              LD A,#040
              OUT (SWIO),A              

              LD A,D                ; Read I2C data
              OUT (ALE),A           
              IN A,(SWIO)           ; A contains dummy data
                       
              CALL I2CPCTRL         ; Test PIN Bit

CORE10STOP:   LD A,E                ; Yes? Send STOP on I2C
              OUT (ALE),A
              LD A,#0C3
              OUT (SWIO),A

              LD A,D                ; I2C read
              OUT (ALE),A           
              IN A,(SWIO)   
              
              LD (HL),A             ; Save data to varptr(I2C DATA)
              INC HL
              LD A,#00
              LD (HL),A             ; Set higher byte of buffer to 0

              CALL IODIS            ; Switched IO disabled

              EI

              POP DE                ; Get DE back from stack

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL I2C RESET(BUS)              *
                                    ;************************************ 
                                    ;* Resets I2C BUS at PCF8584        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE11:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE11ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE11ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR
              LD C,A
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              LD A,C                ; Get BUS address back
              POP BC                ; Get I2C address and data from stack
              JP C,CORE11           ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 11                   *
                                    ;************************************ 
                                    ;* Resets I2C BUS at PCF8584        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE11:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE11SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE11SLT: PUSH BC
              LD B,SI2C             ; Set type SI2C
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC
              JP C,CORE11           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* I2C RESET(BUS)                   *
                                    ;************************************ 
                                    ;* Resets I2C BUS at PCF8584        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE11:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              DI

              LD D,C                ; A0 = 0
              ADD A,#01
              LD E,C                ; A0 = 1

              CALL IOEN             ; Switched IO enabled

              LD A,E                ; Send STOP #0C2 on I2C
              OUT (ALE),A
              LD A,#0C2
              OUT (SWIO),A

              LD B,1
              CALL DELAY            ; Wait

              LD A,E                ; Send CLEAR #0C1 on I2C
              OUT (ALE),A
              LD A,#0C1
              OUT (SWIO),A

              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET                    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* SET TIME (BUS,TIME)              *
                                    ;************************************ 
                                    ;* Write TIME to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: TIME                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE12:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE12ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE12ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A
              PUSH BC               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA
              CALL EVALSTR
              CP #08                ; Len String <> 8 Error
              JP NZ,B_CORE12ERR

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get bus address from stack

              PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack

              JP C,CORE12           ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE12ERR:  POP BC                ; Get bus address from stack

              PUSH HL
              LD HL,PERRTYPESL
              CALL PROMPT
              POP HL
              
              JP B_ERRTYPE

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 12                   *
                                    ;************************************ 
                                    ;* Write TIME to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE12:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE12SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE12SLT: PUSH BC               ; Save BC to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get BC from stack
              JP C,CORE12           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SET TIME (BUS,TIME)              *
                                    ;************************************ 
                                    ;* Write TIME to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE12:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH BC               ; Save bus address on stack
              PUSH DE               ; Save varptr on stack

                                    ; varptr points to 16 bit
                                    ; I2C only send lower 8 bit
                                    ; use only each 2. byte
          
              LD (IX),#00           ; Control/Status register addr 0
              LD (IX+2),#00         ; Addr 0: set flags
              LD (IX+4),#00         ; Addr 1: set hundrets of seconds

              CALL CORE12BCD        ; Convert 2x ASC to 2x BCD
              LD (IX+10),A          ; Addr 4: set hours

              INC DE

              CALL CORE12BCD        ; Convert 2x ASC to 2x BCD
              LD (IX+8),A           ; Addr 3: set minutes

              INC DE

              CALL CORE12BCD        ; Convert 2x ASC to 2x BCD
              LD (IX+6),A           ; Addr 2: set seconds
                                
              LD A,C                ; A = BUS ADDR                     
              LD C,#0A0             ; C = PCF8583                     
              LD B,#06              ; B = I2C LEN 
                                   
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = array pointer            
              
              CALL CORE8            ; I2C WRITE BLOCK

              POP DE                ; Get varptr from stack
              POP BC                ; Get bus address from stack
                                 
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET   

CORE12BCD:    LD A,(DE)
              SUB #030
              SLA A
              SLA A
              SLA A
              SLA A
              LD B,A
              INC DE
              LD A,(DE)
              SUB #030
              OR B
              INC DE
              
              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* GET TIME (BUS,TIME)              *
                                    ;************************************ 
                                    ;* Read TIME from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: TIME                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE13:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE13ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE13ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A
              PUSH BC               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA
              CALL EVALSTR
              CP #08                ; Len String <> 8 Error
              JP NZ,CORE13ERR

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get bus address from stack
              PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORE13           ; JP if slot found

              JP SLTERR             ; No suitable slot found

CORE13ERR:    POP BC                ; Get bus address from stack

              PUSH HL
              LD HL,PERRTYPESL
              CALL PROMPT
              POP HL
              
              JP B_ERRTYPE

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 13                   *
                                    ;************************************ 
                                    ;* Read TIME from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE13:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE13SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE13SLT: PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORE13           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* GET TIME (BUS,TIME)              *
                                    ;************************************ 
                                    ;* Read TIME from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(TIME)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = TIME                      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE13:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH BC               ; Save bus address on stack
              PUSH DE               ; Save varptr on stack

              LD E,C                ; E = BUS ADDR        
              LD C,#0A0             ; C = PCF8583   
              LD B,#02              ; B = Register ADDR 2

              CALL CORE7            ; I2C WRITE DATA

              POP DE                ; Get varptr from stack
              POP BC                ; Get bus address from stack

              PUSH BC               ; Save bus address on stack
              PUSH DE               ; Save varptr on stack
                               
              LD A,C                ; A = BUS ADDR                     
              LD C,#0A0             ; C = PCF8583                    
              LD B,#03              ; B = I2C LEN   
              
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = array pointer                    
              
              CALL CORE10           ; I2C READ BLOCK

              PUSH DE               ; Save array pointer on stack
              POP IX                ; IX = array pointer
   
              POP DE                ; Get varptr from stack
              POP BC                ; Get bus address from stack

              LD A,(IX+4)           ; Convert hours
              AND A,#03F
              CALL CORE13BCD

              LD A,':'
              LD (DE),A
              INC DE

              LD A,(IX+2)           ; Convert minutes
              CALL CORE13BCD

              LD A,':'
              LD (DE),A
              INC DE

              LD A,(IX)             ; Convert seconds
              CALL CORE13BCD
                                                                                      
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET   

CORE13BCD:    LD B,A
              AND #0F0
              SRA A
              SRA A
              SRA A
              SRA A
              ADD A,#030
              LD (DE),A
              LD A,B
              AND #0F
              ADD A,#030
              INC DE
              LD (DE),A
              INC DE

              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* SET DATE (BUS,DATE)              *
                                    ;************************************ 
                                    ;* Write DATE to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: DATE                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATA)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE14:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE14ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE14ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A
              PUSH BC               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA
              CALL EVALSTR
              CP #08                ; Len String <> 8 Error
              JP NZ,CORE14ERR

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get bus address from stack

              PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack

              JP C,CORE14           ; JP if slot found

              JP SLTERR             ; No suitable slot found

CORE14ERR:    POP BC                ; Get bus address from stack

              PUSH HL
              LD HL,PERRTYPESL
              CALL PROMPT
              POP HL
              
              JP B_ERRTYPE

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 14                   *
                                    ;************************************ 
                                    ;* Write DATE to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE14:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE14SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE14SLT: PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORE14           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SET DATE (BUS,DATE)              *
                                    ;************************************ 
                                    ;* Write DATE to PFC8583            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE14:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH BC               ; Save bus address on stack
              PUSH DE               ; Save varptr on stack

                                    ; varptr points to 16 bit
                                    ; I2C only send lower 8 bit
                                    ; use only each 2. byte
          
              LD (IX),#05           ; Control/Status register addr 5
              
              CALL CORE14BCD        ; Convert 2x ASC to 2x BCD
              LD (IX+2),A           ; Addr 5: set year/date

              INC DE                ; Skip /

              CALL CORE14BCD        ; Convert 2x ASC to 2x BCD
              LD (IX+4),A           ; Addr 6: set month

              INC DE                ; Skip /

              PUSH BC               ; Save bus address on stack
              PUSH DE               ; Save string pointer on stack

              LD A,C                ; A = BUS ADDR                     
              LD C,#0A0             ; C = PCF8583                    
              LD B,#03              ; B = I2C LEN 
                                   
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = array pointer            
              
              CALL CORE8            ; I2C WRITE BLOCK

              POP DE                ; Get string pointer from stack
              POP BC                ; Get bus address on stack

              LD (IX),#010          ; Control/Status register addr 16

              CALL CORE14BCD
              LD (IX+2),A           ; Save year in free RAM on PCF8583

              LD A,C                ; A = BUS ADDR                     
              LD C,#0A0             ; C = PCF8583                     
              LD B,#02              ; B = I2C LEN 
                                   
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = array pointer            
              
              CALL CORE8            ; I2C WRITE BLOCK

              POP DE                ; Get string pointer from stack
              POP BC                ; Get bus address on stack
                                
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET   

CORE14BCD:    LD A,(DE)
              SUB #030
              SLA A
              SLA A
              SLA A
              SLA A
              LD B,A
              INC DE
              LD A,(DE)
              SUB #030
              OR B
              INC DE
              
              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* GET DATE (BUS,DATE)              *
                                    ;************************************ 
                                    ;* Read DATE from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: BUS ADDR                  *
                                    ;* PARA2: DATE                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE15:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE15ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE15ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A
              PUSH BC               ; Save BUS address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA
              CALL EVALSTR
              CP #08                ; Len String <> 8 Error
              JP NZ,CORE15ERR

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get bus address from stack
              PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORE15           ; JP if slot found

              JP SLTERR             ; No suitable slot found

CORE15ERR:    POP BC                ; Get bus address from stack

              PUSH HL
              LD HL,PERRTYPESL
              CALL PROMPT
              POP HL
              
              JP B_ERRTYPE

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 15                   *
                                    ;************************************ 
                                    ;* Read DATE from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE15:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE15SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE15SLT: PUSH BC               ; Save bus address to stack
              LD B,SI2C             ; Set type SI2C
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORE15           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* GET DATE (BUS,DATE)              *
                                    ;************************************ 
                                    ;* Read DATE from PFC8583           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = BUS ADDR                     *
                                    ;* DE = varptr(DATE)                *
                                    ;* IX = card memory                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = DATE                      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE15:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH DE               ; Save varptr on stack
              PUSH BC               ; Save bus address on stack
                                                          
              LD E,C                ; E = BUS ADDR        
              LD C,#0A0             ; C = PCF8583
              LD B,#05              ; B = Register ADDR 5

              CALL CORE7            ; I2C WRITE DATA

              POP BC                ; Get bus address from stack
              PUSH BC               ; Save bus address on stack
                               
              LD A,C                ; A = BUS ADDR                     
              LD C,#0A0             ; C = PCF8583                    
              LD B,#02              ; B = I2C LEN   
              
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = array pointer                    
              
              CALL CORE10           ; I2C READ BLOCK

              PUSH DE               ; Save array pointer on stack
              POP IX                ; IX = array pointer
   
              POP BC                ; Get bus address from stack
              POP DE                ; Get varptr from stack
              
              LD A,(IX)             ; Convert day
              AND A,#0C0
              LD B,A                ; Save additional year

              SRA B                 ; B contains additional year
              SRA B
              SRA B
              SRA B
              SRA B
              SRA B

              PUSH BC               ; Save year & bus address on stack

              LD A,(IX)
              AND A,#03F
              CALL CORE15BCD        ; Write day to string

              LD A,'/'              ; Write '/' to string
              LD (DE),A
              INC DE

              LD A,(IX+2)           ; Convert month
              AND A,#01F
              CALL CORE15BCD        ; Write month to string

              LD A,'/'              ; Write '/' to string
              LD (DE),A
              INC DE

              POP BC                ; Get year & bus address from stack

              PUSH BC               ; Save year & bus address on stack
              PUSH DE               ; Save varptr on stack

              LD E,C                ; E = BUS ADDR        
              LD C,#0A0             ; C = PCF8583   
              LD B,#010             ; B = Register ADDR 16

              CALL CORE7            ; I2C WRITE DATA

              POP DE                ; Get varptr from stack
              POP BC                ; Get year & bus address from stack

              PUSH BC               ; Save year & bus address on stack
              PUSH DE               ; Save varptr on stack
              
              LD B,#0A0             ; B = PCF8583 
              
              PUSH IX               ; Save array pointer on stack
              POP DE                ; DE = varptr(I2C DATA)  
              
              CALL CORE9            ; I2C READ
  
              POP DE                ; Get varptr from stack
              POP BC                ; Get year & bus address from stack

              LD C,B                ; C contains additional year

              LD A,(IX)             ; Evaluate year from BCD
              AND #0F
              ADD A,C               ; Calculate year with additional year
              LD C,A
              SUB #0A
              JP NC,CORE15CARRY     ; new year >10
              LD A,(IX)             ; Evaluate year from BCD without Carry
              AND #0F0
              OR C
              CALL CORE15BCD        ; Save year to string
              JP CORE15END

CORE15CARRY:  LD C,A                ; Evaluate year from BCD without Carry
              LD A,(IX)             
              AND #0F0
              ADD A,#010
              OR C
              CALL CORE15BCD        ; Save year to string
                                                                                      
CORE15END:    POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET   

CORE15BCD:    LD B,A
              AND #0F0
              SRA A
              SRA A
              SRA A
              SRA A
              ADD A,#030
              LD (DE),A
              LD A,B
              AND #0F
              ADD A,#030
              INC DE
              LD (DE),A
              INC DE

              RET

;---------------------------
  
                                    ;************************************
                                    ;* I2C-Bus Busy                     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* E = IO address of PCF8584        *
                                    ;* A = STATUS-FLAGS                 *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E = IO address of PCF8584        *
                                    ;* A = STATUS-FLAGS                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

I2CBCTRL:     PUSH BC               ; Save BC on stack
              LD B,#0FF             ; Wait cycles
              LD A,E                ; Test, if bus is ready (Bit 0 = 1)
              OUT (ALE),A           ; Write IO address to Latch
BTESTCTRLL:   IN A,(SWIO)           ; Get data from IO port
              BIT 0,A
              JR NZ,BTESTOK         ; Not Busy? Then go on
              DJNZ BTESTCTRLL

              CALL IODIS            ; Switched IO disabled

              EI

              POP BC                ; Clear stack

              LD HL,PERRI2CBUS1     ; I2C-Bus Error BUSY
              CALL PROMPT

              POP HL                ; Get BASIC Pointer back from stack
              
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS
              
BTESTOK:      POP BC                ; Get BC back from stack

              RET

;---------------------------

                                    ;************************************
                                    ;* I2C-Bus PIN Bit                  *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* E = IO address of PCF8584        *
                                    ;* A = STATUS-FLAGS                 *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E = IO address of PCF8584        *
                                    ;* A = STATUS-FLAGS                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

I2CPCTRL:     PUSH BC               ; Save BC on stack
              LD B,#0FF             ; Wait cycles
              LD A,E                ; Test, if transmission completed
              OUT (ALE),A           ; (Bit 7 = 0)
PTESTCTRLL:   IN A,(SWIO)           
              BIT 7,A
              JR Z,PTESTOK
              DJNZ PTESTCTRLL

              CALL IODIS            ; Switched IO disabled

              EI

              POP BC                ; Clear stack

              LD HL,PERRI2CBUS2     ; I2C-Bus Error Cmd not executable
              CALL PROMPT

              POP HL                ; Get BASIC Pointer back from stack
              
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

PTESTOK:      POP BC                ; Get BC back from stack

              RET

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type B                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with Z80 PIO                                         +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL PIO INIT(ADDR)              *
                                    ;************************************ 
                                    ;* Initialise Z80 PIO               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: PIO addr                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PIO addr                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE16:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE16ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE16ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr from stack 

              LD B,SPIO             ; Set type SPIO 
              
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,SLTFREEERR      ; No free slot found

              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to BASIC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 16                   *
                                    ;************************************ 
                                    ;* Initialise Z80 PIO               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PIO addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PIO addr                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE16:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE16SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE16SLT: PUSH BC               ; Save BC to stack
              LD B,SPIO             ; Set type SPIO 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE16OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE16OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------


                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO A RD (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Get Byte from Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE17:     CALL RCXINIT          ; Module installed?
              LD B,0                ; Z80 PIO A RD
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO A WR (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 1 PIO A WR                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE18:     CALL RCXINIT          ; Module installed?
              LD B,1                ; Z80 PIO A WR
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO B RD (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Get Byte from Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 2 PIO B RD                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE19:     CALL RCXINIT          ; Module installed?
              LD B,2                ; Z80 PIO B RD
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO B WR (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 3 PIO B WR                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE20:     CALL RCXINIT          ; Module installed?
              LD B,3                ; Z80 PIO B WR
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO A CTRL (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO CTRL A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 4 PIO A CTRL                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE21:     CALL RCXINIT          ; Module installed?
              LD B,4                ; Z80 PIO A CTRL
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO B CTRL (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO CTRL B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 5 PIO B CTRL                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE22:     CALL RCXINIT          ; Module installed?
              LD B,5                ; Z80 PIO B CTRL
              JP Z,B_COREPIO        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PIO xxx (ADDR,VALUE)             *
                                    ;************************************ 
                                    ;* Check parameters                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0..5 for functions           *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0..5 for functions           *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_COREPIO:    PUSH BC               ; Save PIO-Mode on stack

              CALL CHKCHAR
              DEFB "("              ; Check for (
             
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              POP BC                ; Get PIO-Mode from stack
              LD C,A
              PUSH BC               ; Save PIO mode & addr on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              POP BC                ; Get PIO mode & addr from stack
              PUSH BC               ; Save PIO mode & addr on stack

              LD A,B                ; Check if RD or WR
              LD B,1                ; Second PARA

              CP 0
              JP Z,B_COREPIOV       ; RD = hand over pointer
              CP 2
              JP Z,B_COREPIOV       ; RD = hand over pointer
        
              CALL EVALBYTEVAR      ; WR = hand over data from Byte or Var 
              LD D,A                ; D contains Byte to write

              JP B_COREPIOW
              
B_COREPIOV:   CALL EVALVARINT       ; DE contains VARPTR to read data
              
B_COREPIOW:   CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get PIO mode & addr from stack

              PUSH BC               ; Save PIO mode & addr on stack
              LD B,SPIO             ; Set type SPIO
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get PIO mode & addr from stack

              JP C,COREPIO          ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 17                   *
                                    ;************************************ 
                                    ;* Get Byte from Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE17:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 18                   *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO PORT A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 PIO A WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 1 PIO A WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE18:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 19                   *
                                    ;************************************ 
                                    ;* Get Byte from Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 PIO B RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 2 PIO B RD                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE19:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 20                   *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO PORT B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 PIO B WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 3 PIO B WR                   *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE20:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 21                   *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO CTRL A     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 PIO A CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 4 PIO A CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE21:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 22                   *
                                    ;************************************ 
                                    ;* Write Byte to Z80 PIO CTRL B     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 PIO B CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = 5 PIO B CTRL                 *
                                    ;* C = PIO BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE22:    JP FN_COREPIO

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;************************************ 
                                    ;* Do activity with Z80 PIO         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPIOSLT         *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
              
FN_COREPIO:   CALL RCXINIT          ; Module installed?
              JP Z,FN_COREPIOST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_COREPIOST: PUSH BC               ; Save bus address to stack
              LD B,SPIO             ; Set type SPIO
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,COREPIO          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* PIO xxx (ADDR,VALUE)             *
                                    ;************************************ 
                                    ;* Do activity with Z80 PIO         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PIO MODE                     *
                                    ;* C = PIO BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COREPIO:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI
            
              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, COREPIOAPR      ; Go to PIO A PORT RD
              CP 1
              JP Z, COREPIOAPW      ; Go to PIO A PORT WR
              CP 2
              JP Z, COREPIOBPR      ; Go to PIO B PORT RD
              CP 3
              JP Z, COREPIOBPW      ; Go to PIO B PORT WR
              CP 4
              JP Z, COREPIOACW      ; Go to PIO A CTRL
              CP 5
              JP Z, COREPIOBCW      ; Go to PIO B CTRL
              JP COREPIOEND

COREPIOAPR:   LD A,C                ; PIO A PORT RD
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)

              JP COREPIOEND

COREPIOAPW:   LD A,C                ; PIO A PORT WR
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOBPR:   LD A,C                ; PIO B PORT RD
              ADD A,#01
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)

              JP COREPIOEND

COREPIOBPW:   LD A,C                ; PIO B PORT WR
              ADD A,#01
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOACW:   LD A,C                ; PIO A CTRL WR
              ADD A,#02
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPIOEND

COREPIOBCW:   LD A,C                ; PIO B CTRL WR
              ADD A,#03
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
                                 
COREPIOEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type C                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with 82C55                                           +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL PPI INIT(ADDR)              *
                                    ;************************************ 
                                    ;* Initialise 82C55                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: PPI addr                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PPI addr                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE23:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE23ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE23ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr from stack 

              LD B,SPPI             ; Set type SPPI 
              
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,SLTFREEERR      ; No free slot found

              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to BASIC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 23                   *
                                    ;************************************ 
                                    ;* Initialise 82C55                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = PPI addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = PPI addr                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE23:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE23SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE23SLT: PUSH BC               ; Save BC to stack
              LD B,SPPI             ; Set type SPPI 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Save BC to stack
              JP C,FN_CORE23OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE23OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine



;---------------------------


                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI A RD (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Get Byte from 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0 PPI A RD                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE24:     CALL RCXINIT          ; Module installed?
              LD B,0                ; 82C55 A RD
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI A WR (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 1 PPI A WR                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE25:     CALL RCXINIT          ; Module installed?
              LD B,1                ; 82C55 A WR
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI B RD (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Read Byte from 82C55 PORT B      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 2 PPI B RD                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE26:     CALL RCXINIT          ; Module installed?
              LD B,2                ; 82C55 B RD
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI B WR (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT B       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 3 PPI B WR                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE27:     CALL RCXINIT          ; Module installed?
              LD B,3                ; 82C55 B WR
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI C RD (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Get Byte from 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 4 PPI C RD                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE28:     CALL RCXINIT          ; Module installed?
              LD B,4                ; 82C55 C RD
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI C WR (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 5 PPI C WR                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE29:     CALL RCXINIT          ; Module installed?
              LD B,5                ; 82C55 C WR
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI CTRL (ADDR,VALUE)            *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 6 PPI CTRL                   *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE30:     CALL RCXINIT          ; Module installed?
              LD B,6                ; 82C55 CTRL
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI SET (ADDR,VALUE)             *
                                    ;************************************ 
                                    ;* Set Bit PORT C 82C55             *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 7 PPI SET BIT                *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE31:     CALL RCXINIT          ; Module installed?
              LD B,7                ; 82C55 SET BIT
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI RESET (ADDR,VALUE)           *
                                    ;************************************ 
                                    ;* Reset Bit PORT C 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE32:     CALL RCXINIT          ; Module installed?
              LD B,8                ; 82C55 RESET BIT
              JP Z,B_COREPPI        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* PPI xxx (ADDR,VALUE)             *
                                    ;************************************ 
                                    ;* Check parameters                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0..8 for functions           *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0..8 for functions           *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_COREPPI:    PUSH BC               ; Save PIO-Mode on stack

              CALL CHKCHAR
              DEFB "("              ; Check for (
             
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              POP BC                ; Get PPI-Mode from stack
              LD C,A
              PUSH BC               ; Save PPI mode & addr on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              POP BC                ; Get PPI mode & addr from stack
              PUSH BC               ; Save PPI mode & addr on stack

              LD A,B                ; Check if RD or WR
              LD B,1                ; Second PARA

              CP 0
              JP Z,B_COREPPIV       ; RD = hand over pointer
              CP 2
              JP Z,B_COREPPIV       ; RD = hand over pointer
              CP 4
              JP Z,B_COREPPIV       ; RD = hand over pointer

              CALL EVALBYTEVAR      ; WR = hand over data from Byte or Var 
              LD D,A                ; D contains Byte to write

              JP B_COREPPIW
              
B_COREPPIV:   CALL EVALVARINT       ; DE contains VARPTR to read data
              
B_COREPPIW:   CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get PPI mode & addr from stack

              PUSH BC               ; Save PPI mode & addr on stack
              LD B,SPPI             ; Set type SPPI
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get PPI mode & addr from stack

              JP C,COREPPI          ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 24                   *
                                    ;************************************ 
                                    ;* Get Byte from 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 PPI A RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 0 PIO A RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE24:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 25                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT A       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 PPI A WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 1 PPI A WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE25:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 26                   *
                                    ;************************************ 
                                    ;* Read Byte from 82C55 PORT B      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 PPI B RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 2 PPI B RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE26:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 27                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT B       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 PPI B WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 3 PPI B WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE27:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 28                   *
                                    ;************************************ 
                                    ;* Get Byte from 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 PPI C RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 4 PPI C RD                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE28:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 29                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 PORT C       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 PPI C WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 5 PPI C WR                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE29:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 30                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 6 PPI CTRL                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 6 PPI CTRL                   *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE30:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 31                   *
                                    ;************************************ 
                                    ;* Set Bit PORT C 82C55             *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 7 PPI SET BIT                *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 7 PPI SET BIT                *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE31:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 32                   *
                                    ;************************************ 
                                    ;* Reset Bit PORT C 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_COREPPI            *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* C = PPI BASE ADDR                *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE32:    JP FN_COREPPI

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;************************************ 
                                    ;* Do activity with 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT                           *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
              
FN_COREPPI:   CALL RCXINIT          ; Module installed?
              JP Z,FN_COREPPIST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_COREPPIST: PUSH BC               ; Save bus address to stack
              LD B,SPPI             ; Set type SPPI
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,COREPPI          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* PPI xxx (ADDR,VALUE)             *
                                    ;************************************ 
                                    ;* Do activity with 82C55           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = PPI MODE                     *
                                    ;* C = PPI BASE ADDR                *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

COREPPI:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI
            
              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, COREPPIAPR      ; Go to PPI A PORT RD
              CP 1
              JP Z, COREPPIAPW      ; Go to PPI A PORT WR
              CP 2
              JP Z, COREPPIBPR      ; Go to PPI B PORT RD
              CP 3
              JP Z, COREPPIBPW      ; Go to PPI B PORT WR
              CP 4
              JP Z, COREPPICPR      ; Go to PPI C PORT RD
              CP 5
              JP Z, COREPPICPW      ; Go to PPI C PORT WR
              CP 6
              JP Z, COREPPICW       ; Go to PPI CTRL
              CP 7
              JP Z, COREPPISET      ; Go to PPI SET BIT
              CP 8
              JP Z, COREPPIRES      ; Go to PPI RESET BIT
             
              JP COREPPIEND

COREPPIAPR:   LD A,C                ; PPI A PORT RD
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)

              JP COREPPIEND

COREPPIAPW:   LD A,C                ; PPI A PORT WR
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPIBPR:   LD A,C                ; PPI B PORT RD
              ADD A,#01
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)

              JP COREPPIEND

COREPPIBPW:   LD A,C                ; PPI B PORT WR
              ADD A,#01
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPICPR:   LD A,C                ; PPI C PORT RD
              ADD A,#02
              OUT (ALE),A
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)

              JP COREPPIEND

COREPPICPW:   LD A,C                ; PPI C PORT WR
              ADD A,#02
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPICW:    LD A,C                ; PPI CTRL
              ADD A,#03
              OUT (ALE),A
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP COREPPIEND

COREPPISET:   LD A,C                ; PPI SET BIT
              ADD A,#03
              OUT (ALE),A
              LD A,D
              CP #08
              JP NC,COREPPIERR      ; Value > 7? only 0..7 allowed
              SLA A
              ADD A,#01             ; Calculate control word
              OUT (SWIO),A          ; Set Bit according to varptr(VALUE)

              JP COREPPIEND

COREPPIRES:   LD A,C                ; PPI RESET BIT
              ADD A,#03
              OUT (ALE),A
              LD A,D
              CP #08
              JP NC,COREPPIERR      ; Value > 7? only 0..7 allowed
              SLA A                 ; Calculate control word
              OUT (SWIO),A          ; Reset Bit according to varptr(VALUE)
                                 
COREPPIEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET  
              
COREPPIERR:   LD HL,PERRTYPE
              CALL PROMPT
              LD A,#02
              CALL CHPUT
              LD HL,PERRVAL2
              CALL PROMPT

              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              JP B_ERRTYPE               

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type D                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with 82C54                                           +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL TIMER INIT(ADDR)            *
                                    ;************************************ 
                                    ;* Initialise 82C54                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: TIMER addr                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = TIMER addr                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE33:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE33ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE33ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr from stack 

              LD B,SCNT             ; Set type SCNT 
              
              PUSH BC
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC

              JP NC,SLTFREEERR      ; No free slot found

              DI
            
              CALL IOEN             ; Switched IO enabled

              LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#030
              OUT (SWIO),A          ; TIMER 0 MODE 0
              LD A,#070
              OUT (SWIO),A          ; TIMER 1 MODE 0
              LD A,#0B0
              OUT (SWIO),A          ; TIMER 2 MODE 0
              LD A,C
              ADD A,#04
              OUT (ALE),A
              LD A,#00
              OUT (SWIO),A          ; CLEAR GATE

              CALL IODIS            ; Switched IO disabled

              EI

              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to BASIC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 33                   *
                                    ;************************************ 
                                    ;* Initialise 82C54                 *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = TIMER addr                   *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = TIMER addr                   *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE33:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE33SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE33SLT: PUSH BC               ; Save BC to stack
              LD B,SCNT             ; Set type SCNT 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE33OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE33OK:  DI
            
              CALL IOEN             ; Switched IO enabled

              LD A,C
              ADD A,#03
              OUT (ALE),A
              LD A,#030
              OUT (SWIO),A          ; TIMER 0 MODE 0
              LD A,#070
              OUT (SWIO),A          ; TIMER 1 MODE 0
              LD A,#0B0
              OUT (SWIO),A          ; TIMER 2 MODE 0
              LD A,C
              ADD A,#04
              OUT (ALE),A
              LD A,#00
              OUT (SWIO),A          ; CLEAR GATE

              CALL IODIS            ; Switched IO disabled

              EI
              CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine



;---------------------------


                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 0 RD (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Get Byte from 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0 TIMER 0 RD                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE34:     CALL RCXINIT          ; Module installed?
              LD B,0                ; 82C54 0 RD
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 0 WR (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 1 TIMER 0 WR                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE35:     CALL RCXINIT          ; Module installed?
              LD B,1                ; 82C54 0 WR
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 1 RD (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Read Byte from 82C54 TIMER 1     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 2 TIMER 1 RD                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE36:     CALL RCXINIT          ; Module installed?
              LD B,2                ; 82C54 1 RD
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 1 WR (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 TIMER 1      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 3 TIMER 1 WR                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE37:     CALL RCXINIT          ; Module installed?
              LD B,3                ; 82C54 1 WR
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 2 RD (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Get Byte from 82C54 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 4 TIMER 2 RD                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE38:     CALL RCXINIT          ; Module installed?
              LD B,4                ; 82C54 2 RD
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER 2 WR (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 5 TIMER 2 WR                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE39:     CALL RCXINIT          ; Module installed?
              LD B,5                ; 82C54 2 WR
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER CTRL (ADDR,VALUE)          *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 6 TIMER CTRL                 *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE40:     CALL RCXINIT          ; Module installed?
              LD B,6                ; 82C54 CTRL
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER SET GATE (ADDR,VALUE)      *
                                    ;************************************ 
                                    ;* Set Bit for GATE 0..2            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 7 TIMER SET GATE             *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE41:     CALL RCXINIT          ; Module installed?
              LD B,7                ; 82C54 SET GATE
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER RESET GATE (ADDR,VALUE)    *
                                    ;************************************ 
                                    ;* Reset Bit for GATE 0..2          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 8 PPI RESET BIT              *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE42:     CALL RCXINIT          ; Module installed?
              LD B,8                ; 82C54 RESET GATE
              JP Z,B_CORETIM        ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* TIMER xxx (ADDR,VALUE)           *
                                    ;************************************ 
                                    ;* Do activity with 82C54           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0..8 for functions           *
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: Value                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* B = 0..8 for functions           *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* DE = INT if WR                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORETIM:    PUSH BC               ; Save TIMER-Mode on stack

              CALL CHKCHAR
              DEFB "("              ; Check for (
             
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              POP BC                ; Get TIMER-Mode from stack
              LD C,A
              PUSH BC               ; Save TIMER mode & addr on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              POP BC                ; Get TIMER mode & addr from stack
              PUSH BC               ; Save TIMER mode & addr on stack

              LD A,B                ; Check if RD or WR
              LD B,1                ; Second PARA

              CP 0
              JP Z,B_CORETIMV       ; RD = hand over pointer
              CP 2
              JP Z,B_CORETIMV       ; RD = hand over pointer
              CP 4
              JP Z,B_CORETIMV       ; RD = hand over pointer

              CALL EVALINTVAR       ; WR = hand over data from INT or Var 
              
              JP B_CORETIMW
              
B_CORETIMV:   CALL EVALVARINT       ; DE contains VARPTR to read data
              
B_CORETIMW:   CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get TIMER mode & addr from stack

              PUSH BC               ; Save TIMER mode & addr on stack
              LD B,SCNT             ; Set type SCNT
              CALL GETRCXSLT        ; Find card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get TIMER mode & addr from stack

              JP C,CORETIM          ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 34                   *
                                    ;************************************ 
                                    ;* Get Byte from 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 0 TIMER 0 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 0 TIMER 0 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE34:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 35                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 TIMER 0      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 1 TIMER 0 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 1 TIMER 0 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE35:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 36                   *
                                    ;************************************ 
                                    ;* Read Byte from 82C54 TIMER 1     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 2 TIMER 1 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 2 TIMER 1 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE36:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 37                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 TIMER 1      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 3 TIMER 1 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 3 TIMER 1 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* D = Byte                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE37:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 38                   *
                                    ;************************************ 
                                    ;* Get Byte from 82C54 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 4 TIMER 2 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 4 TIMER 2 RD                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE38:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 39                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C55 TIMER 2      *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 5 TIMER 2 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 5 TIMER 2 WR                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE39:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 40                   *
                                    ;************************************ 
                                    ;* Write Byte to 82C54 CTRL         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 6 TIMER CTRL                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 6 TIMER CTRL                 *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE40:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 41                   *
                                    ;************************************ 
                                    ;* Set Bit for GATE 0..2            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 7 TIMER SET GATE             *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 7 TIMER SET GATE             *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE41:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 42                   *
                                    ;************************************ 
                                    ;* Reset Bit for GATE 0..2          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = 8 TIMER RESET GATE           *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* OUTPUT via FN_CORETIM            *
                                    ;************************************
                                    ;* B = 8 TIMER RESET GATE           *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = INT                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE42:    JP FN_CORETIM

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;************************************ 
                                    ;* Do activity with 82C54           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* DE = INT if WR                   *
                                    ;************************************
                                    ;* OUTPUT                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* DE = INT if WR                   *
                                    ;* C-Flag not set = ok              *
                                    ;* C-Flag set = error               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
              
FN_CORETIM:   CALL RCXINIT          ; Module installed?
              JP Z,FN_CORETIMST     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine


FN_CORETIMST: PUSH BC               ; Save bus address to stack
              LD B,SCNT             ; Set type SCNT
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP BC                ; Get bus address from stack
              JP C,CORETIM          ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* TIMER xxx (ADDR,VALUE)           *
                                    ;************************************ 
                                    ;* Do activity with 82C54           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = TIMER MODE                   *
                                    ;* C = TIMER BASE ADDR              *
                                    ;* DE = varptr(VALUE) if RD         *
                                    ;* D = Byte if WR                   *
                                    ;* IX = RAM address                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORETIM:      PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI
            
              CALL IOEN             ; Switched IO enabled

              LD A,B                ; Check selected activity
              CP 0
              JP Z, CORETIMAPR      ; Go to TIMER 0 RD
              CP 1
              JP Z, CORETIMAPW      ; Go to TIMER 0 WR
              CP 2
              JP Z, CORETIMBPR      ; Go to TIMER 1 RD
              CP 3
              JP Z, CORETIMBPW      ; Go to TIMER 1 WR
              CP 4
              JP Z, CORETIMCPR      ; Go to TIMER 2 RD
              CP 5
              JP Z, CORETIMCPW      ; Go to TIMER 2 WR
              CP 6
              JP Z, CORETIMCW       ; Go to TIMER CTRL
              CP 7
              JP Z, CORETIMSET      ; Go to TIMER SET GATE
              CP 8
              JP Z, CORETIMRES      ; Go to TIMER RES GATE
             
              JP CORETIMEND

CORETIMAPR:   LD A,C                
              ADD A,#03
              OUT (ALE),A
              LD A,#00
              OUT (SWIO),A          ; Latch Counter 0
              LD A,C
              OUT (ALE),A           ; TIMER 0 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMAPW:   LD A,C                ; TIMER 0 WR
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMBPR:   LD A,C                
              ADD A,#03
              OUT (ALE),A
              LD A,#04
              OUT (SWIO),A          ; Latch Counter 1
              LD A,C
              ADD A,#01
              OUT (ALE),A           ; TIMER 1 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMBPW:   LD A,C                ; TIMER 1 WR
              ADD A,#01
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMCPR:   LD A,C                
              ADD A,#03
              OUT (ALE),A
              LD A,#08
              OUT (SWIO),A          ; Latch Counter 2
              LD A,C
              ADD A,#02
              OUT (ALE),A           ; TIMER 2 RD
              IN A,(SWIO)
              EX DE,HL              ; HL contains varptr(VALUE)
              LD (HL),A             ; Save Byte to varptr(VALUE)
              INC HL
              IN A,(SWIO)
              LD (HL),A

              JP CORETIMEND

CORETIMCPW:   LD A,C                ; TIMER 2 WR
              ADD A,#02
              OUT (ALE),A
              LD A,E
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)
              LD A,D
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMCW:    LD A,C                ; TIMER CTRL
              ADD A,#03
              OUT (ALE),A
             
              LD A,D
              CP 0
              JP NZ,CORETIMERR1
              LD A,E
              
              OUT (SWIO),A          ; Write Byte from varptr(VALUE)

              JP CORETIMEND

CORETIMSET:   LD A,C                ; TIMER SET GATE
              ADD A,#04
              OUT (ALE),A
              LD A,D
              CP 0
              JP NZ,CORETIMERR
              LD A,E
              CP #08
              JP NC,CORETIMERR      ; Value > 7? only 0..7 allowed
              LD D,A                ; Save Value in C
              LD A,(IX)             ; Get old GATE
              OR D                  ; Set Bits in GATE
              LD (IX),A             ; Save new GATE
              OUT (SWIO),A          ; Set Bit according to varptr(VALUE)

              JP CORETIMEND

CORETIMRES:   LD A,C                ; TIMER RESET GATE
              ADD A,#04
              OUT (ALE),A
              LD A,D
              CP 0
              JP NZ,CORETIMERR
              LD A,E
              CP #08
              JP NC,CORETIMERR      ; Value > 7? only 0..7 allowed
              CPL
              AND #07
              LD D,A                ; Save Value in C
              LD A,(IX)             ; Get Bits from RAM
              AND D                 ; Reset Bits in GATE
              LD (IX),A             ; Save new GATE
              OUT (SWIO),A          ; Reset Bit according to varptr(VALUE)

CORETIMEND:   CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
              
              OR A                  ; Clear Carry Flag to end routine handler
              RET  
              
CORETIMERR:   LD HL,PERRTYPE        ; Value > 7
              CALL PROMPT
              LD A,#031
              CALL CHPUT
              LD HL,PERRVAL2
              CALL PROMPT     
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              JP B_ERRTYPE   
              
CORETIMERR1:  LD HL,PERRTYPE        ; Value > 255
              CALL PROMPT
              LD A,#031
              CALL CHPUT
              LD HL,PERRVAL1
              CALL PROMPT     
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              JP B_ERRTYPE                           


;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type E                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with SJA1000                                         +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ; RAM addresses for registers

CANCTRLREG:   EQU #0000             ; Controll register
CANCMDREG:    EQU #0001             ; Command register
CANSTAREG:    EQU #0002             ; Status register
CANINTREG:    EQU #0003             ; Interrupt register
CANIERREG:    EQU #0004             ; Interrupt mask register
CANBUST0REG:  EQU #0006             ; Bustiming 0 register
CANBUST1REG:  EQU #0007             ; Bustiming 1 register
CANOUTCTREG:  EQU #0008             ; Output control register
CANTXBUFREG:  EQU #0010             ; FIFO TX
CANRXBUFREG:  EQU #0010             ; FIFO RX
CANRMC:       EQU #001D             ; Messages to read
CANACCEPTCREG:EQU #0010             ; ACC code, only in reset mode 
CANACCEPTMREG:EQU #0014             ; ACC mask, only in reset mode
CANCLKDIVREG: EQU #001F             ; Clock divider register

                                    ; Bits in CANCTRLREG

CANRES:       EQU #0001             ; Reset Request
CANACCFMODE:  EQU #0008             ; Acceptfilter Mode

                                    ; Bits in CANCMDREG

CANTXREQ:     EQU #0001             ; Transmission request
CANABTX:      EQU #0002             ; Abbort pending TX
CANRELRXBUF:  EQU #0004             ; Release RX Buffer

                                    ; Bits in CANSTAREG

CANTXOK:      EQU #0008             ; Transmission completed

                                    ; Bits in CANIERREG

CANRIE:       EQU #0001             ; receive interrupt  
CANEPIE:      EQU #0020             ; error passive interrupt
CANALIE:      EQU #0040             ; arbitration lost interrupt
CANBEIE:      EQU #0080             ; bus error interrupt

                                    ; Bits in CANOUTCTREG

CANTX1FLOAT:  EQU #0000             ; configured as float
CANTX0PSHPULL:EQU #0018             ; configured as push/pull
CANNORMMODE:  EQU #0002             ; Normal Mode

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN INIT(ADDR,ACC)          *
                                    ;************************************ 
                                    ;* Initialise SJA1000               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: CAN acceptance code&mask  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE43:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE43ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE43ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA

              CALL EVALVARINT       ; DE contains varptr(ACC)

              POP BC                ; Get IO addr back from stack

              DEC DE                ; Check if 1-dim array with 14 bytes
              DEC DE
              DEC DE                ; 1-dim array?
              LD A,(DE)
              CP #01
              JP NZ,B_CORE43ERR1
              DEC DE
              DEC DE
              LD A,(DE)
              CP #013               ; Length array = 8 * 2 + 3 bytes?
              JP NZ,B_CORE43ERR2
              INC DE
              INC DE
              INC DE
              INC DE
              INC DE
              
              PUSH BC               ; Save IO addr to stack
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr back from stack

              PUSH BC               ; Save IO addr to stack

              LD B,SCAN             ; Set type SCAN
                                    ; C contains IO addr

              CALL RCXONCE          ; Check if an SJA1000 was already installed

              POP BC                ; Get IO addr back from stack

              JP C,B_CORE43ERR0     ; JP if already in use

              PUSH BC               ; Save IO addr to stack

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              POP BC                ; Get IO addr back from stack

              JP NC,B_CORE43NF      ; JP if slot not found

B_CORE43ERR0: PUSH HL
              LD HL,PERRSJAINI      ; SJA1000 already installed
              CALL PROMPT
              POP HL
    
              JP B_ERRFUNC

B_CORE43ERR1: PUSH HL
              LD HL,PERRARRAY1

              JP B_CORE43ERR

B_CORE43ERR2: PUSH HL
              LD HL,PERRARRAY2

B_CORE43ERR:  CALL PROMPT
              POP HL

              JP B_ERRFUNC         

                                    ; SJA1000 present?

B_CORE43NF:   PUSH DE               ; Save varptr(ACC) to stack

              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG 
              LD E,CANRES           ; CAN RESET BIT

B_CORE4301:   CALL CANOUT 
              CALL CANINP           ; A contains Value
              AND #01F              ; Mask 0b00011111
              CP CANRES
              JP Z,B_CORE4302       ; Wait until SJA1000 is in RESET MODE
              DJNZ B_CORE4301

              POP DE                ; Clear stack

              PUSH HL
              LD HL,PERRSJA         ; No SJA1000 found
              CALL PROMPT
              POP HL
    
              JP B_ERRFUNC

                                    ; SJA1000 set to RCX table

B_CORE4302:   POP DE                ; Get varptr(ACC) back from stack

              LD B,SCAN             ; Set type SCAN 
              
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Card with BC installed? 
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,CORE43           ; JP if slot found

              JP SLTERR             ; No suitable slot found


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 43                   *
                                    ;************************************ 
                                    ;* Initialise SJA1000               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE43:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE43SLT     ; Installed? Continue with Slot command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 already installed?
              
FN_CORE43SLT: PUSH BC               ; Save BC to stack

              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,FN_CORE43NF     ; JP if slot not found

              POP BC                ; Get BC from stack

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 present?

FN_CORE43NF:  PUSH DE               ; Save varptr(DE) to stack

              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG 
              LD E,CANRES           ; CAN RESET BIT

FN_CORE4301:  CALL CANOUT 
              CALL CANINP           ; A contains Value
              AND #01F              ; Mask 0b00011111
              CP CANRES
              JP Z,FN_CORE4302      ; Wait until SJA1000 is in RESET MODE
              DJNZ FN_CORE4301

              POP DE                ; Get varptr(DE) back from stack

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ; SJA1000 set to RCX table

FN_CORE4302:  POP DE                ; Get varptr(DE) back from stack
              
              LD B,SCAN             ; Set type SCAN 
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,CORE43           ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

                                    ;************************************ 
                                    ;* CAN INIT(ADDR,ACC)               *
                                    ;************************************ 
                                    ;* Initialise SJA1000               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(ACC)                 *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************

CORE43:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH IX               ; Save IX to stack

              PUSH DE               
              POP IX                ; Save varptr(ACC) to IX

                                    ; SJA in RESET MODE 
              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG 
              LD E,CANRES           ; CAN RESET BIT

CORE43S01:    CALL CANOUT 
              CALL CANINP           ; A contains value
              AND #01F              ; Mask 0b00011111, fits for all modes
              CP CANRES
              JP Z,CORE43S02        ; Wait until SJA1000 is in RESET MODE
              DJNZ CORE43S01
    
              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF                   ; Set CARRY flag for ERROR
              RET                       

                                    ; PeliCAN-Mode, Bypass, Clockout off

CORE43S02:    LD D,CANCLKDIVREG     ; CLKDIVIDER REG
              LD E,#0C8
              CALL CANOUT

                                    ; https://kvaser.com/support/calculators/bit-timing-calculator/
                                    ; Bustiming 250 kHz, 24 MHz einstellen
                                    ; T1         12  
                                    ; T2         4
                                    ; BTQ        16
                                    ; SP%        75
                                    ; SJW        4
                                    ; BIT RATE   250
                                    ; ERR%       0

              LD D,CANBUST0REG      ; BUSTIMING0 REG
              LD E,#0082
              CALL CANOUT

              LD D,CANBUST1REG      ; BUSTIMING0 REG
              LD E,#003A
              CALL CANOUT

                                    ; Acceptancecode and -mask

              LD D,CANACCEPTCREG    ; ACCEPTCODE REG
              LD E,(IX)             
              CALL CANOUT           ; ACR.0   ID28...ID21    #0F0
              INC D
              LD E,(IX+2)
              CALL CANOUT           ; ACR.1   ID20...ID13    #0B7
              INC D
              LD E,(IX+4)
              CALL CANOUT           ; ACR.2   ID12...ID5
              INC D
              LD E,(IX+6)
              CALL CANOUT           ; ACR.3   ID4...ID0, RTR, 3 bits n.c.

              LD D,CANACCEPTMREG    ; ACCEPTMASK REG
              LD E,(IX+8)             
              CALL CANOUT           ; AMR.0   ID28...ID21    #0F0
              INC D
              LD E,(IX+10)
              CALL CANOUT           ; AMR.1   ID20...ID13    #007
              INC D
              LD E,(IX+12)
              CALL CANOUT           ; AMR.2   ID12...ID5
              INC D
              LD E,(IX+14)
              CALL CANOUT           ; AMR.3   ID4...ID0, RTR, 3 bits n.c.

                                    ; Outputcontrol

              LD D,CANOUTCTREG      ; OUTPUTCTRL REG
              LD E,CANTX1FLOAT+CANTX0PSHPULL+CANNORMMODE
              CALL CANOUT

                                    ; Receive-Interrupt enable

              LD D,CANIERREG        ; IER REG
              LD E,CANRIE+CANEPIE+CANBEIE+CANALIE
              CALL CANOUT
      
                                    ; Set SJA to Wait Mode

              LD B,#0FF
              LD D,CANCTRLREG       ; CONTROL REG
              LD E,CANACCFMODE   

CORE43S03:    CALL CANOUT 
              CALL CANINP           ; IX for dummy read, A contains value
              AND #01F              ; Mask 0b00011111 fits for PELICAN
              CP CANACCFMODE
              JP Z,CORE43S04        ; Wait until SJA1000 is in ACCEPT MODE
              DJNZ CORE43S03

              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              SCF                   ; Set CARRY flag for ERROR
              RET  

                                    ; Prepare INT routine 

CORE43S04:    CALL GETRAM           ; HL points to Work Area

                                    ; Fill address for CAN msg count
              
              PUSH HL               ; Save HL basis
              LD DE,X_HCANPTR
              ADD HL,DE             ; HL points to CAN msg count
              LD A,0
              LD (HL),A
              INC HL
              LD (HL),A
              DEC HL
              EX HL,DE              ; DE points to CAN msg count
              POP HL                ; Get HL basis back
              PUSH HL               ; Save HL basis
              PUSH DE
              LD DE,X_HCNT
              ADD HL,DE             ; HL points to LD DE,MSGCNT 
              POP DE                ; DE points to CAN msg count
              LD (HL),E
              INC HL
              LD (HL),D
              POP HL                ; Get HL basis back

                                    ; Fill address for CAN msg buffer
              
              PUSH HL               ; Save HL basis
              LD DE,X_HCANMSG
              ADD HL,DE             ; HL points to CAN msg buffer
              EX DE,HL              ; DE points to CAN msg buffer
              POP HL                ; Get HL basis back
              PUSH HL               ; Save HL basis
              PUSH DE
              LD DE,X_HBUF
              ADD HL,DE             ; HL points to LD DE,MSGBUFFER 
              POP DE                ; DE points to CAN msg buffer
              LD (HL),E
              INC HL
              LD (HL),D
              POP HL                ; Get HL basis back

                                    ; Fill address for CAN IO addr

              PUSH HL               ; Save HL basis
              LD DE,X_HCIO
              ADD HL,DE             ; HL points to LD C,0
              LD A,C
              LD (HL),A             ; C contains IO addr
              POP HL                ; Get HL basis back

                                    ; Clear RX FIFO Buffer

CORE43S05:    LD D,CANCMDREG        ; COMMAND REG
              LD E,CANRELRXBUF 
              
              CALL CANOUT

                                    ; Still new msg?

              LD D,CANRMC           ; RMC REG
              CALL CANINP

              OR A
              JP NZ,CORE43S05       ; Jump if more new msg

              LD D,CANCMDREG        ; COMMAND REG
              LD E,CANABTX          ; Clear TX Transmission
              
              CALL CANOUT
                                    
              POP IX                ; Get IX back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET 

;---------------------------              

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN WR(ADDR,REG,DATA)       *
                                    ;************************************ 
                                    ;* Transfer Data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: CAN REG                   *
                                    ;* PARA4: CAN DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE44:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE44ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE44ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR
              LD C,A                ; C contains IO addr

              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              LD D,A                ; D contains CAN REG
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
               
              LD E,A                ; E contains CAN DATA
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed? 
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,CANOUT           ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 44                   *
                                    ;************************************ 
                                    ;* Transfer Data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE44:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE44SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE44SLT: PUSH BC               ; Save BC to stack
              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CANOUT           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN WR(ADDR,REG,DATA)            *                            
                                    ;************************************
                                    ;* Transfer Data to SJA1000         *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* E = CAN DATA                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CANOUT:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH DE               ; Save DE to stack
 
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,D                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,E                ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000
              
              CALL IODIS            ; Switched IO disabled

              EI

              POP DE                ; Get DE from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;--------------------------- 

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN RD(ADDR,REG,DATA)       *
                                    ;************************************ 
                                    ;* Read data from SJA1000           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: CAN REG                   *
                                    ;* PARA4: CAN DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (varptr(CAN DATA)) = CAN DATA    *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE45:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE45ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE45ST:   PUSH DE               ; Save DE to stack
              PUSH BC               ; Save BC to stack

              CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A                ; C contains IO addr

              PUSH BC               ; Save IO addr to stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              POP BC                ; Get IO addr from stack

              LD B,A                ; B contains CAN REG
              
              PUSH BC               ; Save IO addr & CAN REG to stack
                     
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA

              CALL EVALVARINT       ; DE contains varptr(CAN DATA)

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr & CAN REG from stack

              PUSH BC               ; Save IO addr & CAN REG to stack

              LD B,SCAN             ; Set type SCAN
                                    ; C contains IO addr

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              POP BC                ; Get IO addr & CAN REG back from stack

              JP C,B_CORE45IN       ; JP if slot found

              POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack

              JP SLTERR             ; No suitable slot found

B_CORE45IN:   PUSH DE               ; Save varptr(CAN DATA)to stack
                                    ; C contains IO addr 
              LD D,B                ; B contains CAN REG
              CALL CANINP           ; A contains Value
              POP DE                ; Get varptr(CAN DATA)back from stack

              LD (DE),A             ; Save result in (DE)
              INC DE
              LD A,0
              LD (DE),A             ; Set higher Byte (DE+1) to 0

              POP BC                ; Get BC back from stack
              POP DE                ; Get DE back from stack

              OR A
              RET

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 45                   *
                                    ;************************************ 
                                    ;* Read data from SJA1000           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE45:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE45SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE45SLT: PUSH BC               ; Save BC to stack
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed? 
                                    ; IX contains RAM-address
              POP BC                ; Get BC from stack
              JP C,CANINP           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN RD(ADDR,REG,DATA)            *
                                    ;************************************
                                    ;* Read data from SJA1000           *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* D = CAN REG                      *
                                    ;* A = CAN DATA                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CANINP:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH DE               ; Save DE to stack
 
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,D                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)           ; Get Data from SJA1000
                            
              CALL IODIS            ; Switched IO disabled

              EI

              POP DE                ; Get DE from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET                                        

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN CHECK(ADDR,RMC)         *
                                    ;************************************ 
                                    ;* Checks for new msg to read       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN addr                  *
                                    ;* PARA2: varptr(RMC)               *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN addr                     *
                                    ;* (DE) = 1 = new msg               *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE46:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE46ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE46ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr

              PUSH BC               ; Save IO addr to stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1                ; Second PARA

              CALL EVALVARINT       ; DE contains varptr(RMC)
                                         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr back from stack 

              LD B,SCAN             ; Set type SCAN
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,B_CORE4601       ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE4601:   PUSH HL
              PUSH BC

              PUSH DE
              POP IX                ; IX contains varptr (RMC)
                                                                                   
              LD DE,X_HCANPTR
              CALL GETRAMIX         ; HL points to incoming msg

              LD A,(HL)             ; A contains incoming msg pointer
              LD B,A                ; B contains incoming msg pointer
              INC HL
              LD A,(HL)             ; A contains read msg pointer
              CP B
              JP Z,B_CORE4602       ; no new incoming msg
              LD A,#01
              JP B_CORE4603
B_CORE4602:   LD A,#00
B_CORE4603:   LD (IX),A             ; Save result in (IX)
              LD A,0
              LD (IX+1),A           ; Set higher Byte to 0

              POP BC
              POP HL

              OR A
              RET    
              
                       

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 46                   *
                                    ;************************************ 
                                    ;* Checks for new msg to read       *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN addr                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN addr                     *
                                    ;* A = RMC                          *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE46:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE46SLT     ; Installed? Continue with Slot command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE46SLT: PUSH BC               ; Save BC to stack
              PUSH DE               ; Save DE to stack
              
              LD B,SCAN             ; Set type SCAN

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP C,FN_CORE46F       ; JP if slot found

              POP BC                ; Get BC from stack
                                 
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE46F:   PUSH DE               ; Save DE to stack
              LD D,CANRMC
              CALL CANINP           ; Check RX data to read
                                    ; A contains RMC

              POP DE                ; Get BC from stack
              POP BC                ; Get BC from stack

              SCF
              CCF                   ; Clear Carry 

              RET

;--------------------------- 

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN RX(ADDR,DATA)           *
                                    ;************************************ 
                                    ;* Read RX data from SJA1000        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: CAN DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,BC                            *
                                    ;************************************

B_CORE47:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE47ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE47ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR
              LD C,A                ; C contains IO addr

              PUSH BC               ; Save IO addr to stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA

              CALL EVALVARINT       ; DE contains varptr(RX DATA)

              POP BC                ; Get IO addr back from stack

              DEC DE                ; Check if 1-dim array with 14 bytes
              DEC DE
              DEC DE                ; 1-dim array?
              LD A,(DE)
              CP #01
              JP NZ,B_CORE47ERR1
              DEC DE
              DEC DE
              LD A,(DE)
              CP #01D               ; Length array = 13 * 2 + 3 bytes?
              JP NZ,B_CORE47ERR2
              INC DE
              INC DE
              INC DE
              INC DE
              INC DE
              
              PUSH BC               ; Save IO addr to stack
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr back from stack

              PUSH BC               ; Save IO addr to stack
              LD B,SCAN             ; Set type SCAN
                                    ; C contains IO addr

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get IO addr back from stack

              JP C,CORE47           ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE47ERR1: PUSH HL
              LD HL,PERRARRAY1

              JP B_CORE47ERR

B_CORE47ERR2: PUSH HL
              LD HL,PERRARRAY3

B_CORE47ERR:  CALL PROMPT
              POP HL

              JP B_ERRFUNC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 47                   *
                                    ;************************************ 
                                    ;* Read RX data from SJA1000        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE47:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE47SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE47SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CORE47           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN RX(ADDR,DATA)                *
                                    ;************************************
                                    ;* Read RX data from SJA1000        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(RX DATA)             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* (DE) = RX DATA                   *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************ 
              
CORE47:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH BC               ; Save BC to stack
 
              PUSH DE               ; DE points to varptr(RX DATA)

              DI

              LD DE,X_HCANPTR
              CALL GETRAMIX         ; HL points to read msg - 1
              INC HL
              LD A,(HL)
              LD B,A                ; B contains next msg to read

              LD DE,X_HCANMSG
              CALL GETRAMIX         ; HL points to begin of CAN RX buffer
              
              LD DE,#0D

              SCF
              CCF

              SBC HL,DE

CORE47LOOP:   ADD HL,DE
              DJNZ CORE47LOOP       ; HL points to next msg to be written

              POP DE                ; DE points to varptr(RX DATA)

              LD B,#0D              ; Set loop for 13 bytes

CORE47LOOP1:  LD A,(HL)
              LD (DE),A             ; Save result in (DE)
              INC DE             
              LD A,0
              LD (DE),A             ; Set higher Byte (DE+1) to 0
              INC DE                ; Increase DE-Pointer to new buffer
              INC HL                ; Increase RX FIFO place
              DJNZ CORE47LOOP1
   
              LD DE,X_HCANPTR
              CALL GETRAMIX         ; HL points to read msg - 1
              INC HL
              LD A,(HL)
              ADD A,#01
              CP X_CANCNT           ; Max count reached? Set 0
              JP NZ,CORE47_01
              LD A,#00
CORE47_01:    LD (HL),A             ; Save read msg count

              EI


              POP BC                ; Get BC back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET                                        

;---------------------------


                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN GET ADDR(ADDR,INTADDR)  *
                                    ;************************************ 
                                    ;* Get addr of INT routine          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: INTADDR                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* (varptr(INTADDR)) = INTADDR      *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,BC                            *
                                    ;************************************

B_CORE48:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE48ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE48ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR
              LD C,A                ; C contains IO addr

              PUSH BC               ; Save IO addr to stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA

              CALL EVALVARINT       ; DE contains varptr(INTADDR)
                                        
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr back from stack

              PUSH BC               ; Save IO addr to stack

              LD B,SCAN             ; Set type SCAN
                                    ; C contains IO addr
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed? 
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              POP BC                ; Get IO addr back from stack

              JP C,B_CORE48_1       ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE48_1:   PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              PUSH DE               ; Save varptr(INTADDR) to stack
 
              CALL GETRAM           ; HL points to Work Area
              LD DE,X_HINT
              ADD HL,DE             ; HL points to INT routine

              POP DE                ; Get varptr(INTADDR) back from stack

              LD A,L
              LD (DE),A
              INC DE
              LD A,H
              LD (DE),A             ; (DE) contains INT routine address

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A
              Ret

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 48                   *
                                    ;************************************ 
                                    ;* Get addr of INT routine          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = INTADDR                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************

FN_CORE48:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE48SLT     ; Installed? Continue with slt command

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE48SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE48ADR     ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE48ADR: PUSH HL               ; Save HL to stack
 
              CALL GETRAM           ; HL points to Work Area
              LD DE,X_HINT
              ADD HL,DE             ; HL points to INT routine

              EX DE,HL

              POP HL                ; Get HL back from stack

              OR A                  ; Clear CARRY flag
              RET

;--------------------------- 

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL CAN TX(ADDR,DATA)           *
                                    ;************************************ 
                                    ;* Write TX data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CAN ADDR                  *
                                    ;* PARA2: CAN DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,BC                            *
                                    ;************************************

B_CORE49:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE49ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE49ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR
              LD C,A                ; C contains IO addr

              PUSH BC               ; Save IO addr to stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1                ; Second PARA

              CALL EVALVARINT       ; DE contains varptr(TX DATA)

              POP BC                ; Get IO addr back from stack

              DEC DE                ; Check if 1-dim array with 14 bytes
              DEC DE
              DEC DE                ; 1-dim array?
              LD A,(DE)
              CP #01
              JP NZ,B_CORE49ERR1
              DEC DE
              DEC DE
              LD A,(DE)
              CP #01D               ; Length array = 13 * 2 + 3 bytes?
              JP NZ,B_CORE49ERR2
              INC DE
              INC DE
              INC DE
              INC DE
              INC DE

              PUSH BC               ; Save IO addr to stack
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr back from stack

              PUSH BC               ; Save IO addr to stack
              LD B,SCAN             ; Set type SCAN
                                    ; C contains IO addr

              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get IO addr back from stack

              JP C,CORE49           ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE49ERR1: PUSH HL
              LD HL,PERRARRAY1

              JP B_CORE49ERR

B_CORE49ERR2: PUSH HL
              LD HL,PERRARRAY3

B_CORE49ERR:  CALL PROMPT
              POP HL

              JP B_ERRFUNC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 49                   *
                                    ;************************************ 
                                    ;* Write TX data to SJA1000         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE49:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE49SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE49SLT: PUSH BC               ; Save BC to stack
              PUSH IX               ; Result not used
              LD B,SCAN             ; Set type SCAN
              CALL GETRCXSLT        ; Card with BC installed?  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get BC from stack
              JP C,CORE49           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************
                                    ;* CAN TX(ADDR,DATA)                *
                                    ;************************************
                                    ;* Write TX data to SJA1000         *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* DE = varptr(TX DATA)             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* C = CAN ADDR                     *
                                    ;* CARRY-Flag = 0 -> OK             *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************ 
              
CORE49:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              PUSH BC               ; Save BC to stack

              DI

              CALL IOEN             ; Switched IO enabled

                                    ; Clear Interrupts

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANINTREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)
                             
              LD H,CANTXBUFREG
              LD B,#0D              ; Set loop for 13 bytes

CORE49L1:     LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,H                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,(DE)             ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000

              INC DE                ; Increase write buffer index 
              INC DE                ; Increase write buffer index 
              
              INC H                 ; Increase TX FIFO place
                            
              DJNZ CORE49L1         ; Do loop

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANCMDREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,CANTXREQ         ; E contains value
              OUT (SWIO),A          ; Write value to SJA1000

                                    ; Wait till TX is ready

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANSTAREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,C                ; C contains IO addr
              ADD A,#01             ; Set Addr to Data transfer 
              OUT (ALE),A           ; Set Data transfer to SJA1000
CORE49L2:     IN A,(SWIO)
              AND A,CANTXOK          
              CP CANTXOK            
              JP Z,CORE49L2         ; Wait for completed 

              CALL IODIS            ; Switched IO disabled

              EI

              POP BC                ; Get BC back from stack
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy
                      
              OR A                  ; Clear Carry Flag to end routine handler
              RET     

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Card Type F                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ RC2014 card with ATMEGA8 4x SPI master                           +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI INIT(ADDR)              *
                                    ;************************************ 
                                    ;* Initialise SPI                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE50:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE50ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine
   
B_CORE50ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO addr from stack 

              LD B,SSPI             ; Set type SSPI 
              
              PUSH IX               ; Result not used
              CALL SETRCXSLT        ; Initialize card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used

              JP NC,SLTFREEERR      ; No free slot found

              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to BASIC


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 50                   *
                                    ;************************************ 
                                    ;* Initialise SPI                   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE50:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE50SLT     ; Installed? Continue with Slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE50SLT: PUSH BC               ; Save BC to stack
              LD B,SSPI             ; Set type SSPI 
              PUSH IX               ; IX not used
              CALL SETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; IX not used
              POP BC                ; Get BC from stack
              JP C,FN_CORE50OK      ; JP if free slot found, CARRY = 1

              SCF                   ; Set CARRY flag for ERROR
              RET

FN_CORE50OK:  CCF                   ; Clear CARRY flag
              RET                   ; Go back to main routine

;---------------------------


                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI WR(ADDR,BUS,DATA)       *
                                    ;************************************ 
                                    ;* Write data to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE51:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE51ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE51ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR

              LD E,A                
              PUSH DE               ; Save SPI ADDR on stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1
              CALL EVALBYTEVAR
              
              LD C,A                ; Save SPI Bus address
              PUSH BC               ; Save SPI Bus address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,2
              CALL EVALBYTEVAR

              POP BC                
              LD B,A                ; BC contains SPI Bus address and data
                            
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP DE                ; DE contains Bus IO address

              PUSH BC               ; Save SPI Bus address and data to stack
              LD B,SSPI             ; Set type SSPI
              LD C,E 
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI Bus address and data from stack

              JP C,CORE51           ; JP if slot found

              JP SLTERR             ; No suitable slot found
                                                             
                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 51                   *
                                    ;************************************ 
                                    ;* Write data to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE51:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE51SLT     ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE51SLT: PUSH BC               ; Save SPI Bus address & I2C DATA to stack
              LD B,SSPI             ; Set type SSPI 
              LD C,E
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get I2C CHIP & I2C DATA from stack
              JP C,CORE51           ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI WR(ADDR,BUS,DATA)            *
                                    ;************************************ 
                                    ;* Write data to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* E = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI DATA                     *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE51:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack
              DI

              CALL IOEN             ; Switched IO enabled

              LD A,E                ; Set active Bus
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Send data
              ADD A,#03             
              OUT (ALE),A
              LD A,B     
              OUT (SWIO),A

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A
  
  
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI WRB(ADDR,BUS,LEN,DATA)  *
                                    ;************************************ 
                                    ;* Write block data to SPI          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI LEN                   *
                                    ;* PARA4: SPI DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE52:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE52ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE52ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              PUSH AF               ; Save SPI ADDR on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              LD C,A                ; Save SPI address
              PUSH BC               ; Save SPI address on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
               
              POP BC                
              LD B,A                ; Save SPI Len
                                    ; BC contains SPI address & SPI len 
              PUSH BC               ; Save SPI address & SPI len on stack
  
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get SPI address & SPI len on stack
              POP AF                ; Get BUS address from stack

              PUSH BC               ; Save SPI address and data to stack
              LD B,SSPI             ; Set type SSPI
              LD C,A                ; Set BUS address
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              LD A,C                ; Get BUS address back
              POP BC                ; Get SPI address and data from stack
              JP C,CORE52            ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 52                   *
                                    ;************************************ 
                                    ;* Write block data to SPI          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                     *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE52:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE52SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE52SLT: PUSH BC               ; Save SPI BUS & LEN to stack
              LD B,SSPI             ; Set type SSPI
              LD C,H                ; Set SPI ADDR
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI BUS & LEN from stack
              LD A,H                ; Get SPI ADDR
              JP C,CORE52           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI WRB(BUS,ADDR,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Write block data to SPI          *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE52:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,A                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Send data
              ADD A,#03             
              OUT (ALE),A

CORE52LOOP:   LD A,(HL)
              INC HL
              INC HL     
              OUT (SWIO),A

              DJNZ CORE52LOOP

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A     


              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI RD(BUS,ADDR,DATA)       *
                                    ;************************************ 
                                    ;* Read data from SPI               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE53:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE53ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE53ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR

              LD C,A                ; C contains SPI ADDR
              PUSH BC               ; Save SPI ADDR on stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1
              CALL EVALBYTEVAR
              
              POP BC                ; Get SPI ADDR from stack
              LD B,A                ; B contains SPI BUS
              PUSH BC               ; Save SPI ADDR & BUS on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,2                ; Third PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                            
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; BC contains SPI ADDR & BUS

              PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI ADDR & BUS from stack

              JP C,CORE53           ; JP if slot found

              JP SLTERR             ; No suitable slot found



                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 53                   *
                                    ;************************************ 
                                    ;* Read data from SPI               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE53:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE53SLT     ; Installed? Continue with slot command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE53SLT: PUSH BC
              LD B,SSPI             ; Set type SPI 
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Initialize card with BC  
                                    ; HL contains RAM-address
              POP IX                ; Result not used
              POP BC
              JP C,CORE53           ; JP if slot found

              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RD(ADDR,BUS,DATA)            *
                                    ;************************************ 
                                    ;* Read data from SPI               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = SPI DATA                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE53:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(SPI DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,C                ; E contains SPI ADDR
              LD A,E                ; Set active Bus
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,B     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Read data
              ADD A,#03             
              OUT (ALE),A
              IN A,(SWIO)
              LD (HL),A
              LD A,#00
              INC HL
              LD (HL),A

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI RDB(ADDR,BUS,LEN,DATA)  *
                                    ;************************************ 
                                    ;* Read block data from SPI         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI LEN                   *
                                    ;* PARA4: SPI DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE54:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE54ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE54ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              PUSH AF               ; Save SPI ADDR on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              LD C,A                ; Save SPI BUS
              PUSH BC               ; Save SPI BUS on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
               
              POP BC                
              LD B,A                ; Save SPI LEN
                                    ; BC contains SPI BUS & SPI LEN 
              PUSH BC               ; Save SPI BUS & SPI LEN on stack
  
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get SPI BUS & SPI LEN on stack
              POP AF                ; Get SPI ADDR from stack

              PUSH BC               ; Save SPI BUS & SPI LEN to stack
              LD B,SSPI             ; Set type SSPI
              LD C,A                ; Set SPI ADDR
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              LD A,C                ; Get SPI ADDR back
              POP BC                ; Get SPI BUS & SPI LEN from stack
              JP C,CORE54           ; JP if slot found

              JP SLTERR             ; No suitable slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 54                   *
                                    ;************************************ 
                                    ;* Read block data from SPI         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* H = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE54:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE54SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE54SLT: PUSH BC               ; Save SPI BUS & LEN to stack
              LD B,SSPI             ; Set type SSPI
              LD C,H                ; Set SPI ADDR
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI BUS & LEN from stack
              LD A,H                ; Get SPI ADDR
              JP C,CORE54           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RDB(ADDR,BUS,LEN,DATA)       *
                                    ;************************************ 
                                    ;* Read block data from SPI         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* A = SPI ADDR                     *
                                    ;* C = SPI BUS                      *
                                    ;* B = SPI LEN                      *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE54:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(I2C DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,A                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,C     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD A,E                ; Read data
              ADD A,#03             
              OUT (ALE),A

CORE54LOOP:   IN A,(SWIO)
              LD (HL),A
              INC HL
              LD A,#00
              LD (HL),A
              INC HL     

              DJNZ CORE54LOOP

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A   
              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI RDH(BUS,ADDR,LN,LN,DATA)*
                                    ;************************************ 
                                    ;* Read block data from SPI after   *
                                    ;* Header                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI LEN HEADER            *
                                    ;* PARA3: SPI LEN DATA              *
                                    ;* PARA4: SPI DATA                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* (IX) = SPI LEN HEADER            *
                                    ;* (IX+1) = SPI LEN DATA            *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE55:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE55ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE55ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A                ; C contains SPI ADDR

              PUSH BC               ; Save SPI ADDR on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR

              POP BC                ; Get SPI ADDR

              LD B,A                ; B contains SPI BUS
              PUSH BC               ; Save SPI ADDR & BUS on stack

              LD B,SSPI             ; Set type SSPI
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address

              POP BC                ; Get SPI ADDR & BUS

              JP C,B_CORE55_1       ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE55_1:   PUSH BC               ; Save SPI ADDR & BUS on stack
              PUSH IX
                           
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
              
              POP IX 
              POP BC                ; Get SPI ADDR & BUS

              LD (IX),A             ; SPI Len Header 

              PUSH BC               ; Save SPI ADDR & BUS on stack
              PUSH IX               ; Save SPI Len Header on stack

              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALBYTEVAR
              
              POP IX                ; Get SPI Len Header  
              POP BC                ; Get SPI ADDR & BUS

              LD (IX+1),A           ; SPI Len Data
               
              PUSH BC               ; Save SPI ADDR & BUS on stack
              PUSH IX               ; Save SPI Len Header on stack
  
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,4                ; Fith PARA
              CALL EVALVARINT       ; DE contains VARPTR to data
                                          
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP IX                ; Get SPI Len Header 
              POP BC                ; Get SPI ADDR & BUS from stack
     
              JP CORE55             ; JP if slot found

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 55                   *
                                    ;************************************ 
                                    ;* Read block data from SPI after   *
                                    ;* Header                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* H = SPI LEN HEADER               *
                                    ;* L = SPI LEN DATA                 *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* (IX) = SPI LEN HEADER            *
                                    ;* (IX+1) = SPI LEN DATA            *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE55:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE55SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE55SLT: LD A,C                ; Get SPI ADDR
              PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              LD A,H                ; SPI LEN HEADER
              LD (IX),A
              LD A,L                ; SPI LEN DATA
              LD (IX+1),A

              POP BC                ; Get SPI ADDR & BUS from stack
              JP C,CORE55           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI RDH(ADDR,BUS,LEN,LEN,DATA)   *
                                    ;************************************ 
                                    ;* Read block data from SPI after   *
                                    ;* Header                           *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* (IX) = SPI LEN HEADER            *
                                    ;* (IX+1) = SPI LEN DATA            *
                                    ;* DE = varptr(SPI DATA)            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE55:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              EX DE,HL              ; HL contains varptr(SPI DATA)

              DI

              CALL IOEN             ; Switched IO enabled

              LD E,C                ; E contains SPI ADDR
              ADD A,#01             ; Cmd = 0x00
              OUT (ALE),A
              LD A,B     
              AND #03               ; Bits 0..1 = active Bus number
              OUT (SWIO),A

              LD A,E                ; Set /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#01     
              OUT (SWIO),A

              LD B,(IX)             ; SPI LEN HEADER

              LD A,E                ; Write Header
              ADD A,#03             
              OUT (ALE),A

CORE55LOOP:   LD A,(HL)
              INC HL
              INC HL     
              OUT (SWIO),A

              DJNZ CORE55LOOP

              LD B,(IX+1)           ; SPI LEN HEADER

CORE55LOOPR:  IN A,(SWIO)
              LD (HL),A
              INC HL
              LD A,#00
              LD (HL),A
              INC HL

              DJNZ CORE55LOOPR

              LD A,E                ; Reset /CS
              ADD A,#02             
              OUT (ALE),A
              LD A,#00     
              OUT (SWIO),A   

              CALL IODIS            ; Switched IO disabled

              EI

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET   

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL SPI MODE(ADDR,BUS,MOD,FQZ)  *
                                    ;************************************ 
                                    ;* Write control data to SPI        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: SPI ADDR                  *
                                    ;* PARA2: SPI BUS                   *
                                    ;* PARA3: SPI MODE                  *
                                    ;* PARA4: SPI FREQUENCY             *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE56:     CALL RCXINIT          ; Module installed?
              JP Z,B_CORE56ST       ; Installed? Continue
              CALL MODERR           ; Prompt error no RCX
              RET                   ; Go back to calling routine

B_CORE56ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0                ; First PARA
              CALL EVALBYTEVAR

              LD C,A
              PUSH BC               ; Save SPI ADDR on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,
                            
              LD B,1                ; Second PARA
              CALL EVALBYTEVAR
              CP #04
              JP NC,B_CORE56ERR

              POP BC                ; Get SPI BUS  
 
              LD B,A                ; Save SPI BUS
              PUSH BC               ; Save SPI ADDR & BUS on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,2                ; Third PARA
              CALL EVALBYTEVAR
              CP #04
              JP NC,B_CORE56ERR

              LD D,A                ; D contains SPI MODE
               
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,3                ; Fourth PARA
              CALL EVALBYTEVAR      ; DE contains VARPTR to data
              CP #04
              JP NC,B_CORE56ERR

              LD E,A                ; E contains SPI FREQUENCY

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get SPI ADDR & BUS from stack

              PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI ADDR & BUS from stack

              JP C,CORE56           ; JP if slot found

              JP SLTERR             ; No suitable slot found

B_CORE56ERR:  PUSH HL
              LD HL,PERRVAL
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRVAL3
              CALL PROMPT
              POP HL

              POP BC                ; Clear stack 
               
              JP B_ERRTYPE 

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 56                   *
                                    ;************************************ 
                                    ;* Write MODE to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE56:    CALL RCXINIT          ; Module installed?
              JP Z,FN_CORE56SLT     ; Installed? Continue with slt command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

FN_CORE56SLT: PUSH BC               ; Save SPI ADDR & BUS to stack
              LD B,SSPI             ; Set type SSPI
              PUSH IX               ; Result not used
              CALL GETRCXSLT        ; Check card with BC  
                                    ; IX contains RAM-address
              POP IX                ; Result not used
              POP BC                ; Get SPI ADDR & BUS from stack
              JP C,CORE56           ; JP if slot found
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* SPI MODE(ADDR,BUS,MOD,FQZ)       *
                                    ;************************************ 
                                    ;* Write MODE to SPI                *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = SPI ADDR                     *
                                    ;* B = SPI BUS                      *
                                    ;* D = SPI MODE                     *
                                    ;* E = SPI FREQUENCY                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE56:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise write dummy to stack

              DI

              CALL IOEN             ; Switched IO enabled

              LD A,C                ; C contains SPI ADDR
              OUT (ALE),A

              LD A,D                ; Prepare SPI MODE
              SLA A
              SLA A
              SLA A
              SLA A
              LD D,A

              LD A,E                ; Prepare SPI FREQUENCY
              SLA A
              SLA A
              SLA A
              SLA A
              SLA A
              SLA A
              LD E,A

              LD A,B                ; Prepare SPI BUS
              OR D                  ; Prepare MODE
              OR E                  ; 0bEEDDxxBB
   
              OUT (SWIO),A          ; Send new MODE to SPI

              CALL IODIS            ; Switched IO disabled

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
                                    ; Otherwise clear stack from HL dummy

              OR A                  ; Clear Carry Flag to end routine handler
              RET    
                                                
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ BASIC Parameter check                                            +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


                                    ;************************************
                                    ;* BASIC get string pointer         *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A  = String lenght               *
                                    ;* DE = varptr(String)              *                                                                
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,DE                            *
                                    ;************************************

GETSTRPNT:    PUSH HL
              LD HL,(USRVAR)
              LD A,(HL)
              INC HL
              LD  E,(HL)
              INC HL
              LD  D,(HL)
              POP HL
              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC String as Parameter        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = String length                *
                                    ;* DE = varptr(String)              *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, IX                           *
                                    ;************************************

EVALSTR:      PUSH BC               ; BYTE
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP BC

              LD A,(VALTYP)
              CP #03                ; String?
              JP Z,EVALSTR1

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPES
              CALL PROMPT
              POP HL

              JP B_ERRTYPE
              
EVALSTR1:     PUSH HL               
              LD IX,FRESTR          ; Free temporary String
              CALL CALBAS
              LD HL,(USRVAR)
              LD A,(HL)
              INC HL
              LD  E,(HL)
              INC HL
              LD  D,(HL)
              POP HL

              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC Eval Int as Parameter      *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* DE  = Integer Value              *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, IX                           *
                                    ;************************************

EVALINT:      PUSH IX               ; Save IX to stack
              PUSH BC               ; Parameter number INTEGER
              PUSH HL               ; Save HL to stack
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP HL
              POP BC

              LD A,(VALTYP)
              CP #02                ; Integer type?
              JP Z,EVALINT1

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              JP B_ERRTYPE
              
EVALINT1:     LD IX,GETBYT          ; Get 1-byte-integer
              CALL CALBAS
              LD E,A
              LD IX,GETBYT          ; Get 1-byte-integer
              CALL CALBAS
              LD D,A
              POP IX
              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC Eval Byte as Parameter     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = Byte Value                   *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, IX                           *
                                    ;************************************

EVALBYTE:     PUSH BC               ; BYTE
              PUSH HL               
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP HL
              POP BC

              LD A,(VALTYP)
              CP #02                ; Integer type?
              JP Z,EVALBYTE1

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              JP B_ERRTYPE
              
EVALBYTE1:    LD IX,GETBYT          ; Get 1-byte-integer
              CALL CALBAS
              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC Eval Int Var as Parameter  *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* DE = Varptr                      *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, IX                           *
                                    ;************************************

EVALVARINT:   PUSH BC               ; Check for type variable
              PUSH HL               
              LD IX,CHKALF
              CALL CALBAS
              POP HL
              POP BC

              JP NC, EVALVARINT1

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEIV
              CALL PROMPT
              POP HL

              JP B_ERRTYPE                                     
               
              
EVALVARINT1:  PUSH BC
              PUSH HL               ; Evaluate expression
              LD IX,FRMEVL
              CALL CALBAS           
              POP HL
              POP BC

              LD A,(VALTYP)
              CP #02                ; Integer type?
              JP Z,EVALVARINT2

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              JP B_ERRTYPE

EVALVARINT2:  LD IX,PTRGET          ; Get Integer as data pointer
              CALL CALBAS           ; and save it in DE

              RET

;---------------------------

                                    ;************************************
                                    ;* BASIC Eval Byte&Var as Parameter *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A  = Byte Value                  *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

EVALBYTEVAR:  PUSH DE               ; Save DE to stack
              PUSH IX               ; Save IX to stack

              PUSH BC
              PUSH HL               ; Check for type variable
              LD IX,CHKALF
              CALL CALBAS
              POP HL
              POP BC
              
              JP C,EVALBYTEVAR3     ; No variable? Jump integer value 
              
              PUSH BC               ; VARIABLE
              PUSH HL               
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP HL
              POP BC

              LD A,(VALTYP)         ; Integer type?
              CP #02                  
              JP Z,EVALBYTEVAR1
              
              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              POP IX                ; Get IX back
              POP DE                ; Get DE back

              JP B_ERRTYPE
                            
EVALBYTEVAR1: PUSH BC               ; Save BC
                                             
              LD IX,PTRGET          ; Get Integer as data pointer
              CALL CALBAS           ; and save it in DE

              INC DE                ; Test value < 256
              LD A,(DE)
              AND A
 
              POP BC                ; Get BC back             
              
              JP Z,EVALBYTEVAR2     ; Call value > 255? ERROR
              
              PUSH HL
              LD HL,PERRVAL
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRVAL1
              CALL PROMPT
              POP HL

              POP IX                ; Get IX back
              POP DE                ; Get DE back

              JP B_ERRVAL

EVALBYTEVAR2: DEC DE                
              LD A,(DE)             ; A contains Byte Parameter

              POP IX                ; Get IX back
              POP DE                ; Get DE back

              RET

EVALBYTEVAR3: PUSH BC               ; INTEGER
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP BC
              
              LD A,(VALTYP)
              CP #02                ; Integer type?
              JP Z,EVALBYTEVAR4

              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              POP IX                ; Get IX back
              POP DE                ; Get DE back

              JP B_ERRTYPE
              
EVALBYTEVAR4: LD DE,(USRVAR)        ; Get integer
              LD A,D
              CP #00
              LD A,E                ; A contains value

              POP IX                ; Get IX back
              POP DE                ; Get DE back

              JP Z,EVALBYTEVAR5

              PUSH HL
              LD HL,PERRVAL
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRVAL1
              CALL PROMPT
              POP HL

              JP B_ERRVAL

EVALBYTEVAR5: RET

;---------------------------

                                    ;************************************
                                    ;* BASIC Eval Int&Var as Parameter  *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;* B = Parameter number             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* DE = Integer Value               *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, IX                           *
                                    ;************************************

EVALINTVAR:   PUSH BC
              PUSH HL               ; Check for type variable
              LD IX,CHKALF
              CALL CALBAS
              POP HL
              POP BC
              
              JP C,EVALINTVAR2      ; No variable? Jump integer value 
              
              PUSH BC               ; VARIABLE
              PUSH HL               
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP HL
              POP BC

              LD A,(VALTYP)         ; Integer type?
              CP #02                  
              JP Z,EVALINTVAR1
              
              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              JP B_ERRTYPE
              
EVALINTVAR1:  LD IX,PTRGET          ; Get Integer as data pointer
              CALL CALBAS           ; and save value in DE
              PUSH HL
              EX DE,HL
              LD A,(HL)
              LD E,A
              INC HL
              LD A,(HL)
              LD D,A
              POP HL
              RET

EVALINTVAR2:  PUSH BC               ; INTEGER
              LD IX,FRMEVL
              CALL CALBAS           ; Evaluate expression
              POP BC

              LD A,(VALTYP)
              CP #02                ; Integer type?
              JP Z,EVALINTVAR3
              
              PUSH HL
              LD HL,PERRTYPE
              CALL PROMPT
              LD A,B
              ADD A,#030
              CALL CHPUT
              LD HL,PERRTYPEI
              CALL PROMPT
              POP HL

              JP B_ERRTYPE
              
EVALINTVAR3:  LD DE,(USRVAR)        ; Get integer
              
              RET

;--------------------------- 

                                    ;************************************
                                    ;* BASIC check valid char           *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = BASIC address line          *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E, IX                            *
                                    ;* HL = Pointer in BASIC-Line       *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL, E, IX                    *
                                    ;************************************
   
CHKCHAR:      CALL GETPREVCHAR      ; Get previous basic char
              EX (SP),HL            ; A contains char
              CP (HL)               ; Check if good char
              JR NZ,SYNTAXERROR     ; No, Syntax error
              INC HL
              EX (SP),HL
              INC HL                ; Get next basic char
 
GETPREVCHAR:  DEC HL
              LD IX,CHRGTR
              JP CALBAS

SYNTAXERROR:  LD E,2
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS


;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ BASIC ERROR handling                                             +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* BASIC ERROR routine with PROMPT  *
                                    ;* Illegal Function Call            *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E, IX                            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL, E, IX                    *
                                    ;************************************

B_ERRFUNC:    OR A                  ; Clear Carry Flag
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS 

;---------------------------

                                    ;************************************
                                    ;* BASIC ERROR routine with PROMPT  *
                                    ;* Parameter errors                 *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E, IX                            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL, E, IX                    *
                                    ;************************************

B_ERRVAL:     OR A                  ; Clear Carry Flag
              LD E,2
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS 

;---------------------------

                                    ;************************************
                                    ;* BASIC ERROR routine with PROMPT  *
                                    ;* Parameter type error             *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* E, IX                            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL, E, IX                    *
                                    ;************************************

B_ERRTYPE:    OR A                  ; Clear Carry Flag
              LD E,13
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS 

;--------------------------- 

                                    ;************************************
                                    ;* no free slot found (BASIC)       *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

SLTFREEERR:   PUSH HL
              LD HL,PERRNOFRESLT    ; No free slot found
              CALL PROMPT
              POP HL
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

;--------------------------- 

                                    ;************************************
                                    ;* no slot found (BASIC)            *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

SLTERR:       PUSH HL               ; Save BASIC-Pointer to stack
              LD HL,PERRNOSLT       ; Prompt message 
              CALL PROMPT           ; that slot card is not present
              POP HL                ; Get BASIC Pointer from stack
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

;--------------------------- 

                                    ;************************************
                                    ;* Module not installed             *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

MODERR:       PUSH HL
              LD HL,PERRINIT        ; Not installed? Prompt Message
              CALL PROMPT
              POP HL
              RET

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Slot/Workarea functions based on KONAMIMAN                       +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* Get slot connected on page 1     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = Slot number                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

GETSLT:       DI                    ; Disable INT
       
              EXX                   ; Exchange register set
              
              IN A,(#0A8)
              LD E,A
              and  00001100b
              SRA A
              SRA A
              LD C,A                ; C = Slot
              LD B,0
              LD HL,EXPTBL
              ADD HL,BC
              BIT 7,(HL)
              JR Z,NOEXP1
EXP1:         INC HL
              INC HL
              INC HL
              INC HL
              LD A,(HL)
              AND  00001100b
              OR C
              OR #080
              LD C,A
NOEXP1:       LD A,C
              
              EXX                   ; Exchange register set
              
              EI                    ; Enable INT
              
              RET

;---------------------------

                                    ;************************************
                                    ;* Obtain slot work area (8 bytes)  *
                                    ;* on SLTWRK                        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* A  = Slot number                 *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = Work area address           *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, HL                           *
                                    ;************************************

GETWRK:       PUSH BC
              LD B,A
              RRCA
              RRCA
              RRCA
              AND  01100000b
              LD C,A                ; C = Slot * 32
              LD A,B
              RLCA
              AND  00011000b        ; A = Subslot * 8
              OR C
              LD C,A
              LD B,0
              LD HL,SLTWRK
              ADD HL,BC
              POP BC
              RET

;---------------------------

                                    ;************************************
                                    ;* Get slot connected on designed   *
                                    ;* page                             *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = Slot number                  *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

GETSLTP:      DI                    ; Disable INT
              PUSH HL               ; Save registers
              PUSH DE
              IN A,(#0A8)           ; read primary slot #
              RRCA
              RRCA
              AND 011b              ; [A]=000000PP
              LD E,A
              LD D,0                ; [DE]=000000PP
              LD HL,EXPTBL
              ADD HL,DE             ; [HL]=EXPTBL+000000PP
              LD E,A                ; [E]=000000PP
              LD A,(HL)             ; [A]=(EXPTBL+000000PP)
              AND #080              ; Use only MSB
              JR Z,GETSLTNOEXP
              OR E                  ; [A]=F00000PP
              LD E,A                ; save primary slot number
              INC HL                ; point to SLTTBL entry
              INC HL
              INC HL
              INC HL
              LD A,(HL)             ; get current expansion slot register
              RRCA
              RRCA
              AND 011b              ; [A] = 000000SS
              RLCA
              RLCA                  ; [A] = 0000SS00
              OR E                  ; [A] = F000SSPP

GETSLTPEND:   POP DE
              POP HL
              EI                    ; Enable INT
              RET

GETSLTNOEXP:  LD A,E                ;[A] = 000000PP
              JR GETSLTPEND

;---------------------------

                                    ;************************************
                                    ;* Obtain slot work area            *
                                    ;* on SLTWRK                        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = Work area address           *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************

GETWRKP:      PUSH DE
              PUSH AF
              CALL GETSLTP          ;[A] = F000SSPP, SS = 00 if not expanded
              AND 00001111b         ;[A] = 0000SSPP
              LD L,A                ;[A] = 0000SSPP
              RLCA
              RLCA
              RLCA
              RLCA                  ;[A] = SSPP0000
              AND 00110000b         ;[A] = 00PP0000
              OR L                  ;[A] = 00PPSSPP
              AND 00111100b         ;[A] = 00PPSS00
              OR 01b                ;[A] = 00PPSSBB
              RLCA                  ;*=2
              LD E,A
              LD D,0                ;[DE] = 0PPSSBB0
              LD HL,SLTWRK
              ADD HL,DE
              POP AF
              POP DE
              RET

;---------------------------

                                    ;************************************
                                    ;* Read Work Area Adress for  RAM   *
                                    ;* from SLOTWRK                     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = Work area in RAM            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************

GETRAM:       PUSH AF
              PUSH BC
              CALL GETSLT           ; Get Work Area Adress for reserved RAM
              CALL GETWRK           ; [HL] = address of slot work
              PUSH DE
              LD DE,#06
              ADD HL,DE
              EX DE,HL
              LD A,(DE)
              LD L,A
              INC DE
              LD A,(DE)
              LD H,A
              POP DE
              POP BC
              POP AF
              RET

;---------------------------

                                    ;************************************
                                    ;* Read Work Area Adress for  RAM   *
                                    ;* from SLOTWRK for specific index  *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Work area in RAM            *
                                    ;* DE = Specific index              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = Work area + index in RAM    *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************

GETRAMIX:     CALL GETRAM
              ADD HL,DE
              RET

;---------------------------

                                    ;************************************
                                    ;* Write Work Area Adress for RAM   *
                                    ;* from SLOTWRK                     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Work area in RAM            *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************
SETRAM:       PUSH DE
              PUSH HL               ; [SP] = data to write
              CALL GETSLT           ; Get Work Area Adress for reserved RAM
              CALL GETWRK           ; [HL] = address of slot work
              LD DE,#06
              ADD HL,DE
              EX DE,HL              ; [DE] = address of slot work RAM
              POP HL                ; [HL] = address RAM
              LD A,L
              LD (DE),A
              INC DE
              LD A,H
              LD (DE),A
              POP DE
              RET

;---------------------------                      

                                    ;************************************
                                    ;* Allocate RAM as Work Area        *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Size in Bytes               *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = address of Work area in RAM *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL, AF                           *
                                    ;************************************              
                            
ALLOC:        LD A,L                ; is requested size 0?
              OR H
              RET Z                 ; yes, allocation always succeeds
              EX DE,HL              ; calculate -size
              LD HL,0
              SBC HL,DE
              LD C,L                ; remember specified size
              LD B,H
              ADD HL,SP             ; [HL] = [SP] - size
              CCF
              RET C                 ; size too big

              LD DE,(BOTTOM)        ; get current RAM bottom
              SBC HL,DE             ; get memory space left after allocation
              RET C                 ; no space left
              LD A,H                ; do we still have breathing room?

              PUSH BC               ; save -size
              LD HL,0
              ADD HL,SP             ; get current stack pointer to [HL]
              LD E,L                ; move source address to [DE]
              LD D,H
              ADD HL,BC
              PUSH HL               ; save destination
              LD HL,(STKTOP)
              OR A
              SBC HL,DE
              LD C,L                ; move byte count to move to [BC]
              LD B,H
              INC BC
              POP HL                ; restore destination
              LD SP,HL              ; destination becomes the new SP
              EX DE,HL
              LDIR                  ; move stack contents
              POP BC                ; restore -size
              LD HL,(HIMEM)
              ADD HL,BC
              LD (HIMEM),HL
              LD DE,-2*(2+9+256)
              ADD HL,DE
              LD (FILTAB),HL        ; pointer to first FCB
              EX DE,HL
              LD HL,(MEMSIZ)        ; update MEMSIZ
              ADD HL,BC
              LD (MEMSIZ),HL
              LD HL,(NULBUF)        ; update NULBUF
              ADD HL,BC
              LD (NULBUF),HL
              LD HL,(STKTOP)        ; update STKTOP
              ADD HL,BC
              JR CLRFCB
                                    ;
                                    ; Re-build BASIC's file structures
                                    ;

DEFILE:       LD A,1                ; load default MAXFIL
              LD (MAXFIL),A
              LD HL,(HIMEM)
              LD DE,-2*(256+9+2)
              ADD HL,DE
              LD (FILTAB),HL        ; pointer to first FCB
              LD E,L
              LD D,H
              DEC HL
              DEC HL
              LD (MEMSIZ),HL
              LD BC,200             ; load default string space
              OR A
              SBC HL,BC
              PUSH HL               ; save new STKTOP
              LD HL,2*2+9           ; 4 for two FCB pointers, 9 for flags
              ADD HL,DE
              LD (NULBUF),HL
              POP HL

CLRFCB:       LD (STKTOP),HL
              DEC HL                ; and SAVSTK
              DEC HL
              LD (SAVSTK),HL
              LD L,E                ; get FILTAB in [HL]
              LD H,D
              INC HL                ; point to first FCB
              INC HL
              INC HL
              INC HL
              LD A,2

DSKFLL:       EX DE,HL
              LD (HL),E             ; set address in FILTAB
              INC HL
              LD (HL),D
              INC HL
              EX DE,HL
              LD BC,7
              LD (HL),B             ; make it look closed
              ADD HL,BC
              LD (HL),B             ; clear flag byte
              LD BC,9+256-7
              ADD HL,BC             ; point to next FCB
              DEC A
              JR NZ,DSKFLL
          
              RET    
              
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Auxillary functions                                              +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* Convert a character to upper-case*
                                    ;* if it is a lower-case letter     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* A = character                    *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = upper character              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

TOUPPER:
              CP  "a"
              RET C
              CP  "z"+1
              RET NC
              AND  #0DF
              RET

;---------------------------
                
                                    ;************************************
                                    ;* Prompt Text on Screen            *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = pointer to Text             *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************

PROMPT:       PUSH AF               ; Save A on Stack

              LD A,(SCRMOD)         ; Test screen mode
              AND A                 ; Text mode? No? End routine
              JR NZ,PROMPTEXIT

PROMPTLOOP:   LD A,(HL)             ; Get character
              CP 0                  ; Send character to BIOS
              JR Z,PROMPTEXIT       ; PUT CHAR until 0
              PUSH HL                
              CALL CHPUT
              POP  HL
              INC  HL
              JR   PROMPTLOOP
     
PROMPTEXIT:   POP AF                ; Get A from Stack
              
              RET

;---------------------------
 
                                    ;************************************
                                    ;* Clear Screen                     *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

CLEARSCR:     PUSH AF               ; Save A on Stack
              PUSH BC
              PUSH DE
            
              SUB A                 ; Generate Z-Flag
                               
              CALL CLS              ; Call BIOS
     
CLEAREXIT:    POP DE
              POP BC
              POP AF                ; Get A from Stack

              RET

;---------------------------

                                    ;************************************
                                    ;* HEXtoASCII                       *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* A = number dec                   *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = number ASCII                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
                                    
HEX2ASC:      CP #0A
              JP C,ISNUM
              ADD A,#07             
ISNUM:        ADD A,#030            

HEX2ASCEXIT:  RET

                                    ;************************************
                                    ;* ASCIItoHEX                       *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* A = number ASCII                 *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* A = number HEX                   *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
                                    
ASC2HEX:      CP #040
              JP C,ASC2HEXEXIT
              SUB A,#037

              RET

ASC2HEXEXIT:  SUB A,#030
              
              RET

;---------------------------

                                    ;************************************
                                    ;* Delay                            *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* B = Delay in 0,5 s at 3,81 MHz   *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

DELAY:        PUSH DE
              PUSH AF

              LD A,B
              OR A
              JR Z,DELAYSEKEND

DELAYSEKL:    LD DE,#0FFFF
DELAYMUSL:    DEC DE
              LD A,D
              OR E
              JR NZ,DELAYMUSL
  
              DJNZ DELAYSEKL

DELAYSEKEND:  POP AF
              POP DE

              RET

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Strings for prompting                                            +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                   
                                    ;************************************
                                    ;* Messages for BASIC CALLs         *
                                    ;************************************
           
PSTART:       DEFM "[RCX] - RC2014 Interface",#0D,#0A,#0A
              DEFM "Driver for cards adapted to [XIO]",#0D,#0A
              DEFM "via RC2014",#0D,#0A,#0A
              DEFM "Firmware version ",#030+ROM_V_P,".",#030+ROM_V_S,#0D,#0A,#0A
              DEFM "(c) 2025 by Thomas Wiesner",#0D,#0A,#0A
              DEFM "Switched-IO based on ASCII(R)",#0D,#0A
              DEFM "Vendor ID: &HC0",#0D,#0A,#0A
              DEFM "UNAPI design (c) by Konamiman",#0D,#0A,#0A
              DEFM "CALL design (c) by www.msx.org",#0D,#0A,#0A
              DEFM "Type 'CALL RCX ?' for HELP under BASIC",#0D,#0A,#0A
              DEFM "Card installed on slot ",0
PMEMERR:      DEFM "Not enough RAM left to install [RCX]",#0D,#0A,#0A,0
PCRLF:        DEFM #0D,#0A,0
PISINIT:      DEFM "[RCX] already installed",#0D,#0A,0
PINIT:        DEFM "[RCX] installed",#0D,#0A,0
PERRINIT:     DEFM "[RCX] not installed",#0D,#0A,0
PERRNOFRESLT: DEFM "[RCX] no free RCX slot found",#0D,#0A,0
PERRNOSLT:    DEFM "[RCX] no suitable RCX card found",#0D,#0A,0
PERRADDR:     DEFM "[RCX] IO address already in use",#0D,#0A,0
PERRINITX:    DEFM "[XIO] not installed",#0D,#0A,0
PERRINITY:    DEFM "[XIO] not present",#0D,#0A,0
PERRI2CBUS1:  DEFM "[I2C] Bus error. Bus is too busy.",#0D,#0A,0
PERRI2CBUS2:  DEFM "[I2C] Bus error. Cmd not executable.",#0D,#0A,0
PERRSJA:      DEFM "[CAN] SJA1000 not found",#0D,#0A,0
PERRSJAINI:   DEFM "[CAN] SJA1000 already installed",#0D,#0A,0
PERRVAL:      DEFM "Value of parameter ",0
PERRVAL1:     DEFM " must be 0 .. 255",#0D,#0A,0
PERRVAL2:     DEFM " must be 0 .. 7",#0D,#0A,0
PERRVAL3:     DEFM " must be 0 .. 3",#0D,#0A,0
PERRTYPE:     DEFM "Type of parameter ",0
PERRTYPEIV:   DEFM " must be integer variable",#0D,#0A,0
PERRTYPEI:    DEFM " must be integer",#0D,#0A,0
PERRTYPES:    DEFM " must be string",#0D,#0A,0
PERRARRAY1:   DEFM "Array in parameter 1 must be DIM 1",#0D,#0A,0
PERRARRAY2:   DEFM "Array in parameter 1 must be LEN 8",#0D,#0A,0
PERRARRAY3:   DEFM "Array in parameter 1 must be LEN 12",#0D,#0A,0
PERRTYPESL:   DEFM "Length of string not correct",#0D,#0A,0
PHELP:        DEFM "[RCX] HELP - BASIC commands",#0D,#0A,#0A
              DEFM "[RCX] BASIC CMD 'RCX ?'",#0D,#0A
              DEFM "FORMAT: _RCX ?",#0D,#0A,#0A
              DEFM "[RCX] BASIC CMD 'RCX INIT'",#0D,#0A
              DEFM "FORMAT: _RCX INIT",#0D,#0A,#0A
              DEFM "[RCX] BASIC CMD 'RCX ISINIT'",#0D,#0A
              DEFM "FORMAT: _RCX ISINIT(FLAG)",#0D,#0A,#0A
              DEFM "[RCX] BASIC CMD 'RCX DEINIT'",#0D,#0A
              DEFM "FORMAT: _RCX DEINIT",#0D,#0A,#0A
              DEFM "[RCX] BASIC CMD 'RCX TABLE'",#0D,#0A
              DEFM "FORMAT: _RCX TABLE(ADDR)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPA1:      DEFM "[TYPE A] I2C-Bus & Clock (1)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C INIT'",#0D,#0A
              DEFM "FORMAT: _I2C INIT(BUS)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C WR'",#0D,#0A
              DEFM "FORMAT: _I2C WR(BUS,CHIP,DATA)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C WRB'",#0D,#0A
              DEFM "FORMAT: _I2C WRB(BUS,CHIP,LEN,DATA)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C RD'",#0D,#0A
              DEFM "FORMAT: _I2C RD(BUS,CHIP,DATA)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C RDB'",#0D,#0A
              DEFM "FORMAT: _I2C RDB(BUS,CHIP,LEN,DATA)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPA2:      DEFM "[TYPE A] I2C-Bus & Clock (2)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'I2C RESET'",#0D,#0A
              DEFM "FORMAT: _I2C RESET(BUS)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'SET TIME'",#0D,#0A
              DEFM "FORMAT: _SET TIME(BUS,TIME$)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'GET TIME'",#0D,#0A
              DEFM "FORMAT: _GET TIME(BUS,TIME$)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'SET DATE'",#0D,#0A
              DEFM "FORMAT: _SET DATE(BUS,DATE$)",#0D,#0A,#0A
              DEFM "[I2C] BASIC CMD 'GET DATE'",#0D,#0A
              DEFM "FORMAT: _GET DATE(BUS,DATE$)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPB1:      DEFM "[TYPE B] Z80 PIO (1)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO INIT'",#0D,#0A
              DEFM "FORMAT: _PIO INIT(ADDR)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO A RD'",#0D,#0A
              DEFM "FORMAT: _PIO A RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO A WR'",#0D,#0A
              DEFM "FORMAT: _PIO A WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO B RD'",#0D,#0A
              DEFM "FORMAT: _PIO B RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO B WR'",#0D,#0A
              DEFM "FORMAT: _PIO B WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPB2:      DEFM "[TYPE B] Z80 PIO (2)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO A CTRL'",#0D,#0A
              DEFM "FORMAT: _PIO A CTRL(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PIO] BASIC CMD 'PIO B CTRL'",#0D,#0A
              DEFM "FORMAT: _PIO B CTRL(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPC1:      DEFM "[TYPE C] 82C55 (1)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI INIT'",#0D,#0A
              DEFM "FORMAT: _PPI INIT(ADDR)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI A RD'",#0D,#0A
              DEFM "FORMAT: _PPI A RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI A WR'",#0D,#0A
              DEFM "FORMAT: _PPI A WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI B RD'",#0D,#0A
              DEFM "FORMAT: _PPI B RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI B WR'",#0D,#0A
              DEFM "FORMAT: _PPI B WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPC2:      DEFM "[TYPE C] 82C55 (2)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI C RD'",#0D,#0A
              DEFM "FORMAT: _PPI C RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI C WR'",#0D,#0A
              DEFM "FORMAT: _PPI C WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI CTRL'",#0D,#0A
              DEFM "FORMAT: _PPI CTRL(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI SET'",#0D,#0A
              DEFM "FORMAT: _PPI SET(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[PPI] BASIC CMD 'PPI RESET'",#0D,#0A
              DEFM "FORMAT: _PPI RESET(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPD1:      DEFM "[TYPE D] 82C54 (1)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER INIT'",#0D,#0A
              DEFM "FORMAT: _TIMER INIT(ADDR)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 0 RD'",#0D,#0A
              DEFM "FORMAT: _TIMER 0 RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 0 WR'",#0D,#0A
              DEFM "FORMAT: _TIMER 0 WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 1 RD'",#0D,#0A
              DEFM "FORMAT: _TIMER 1 RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 1 WR'",#0D,#0A
              DEFM "FORMAT: _TIMER 1 WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPD2:      DEFM "[TYPE D] 82C54 (2)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 2 RD'",#0D,#0A
              DEFM "FORMAT: _TIMER 2 RD(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER 2 WR'",#0D,#0A
              DEFM "FORMAT: _TIMER 2 WR(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER CTRL'",#0D,#0A
              DEFM "FORMAT: _TIMER CTRL(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER SET GATE'",#0D,#0A
              DEFM "FORMAT: _TIMER SET GATE(ADDR,VAL)",#0D,#0A,#0A
              DEFM "[TIMER] BASIC CMD 'TIMER RES GATE'",#0D,#0A
              DEFM "FORMAT: _TIMER RES GATE(ADDR,VAL)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPE1:      DEFM "[TYPE E] SJA1000 (1)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN INIT'",#0D,#0A
              DEFM "FORMAT: _CAN INIT(ADDR,ACC)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN WR'",#0D,#0A
              DEFM "FORMAT: _CAN WR(ADDR,REG,DATA)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN RD'",#0D,#0A
              DEFM "FORMAT: _CAN RD(ADDR,REG,DATA)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN CHECK'",#0D,#0A
              DEFM "FORMAT: _CAN CHECK(ADDR,RMC)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN RX'",#0D,#0A
              DEFM "FORMAT: _CAN RX(ADDR,DATA)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPE2:      DEFM "[TYPE E] SJA1000 (2)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN GET ADDR'",#0D,#0A
              DEFM "FORMAT: _CAN GET ADDR(ADDR,INTADDR)",#0D,#0A,#0A
              DEFM "[CAN] BASIC CMD 'CAN TX'",#0D,#0A
              DEFM "FORMAT: _CAN TX(ADDR,DATA)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0

              DEFS X_ROM-$,#00 


;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Working area (X_WRK) to be copied to reserved RAM                +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************ 
                                    ;* Module variables                 *
                                    ;************************************ 

HFLAGS:       DEFB #00              ; RCX init flag
HGENPURP:     DEFW #00              ; 16 bit free memory
HCANPTR:      DEFW #00              ; CAN Msg Count, incoming / read
HCANMSG:      DEFS #1A0,#00         ; CAN Msg Buffer, 32 messages x 13 Bytes
HCARD:        DEFS X_CD*X_CDSTEP,#00; Usage of slot (type/IO)

                                    ;************************************ 
                                    ;* CAN interrupt routine            *
                                    ;************************************

HCANINT:      LD A,WTEC
              OUT (IOENDIS),A

HCAN001:      LD HL,0               ; Will be filled when RXA is init
                                    ; HL points to CAN msg buffer start

HCAN002:      LD DE,0               ; Will be filled when RXA is init
                                    ; DE points to CAN msg count

              LD A,(DE)             ; Get CAN msg count
              LD B,A                ; B contains CAN msg count

HCAN003:      LD C,0                ; Will be filled when RXA is init
                                    ; C contains IO address

              ADD A,#01             ; Increase cnt of msg
              CP X_CANCNT
              JR NZ,HCANINTL
              LD A,#00              ; If max cnt set ring index to 0

HCANINTL:     LD (DE),A             ; Save new CAN msg count

              LD DE,#00D            ; 13 Bytes to be read

              SCF
              CCF                   ; Clear CARRY flag

              SBC HL,DE             ; Prepare index

HCANLOOP1:    ADD HL,DE             ; Move pointer to next buffer to be written 
              DJNZ HCANLOOP1        ; Do loop

              PUSH HL               ; HL points to next buffer to be written
              POP IX                ; IX points to active buffer index

              LD E,C
              INC E
              
              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANINTREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000  
              LD A,E 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)           ; Get Data from SJA1000

              LD D,CANRXBUFREG
              LD B,#0D              ; Set loop for 13 bytes

HCANINTL1:    LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,D                ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,E
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)           ; Get Data from SJA1000
              LD (IX),A             ; Save result in (IX)

              INC IX                ; Increase read buffer index 
              INC D                 ; Increase RX FIFO place
              
              DJNZ HCANINTL1        ; Do loop
              
              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANCMDREG        ; D contains SJA1000 Register addr
              OUT (SWIO),A          ; Write ALE to SJA1000  
              LD A,E 
              OUT (ALE),A           ; Set Data transfer to SJA1000
              LD A,CANRELRXBUF      ; E contains value
              OUT (SWIO),A          ; Release RX Interrupt

                                    ; Still new msg?

              LD A,C                ; C contains IO addr
              OUT (ALE),A           ; Set ALE to SJA1000
              LD A,CANRMC           ; RMC
              OUT (SWIO),A          ; Write ALE to SJA1000   
              LD A,E
              OUT (ALE),A           ; Set Data transfer to SJA1000
              IN A,(SWIO)           ; Any more new messages?
              OR A
              JR NZ,HCAN001         ; Jump if more new msg
                                    ; Otherwise the interrupt will not
                                    ; go back to edge

              LD A,0
              OUT (IOENDIS),A

              RET

HENDE:        DEFB #00

TEND:         DEFS #08000-$
              END
