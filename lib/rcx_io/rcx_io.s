;***************************************************************************************************
;* HEADER - rcx_io.s - UNAPI/EXTBIOS access to RCX_INTERFACE                                       *
;***************************************************************************************************
;* UNAPI/EXTBIOS access to RCX_INTERFACE for SDCC/MSXgl C tools (sdasz80). Covers every FN_CORE     *
;* entry RCXROM.ASM exposes except FN_CORE1 (BASIC-only help) - see rcx_io.h for the full function  *
;* list and calling convention.                                                                      *
;*                                                                                                   *
;* Two places where a per-function EXTBIOS doc comment in RCXROM.ASM disagrees with the actual code *
;* (verified by reading COREPIO/COREPPI/CORETIM directly, not just the comments):                   *
;*  - TIMER write/ctrl/gate (CORE35/37/39/40/41/42): CORE35's own comment says "D=Byte" but          *
;*    CORETIMAPW actually sends both D and E to hardware (LD A,E / OUT.. / LD A,D / OUT..) - a real  *
;*    16-bit value, matching what CORE37/39/40/41/42's own comments already say (DE=INT). Treated as *
;*    16-bit (DE) for all six here.                                                                  *
;*  - PPI SET/RESET BIT (CORE31/32): CORE31's own comment says "DE=varptr(VALUE)" but COREPPISET     *
;*    actually reads D directly ("LD A,D"), exactly like COREPPIRES (CORE32, whose own comment does  *
;*    say D=Byte). Treated as D=Byte (single parameter) for both here.                               *
;* Dispatch (_RcxCall) mirrors RTCLCD.ASM's CALLFN: SEG=FFh (ROM) calls through CALSLT, SEG<>FFh     *
;* (mapped RAM) calls through the RAM helper via a self-modifying trampoline, so control returns     *
;* here afterward for the IX/IY restore - see the comment on _RcxCall below.                         *
;*                                                                                                   *
;* Card address/bus/chip parameters are NOT cached anywhere in this file - every function that       *
;* needs one takes it as an explicit argument on every call, matching the BASIC/UNAPI original       *
;* exactly (up to 16 cards of most types can be installed simultaneously, so a client-side cache      *
;* would silently target the wrong card the moment a program touches a second one). Functions        *
;* needing more than sdcccall(1)'s 2-argument register limit (3+ discrete values, or an 8-bit value   *
;* alongside two 16-bit ones) use sdcccall(0) instead - a real stack-based call, parameters pushed    *
;* by the caller in declaration order, no cleanup needed here (the caller pops its own arguments      *
;* after the call returns - verified empirically by compiling a throwaway sdcccall(0) test function   *
;* with this project's own bundled SDCC 4.2.0 and reading its generated .asm, not assumed from        *
;* general SDCC documentation). Each sdcccall(0) function below establishes its own small IX frame    *
;* (push ix / ld ix,#4 / add ix,sp, so the first argument lands at 0 (ix), the second at 1 (ix), and  *
;* so on in declaration order with no padding) and restores IX before returning, exactly like         *
;* _RcxCall's own IX preservation - a caller may be using IX as its own local-variable frame pointer  *
;* across this call.                                                                                  *
;***************************************************************************************************
;* Datei: rcx_io.s                                                                                  *
;***************************************************************************************************
;* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
;***************************************************************************************************
;* VERSION: 23/09/26                                                                                *
;***************************************************************************************************
	.module rcx_io

	.globl _Rcx_Discover
	.globl _Rcx_GetInfo
	.globl _Rcx_IsInit
	.globl _Rcx_Init
	.globl _Rcx_Deinit
	.globl _Rcx_GetTable
	.globl _I2c_Init
	.globl _I2c_Write
	.globl _I2c_Wrb
	.globl _I2c_Read
	.globl _I2c_Rdb
	.globl _I2c_Reset
	.globl _Rtc_SetTime
	.globl _Rtc_GetTime
	.globl _Rtc_SetDate
	.globl _Rtc_GetDate
	.globl _Pio_Init
	.globl _Pio_ReadA
	.globl _Pio_WriteA
	.globl _Pio_ReadB
	.globl _Pio_WriteB
	.globl _Pio_CtrlA
	.globl _Pio_CtrlB
	.globl _Ppi_Init
	.globl _Ppi_ReadA
	.globl _Ppi_WriteA
	.globl _Ppi_ReadB
	.globl _Ppi_WriteB
	.globl _Ppi_ReadC
	.globl _Ppi_WriteC
	.globl _Ppi_Ctrl
	.globl _Ppi_SetBit
	.globl _Ppi_ResetBit
	.globl _Timer_Init
	.globl _Timer_Read0
	.globl _Timer_Write0
	.globl _Timer_Read1
	.globl _Timer_Write1
	.globl _Timer_Read2
	.globl _Timer_Write2
	.globl _Timer_Ctrl
	.globl _Timer_SetGate
	.globl _Timer_ResetGate
	.globl _Can_Init
	.globl _Can_Write
	.globl _Can_Read
	.globl _Can_Check
	.globl _Can_Rx
	.globl _Can_GetIntAddr
	.globl _Can_Tx
	.globl _Spi_Init
	.globl _Spi_Write
	.globl _Spi_Wrb
	.globl _Spi_Read
	.globl _Spi_Rdb
	.globl _Spi_Rdh
	.globl _Spi_Mode
	.globl _Ads_Init
	.globl _Ads_SetMode
	.globl _Ads_Cmd
	.globl _Ads_GetStatus
	.globl _Ads_Read
	.globl _Ads_Get
	.globl _g_RSlot
	.globl _g_RSeg
	.globl _g_RCount
	.globl _g_RRomVersion
	.globl _g_RApiVersion
	.globl _g_RApiInfo

