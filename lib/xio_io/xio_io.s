;***************************************************************************************************
;* HEADER - xio_io.s - UNAPI/EXTBIOS access to XIO_IO_EXPANDER                                     *
;***************************************************************************************************
;* UNAPI/EXTBIOS access to XIO_IO_EXPANDER for SDCC/MSXgl C tools (sdasz80). Covers every FN_CORE  *
;* entry XIOROM.ASM exposes except BASIC's own interactive "XIO ?" help menu (FN_CORE1), which has *
;* no meaning outside the BASIC command line.                                                      *
;***************************************************************************************************
;* Datei: xio_io.s                                                                                 *
;***************************************************************************************************
;* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
;***************************************************************************************************
;* VERSION: 17/09/26                                                                               *
;***************************************************************************************************
	.module xio_io

	.globl _Xio_Discover
	.globl _Xio_GetInfo
	.globl _Xio_Out
	.globl _Xio_Inp
	.globl _Xio_Init
	.globl _Xio_IsInit
	.globl _Xio_Deinit
	.globl _Xio_SetAddr
	.globl _Xio_SetMask
	.globl _Xio_GetAddr
	.globl _Xio_GetMask
	.globl _g_XSlot
	.globl _g_XSeg
	.globl _g_XCount
	.globl _g_XRomVersion
	.globl _g_XApiVersion
	.globl _g_XApiInfo

EXTBIO		=	0xFFCA
CALSLT		=	0x001C
ARG		=	0xF847

XIO_FN_INFO		=	0
XIO_FN_OUT		=	2
XIO_FN_INP		=	3
XIO_FN_INIT		=	4
XIO_FN_ISINIT		=	5
XIO_FN_DEINIT		=	6
XIO_FN_SETADDR		=	7
XIO_FN_SETMASK		=	8
XIO_FN_GETADDR		=	9
XIO_FN_GETMASK		=	10

	.area _DATA
_g_XSlot::
	.db	0
_g_XSeg::
	.db	0
_g_XCount::
	.db	0
_g_XRomVersion::
	.dw	0
_g_XApiVersion::
	.dw	0
_g_XApiInfo::
	.dw	0
_g_XEntry:
	.dw	0
_ISIBUF:
	.dw	0
_INPBUF:
	.dw	0
_GETADDRBUF:
	.dw	0
_GETMASKBUF:
	.dw	0

	.area _CODE

;---------------------------------------------------------------------------------------------------
; bool Xio_Discover(void)
; Copies the "XIO_IO_EXPANDER" ID string to ARG, then runs the two-step
; EXTBIO discovery (implementation count, then slot/segment/entry). Unlike
; every other function in this file, this one calls EXTBIO directly instead
; of going through _XioCall - so it needs its own IX/IY save/restore around
; that call, matching _XioCall's own reasoning (IX is a C caller's local-
; variable frame pointer, IY is the system-wide BIOS/interrupt pointer;
; EXTBIO is not documented to preserve either).
; Returns TRUE (A=1) if found, FALSE (A=0) otherwise.
;---------------------------------------------------------------------------------------------------
_Xio_Discover::
	push	ix
	push	iy

	ld	hl, #_XIDSTR
	ld	de, #ARG
	ld	bc, #_XIDSTRLEN
	ldir

	xor	a
	ld	b, a
	ld	de, #0x2222
	call	EXTBIO
	ld	a, b
	ld	(_g_XCount), a
	or	a
	jr	z, discover_fail

	ld	a, #1
	ld	de, #0x2222
	call	EXTBIO
	ld	(_g_XSlot), a
	ld	a, b
	ld	(_g_XSeg), a
	ld	(_g_XEntry), hl

	pop	iy
	pop	ix
	ld	a, #1
	ret
discover_fail:
	pop	iy
	pop	ix
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; Shared dispatch: entry A=function number, B/C/D/E preset by the caller
; as documented by the target function. Builds IX/IY from the discovered
; slot/entry and calls CALSLT. Carry, and any of BC/DE/HL the target
; function documents as output, come back unchanged to the caller. ROM
; slot dispatch only - XIO_IO_EXPANDER has no mapped-RAM alternative in
; this project. Preserves IX across the call - C callers use it as their
; own local-variable frame pointer. Also preserves IY: on MSX, IY is a
; system-wide constant (BIOS/interrupt code relies on it pointing at the
; system variable area near EXPTBL), not a scratch register - overwriting
; it for CALSLT and never restoring it would leave it wrong for the rest
; of the program, interrupt handling included.
;---------------------------------------------------------------------------------------------------
_XioCall:
	push	ix
	push	iy
	push	af
	push	hl

	ld	hl, (_g_XEntry)
	push	hl
	pop	ix

	ld	a, (_g_XSlot)
	ld	h, a
	ld	l, #0
	push	hl
	pop	iy

	pop	hl
	pop	af

	call	CALSLT

	pop	iy
	pop	ix
	ret

