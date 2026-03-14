
                                    ;************************************
                                    ;* MSX-IO-Expander                  * 
                                    ;************************************
                                    ;* (c) 2023 by Dr.-Ing. Th. Wiesner *
                                    ;* Based on the UNAPI ROM standard  *
                                    ;* by KONAMIMAN 2019                *
                                    ;* and CALL statement posted on     *
                                    ;* https://www.msx.org              *
                                    ;************************************
 
              ORG    #04000

;---------------------------

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
PTRGET:       EQU    #05EA4         ; BASIC routine get variable ptr
CHKALF:       EQU    #064A7         ; BASIC routine get flag C if variable 
FRESTR:       EQU    #067D0         ; BASIC routine clear string
VALTYP:       EQU    #0F663         ; BASIC type of parameter
MEMSIZ:       EQU    #0F672         ; Stringarea
STKTOP:       EQU    #0F674         ; Stacktop
SAVSTK:       EQU    #0F6B1         ; Pointer to valid stack bottom
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

                                    ;************************************
                                    ;* IO addresses of peripherie       *
                                    ;************************************

WTEC:         EQU    #0C0           ; Assembly ID
WTECINV:      EQU    #03F           ; Complement of ID
IOENDIS:      EQU    #040           ; Switched IO En/Dis
GETINT:       EQU    #041           ; Get INT-Source PIC = 0xC0, Z80 = 0x00
PICCTRL0:     EQU    #042           ; PIC-A0
PICCTRL1:     EQU    #043           ; PIC-A1
INTA:         EQU    #044           ; PIC-INTA-Vector read
IM2:          EQU    #045           ; Z80-INT-Vector read
ALE:          EQU    #046           ; IO-Address set
SWIO:         EQU    #047           ; IO-Data read/write

                                    ;************************************
                                    ;* PIC-Service-Values               *
                                    ;************************************

ICW1:         EQU    #012           ; Intervall = 8, EDGE-Triggered, 8080-Mode          
ICW2:         EQU    #000           ; Address = 0 (will not be used)
OCW2:         EQU    #020           ; EOI trigger

                                    ;************************************
                                    ;* HFLAGS - Bit field               *
                                    ;************************************

FINIT:        EQU    #000           ; Bit 0 = Init

                                    ;************************************
                                    ;* Alias for working area           *
                                    ;************************************

X_ROM:        EQU    #07000         ; Start address in ROM
X_HINTPIC:    EQU    #00000         ; Relative address in Main RAM Page 3
X_HINTZ80:    EQU    #00028         ; Relative address in Main RAM Page 3
X_HKEYIALT:   EQU    #00050         ; Relative address in Main RAM Page 3
X_HERROALT:   EQU    #00055         ; Relative address in Main RAM Page 3
X_HFLAGS:     EQU    #0005A         ; Relative address in Main RAM Page 3
X_HINTMASK:   EQU    #0005B         ; Relative address in Main RAM Page 3
X_HVARPTR:    EQU    #0005D         ; Relative address in Main RAM Page 3
X_HKENNUNG:   EQU    #0005F         ; Relative address in Main RAM Page 3
X_HENDE:      EQU    #00061         ; Relative address in Main RAM Page 3

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

UNAPI_ID:     DEFM  "XIO_IO_EXPANDER",0

                                    ; Implementation identifier 
                                    ; (up to 63 chars and zero terminated)

APIINFO:      DEFM  "XIO IO Expander UNAPI ROM",0

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

MAX_FN:       EQU  10               ; Must be 0 to 127

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
              DEFM "XIO ?",0        ; Call HELP
              DEFW B_CORE1

              DEFM "XIO OUT",0      ; Call OUT
              DEFW B_CORE2

              DEFM "XIO INP",0      ; Call INP
              DEFW B_CORE3

              DEFM "XIO INIT",0     ; Call INIT
              DEFW B_CORE4

              DEFM "XIO ISINIT",0   ; Call ISINIT
              DEFW B_CORE5

              DEFM "XIO DEINIT",0   ; Call DEINIT
              DEFW B_CORE6

              DEFM "XIO SET ADDR",0 ; Call SET ADDR
              DEFW B_CORE7
              
              DEFM "XIO SET MASK",0 ; Call SET MASK
              DEFW B_CORE8

              DEFM "XIO GET ADDR",0 ; Call GET ADDR
              DEFW B_CORE9

              DEFM "XIO GET MASK",0 ; Call GET MASK
              DEFW B_CORE10
                             
              DEFB 0                ; No more instructions

;---------------------------

                                    ;************************************
                                    ;* EXTBIOS                          *
                                    ;* FUNCTIONS ADDRESSES TABLE        *
                                    ;************************************

                                    ; TODO: Adjust for the routines 
                                    ; of your implementation

FN_TABLE:
              DEFW  FN_INFO         ; Call INFO

              DEFW  FN_CORE1        ; Call HELP

              DEFW  FN_CORE2        ; CALL OUT

              DEFW  FN_CORE3        ; CALL INP

              DEFW  FN_CORE4        ; CALL INIT

              DEFW  FN_CORE5        ; CALL INIT

              DEFW  FN_CORE6        ; CALL DEINIT

              DEFW  FN_CORE7        ; CALL SET ADDR

              DEFW  FN_CORE8        ; CALL SET MASK

              DEFW  FN_CORE9        ; CALL GET ADDR

              DEFW  FN_CORE10       ; CALL GET MASK

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
;+++ 1. BASIC entry                                                   +++
;+++ 2. EXTBIOS entry                                                 +++
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

;---------------------------

                                    ;************************************
                                    ;* BASIC                            * 
                                    ;* CALL XIO ?                       *
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

              LD HL,PHELPA
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1HM1
              CP 'P'
              JR Z,CORE1HM
              CP 'Z'
              JR Z,CORE1END
              JR CORE1HM            ; Invalid char. Go back to beginning

CORE1HM1:     CALL CLEARSCR

              LD HL,PHELP 
              CALL PROMPT           ; Meldung auf Screen ausgeben

              LD HL,PHELPB
              CALL PROMPT           ; Meldung auf Screen ausgeben

              CALL CHGET            ; Get character from keyboard
              CALL TOUPPER

              CP 'N'
              JR Z,CORE1END
              CP 'P'
              JR Z,CORE1HM
              CP 'Z'
              JR Z,CORE1END
              JR CORE1HM1           ; Invalid char. Go back to beginning

