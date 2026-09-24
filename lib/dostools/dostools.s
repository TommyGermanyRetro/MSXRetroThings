;***************************************************************************************************
;* HEADER - dostools.s - Direct BDOS wrappers shared across this project's SDCC ports                *
;***************************************************************************************************
;* Plain MSX-DOS console I/O, not tied to any UNAPI driver - moved here verbatim from xio_io.s,      *
;* which only ever held these because no other shared library existed yet at the time.               *
;***************************************************************************************************
;* Datei: dostools.s                                                                                *
;***************************************************************************************************
;* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
;***************************************************************************************************
;* VERSION: 16/09/26                                                                                *
;***************************************************************************************************
	.module dostools

	.globl _Dos_ReadLine
	.globl _Dos_ConsoleStatus
	.globl _Dos_ConsoleInput

BDOS		=	0x0005

	.area _CODE

;---------------------------------------------------------------------------------------------------
; void Dos_ReadLine(u8* buf)
; Entry: HL=buf. buf[0] must already hold the max length before calling;
; BDOS fills buf[1] with the actual length and buf[2..] with the
; characters (the raw BDOS function 0x0A "buffered console input"
; structure) - the same mechanism BASIC's own INPUT uses internally.
;---------------------------------------------------------------------------------------------------
_Dos_ReadLine::
	ex	de, hl
	ld	c, #0x0A
	call	BDOS
	ret

;---------------------------------------------------------------------------------------------------
; u8 Dos_ConsoleStatus(void)
; BDOS function 0x0B - non-zero if a key is waiting.
;---------------------------------------------------------------------------------------------------
_Dos_ConsoleStatus::
	ld	c, #0x0B
	call	BDOS
	ret

;---------------------------------------------------------------------------------------------------
; c8 Dos_ConsoleInput(void)
; BDOS function 0x01 - blocking read (with echo) of one waiting character.
;---------------------------------------------------------------------------------------------------
_Dos_ConsoleInput::
	ld	c, #0x01
	call	BDOS
	ret