;---------------------------------------------------------------------------------------------------
; bool Xio_GetInfo(void)
; Populates g_XRomVersion/g_XApiVersion/g_XApiInfo. Entry: no parameters -
; FN_INFO does not require the module to be initialized first.
;---------------------------------------------------------------------------------------------------
_Xio_GetInfo::
	xor	a
	call	_XioCall
	ld	(_g_XRomVersion), bc
	ld	(_g_XApiVersion), de
	ld	(_g_XApiInfo), hl
	ld	a, #1
	ret

;---------------------------------------------------------------------------------------------------
; void Xio_Out(u8 addr, u8 data)
; Entry: A=addr, L=data
;---------------------------------------------------------------------------------------------------
_Xio_Out::
	ld	c, a
	ld	b, l
	ld	a, #XIO_FN_OUT
	call	_XioCall
	ret

;---------------------------------------------------------------------------------------------------
; u8 Xio_Inp(u8 addr)
; Entry: A=addr. Reads into a fixed static buffer (see Xio_GetAddr for why
; a caller's stack-relative address is not used here), then returns the
; low byte in A.
;---------------------------------------------------------------------------------------------------
_Xio_Inp::
	ld	c, a
	ld	de, #_INPBUF
	ld	a, #XIO_FN_INP
	call	_XioCall
	ld	a, (_INPBUF)
	ret

;---------------------------------------------------------------------------------------------------
; u8 Xio_IsInit(void)
;---------------------------------------------------------------------------------------------------
_Xio_IsInit::
	ld	de, #_ISIBUF
	ld	a, #XIO_FN_ISINIT
	call	_XioCall
	ld	a, (_ISIBUF+1)
	ret

;---------------------------------------------------------------------------------------------------
; void Xio_Deinit(void)
;---------------------------------------------------------------------------------------------------
_Xio_Deinit::
	ld	a, #XIO_FN_DEINIT
	call	_XioCall
	ret

;---------------------------------------------------------------------------------------------------
; bool Xio_Init(u16 workarea, u16 intflags)
; Entry: HL=workarea, DE=intflags
;---------------------------------------------------------------------------------------------------
_Xio_Init::
	ld	a, #XIO_FN_INIT
	call	_XioCall
	jr	c, init_fail
	ld	a, #1
	ret
init_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; bool Xio_SetAddr(u8 channel, u16 addr)
; Entry: A=channel, DE=addr
;---------------------------------------------------------------------------------------------------
_Xio_SetAddr::
	ld	b, a
	ld	c, #0xFF
	ld	a, #XIO_FN_SETADDR
	call	_XioCall
	jr	c, setaddr_fail
	ld	a, #1
	ret
setaddr_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; bool Xio_SetMask(u16 mask)
; Entry: HL=mask
;---------------------------------------------------------------------------------------------------
_Xio_SetMask::
	ex	de, hl
	ld	a, #XIO_FN_SETMASK
	call	_XioCall
	jr	c, setmask_fail
	ld	a, #1
	ret
setmask_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; bool Xio_GetAddr(u8 channel, u16* outAddr)
; Entry: A=channel, DE=outAddr. The ROM writes its result to a fixed
; static buffer, never to outAddr directly - outAddr may be a stack-
; relative address of a caller's local variable, and the ROM write
; happens deep inside a CALSLT inter-slot call, several more pushes below
; where outAddr was computed; routing through fixed storage and copying
; to outAddr only after _XioCall returns avoids relying on that stack
; address staying meaningful across the whole call chain.
;---------------------------------------------------------------------------------------------------
_Xio_GetAddr::
	ld	b, a
	push	de
	ld	de, #_GETADDRBUF
	ld	c, #0xFF
	ld	a, #XIO_FN_GETADDR
	call	_XioCall
	pop	de
	jr	c, getaddr_fail
	ld	hl, #_GETADDRBUF
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	ld	a, #1
	ret
getaddr_fail:
	xor	a
	ret

;---------------------------------------------------------------------------------------------------
; void Xio_GetMask(u16* outMask)
; Entry: HL=outMask. Same fixed-buffer indirection as Xio_GetAddr, for
; the same reason.
;---------------------------------------------------------------------------------------------------
_Xio_GetMask::
	push	hl
	ld	de, #_GETMASKBUF
	ld	a, #XIO_FN_GETMASK
	call	_XioCall
	pop	de
	ld	hl, #_GETMASKBUF
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	ret

	.area _CODE
_XIDSTR:
	.ascii "XIO_IO_EXPANDER"
	.db	0
_XIDSTRLEN	=	. - _XIDSTR