CORE1END:     CALL CLEARSCR

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO OUT(ADDR,DATA)          *
                                    ;************************************ 
                                    ;* Checks, if ADDR and DATA are     *
                                    ;* valid                            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: DATA                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = ADDR                         *
                                    ;* B = DATA                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, BC                           *
                                    ;************************************

B_CORE2:      CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR

              LD C,A                ; Save IO address
              PUSH BC               ; Save io address on stack
              
              CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              LD B,1
              CALL EVALBYTEVAR

              POP BC                ; Save IO data on stack
              LD B,A                
              PUSH BC
              
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              POP BC                ; Get IO Address and data back

              JP CORE2              ; Continue with CORE command


                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 2                    *
                                    ;************************************ 
                                    ;* pass through                     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = ADDR                         *
                                    ;* B = DATA                         *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* C = ADDR                         *
                                    ;* B = DATA                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************

FN_CORE2:     JP CORE2              ; Continue with CORE command

                                    ;************************************ 
                                    ;* OUTPUT                           *
                                    ;************************************ 
                                    ;* Sends DATA to Switched           *
                                    ;* IO Port ADDR                     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* C = ADDR                         *
                                    ;* B = DATA                         *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE2:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)

              DI

              CALL IOEN             ; Enable Switched IO

              LD A,C                ; Set IO address
              OUT (ALE),A
              LD A,B                ; Send data
              OUT (SWIO),A

              CALL IODIS            ; Disable Switched IO

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO INP(ADDR,DATA)          *
                                    ;************************************ 
                                    ;* Checks, if ADDR and DATA are     *
                                    ;* valid                            *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: ADDR                      *
                                    ;* PARA2: DATA                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = varptr(DATA)                *
                                    ;* C = ADDR                         * 
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, BC                       *
                                    ;************************************

B_CORE3:      CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Test integer or variable

              LD C,A                ; Save IO addr
              PUSH BC               ; Save IO addr on stack
                            
              CALL CHKCHAR
              DEFB ","              ; Check for ,

              LD B,1
              CALL EVALVARINT       ; Test variable, DE contains varptr
         
              CALL CHKCHAR
              DEFB ")"              ; Check for )
              POP BC                ; Get IO addr from stack

              JP CORE3              ; Continue with CORE command

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 3                    *
                                    ;************************************ 
                                    ;* pass through                     *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(DATA)                *
                                    ;* C = ADDR                         * 
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = varptr(DATA)                *
                                    ;* C = ADDR                         *     
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* -/-                              *
                                    ;************************************


FN_CORE3:     JP CORE3              ; Continue with CORE command


                                    ;************************************ 
                                    ;* INPUT                            *
                                    ;************************************
                                    ;* Gets DATA from Switched          *
                                    ;* IO Port ADDR                     *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(DATA)                *
                                    ;* C = ADDR                         * 
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (DE) = DATA                      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, (DE)                         *
                                    ;************************************

CORE3:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)

              DI

              CALL IOEN             ; Enable Switched IO

              LD A,C                ; Set IO address
              OUT (ALE),A
              IN A,(SWIO)           ; Get data
              LD (DE),A             ; Save data in variable
              INC DE                ; Delete high part of integer
              LD A,0
              LD (DE),A 
              DEC DE

              CALL IODIS            ; Disable Switched IO

              EI
     
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO INIT(INTCH)             *
                                    ;************************************ 
                                    ;* Checks, if working area is       *
                                    ;* reserved, working area is pre-   *
                                    ;* pared and module is init         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* Varptr(INTCH) = INTCH            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = Varptr(INTCH)               *
                                    ;* (SLTWORK+6/7) = RAM base address *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, BC, IX                   *
                                    ;************************************

B_CORE4:      CALL CHKCHAR
              DEFB "("              ; Check for (

              LD B,0
              CALL EVALVARINT       ; Test variable, DE contains varptr

              CALL CHKCHAR
              DEFB ")"              ; Check for )

B_CORE4PREP:  PUSH HL               ; Save BASIC Line-Pointer
              PUSH DE               ; [SP] = varptr
              LD DE,X_HKENNUNG
              CALL GETRAMIX
              PUSH HL               ; [SP] = BASE+X_HKENNUNG
              POP BC                ; [BC] = BASE+X_HKENNUNG
              LD A,(BC)
              CP 'W'
              JP NZ,B_CORE4RAM      ; No? then install module 
              INC BC
              LD A,(BC)
              CP 'T'                ; No? then install module
              JP NZ,B_CORE4RAM
                            
B_CORE4INIT:  LD DE,X_HFLAGS
              CALL GETRAMIX
              LD A,(HL)             ; Already installed?
              BIT FINIT,A           ; Test BIT 0 
              JP Z,B_CORE4RAME      ; No? Continue with CORE command
              LD HL,PISINIT         ; Yes? Prompt message 
              CALL PROMPT           ; that module is already installed
              POP DE                ; Clean SP
              POP HL                ; Get BASIC Line-Pointer
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS

B_CORE4RAM:   LD HL,HENDE-X_ROM     ; Prepare working area  
              CALL ALLOC
              JP C,CORE4MEMERR      ; Carry Flag set = Error
              CALL SETRAM
              CALL DEFILE 

B_CORE4RAME:  POP DE                ; [DE] = varptr
              POP HL                ; Get BASIC Line-Pointer

              JP CORE4

CORE4MEMERR:  LD HL,PMEMERR         ; Yes? Prompt message 
              CALL PROMPT           ; that module is already installed
              POP DE                ; [DE] = varptr
              POP HL                ; Get BASIC Line-Pointer
              OR A                  ; Clear Carry Flag to end routine handler
              LD E,5
              LD IX,ERRHAND         ; Call the Basic error handler
              JP CALBAS
              

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 4                    *
                                    ;************************************ 
                                    ;* Checks, if working area is       *
                                    ;* reserved, working area is pre-   *
                                    ;* pared and module is init         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = Varptr(INTCH)               *
                                    ;* HL = RAM base address            *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = Varptr(INTCH)               *
                                    ;* (SLTWORK+6/7) = RAM base address *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE4:     CALL SETRAM           ; Save RAM base address in Slot Work

              PUSH BC
              PUSH DE
              LD DE,X_HKENNUNG
              CALL GETRAMIX
              PUSH HL               
              POP BC                ; [BC] = content RAM
              POP DE
              LD A,(BC)
              CP 'W'
              JP NZ,FN_CORE4_GO     ; No? then install module 
              INC BC
              LD A,(BC)
              CP 'T'                ; No? then install module
              JP NZ,FN_CORE4_GO
                            
              PUSH DE
              LD DE,X_HFLAGS
              CALL GETRAMIX
              POP DE
              LD A,(HL)             ; Already installed?
              BIT FINIT,A           ; Test BIT 0 
              JP Z,FN_CORE4_GO      ; No? Continue with CORE command
              
              POP BC
              SCF                   ; Set Carry Flag for ERROR 
              RET                   ; Go back to main routine