EXTBIO		=	0xFFCA
CALSLT		=	0x001C
ARG		=	0xF847

RCX_FN_INIT	=	2
RCX_FN_ISINIT	=	3
RCX_FN_DEINIT	=	4
RCX_FN_TABLE	=	5
I2C_FN_INIT	=	6
I2C_FN_WRITE	=	7
I2C_FN_WRB	=	8
I2C_FN_READ	=	9
I2C_FN_RDB	=	10
I2C_FN_RESET	=	11
RTC_FN_SETTIME	=	12
RTC_FN_GETTIME	=	13
RTC_FN_SETDATE	=	14
RTC_FN_GETDATE	=	15
PIO_FN_INIT	=	16
PIO_FN_ARD	=	17
PIO_FN_AWR	=	18
PIO_FN_BRD	=	19
PIO_FN_BWR	=	20
PIO_FN_ACTRL	=	21
PIO_FN_BCTRL	=	22
PPI_FN_INIT	=	23
PPI_FN_ARD	=	24
PPI_FN_AWR	=	25
PPI_FN_BRD	=	26
PPI_FN_BWR	=	27
PPI_FN_CRD	=	28
PPI_FN_CWR	=	29
PPI_FN_CTRL	=	30
PPI_FN_SET	=	31
PPI_FN_RESET	=	32
TIMER_FN_INIT	=	33
TIMER_FN_0RD	=	34
TIMER_FN_0WR	=	35
TIMER_FN_1RD	=	36
TIMER_FN_1WR	=	37
TIMER_FN_2RD	=	38
TIMER_FN_2WR	=	39
TIMER_FN_CTRL	=	40
TIMER_FN_SETGATE =	41
TIMER_FN_RESGATE =	42
CAN_FN_INIT	=	43
CAN_FN_WR	=	44
CAN_FN_RD	=	45
CAN_FN_CHECK	=	46
CAN_FN_RX	=	47
CAN_FN_GETADDR	=	48
CAN_FN_TX	=	49
SPI_FN_INIT	=	50
SPI_FN_WR	=	51
SPI_FN_WRB	=	52
SPI_FN_RD	=	53
SPI_FN_RDB	=	54
SPI_FN_RDH	=	55
SPI_FN_MODE	=	56
ADS_FN_INIT	=	57
ADS_FN_MODE	=	58
ADS_FN_CMD	=	59
ADS_FN_STATUS	=	60
ADS_FN_READ	=	61
ADS_FN_GET	=	62

	.area _DATA
_g_RSlot::
	.db	0
_g_RSeg::
	.db	0
_g_RCount::
	.db	0
_g_RRomVersion::
	.dw	0
_g_RApiVersion::
	.dw	0
_g_RApiInfo::
	.dw	0
_g_REntry:
	.dw	0
_g_RHelperAddr:
	.dw	0
_RISIBUF:
	.dw	0
_RTABLEBUF:
	.dw	0
_RI2CREADBUF:
	.dw	0
_RPIOBUF:
	.dw	0
_RPPIBUF:
	.dw	0
_RTIMERBUF:
	.dw	0
_RSPIREADBUF:
	.dw	0
_RADSBUF:
	.dw	0

	.area _CODE

