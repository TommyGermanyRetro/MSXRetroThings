//***************************************************************************************************
//* HEADER - RCXSLT.c - C/SDCC/MSXgl port of BASIC/RCXSLT/RCXSLT.ASC                                *
//***************************************************************************************************
//* Diagnostic tool, no XIO/RCX UNAPI calls at all - reads MSX BIOS structures directly. Asks for a  *
//* main slot number, dumps the 8-byte slot work area (SLTWRK) for that slot, then follows the       *
//* pointer stored at its bytes 6+7 and dumps &H60 (96) further bytes from there.                    *
//*                                                                                                 *
//* Generic and card-independent, exactly like the BASIC original - no EXTBIO discovery is needed   *
//* (there is nothing to discover), only the shared dostools library is used, for its               *
//* Dos_ReadLine/Dos_PrintHex2 helpers (plain MSX-DOS console I/O, no Xio_*/RCX call).                   *
//***************************************************************************************************
//* Datei: RCXSLT.c                                                                                 *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 16/09/26                                                                               *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "dostools.h"

//===================================================================================================
// DEFINE
//===================================================================================================

#define SLTWRK 0xFD09 // MSX BIOS slot work area, 32 bytes per main slot

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Dumps <count> bytes starting at <addr> as Dos_PrintHex2 plus a trailing
// space (matches the original's "0"+HEX$(x)+" " / HEX$(x)+" " leading-zero
// padding exactly), inserting a CRLF after every 8 bytes - matches the
// original's C=0/C=C+1/IF C=8 line-break counter used identically for both
// dumps.
void DumpBytes(u16 addr, u16 count)
{
	u8 col = 0;
	for (u16 i = 0; i < count; i++)
	{
		Dos_PrintHex2(*(u8*)(addr + i));
		DOS_CharOutput(' ');
		col++;
		if (col == 8)
		{
			col = 0;
			DOS_StringOutput("\r\n$");
		}
	}
}

//---------------------------------------------------------------------------------------------------
// Parses a run of leading decimal digits (like BASIC's VAL) - stops at the
// first non-digit, returns 0 if there is none.
u16 ParseVal(u8* text, u8 length)
{
	u16 value = 0;
	for (u8 i = 0; i < length; i++)
	{
		if ((text[i] < '0') || (text[i] > '9'))
			break;
		value = (value * 10) + (text[i] - '0');
	}
	return value;
}

//===================================================================================================
// MAIN LOOP
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Program entry point
void main(void)
{
	// Set the stack pointer to the top of the Transient Program Area
	// (word at 0x0006, set up by MSX-DOS before jumping to 0x0100). The
	// crt0 does not do this itself, so without it SP keeps whatever value
	// happened to be in place at program start.
	__asm
		ld	hl, (0x0006)
		ld	sp, hl
	__endasm;

	// BDOS function 0x0A ("buffered console input") structure: byte 0 is
	// the max length (set before the call), byte 1 becomes the actual
	// length, byte 2.. holds the typed characters (no terminator) - the
	// exact mechanism BASIC's own INPUT uses internally.
	u8 inbuf[22];
	inbuf[0] = 20;

	DOS_StringOutput("Main slot number:? $");
	Dos_ReadLine(inbuf);
	DOS_StringOutput("\r\n$");

	u8 slot = (u8)ParseVal(&inbuf[2], inbuf[1]);
	u16 base = SLTWRK + (u16)slot * 32;

	DOS_StringOutput("Slot Work Area\r\n$");
	DumpBytes(base, 8); // hits the col==8 CRLF exactly once - same as the
	                     // original's single bare PRINT after its loop

	u16 ptr = *(u8*)(base + 6) | ((u16)(*(u8*)(base + 7)) << 8);

	DOS_StringOutput("Slot Work Area from Ptr Byte 6+7\r\n$");
	DumpBytes(ptr, 0x60);

	Bios_Exit(0);
}