FN_CORE4_GO:  POP BC
              JP CORE4              ; Continue with CORE command


                                    ;************************************ 
                                    ;* PIC Initialization               *
                                    ;************************************ 
                                    ;* Installs working area and 82C59A *
                                    ;* Prepares INT vector table for    *
                                    ;* PIC and Z80-IM2                  *
                                    ;* Save old HKEYI and sets PIC      *
                                    ;* service address as new handler   *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = Varptr(INTCH)               * 
                                    ;* (SLTWORK+6/7) = RAM base address *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (X_HFLAGS) Bit FINIT set         *
                                    ;* (X_HINTMASK) = 0                 *
                                    ;* (X_HVARPTR) = Varptr(INTCH)      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF,IX                            *
                                    ;************************************

CORE4:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
              PUSH DE               ; Save VARPTR to stack

              CALL GETRAM           ; Obtain RAM base address
              LD BC,HENDE-X_ROM     ; Prepare working area 
              EX DE,HL
              LD HL,X_ROM
              LDIR                  
                                                
              LD DE,X_HVARPTR       ; VARPTR INT ID
              CALL GETRAMIX         ; [HL] = RAM base address + DE

              POP DE                ; Get VARPTR back from stack

              LD (HL),E             ; Save VARPTR to RAM
              INC HL
              LD (HL),D

              EX DE,HL              ; [HL] = VARPTR

              LD A,0
              LD (HL),A
              INC HL
              LD (HL),A             ; INT ID = 0
                     
              DI                    ; Disable interrupts
              
              CALL IOEN             ; Enable Switched IO

              LD A,ICW1             ; ICW1
              OUT (PICCTRL0),A 
              LD A,ICW2             ; ICW2
              OUT (PICCTRL1),A
              LD A,#0FF             ; All 8259 interrupts off
              OUT (PICCTRL1),A
                             
              CALL IODIS            ; Disable Switched IO
             
              LD DE,X_HINTMASK      ; INT Mask
              CALL GETRAMIX         ; [HL] = RAM base address + DE
              LD A,#00              ; Save INT Mask 
              LD (HL),A

CORE4INTSET:  LD DE,X_HKEYIALT      ; Target address old INT pointer              
              CALL GETRAMIX         ; [HL] = RAM base address + DE
              EX DE,HL
              LD HL,HKEYI           ; Source address old INT pointer
              LD BC,#05
              LDIR                  ; Save old INT pointer

              LD A,#0F7             ; code for "RST #030"
              LD (HKEYI),A
              CALL GETSLT           ; Get slot number
              LD (HKEYI+1),A
              LD HL,INTSRV
              LD (HKEYI+2),HL       ; Save new INT pointer
              LD A,#0C9             ; code for "RET"
              LD (HKEYI+4),A

CORE4AINTTAB: LD DE,X_HINTPIC       ; Target address PIC INT pointer
              CALL GETRAMIX         ; [HL] = RAM base address + DE
              CALL GETSLT           ; Get slot number
              LD B,16               ; PIC+Z80 to be filled
CORE4L1:      INC HL
              LD (HL),A             ; Save slot number
              INC HL
              INC HL
              INC HL
              INC HL
              DJNZ CORE4L1

CORE4INITS:   LD DE,X_HFLAGS
              CALL GETRAMIX
              LD (HL),#01           ; Sign module as initialized
             
              CALL IOEN             ; Enable Switched IO

              IN A,(GETINT)         ; Get INT source to set INT-FF

              CALL IODIS            ; Disable Switched IO
   
              EI                    ; Enable interrupts

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler

              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO ISINIT(ISI)             *
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
                                    ;* DE = Varptr(INTCH)               *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE, BC, IX                   *
                                    ;************************************

B_CORE5:      CALL CHKCHAR
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
              DEC DE

              CALL MODINIT          ; Module installed?
              JP Z,B_CORE5ISI    
              LD A,0
              LD (DE),A             ; No? Set ISI = 0 

              RET                   ; Go back to main routine

B_CORE5ISI:   LD A,1                ; Yes? Set ISI = 1 
              LD (DE),A

              RET                   ; Go back to main routine             

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 5                    *
                                    ;************************************ 
                                    ;* Checks, if working area is       *
                                    ;* reserved, working area is pre-   *
                                    ;* pared and module is init         *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = Varptr(INTCH)               *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (X_HVARPTR) = Varptr(INTCH)      *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE5:     INC DE
              LD A,0
              LD (DE),A
              DEC DE

              CALL MODINIT          ; Module installed?
              JP Z,FN_CORE5ISI    
              LD A,0
              LD (DE),A             ; No? Set ISI = 0 

              RET                   ; Go back to main routine

FN_CORE5ISI:  LD A,1                ; Yes? Set ISI = 1 
              LD (DE),A

              RET                   ; Go back to main routine


;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO DEINIT                  *
                                    ;************************************ 
                                    ;* Direct Jump to CORE command      *
                                    ;* after checking if module is      *
                                    ;* installed                        *
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
              
B_CORE6:      CALL MODINIT          ; Module installed?
              JP Z,CORE6            ; Yes? Continue with CORE command 
              CALL MODERR           ; Prompt error no XIO
              RET                   ; Go back to calling routine

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 6                    *
                                    ;************************************
                                    ;* Direct Jump to CORE command      *
                                    ;* after checking if module is      *
                                    ;* installed                        *
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

FN_CORE6:     CALL MODINIT          ; Module installed?
              JP Z,CORE6            ; Yes? Continue with CORE command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* PIC Deinitialization             *
                                    ;************************************
                                    ;* Uninstalls 82C59A                *
                                    ;* Clears INT vector table for      *
                                    ;* PIC and Z80-IM2                  *
                                    ;* Sets old HKEYI as new Handler    *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* -/-                              * 
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (X_HFLAGS) Bit FINIT reset       *
                                    ;* (X_HINTMASK) = 0                 *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************
                                    