;---------------------------------------------------------------------------------------------------
; bool Rcx_Discover(void)
; Locates the RAM helper (EXTBIO A=0xFFh - harmless to look up even on a ROM-
; only system, needed only when a discovered driver turns out to be RAM-
; mapped), then copies "RCX_INTERFACE" to ARG and runs the two-step EXTBIO
; discovery (implementation count, then slot/segment/entry). Unlike every
; other function in this file, this one calls EXTBIO directly instead of
; going through _RcxCall - so it needs its own IX/IY save/restore around
; those calls, matching _RcxCall's own reasoning (IX is a C caller's local-
; variable frame pointer, IY is the system-wide BIOS/interrupt pointer;
; EXTBIO is not documented to preserve either). Returns TRUE (A=1) if found,
; FALSE (A=0) otherwise.
;---------------------------------------------------------------------------------------------------
_Rcx_Discover::
	push	ix
	push	iy

	ld	de, #0x2222
	ld	hl, #0
	ld	a, #0xFF
	call	EXTBIO
	ld	(_g_RHelperAddr), hl

	ld	hl, #_RIDSTR
	ld	de, #ARG
	ld	bc, #_RIDSTRLEN
	ldir

	xor	a
	ld	b, a
	ld	de, #0x2222
	call	EXTBIO
	ld	a, b
	ld	(_g_RCount), a
	or	a
	jr	z, rdiscover_fail

	ld	a, #1
	ld	de, #0x2222
	call	EXTBIO
	ld	(_g_RSlot), a
	ld	a, b
	ld	(_g_RSeg), a
	ld	(_g_REntry), hl

	pop	iy
	pop	ix
	ld	a, #1
	ret
