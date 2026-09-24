//***************************************************************************************************
//* HEADER - Z80PIO.c - C/SDCC/MSXgl port of COM/CARD_B/Z80PIO/Z80PIO.ASM                           *
//***************************************************************************************************
//* RCX INIT, PIO INIT, Port A MODE 0 (byte output), Port B MODE 1 (byte input), then writes an     *
//* incrementing byte to Port A and reads Port B back in a loop, printed via LOCATE (no scroll),     *
//* exit on SPACE - via FN_CORE2/3/4/16/17/18/21/22 directly through EXTBIO discovery, no BASIC       *
//* interpreter involved. Works against RCX_INTERFACE in ROM (CALSLT) or in mapped RAM (RCXRAM.COM), *
//* through the rcx_io library (see MSXgl/lib/rcx_io/). Uses RC2014 SC103 as DUT.                    *
//***************************************************************************************************
//* Datei: Z80PIO.c                                                                                 *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "rcx_io.h"
#include "dostools.h"

//===================================================================================================
// DEFINE
//===================================================================================================

#define LINLEN	0xF3B0	// current active screen text width

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
				// Z80PIO.ASM's WORKAREA)

//===================================================================================================
// MAIN LOOP
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Program entry point
void main(void)
{
	// Screen width: reads the currently active column count (LINLEN),
	// never changes it - 40-column banner below the 41-column threshold,
	// 80-column banner at or above it
	u8 cols = (*(u8*)LINLEN >= 41) ? 80 : 40;

	if (cols == 80)
		DOS_StringOutput("\x0C"
			"Z80PIO - UNAPI/EXTBIOS port of Z80PIO.ASC (card\r\n"
			"type B, Z80 PIO). Port A MODE 0 (byte output),\r\n"
			"Port B MODE 1 (byte input) - writes an\r\n"
			"incrementing byte to Port A and reads Port B\r\n"
			"back in a loop, via FN_CORE2/3/4/16/17/18/21/22\r\n"
			"directly, no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"Z80PIO - UNAPI/EXTBIOS driver for RCX\r\n"
			"card type B (Z80 PIO). Port A as byte\r\n"
			"output, Port B as byte input, writes\r\n"
			"an incrementing byte and reads it back.\r\n"
			"\r\n$");

	// static: &pioAddr is passed to Dos_ParseParam below - see the SDCC
	// SP-reset checklist for why a plain block-local here would be
	// addressed inconsistently once SP has moved.
	static u8 pioAddr;
	if (!Dos_ParseParam("/IO:", 252, &pioAddr))
	{
		DOS_StringOutput("Usage: Z80PIO /IO:<0..252>\r\n$");
		Dos_PauseThenExit();
	}

	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("RCX_INTERFACE found, slot=$");
	Dos_PrintHex2(g_RSlot);
	DOS_StringOutput("\r\n$");

	// Setup: RCX ISINIT/DEINIT/INIT, PIO INIT, Port A/B CTRL
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	Pio_Init(pioAddr);
	Pio_CtrlA(pioAddr, 0x0F);	// MODE 0 (byte output)
	Pio_CtrlB(pioAddr, 0x4F);	// MODE 1 (byte input)

	DOS_CharOutput(12);	// CLS
	Dos_PrLoc(0, 0);
	DOS_StringOutput("<SPACE> ends programme\r\n$");

	// static: outByte is read/written every MAINLOOP iteration - see the
	// SDCC SP-reset checklist for why a plain block-local surviving
	// across many intervening calls must not be trusted here.
	static u8 outByte = 0;

	for (;;)
	{
		Pio_WriteA(pioAddr, outByte);
		u8 inByte = Pio_ReadB(pioAddr);

		Dos_PrLoc(3, 0);
		DOS_StringOutput("OUT: $");
		Dos_PrintHexNoLead(outByte);
		DOS_StringOutput(" IN: $");
		Dos_PrintHexNoLead(inByte);
		DOS_StringOutput("    $");

		outByte++;

		// Non-blocking key poll
		if (Dos_ConsoleStatus() != 0)
		{
			if (Dos_ConsoleInput() == ' ')
				break;
		}
	}

	Dos_PrLoc(6, 0);
	Rcx_Deinit();
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