CORE6:        DI                    ; Disable INT

              PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
              PUSH BC
              PUSH DE

CORE6START:   CALL IOEN             ; Enable Switched IO
              
CORE6PICOFF:  LD A,#0FF             ; All 8259 interrupts off
              OUT (PICCTRL1),A      ; Set INT mask to 0x00

CORE6INTFF:   LD A,#00               
              OUT (GETINT),A        ; Reset INT-FF
              
              CALL IODIS            ; Disable Switched IO
              
CORE6INTOFF:  LD DE,X_HKEYIALT
              CALL GETRAMIX         ; Source address old INT handler
              LD DE,HKEYI           ; Target address old INT handler
              LD BC,#05
              LDIR                  ; Save old INT handler back

CORE6RAMOFF:  LD DE,X_HFLAGS
              CALL GETRAMIX
              LD (HL),#00           ; Sign module as not initialized              
    
              POP DE
              POP BC
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)

              EI                    ; Enable INT
    
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine



;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO SET ADDR (CH,ADDR)      *
                                    ;************************************ 
                                    ;* Checks, if ADDR and CH are       *
                                    ;* valid                            *
                                    ;* ADDR must be between &HD000 ..   *
                                    ;* &HDFFF                           *
                                    ;* CH must be 0 .. 15               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CHANNEL                   *
                                    ;* PARA2: INT addr                  *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = INT addr                    *
                                    ;* B = CHANNEL                      *
                                    ;* C = Slot (BASIC = 0xff)          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

B_CORE7:      CALL MODINIT          ; Module installed?
              JP Z,B_CORE7START     ; Yes? Continue  
              CALL MODERR           ; Prompt error no XIO
              RET                   ; Go back to calling routine

B_CORE7START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR      ; Check parameter 0

              LD B,A                ; Save channel
              CP #10                ; Check channel
              JP C,B_CORE7SAVE1     ; Call value > 15? Call ERROR

              PUSH HL
              LD HL,PERRVAL
              CALL PROMPT
              LD A,#030
              CALL CHPUT
              LD HL,PERRVAL2
              CALL PROMPT
              POP HL

              JP B_ERRVAL
              
B_CORE7SAVE1: CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              PUSH BC
              LD B,1                ; Check parameter 1
              CALL EVALINTVAR       ; [DE] = INT addr
              POP BC

              LD A,D
              OR E                  ; Address = &H0000?
              JP Z,B_CORE7SAVE2     ; Address reset forced
              LD A,D
              AND #0F0
              CP #0E0               ; Address = &HExxx?
              JP Z,B_CORE7SAVE2     ; Handler = &HExxx, go on
              
              PUSH HL
              LD HL,PERRADDR
              CALL PROMPT
              POP HL

              JP B_ERRVAL

B_CORE7SAVE2: CALL CHKCHAR
              DEFB ")"              ; Check for )

              LD C,#0FF             ; Set SLOT to BASIC Call

              JP CORE7              ; Continue with CORE command

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 7                    *
                                    ;************************************ 
                                    ;* Checks, if CH is valid           *
                                    ;* CH must be 0 .. 15               *
                                    ;* If INT addr on PAGE 3, then      *
                                    ;* handle as if it is a BASIC CAll  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = INT addr                    *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = INT addr                    *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE7:     CALL MODINIT          ; Module installed?
              JP Z,FN_CORE7SV       ; Yes? Continue
              SCF                   ; Set CARRY Flag for ERROR
              RET                   ; Go back to main routine

FN_CORE7SV:   LD A,B
              CP #010               ; Call value < 16?
              JP C,CORE7            ; Yes? Continue with CORE command
              SCF                   ; Set CARRY Flag for ERROR
              RET                   ; Go back to main routine
              
   

                                    ;************************************ 
                                    ;* Set INT Handler for each channel *
                                    ;************************************ 
                                    ;* Sets INT Handler to the          *
                                    ;* sign channel for PIC (0..7)      *
                                    ;* Z80-IM2 (8..15)                  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = INT addr                    *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
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
              PUSH BC               ; Save Channel and slot
              PUSH DE               ; Save INT addr

              LD A,B
              CP #08                ; Check if PIC (0..7)
              JP C,CORE7PIC         ; if PIC jump over

CORE7Z80:     LD B,#08              ; Sub 8 from A 
              SUB B                 ; for target address for Z80
              LD B,A                ; Save vector index
              LD A,D                ; Address reset forced?
              LD C,E
              OR C                  ; Addr != 0? Jump to address setting
              JP NZ,CORE7SETZ80
              POP DE
              POP BC
              LD DE,INTZ80ST        ; Yes? Reset to standard
              CALL GETSLT
              LD C,A                ; Slot set to module
              PUSH BC
              PUSH DE

CORE7SETZ80:  LD DE,X_HINTZ80
              CALL GETRAMIX         ; [HL] = Get refered routine addr table
              JP CORE7INTGET
              
CORE7PIC:     LD B,A                ; Save vector index
              LD A,D                ; Address reset forced?
              LD C,E
              OR C                  ; No? Jump to address setting
              JP NZ,CORE7SETPIC
              POP DE
              POP BC
              LD DE,INTPICST        ; Yes? Reset to standard
              CALL GETSLT
              LD C,A                ; Slot set to module
              PUSH BC
              PUSH DE

CORE7SETPIC:  LD DE,X_HINTPIC
              CALL GETRAMIX         ; [HL] = Get refered routine addr table
        
CORE7INTGET:  LD DE,#05             ; Get HOOK position
              LD A,B
              ADD A,#01
              OR A
              LD B,A                ; Get loop index
              SBC HL,DE
CORE7L1:      ADD HL,DE
              DJNZ CORE7L1          ; HL points to refered HOOK
                            
CORE7INT:     POP DE                ; Get INT addr back
              POP BC                ; Get channel and slot back
              LD A,C                ; Check for BASIC / Page 3 Call
              CP #0FF
              JP Z,CORE7INTBAS      ; BASIC / Page 3 Call?

CORE7INTMC:   DI
              LD A,#0F7             ; code for "RST 30h"
              LD (HL),A
              INC HL
              LD A,C
              LD (HL),A             ; Save slot number
              INC HL
              LD (HL),E             ; Save new INT pointer
              INC HL
              LD (HL),D
              INC HL
              LD A,#0C9             ; code for "RET"
              LD (HL),A
              EI

              JP CORE7END