rdiscover_fail:
	pop	iy
	pop	ix
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; Shared dispatch: entry A=function number, B/C/D/E/H/L preset by the caller
; as documented by the target function. Builds IX=discovered entry address
; (both paths) and either calls CALSLT (SEG=FFh, ROM-resident driver) with
; IY=slot:00h, or calls through the RAM helper (SEG<>FFh, e.g. RCXRAM.COM)
; with IY=slot:seg, via a self-modifying trampoline (JP nnnn patched to the
; helper address) - a real indirect CALL, unlike RTCLCD.ASM's own tail-jump
; version of this dispatch, so control returns here afterward for the IX/IY
; restore below. Carry, and any of BC/DE/HL the target function documents as
; output, come back unchanged to the caller. Preserves IX across the call -
; C callers use it as their own local-variable frame pointer. Also preserves
; IY: on MSX, IY is a system-wide constant (BIOS/interrupt code relies on it
; pointing at the system variable area near EXPTBL), not a scratch register -
; CALSLT does not restore it, so overwriting it here and never restoring it
; would leave it wrong for the rest of the program (same reasoning already
; verified for xio_io.s's _XioCall).
;---------------------------------------------------------------------------------------------------
_RcxCall:
	push	ix
	push	iy
	push	af
	push	hl

	ld	hl, (_g_REntry)
	push	hl
	pop	ix

	ld	a, (_g_RSeg)
	cp	#0xFF
	jr	z, rcxcall_rom

	; --- RAM-mapped driver: IY = slot:seg, call through the RAM helper ---
	ld	a, (_g_RSlot)
	ld	h, a
	ld	a, (_g_RSeg)
	ld	l, a
	push	hl
	pop	iy

	ld	hl, (_g_RHelperAddr)
	ld	(rcx_trampoline_target), hl

	pop	hl
	pop	af
	call	rcx_trampoline
	jr	rcxcall_done

rcxcall_rom:
	; --- ROM-resident driver: IY = slot:00h, call via CALSLT ---
	ld	a, (_g_RSlot)
	ld	h, a
	ld	l, #0
	push	hl
	pop	iy

	pop	hl
	pop	af
	call	CALSLT

rcxcall_done:
	pop	iy
	pop	ix
	ret

rcx_trampoline:
	jp	0
rcx_trampoline_target	=	rcx_trampoline + 1

;=====================================================================================================
; RCX CORE (CORE2-5)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Rcx_GetInfo(void)
; FN_INFO (function number 0) - does not require Rcx_Init() first.
;---------------------------------------------------------------------------------------------------
_Rcx_GetInfo::
	xor	a
	call	_RcxCall
	ld	(_g_RRomVersion), bc
	ld	(_g_RApiVersion), de
	ld	(_g_RApiInfo), hl
	ld	a, #1
	ret

;---------------------------------------------------------------------------------------------------
; u8 Rcx_IsInit(void)
; Entry: no parameters. FN_CORE3 writes its 1/0 result to the low byte of a
; 1-word buffer (the high byte, pre-cleared by the ROM's own wrapper, is not
; used) - matches RTCLCD.ASM's ISIBUF usage exactly (unlike XIO_FN_ISINIT,
; which returns its result in the high byte - a real difference between the
; two drivers' ISINIT conventions, not a copy/paste slip).
;---------------------------------------------------------------------------------------------------
_Rcx_IsInit::
	ld	de, #_RISIBUF
	ld	a, #RCX_FN_ISINIT
	call	_RcxCall
	ld	a, (_RISIBUF)
	ret

;---------------------------------------------------------------------------------------------------
; bool Rcx_Init(u16 workarea)
; Entry: HL=workarea (RAM base address for the driver's own use)
;---------------------------------------------------------------------------------------------------
_Rcx_Init::
	ld	a, #RCX_FN_INIT
	call	_RcxCall
	jr	c, rinit_fail
	ld	a, #1
	ret
rinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; void Rcx_Deinit(void)
;---------------------------------------------------------------------------------------------------
_Rcx_Deinit::
	ld	a, #RCX_FN_DEINIT
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; bool Rcx_GetTable(u16* outAddr)
; Entry: HL=outAddr. FN_CORE5 writes the RCX card table's address into the
; varptr it is given - routed through a fixed buffer first, same reasoning
; as xio_io.s's Xio_GetAddr (outAddr may be a stack-relative local variable,
; and the ROM write happens deep inside a CALSLT inter-slot call).
;---------------------------------------------------------------------------------------------------
_Rcx_GetTable::
	push	hl
	ld	de, #_RTABLEBUF
	ld	a, #RCX_FN_TABLE
	call	_RcxCall
	pop	de
	jr	c, rtable_fail
	ld	hl, #_RTABLEBUF
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	ld	a, #1
	ret
rtable_fail:
	xor	a
	ret

;=====================================================================================================
; I2C / PCF8584 (CORE6-11) - addr/chip passed explicitly on every call, matching the BASIC/UNAPI
; original (no client-side caching - see file header).
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; void I2c_Init(u8 addr)
; Entry: A=addr. Registers the card in the RCX table - does not need to be
; remembered afterward, every other I2c_*/Rtc_* call below takes addr again.
;---------------------------------------------------------------------------------------------------
_I2c_Init::
	ld	c, a
	ld	a, #I2C_FN_INIT
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void I2c_Write(u8 addr, u8 chip, u8 data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=chip, 2 (ix)=data. FN_CORE7 wants
; E=BUS ADDR, C=I2C CHIP, B=I2C DATA.
;---------------------------------------------------------------------------------------------------
_I2c_Write::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	e, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	a, #I2C_FN_WRITE
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; void I2c_Wrb(u8 addr, u8 chip, u8 len, void* data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=chip, 2 (ix)=len, 3/4 (ix)=data (lo/hi).
; FN_CORE8 wants H=BUS ADDR, C=I2C CHIP, B=I2C LEN, DE=varptr(I2C DATA).
;---------------------------------------------------------------------------------------------------
_I2c_Wrb::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	h, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	e, 3 (ix)
	ld	d, 4 (ix)
	ld	a, #I2C_FN_WRB
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; u8 I2c_Read(u8 addr, u8 chip)
; sdcccall(1) "8+8": A=addr, L=chip. FN_CORE9 wants C=BUS ADDR, B=I2C CHIP,
; DE=varptr(I2C DATA) - a single byte written to *(DE).
;---------------------------------------------------------------------------------------------------
_I2c_Read::
	ld	c, a
	ld	b, l
	ld	de, #_RI2CREADBUF
	ld	a, #I2C_FN_READ
	call	_RcxCall
	ld	a, (_RI2CREADBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void I2c_Rdb(u8 addr, u8 chip, u8 len, void* data) __sdcccall(0)
; Same stack layout as I2c_Wrb. FN_CORE10 wants H=BUS ADDR, C=I2C CHIP,
; B=I2C LEN, DE=varptr(I2C DATA).
;---------------------------------------------------------------------------------------------------
_I2c_Rdb::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	h, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	e, 3 (ix)
	ld	d, 4 (ix)
	ld	a, #I2C_FN_RDB
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; bool I2c_Reset(u8 addr)
; FN_CORE11 wants C=BUS ADDR.
;---------------------------------------------------------------------------------------------------
_I2c_Reset::
	ld	c, a
	ld	a, #I2C_FN_RESET
	call	_RcxCall
	jr	c, i2creset_fail
	ld	a, #1
	ret
i2creset_fail:
	xor	a
	ret

;=====================================================================================================
; RTC / PCF8583 (CORE12-15) - addr is the same I2C bus address as the group above
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; void Rtc_SetTime(u8 addr, void* time)
; sdcccall(1) "8+16": A=addr, DE=time. FN_CORE12 wants C=BUS ADDR,
; DE=varptr(TIME) - DE already holds time unchanged.
;---------------------------------------------------------------------------------------------------
_Rtc_SetTime::
	ld	c, a
	ld	a, #RTC_FN_SETTIME
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Rtc_GetTime(u8 addr, void* time) - FN_CORE13, same pattern as Rtc_SetTime
;---------------------------------------------------------------------------------------------------
_Rtc_GetTime::
	ld	c, a
	ld	a, #RTC_FN_GETTIME
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Rtc_SetDate(u8 addr, void* date) - FN_CORE14, same pattern
;---------------------------------------------------------------------------------------------------
_Rtc_SetDate::
	ld	c, a
	ld	a, #RTC_FN_SETDATE
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Rtc_GetDate(u8 addr, void* date) - FN_CORE15, same pattern
;---------------------------------------------------------------------------------------------------
_Rtc_GetDate::
	ld	c, a
	ld	a, #RTC_FN_GETDATE
	call	_RcxCall
	ret

;=====================================================================================================
; Z80 PIO (CORE16-22)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Pio_Init(u8 addr)
;---------------------------------------------------------------------------------------------------
_Pio_Init::
	ld	c, a
	ld	a, #PIO_FN_INIT
	call	_RcxCall
	jr	c, pioinit_fail
	ld	a, #1
	ret
pioinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; u8 Pio_ReadA(u8 addr)
; FN_COREPIO (B=0), C=PIO addr, DE=varptr(VALUE) - the ROM writes a single
; byte plus a cleared high byte (BASIC INTEGER convention); only the low
; byte is meaningful.
;---------------------------------------------------------------------------------------------------
_Pio_ReadA::
	ld	c, a
	ld	de, #_RPIOBUF
	ld	b, #0
	ld	a, #PIO_FN_ARD
	call	_RcxCall
	ld	a, (_RPIOBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Pio_WriteA(u8 addr, u8 value)
; sdcccall(1) "8+8": A=addr, L=value. FN_COREPIO (B=1), C=PIO addr, D=Byte.
;---------------------------------------------------------------------------------------------------
_Pio_WriteA::
	ld	c, a
	ld	d, l
	ld	b, #1
	ld	a, #PIO_FN_AWR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Pio_ReadB(u8 addr) - FN_COREPIO (B=2)
;---------------------------------------------------------------------------------------------------
_Pio_ReadB::
	ld	c, a
	ld	de, #_RPIOBUF
	ld	b, #2
	ld	a, #PIO_FN_BRD
	call	_RcxCall
	ld	a, (_RPIOBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Pio_WriteB(u8 addr, u8 value) - FN_COREPIO (B=3)
;---------------------------------------------------------------------------------------------------
_Pio_WriteB::
	ld	c, a
	ld	d, l
	ld	b, #3
	ld	a, #PIO_FN_BWR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Pio_CtrlA(u8 addr, u8 value) - FN_COREPIO (B=4)
;---------------------------------------------------------------------------------------------------
_Pio_CtrlA::
	ld	c, a
	ld	d, l
	ld	b, #4
	ld	a, #PIO_FN_ACTRL
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Pio_CtrlB(u8 addr, u8 value) - FN_COREPIO (B=5)
;---------------------------------------------------------------------------------------------------
_Pio_CtrlB::
	ld	c, a
	ld	d, l
	ld	b, #5
	ld	a, #PIO_FN_BCTRL
	call	_RcxCall
	ret

;=====================================================================================================
; 82C55 PPI (CORE23-32)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Ppi_Init(u8 addr)
;---------------------------------------------------------------------------------------------------
_Ppi_Init::
	ld	c, a
	ld	a, #PPI_FN_INIT
	call	_RcxCall
	jr	c, ppiinit_fail
	ld	a, #1
	ret
ppiinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; u8 Ppi_ReadA(u8 addr) - FN_COREPPI (B=0), DE=varptr(VALUE)
;---------------------------------------------------------------------------------------------------
_Ppi_ReadA::
	ld	c, a
	ld	de, #_RPPIBUF
	ld	b, #0
	ld	a, #PPI_FN_ARD
	call	_RcxCall
	ld	a, (_RPPIBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_WriteA(u8 addr, u8 value) - FN_COREPPI (B=1), D=Byte
;---------------------------------------------------------------------------------------------------
_Ppi_WriteA::
	ld	c, a
	ld	d, l
	ld	b, #1
	ld	a, #PPI_FN_AWR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Ppi_ReadB(u8 addr) - FN_COREPPI (B=2)
;---------------------------------------------------------------------------------------------------
_Ppi_ReadB::
	ld	c, a
	ld	de, #_RPPIBUF
	ld	b, #2
	ld	a, #PPI_FN_BRD
	call	_RcxCall
	ld	a, (_RPPIBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_WriteB(u8 addr, u8 value) - FN_COREPPI (B=3)
;---------------------------------------------------------------------------------------------------
_Ppi_WriteB::
	ld	c, a
	ld	d, l
	ld	b, #3
	ld	a, #PPI_FN_BWR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Ppi_ReadC(u8 addr) - FN_COREPPI (B=4)
;---------------------------------------------------------------------------------------------------
_Ppi_ReadC::
	ld	c, a
	ld	de, #_RPPIBUF
	ld	b, #4
	ld	a, #PPI_FN_CRD
	call	_RcxCall
	ld	a, (_RPPIBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_WriteC(u8 addr, u8 value) - FN_COREPPI (B=5)
;---------------------------------------------------------------------------------------------------
_Ppi_WriteC::
	ld	c, a
	ld	d, l
	ld	b, #5
	ld	a, #PPI_FN_CWR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_Ctrl(u8 addr, u8 value) - FN_COREPPI (B=6)
;---------------------------------------------------------------------------------------------------
_Ppi_Ctrl::
	ld	c, a
	ld	d, l
	ld	b, #6
	ld	a, #PPI_FN_CTRL
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_SetBit(u8 addr, u8 bit) - FN_COREPPI (B=7), D=Byte (bit 0..7) -
; verified against COREPPISET's actual code, which reads D directly, not
; per FN_CORE31's own (incorrect) "DE=varptr(VALUE)" comment.
;---------------------------------------------------------------------------------------------------
_Ppi_SetBit::
	ld	c, a
	ld	d, l
	ld	b, #7
	ld	a, #PPI_FN_SET
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Ppi_ResetBit(u8 addr, u8 bit) - FN_COREPPI (B=8), D=Byte (bit 0..7)
;---------------------------------------------------------------------------------------------------
_Ppi_ResetBit::
	ld	c, a
	ld	d, l
	ld	b, #8
	ld	a, #PPI_FN_RESET
	call	_RcxCall
	ret

;=====================================================================================================
; 82C54 TIMER (CORE33-42)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Timer_Init(u8 addr)
;---------------------------------------------------------------------------------------------------
_Timer_Init::
	ld	c, a
	ld	a, #TIMER_FN_INIT
	call	_RcxCall
	jr	c, timerinit_fail
	ld	a, #1
	ret
timerinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; u16 Timer_Read0(u8 addr)
; FN_CORETIM (B=0), C=TIMER addr, DE=varptr(VALUE) - a real 16-bit word
; (verified against CORETIMAPR's actual code: it writes 2 real bytes).
;---------------------------------------------------------------------------------------------------
_Timer_Read0::
	ld	c, a
	ld	de, #_RTIMERBUF
	ld	b, #0
	ld	a, #TIMER_FN_0RD
	call	_RcxCall
	ld	de, (_RTIMERBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_Write0(u8 addr, u16 value)
; sdcccall(1) "8+16": A=addr, DE=value. FN_CORETIM (B=1), C=TIMER addr,
; DE=value (already in DE, a real 16-bit word - verified against
; CORETIMAPW's actual code, not CORE35's own comment which incorrectly says
; "D=Byte" - see the file header).
;---------------------------------------------------------------------------------------------------
_Timer_Write0::
	ld	c, a
	ld	b, #1
	ld	a, #TIMER_FN_0WR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u16 Timer_Read1(u8 addr) - FN_CORETIM (B=2)
;---------------------------------------------------------------------------------------------------
_Timer_Read1::
	ld	c, a
	ld	de, #_RTIMERBUF
	ld	b, #2
	ld	a, #TIMER_FN_1RD
	call	_RcxCall
	ld	de, (_RTIMERBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_Write1(u8 addr, u16 value) - FN_CORETIM (B=3)
;---------------------------------------------------------------------------------------------------
_Timer_Write1::
	ld	c, a
	ld	b, #3
	ld	a, #TIMER_FN_1WR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u16 Timer_Read2(u8 addr) - FN_CORETIM (B=4)
;---------------------------------------------------------------------------------------------------
_Timer_Read2::
	ld	c, a
	ld	de, #_RTIMERBUF
	ld	b, #4
	ld	a, #TIMER_FN_2RD
	call	_RcxCall
	ld	de, (_RTIMERBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_Write2(u8 addr, u16 value) - FN_CORETIM (B=5)
;---------------------------------------------------------------------------------------------------
_Timer_Write2::
	ld	c, a
	ld	b, #5
	ld	a, #TIMER_FN_2WR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_Ctrl(u8 addr, u16 value) - FN_CORETIM (B=6)
;---------------------------------------------------------------------------------------------------
_Timer_Ctrl::
	ld	c, a
	ld	b, #6
	ld	a, #TIMER_FN_CTRL
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_SetGate(u8 addr, u16 value) - FN_CORETIM (B=7)
;---------------------------------------------------------------------------------------------------
_Timer_SetGate::
	ld	c, a
	ld	b, #7
	ld	a, #TIMER_FN_SETGATE
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Timer_ResetGate(u8 addr, u16 value) - FN_CORETIM (B=8)
;---------------------------------------------------------------------------------------------------
_Timer_ResetGate::
	ld	c, a
	ld	b, #8
	ld	a, #TIMER_FN_RESGATE
	call	_RcxCall
	ret

;=====================================================================================================
; SJA1000 CAN (CORE43-49) - single-card-only in practice, addr still passed explicitly on every
; call for consistency with every other card type in this file.
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Can_Init(u8 addr, void* acc)
; sdcccall(1) "8+16": A=addr, DE=acc. FN_CORE43 wants C=CAN ADDR,
; DE=varptr(ACC) - DE left untouched.
;---------------------------------------------------------------------------------------------------
_Can_Init::
	ld	c, a
	ld	a, #CAN_FN_INIT
	call	_RcxCall
	jr	c, caninit_fail
	ld	a, #1
	ret
caninit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; void Can_Write(u8 addr, u8 reg, u8 data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=reg, 2 (ix)=data. FN_CORE44 wants
; C=CAN ADDR, D=CAN REG, E=CAN DATA.
;---------------------------------------------------------------------------------------------------
_Can_Write::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	c, a
	ld	a, 1 (ix)
	ld	d, a
	ld	a, 2 (ix)
	ld	e, a
	ld	a, #CAN_FN_WR
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; u8 Can_Read(u8 addr, u8 reg)
; sdcccall(1) "8+8": A=addr, L=reg. FN_CORE45 wants C=CAN ADDR, D=CAN REG;
; output A=CAN DATA (from CANINP's own doc block, which the FN_CORE45
; wrapper-level comment omits).
;---------------------------------------------------------------------------------------------------
_Can_Read::
	ld	c, a
	ld	d, l
	ld	a, #CAN_FN_RD
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Can_Check(u8 addr)
; FN_CORE46 wants C=CAN ADDR; output A=RMC (1 if a new RX message is
; pending, 0 otherwise), Carry always cleared.
;---------------------------------------------------------------------------------------------------
_Can_Check::
	ld	c, a
	ld	a, #CAN_FN_CHECK
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Can_Rx(u8 addr, void* data)
; sdcccall(1) "8+16": A=addr, DE=data. FN_CORE47 wants C=CAN ADDR,
; DE=varptr(RX DATA).
;---------------------------------------------------------------------------------------------------
_Can_Rx::
	ld	c, a
	ld	a, #CAN_FN_RX
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u16 Can_GetIntAddr(u8 addr)
; FN_CORE48 wants C=CAN ADDR; output DE=INTADDR - already the correct
; sdcccall(1) 16-bit return register, no shuffling needed.
;---------------------------------------------------------------------------------------------------
_Can_GetIntAddr::
	ld	c, a
	ld	a, #CAN_FN_GETADDR
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Can_Tx(u8 addr, void* data)
; sdcccall(1) "8+16": A=addr, DE=data. FN_CORE49 wants C=CAN ADDR,
; DE=varptr(TX DATA).
;---------------------------------------------------------------------------------------------------
_Can_Tx::
	ld	c, a
	ld	a, #CAN_FN_TX
	call	_RcxCall
	ret

;=====================================================================================================
; SPI (CORE50-56)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Spi_Init(u8 addr)
;---------------------------------------------------------------------------------------------------
_Spi_Init::
	ld	c, a
	ld	a, #SPI_FN_INIT
	call	_RcxCall
	jr	c, spiinit_fail
	ld	a, #1
	ret
spiinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; void Spi_Write(u8 addr, u8 bus, u8 data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=bus, 2 (ix)=data. FN_CORE51 wants
; E=SPI ADDR, C=SPI BUS, B=SPI DATA.
;---------------------------------------------------------------------------------------------------
_Spi_Write::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	e, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	a, #SPI_FN_WR
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; void Spi_Wrb(u8 addr, u8 bus, u8 len, void* data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=bus, 2 (ix)=len, 3/4 (ix)=data (lo/hi).
; FN_CORE52 wants H=SPI ADDR, C=SPI BUS, B=SPI LEN, DE=varptr(SPI DATA).
;---------------------------------------------------------------------------------------------------
_Spi_Wrb::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	h, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	e, 3 (ix)
	ld	d, 4 (ix)
	ld	a, #SPI_FN_WRB
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; u8 Spi_Read(u8 addr, u8 bus)
; sdcccall(1) "8+8": A=addr, L=bus. FN_CORE53 wants C=SPI ADDR, B=SPI BUS,
; DE=varptr(SPI DATA) - a single byte written to *(DE).
;---------------------------------------------------------------------------------------------------
_Spi_Read::
	ld	c, a
	ld	b, l
	ld	de, #_RSPIREADBUF
	ld	a, #SPI_FN_RD
	call	_RcxCall
	ld	a, (_RSPIREADBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Spi_Rdb(u8 addr, u8 bus, u8 len, void* data) __sdcccall(0)
; Same stack layout as Spi_Wrb. FN_CORE54 wants H=SPI ADDR, C=SPI BUS,
; B=SPI LEN, DE=varptr(SPI DATA).
;---------------------------------------------------------------------------------------------------
_Spi_Rdb::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	h, a
	ld	a, 1 (ix)
	ld	c, a
	ld	a, 2 (ix)
	ld	b, a
	ld	e, 3 (ix)
	ld	d, 4 (ix)
	ld	a, #SPI_FN_RDB
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; void Spi_Rdh(u8 addr, u8 bus, u16 lenHeaderData, void* data) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=bus, 2/3 (ix)=lenHeaderData (lo/hi),
; 4/5 (ix)=data (lo/hi). FN_CORE55SLT reads H/L directly at entry (verified
; against its actual code, not just FN_CORE55's own doc comment): loading
; L=lenHeaderData's low byte (SPI LEN DATA) and H=its high byte (SPI LEN
; HEADER) reproduces exactly that H/L split, since lenHeaderData is built as
; ((u16)headerLen << 8) | dataLen - little-endian in memory, so its low byte
; already is dataLen and its high byte already is headerLen. C=SPI ADDR and
; B=SPI BUS still need injecting; D/E=varptr(SPI DATA).
;---------------------------------------------------------------------------------------------------
_Spi_Rdh::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	c, a
	ld	a, 1 (ix)
	ld	b, a
	ld	l, 2 (ix)
	ld	h, 3 (ix)
	ld	e, 4 (ix)
	ld	d, 5 (ix)
	ld	a, #SPI_FN_RDH
	call	_RcxCall
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; void Spi_Mode(u8 addr, u8 bus, u8 mode, u8 freq) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=bus, 2 (ix)=mode, 3 (ix)=freq.
; FN_CORE56 wants C=SPI ADDR, B=SPI BUS, D=SPI MODE, E=SPI FREQUENCY.
;---------------------------------------------------------------------------------------------------
_Spi_Mode::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	c, a
	ld	a, 1 (ix)
	ld	b, a
	ld	a, 2 (ix)
	ld	d, a
	ld	a, 3 (ix)
	ld	e, a
	ld	a, #SPI_FN_MODE
	call	_RcxCall
	pop	ix
	ret

;=====================================================================================================
; ADS1220 (CORE57-62)
;=====================================================================================================

;---------------------------------------------------------------------------------------------------
; bool Ads_Init(u8 addr)
;---------------------------------------------------------------------------------------------------
_Ads_Init::
	ld	c, a
	ld	a, #ADS_FN_INIT
	call	_RcxCall
	jr	c, adsinit_fail
	ld	a, #1
	ret
adsinit_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; void Ads_SetMode(u8 addr, u8 type)
; sdcccall(1) "8+8": A=addr, L=type. FN_CORE58 wants C=ADS ADDR, E=ADS TYPE.
;---------------------------------------------------------------------------------------------------
_Ads_SetMode::
	ld	c, a
	ld	e, l
	ld	a, #ADS_FN_MODE
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; void Ads_Cmd(u8 addr, u8 val)
; sdcccall(1) "8+8": A=addr, L=val. FN_CORE59 wants C=ADS ADDR, E=ADS VAL.
;---------------------------------------------------------------------------------------------------
_Ads_Cmd::
	ld	c, a
	ld	e, l
	ld	a, #ADS_FN_CMD
	call	_RcxCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Ads_GetStatus(u8 addr)
; FN_CORE60 wants C=ADS ADDR, DE=varptr(ADS VAL) - a single byte.
;---------------------------------------------------------------------------------------------------
_Ads_GetStatus::
	ld	c, a
	ld	de, #_RADSBUF
	ld	a, #ADS_FN_STATUS
	call	_RcxCall
	ld	a, (_RADSBUF)
	ret

;---------------------------------------------------------------------------------------------------
; u8 Ads_Read(u8 addr)
; FN_CORE61 wants C=ADS ADDR, DE=varptr(ADS VAL) - one RESULT byte.
;---------------------------------------------------------------------------------------------------
_Ads_Read::
	ld	c, a
	ld	de, #_RADSBUF
	ld	a, #ADS_FN_READ
	call	_RcxCall
	ld	a, (_RADSBUF)
	ret

;---------------------------------------------------------------------------------------------------
; void Ads_Get(u8 addr, u8 type, void* val) __sdcccall(0)
; sdcccall(0): 0 (ix)=addr, 1 (ix)=type, 2/3 (ix)=val (lo/hi). FN_CORE62
; wants C=ADS ADDR, B=ADS TYPE, DE=varptr(ADS VAL) - DE left untouched.
;---------------------------------------------------------------------------------------------------
_Ads_Get::
	push	ix
	ld	ix, #4
	add	ix, sp
	ld	a, 0 (ix)
	ld	c, a
	ld	a, 1 (ix)
	ld	b, a
	ld	e, 2 (ix)
	ld	d, 3 (ix)
	ld	a, #ADS_FN_GET
	call	_RcxCall
	pop	ix
	ret

	.area _CODE
_RIDSTR:
	.ascii "RCX_INTERFACE"
	.db	0
_RIDSTRLEN	=	. - _RIDSTR