CORE7INTBAS:  DI
              LD A,#00              ; code for "NOP"
              LD (HL),A             ; allows same position of ADDR pointer 
              INC HL                ; for interslot and direct calls
              LD A,#0C3             ; code for "JP"
              LD (HL),A
              INC HL
              LD (HL),E             ; Save new INT pointer
              INC HL
              LD (HL),D
              INC HL
              LD A,#0C9             ; code for "RET"
              LD (HL),A
              EI

CORE7END:     POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO SET MASK (MASK)         *
                                    ;************************************
                                    ;* Checks for valid mask            *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: MASK                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = MASK                        *
                                    ;************************************                                    
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE                           *
                                    ;************************************

B_CORE8:      CALL MODINIT          ; Module installed?
              JP Z,B_CORE8START     ; Yes? Continue 
              CALL MODERR           ; Prompt error no XIO
              RET                   ; Go back to calling routine

B_CORE8START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              PUSH BC
              LD B,0
              CALL EVALINTVAR      
              POP BC
              
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              JP CORE8              ; Continue with CORE command



                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 8                    *
                                    ;************************************ 
                                    ;* Save register to variable        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = MASK                        *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = MASK                        *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************


FN_CORE8:     CALL MODINIT          ; Module installed?
              JP Z,CORE8            ; Yes? Continue with CORE command
              SCF                   ; Set CARRY flag for ERROR 
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* Set INT Mask for each PIC channel*
                                    ;************************************ 
                                    ;* Sets INT Mask for PIC (0..7)     *
                                    ;* and IM2 (8..15)                  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* B = MASK                         *
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

              DI                    ; Disable INT
              
              CALL IOEN             ; Enable Switched IO
              
              PUSH DE
              LD DE,X_HINTMASK
              CALL GETRAMIX
              POP DE
              LD A,E                ; Save E to MASK variable for PIC
              LD (HL),A 
              CPL                   ; Invert INT mask
              OUT (PICCTRL1),A      ; Send mask to 8259
              INC HL
              LD (HL),D             ; Save D to MASK variable for IM2

              CALL IODIS            ; Disable INT mask

              EI                    ; Enable INT

              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO GET ADDR (CH,ADDR)      *
                                    ;************************************ 
                                    ;* Checks, if CH is valid           *
                                    ;* CH must be 0 .. 15               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: CHANNEL                   *
                                    ;* PARA2: ADDR                      *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = varptr(INT addr)            *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, B                            *
                                    ;************************************

B_CORE9:      CALL MODINIT          ; Module installed?
              JP Z,B_CORE9START     ; Yes? Continue 
              CALL MODERR           ; Prompt error no XIO
              RET                   ; Go back to calling routine

B_CORE9START: CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALBYTEVAR
              LD B,A                ; Save channel

              CP #10                ; Check channel
              JP C,B_CORE9SAVE2     ; Call value > 15? Call ERROR

              PUSH HL
              LD HL,PERRVAL
              CALL PROMPT
              LD A,#030
              CALL CHPUT
              LD HL,PERRVAL2
              CALL PROMPT
              POP HL

              JP B_ERRVAL
              
B_CORE9SAVE2: CALL CHKCHAR
              DEFB ","              ; Check for ,
              
              PUSH BC
              LD B,1
              CALL EVALINTVAR       ; DE = varptr(INT addr)
              POP BC
              
              CALL CHKCHAR
              DEFB ")"              ; Check for )

              LD A,#0FF             ; Sign as BASIC Call
              LD C,A

              JP CORE9              ; Continue with CORE command



                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 9                    *
                                    ;************************************ 
                                    ;* Checks, if CH is valid           *
                                    ;* CH must be 0 .. 15               *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(INT addr)            *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = varptr(INT addr)            *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE9:     CALL MODINIT          ; Module installed?
              JP Z,FN_CORE9SV3      ; Yes? Continue
              SCF                   ; Set CARRY Flag for ERROR
              RET                   ; Go back to main routine
              
FN_CORE9SV3:  LD A,B
              CP #10                ; Call value < 16?
              JP C,CORE9            ; Yes? Continue with CORE command
              SCF                   ; Set CARRY Flag for ERROR
              RET                   ; Go back to main routine
              
  

                                    ;************************************ 
                                    ;* Get INT Handler for each channel *
                                    ;************************************ 
                                    ;* Gets INT Handler for the         *
                                    ;* signed channel for PIC (0..7)    *
                                    ;* Z80-IM2 (8..15)                  *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = varptr(INT addr)            *
                                    ;* B = CHANNEL                      *
                                    ;* C = SLOT (&HFF = Page 3 call)    *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* varptr(INT addr) = INT addr      *
                                    ;* C = Slot number                  * 
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE9:        PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)
              PUSH BC               ; Save Channel and slot
              PUSH DE               ; Save INT addr

              LD A,B
              CP #08                ; Check if PIC (0..7)
              JP C,CORE9PIC         ; if PIC jump over

CORE9Z80:     LD B,#08              ; Sub 8 from A 
              SUB B                 ; for target address for Z80
              LD B,A                ; Save vector index
              LD DE,X_HINTZ80+2
              CALL GETRAMIX         ; [HL] = refered routine addr
              JP CORE9ADDR
              
CORE9PIC:     LD B,A                ; Save vector index
              LD DE,X_HINTPIC+2
              CALL GETRAMIX         ; [HL] = refered routine addr
              
CORE9ADDR:    LD DE,#05             ; Get position of requested addr pointer
              LD A,B
              ADD A,#01
              OR A
              LD B,A
              SBC HL,DE
CORE9L1:      ADD HL,DE
              DJNZ CORE9L1          ; HL points to addr pointer position
              
CORE9GETINT:  POP DE                ; Get INT addr back
              POP BC                ; Get channel and slot back
              LD A,(HL)             ; Get lower part of addr
              LD (DE),A             ; Save to addr pointer
              INC DE
              INC HL
              LD A,(HL)             ; Get higher part of addr
              LD (DE),A             ; Save to addr pointer
              DEC HL                ; Restore addr pointer position
              LD A,C                ; Check if BASIC Call
              CP #0FF
              JP Z,CORE9END         ; BASIC Call? Yes? Prepare End
              DEC HL                ; Get slot number position
              LD C,(HL)             ; Save slot number in C
              
CORE9END:     POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine

;---------------------------

                                    ;************************************ 
                                    ;* BASIC                            *
                                    ;* CALL XIO GET MASK (MASK)         *
                                    ;************************************ 
                                    ;* Gets Mask                        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* PARA1: VARPTR(MASK)              *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = VARPTR(MASK)                *
                                    ;************************************                                    
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF, DE                           *
                                    ;************************************

B_CORE10:     CALL MODINIT          ; Module installed?
              JP Z,B_CORE10ST       ; Yes? Continue 
              CALL MODERR           ; Prompt error no XIO
              RET                   ; Go back to calling routine

B_CORE10ST:   CALL CHKCHAR
              DEFB "("              ; Check for (
              
              LD B,0
              CALL EVALVARINT       ; DE = varptr(MASK)

              CALL CHKCHAR
              DEFB ")"              ; Check for )

              JP CORE10             ; Continue with CORE command

                                    ;************************************
                                    ;* EXTBIOS                          * 
                                    ;* Routine A = 10                   *
                                    ;************************************ 
                                    ;* Check if XIO is installed        *
                                    ;************************************
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = VARPTR(MASK)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* DE = VARPTR(MASK)                *
                                    ;* CARRY-Flag = 1 -> ERROR          *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

FN_CORE10:    CALL MODINIT          ; Module installed?
              JP Z,CORE10           ; Yes? Continue with CORE command
              SCF                   ; Set CARRY flag for ERROR
              RET                   ; Go back to main routine

                                    ;************************************ 
                                    ;* Get INT Mask for each PIC channel*
                                    ;************************************
                                    ;* Sets INT Mask for PIC (0..7)     *
                                    ;************************************ 
                                    ;* INPUT:                           *
                                    ;************************************
                                    ;* DE = VARPTR(MASK)                *
                                    ;************************************
                                    ;* OUTPUT:                          *
                                    ;************************************
                                    ;* (VARPTR(MASK)) = MASK            *
                                    ;* A = MASK                         *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* AF                               *
                                    ;************************************

CORE10:       PUSH HL               ; Save BASIC Line-Pointer (if CALL is used)

              PUSH DE
              LD DE,X_HINTMASK
              CALL GETRAMIX         ; [HL] points to INT mask
              POP DE
              LD A,(HL)             ; Get INT mask PIC
              LD (DE),A
              INC DE
              INC HL
              LD A,(HL)             ; Get INT mask IM2
              LD (DE),A
            
              POP HL                ; Get BASIC Line-Pointer (if CALL is used)
              OR A                  ; Clear Carry Flag to end routine handler
              RET                   ; Go back to main routine


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
                                    ;* AF, IX, DE                       *
                                    ;************************************

EVALBYTEVAR:  PUSH BC
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

              JP B_ERRTYPE
                            
EVALBYTEVAR1: PUSH BC
              LD IX,PTRGET          ; Get Integer as data pointer
              CALL CALBAS           ; and save it in DE
              POP BC

              INC DE                ; Test value < 256
              LD A,(DE)
              CP #00
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

              JP B_ERRVAL

EVALBYTEVAR2: DEC DE                
              LD A,(DE)             ; A contains Byte Parameter
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

              JP B_ERRTYPE
              
EVALBYTEVAR4: PUSH DE
              LD DE,(USRVAR)        ; Get integer
              LD A,D
              CP #00                ; Test if val < 256
              LD A,E
              POP DE

              JP Z,EVALBYTEVAR5

              PUSH HL               ; Error that val > 255
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

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Slot/Workarea functions from KONAMIMAN                           +++
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
                                    ;* Write Work Area Adress for Slot  *
                                    ;* Page in RAM                      *
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

WRWRKP:       PUSH DE
              EX DE,HL              ;[DE] = data to write
              CALL GETWRKP          ;[HL] = address of slot work page
              LD (HL),E
              INC HL
              LD (HL),D
              EX DE,HL              ;[HL] = data to write
              POP DE
              RET

;---------------------------

                                    ;************************************
                                    ;* Read Work Area Adress for Slot   *
                                    ;* Page in RAM                      *
                                    ;************************************
                                    ;* Input:                           *
                                    ;************************************
                                    ;* HL = Work area in RAM            *
                                    ;************************************
                                    ;* Output:                          *
                                    ;************************************
                                    ;* HL = Work area in RAM            *
                                    ;************************************
                                    ;* Modified:                        *
                                    ;************************************
                                    ;* HL                               *
                                    ;************************************

RDWRKP:       PUSH DE
              CALL GETWRKP          ;[HL] = address of slot work page
              LD E,(HL)
              INC HL
              LD D,(HL)             ;[DE] = (slot work)
              EX DE,HL              ;[HL] = (slot work)
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
              LD SP,HL              ;d estination becomes the new SP
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

MODINIT:      PUSH HL               ; Save BASIC Line-Pointer
              PUSH BC
              PUSH DE
              LD DE,X_HKENNUNG
              CALL GETRAMIX
              LD E,(HL)
              INC HL
              LD D,(HL)
              EX DE,HL
              LD BC,"TW"            ; Working area prepared?
              OR A                  ; Clear Carry Flag
              SBC HL,BC
              POP DE
              POP BC
              POP HL                ; Get BASIC Line-Pointer
              RET NZ                ; No? Prepare intialization
              PUSH HL               ; Save BASIC Line-Pointer
              PUSH DE
              LD DE,X_HFLAGS
              CALL GETRAMIX
              LD A,(HL)             ; Schon initialisiert?
              POP DE
              POP HL                ; Get BASIC Line-Pointer
              CPL
              BIT FINIT,A           ; Teste BIT 0 
              RET NZ
              LD A,#00
              OR A                  ; Clear Carry Flag
              RET

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

;---------------------------
                                              
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
;+++ Interrupt functions                                              +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* INT service routine              *
                                    ;* called by H.KEYI                 *
                                    ;************************************
                                          
INTSRV:       IN A,(IOENDIS)        ; Get active #40-IO-unit
              CPL                   ; Get Complement
              AND A                 ; Reset all Flags
              PUSH AF               ; Save active #40-IO-unit on stack

              LD C,IOENDIS
              LD B,WTEC             ; Enable Switched IO
              OUT (C),B 

              IN A,(GETINT)         ; Get INT source from address &H41
              
              LD B,0                ; Disable Switched IO
              OUT (C),B
 
              CP #040               ; No INT from module?
              JP Z,INTSRV3          ; Go to old H.KEYI INT
              
              CP #000               ; Only Z80 INT forced?
              JP Z,Z80SRV           ; Go to IM2 emulator

                                    ;************************************
                                    ;* Get PIC jump address             *
                                    ;* and serve it                     *
                                    ;************************************
              
              PUSH AF               ; Save INT sources on stack
                                    
              LD A,WTEC             ; Enable Switched IO
              OUT (IOENDIS),A 
                           
              IN A,(INTA)           ; Send first INTA
              IN A,(INTA)           ; Send second INTA
              LD C,A
              IN A,(INTA)           ; Send third INTA

              LD A,0                ; Disable Switched IO
              OUT (IOENDIS),A

              LD A,C
              AND #038              ; Only use vector bits
              SRL A                 ; Calculate INT vector index
              SRL A
              SRL A
              LD C,A                ; Save INT vector index

              LD A,C                ; Get INT vector and test of mask
              CP 0
              JP Z,INTSRVI0
              CP 1
              JP Z,INTSRVI1
              CP 2
              JP Z,INTSRVI2
              CP 3
              JP Z,INTSRVI3
              CP 4
              JP Z,INTSRVI4
              CP 5
              JP Z,INTSRVI5
              CP 6
              JP Z,INTSRVI6
              JP INTSRVI7

INTSRVI0:     LD A,1
              JP INTSRVOK        
INTSRVI1:     LD A,2
              JP INTSRVOK       
INTSRVI2:     LD A,4
              JP INTSRVOK       
INTSRVI3:     LD A,8
              JP INTSRVOK       
INTSRVI4:     LD A,16
              JP INTSRVOK        
INTSRVI5:     LD A,32
              JP INTSRVOK        
INTSRVI6:     LD A,64
              JP INTSRVOK       
INTSRVI7:     LD A,128
              JP INTSRVOK        

INTSRVOK:     LD DE,X_HVARPTR       ; Save INT CH to INT Variable
              CALL GETRAMIX
              LD E,(HL)
              INC HL
              LD D,(HL)
              EX DE,HL
              LD B,A
              LD A,(HL)
              OR B
              LD (HL),A
 
              LD DE,X_HINTPIC       ; Set PIC INT vector table
              CALL GETRAMIX         ; [HL] points to INT vector table 
              CALL GOINT            ; CALL INT-routine
                                    ; CALL is needed to come back from 
                                    ; routine to start old H.KEYI
                                                                        
              LD A,WTEC             ; Enable Switched IO
              OUT (IOENDIS),A 
              
              LD A,OCW2             ; End INT in PIC
              OUT (PICCTRL0),A      ; and send EOI

              LD A,0                ; Disable Switched IO
              OUT (IOENDIS),A

              POP AF                ; Get INT source from stack
              CP #080               ; If PIC & Z80 INT go to Z80 INT
              JR Z, Z80SRV          
 
INTSRV3:      JP OLDINT             ; Serve old H.KEYI

                                    ;************************************
                                    ;* Get Z80 jump address             *
                                    ;* and serve it                     *
                                    ;************************************
                                         
Z80SRV:       LD A,WTEC             ; Enable Switched IO
              OUT (IOENDIS),A 
              
              IN A,(IM2)            ; Get Z80 INT vector
                                    ; Emulate !M1/!IORQ INTA for IM2

              SRL A                 ; Calculate INT vector index
              AND #007              ; Mirror each 8 INT vector
              LD C,A                ; Save INT vector index

              LD A,0                ; Disable Switched IO
              OUT (IOENDIS),A

              LD DE,X_HINTMASK      ; Test if mask allows INT
              CALL GETRAMIX
              INC HL
              LD A,C                ; Get INT vector and test of mask
              CP 0
              JP Z,Z80SRVI0
              CP 1
              JP Z,Z80SRVI1
              CP 2
              JP Z,Z80SRVI2
              CP 3
              JP Z,Z80SRVI3
              CP 4
              JP Z,Z80SRVI4
              CP 5
              JP Z,Z80SRVI5
              CP 6
              JP Z,Z80SRVI6
              JP Z80SRVI7

Z80SRVI0:     LD A,(HL)             ; Get allowed MASK
              AND 1
              LD A,1
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT  
Z80SRVI1:     LD A,(HL)             ; Get allowed MASK
              AND 2
              LD A,2
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI2:     LD A,(HL)             ; Get allowed MASK
              AND 4
              LD A,4
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI3:     LD A,(HL)             ; Get allowed MASK
              AND 8
              LD A,8
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI4:     LD A,(HL)             ; Get allowed MASK
              AND 16
              LD A,16
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI5:     LD A,(HL)             ; Get allowed MASK
              AND 32
              LD A,32
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI6:     LD A,(HL)             ; Get allowed MASK
              AND 64
              LD A,64
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 
Z80SRVI7:     LD A,(HL)             ; Get allowed MASK
              AND 128
              LD A,128
              JP NZ,Z80SRVOK        ; Allowed? Go on
              CALL INTZ80ST         ; No? Quit with RETI
              JP OLDINT             ; Go to old INT 

Z80SRVOK:     LD DE,X_HVARPTR       ; Save INT CH to INT Variable
              CALL GETRAMIX
              LD E,(HL)
              INC HL
              LD D,(HL)
              EX DE,HL
              INC HL
              LD B,A
              LD A,(HL)
              OR B
              LD (HL),A

Z80SRV3:      LD DE,X_HINTZ80       ; Set Z80 INT vector table
              CALL GETRAMIX         ; [HL] points to INT vector table             
              CALL GOINT            ; CALL INT-routine
                                    ; CALL is needed to come back from 
                                    ; routine to start old H.KEYI
              
                                    ;************************************
                                    ;* Serve old H.KEYI                 *
                                    ;************************************

OLDINT:       POP AF                ; Get old activated #40 IO unit
              OUT (IOENDIS),A       ; Enable old activated #40 IO unit

              JP HKEYIALT           ; End INT handling and go to
                                    ; old H.KEYI INT

                                    ;************************************
                                    ;* Serve INT routine                *
                                    ;************************************

GOINT:        LD DE,#05             ; Get HOOK position
              LD A,C                ; C contains INT vector index
              ADD A,#01             ; A = 0..7 -> 1..8
              OR A                  ; Clear Carry flag
              LD B,A                ; Set loop index
              SBC HL,DE             ; Set HL to HL-5
GOINTLOOP:    ADD HL,DE             ; Calculate refered HOOK
              DJNZ GOINTLOOP        ; HL points to refered HOOK afterwards
              JP (HL)               ; Jump to executive routine
                                    ; PIC and also Z80 have got
                                    ; an unique target address 
                                    ; Please be sure that PIC service ends
                                    ; RET and Z80 service ends RETI

;---------------------------

                                    ;************************************
                                    ;* Standard PIC INT routine         *
                                    ;************************************

INTPICST:     RET

;---------------------------

                                    ;************************************
                                    ;* Standard Z80 INT routine         *
                                    ;************************************

INTZ80ST:     RETI

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Prompt text                                                      +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                    
                                    ;************************************
                                    ;* Messages for BASIC CALLs         *
                                    ;************************************
           
PSTART:       DEFM "[XIO] - IO-Expander",#0D,#0A,#0A
              DEFM "Switched-IO based IO-Expander with",#0D,#0A
              DEFM "8 channel interrupt controller 82C59A",#0D,#0A
              DEFM "and 8 channel Z80-IM2-Emulator",#0D,#0A,#0A
              DEFM "Firmware version ",#030+ROM_V_P,".",#030+ROM_V_S,#0D,#0A,#0A
              DEFM "(c) 2023 by Thomas Wiesner",#0D,#0A,#0A
              DEFM "Switched-IO based on ASCII(R)",#0D,#0A
              DEFM "Vendor ID: &HC0",#0D,#0A,#0A
              DEFM "UNAPI design (c) by Konamiman",#0D,#0A,#0A
              DEFM "CALL design (c) by www.msx.org",#0D,#0A,#0A
              DEFM "Type 'CALL XIO ?' for HELP under BASIC",#0D,#0A,#0A
              DEFM "Card installed on slot ",0
PMEMERR:      DEFM "Not enough RAM left to install [XIO]",#0D,#0A,#0A,0
PCRLF:        DEFM #0D,#0A,0
PISINIT:      DEFM "[XIO] already installed",#0D,#0A,0
PINIT:        DEFM "[XIO] installed",#0D,#0A,0
PERRINIT:     DEFM "[XIO] not installed",#0D,#0A,0
PERRADDR:     DEFM "[XIO] needs INT handler between",#0D,#0A
              DEFM "&HD000 and &HEFFF for legal call or",#0D,#0A
              DEFM "&H0000 for reset to standard",#0D,#0A,0
PERRVAL:      DEFM "Value of parameter ",0
PERRVAL1:     DEFM " must be 0 .. 255",#0D,#0A,0
PERRVAL2:     DEFM " must be 0 .. 15",#0D,#0A,0
PERRTYPE:     DEFM "Type of parameter ",0
PERRTYPEIV:   DEFM " must be integer variable",#0D,#0A,0
PERRTYPEI:    DEFM " must be integer",#0D,#0A,0
PERRTYPES:    DEFM " must be string",#0D,#0A,0
PERRTYPESL:   DEFM "Length of string not correct",#0D,#0A,0
PPIC:         DEFM "[XIO] PIC INT CH: ",0
PZ80:         DEFM "[XIO] Z80 INT CH: ",0
PHELP:        DEFM "[XIO] HELP - BASIC commands",#0D,#0A,#0A,0
PHELPA:       DEFM "[XIO] BASIC CMD '?'",#0D,#0A
              DEFM "FORMAT: _XIO ?",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'OUT'",#0D,#0A
              DEFM "FORMAT: _XIO OUT(ADDR,VALUE)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'INP'",#0D,#0A
              DEFM "FORMAT: _XIO INP(ADDR,VALUE)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'INIT'",#0D,#0A
              DEFM "FORMAT: _XIO INIT(INTID)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'ISINIT'",#0D,#0A
              DEFM "FORMAT: _XIO ISINIT(ISI)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
PHELPB:       DEFM "[XIO] BASIC CMD 'DEINIT'",#0D,#0A
              DEFM "FORMAT: _XIO DEINIT",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'SET ADDR'",#0D,#0A
              DEFM "FORMAT: _XIO SET ADDR(CH,ADDR)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'SET MASK'",#0D,#0A
              DEFM "FORMAT: _XIO SET MASK(MASK)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'GET ADDR'",#0D,#0A
              DEFM "FORMAT: _XIO GET ADDR(CH,ADDR)",#0D,#0A,#0A
              DEFM "[XIO] BASIC CMD 'GET MASK'",#0D,#0A
              DEFM "FORMAT: _XIO GET MASK(MASK)",#0D,#0A,#0A
              DEFM "(N) Next / (P) Prev / (Z) END",#0D,#0A
              DEFM "Please choose an option: ",0
              DEFS X_ROM-$,#00 

;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;+++ Working area (X_WRK) to be copied to reserved RAM                +++
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                                    ;************************************
                                    ;* PIC INT routines INT0 to INT7    *
                                    ;************************************

HINTPIC_0:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_1:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_2:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_3:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_4:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_5:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_6:    RST #030
              DEFB #00
              DEFW INTPICST
              RET
HINTPIC_7:    RST #030
              DEFB #00
              DEFW INTPICST
              RET

                                    ;************************************
                                    ;* Z80 INT routines INT8 to INT15   *
                                    ;* Routines are mirrored            *
                                    ;************************************

HINTZ80_0:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_1:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_2:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_3:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_4:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_5:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_6:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
HINTZ80_7:    RST #030
              DEFB #00
              DEFW INTZ80ST
              RET
                                          
                                    ;************************************ 
                                    ;* INT hook storage                 *
                                    ;************************************ 
              
HKEYIALT:     DEFB #0C9
              DEFB #0C9
              DEFB #0C9
              DEFB #0C9
              DEFB #0C9

                                    ;************************************ 
                                    ;* ERROR hook storage               *
                                    ;************************************ 
              
HERROALT:     DEFB #0C9
              DEFB #0C9
              DEFB #0C9
              DEFB #0C9
              DEFB #0C9

                                    ;************************************ 
                                    ;* Module variables                 *
                                    ;************************************ 

HFLAGS:       DEFB #00              ; PIC-Card flags
                                    ; Bit 0 = installed 1 = j / 0 = n
                                    ; Bit 1 = TRACE 1 = on / 0 = off
HINTMASK:     DEFW #00              ; MASK register INT channel external
HVARPTR:      DEFW #00              ; Ptr to variable for INT ID
HKENNUNG:     DEFM "WT"             ; ID for installed work area
HENDE:        DEFB #00

TEND:         DEFS #08000-$
              END
