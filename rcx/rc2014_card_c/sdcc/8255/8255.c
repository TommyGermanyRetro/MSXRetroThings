//***************************************************************************************************
//* HEADER - 8255.c - C/SDCC/MSXgl port of COM/CARD_C/8255/8255.ASM                                 *
//***************************************************************************************************
//* RCX INIT, PPI INIT, PPI CTRL (Mode 0, Port A output, Port B input, Port C output), then writes    *
//* an incrementing byte to Port A and reads Port A/B/C back, toggles a Port C bit via SET/RESET,     *
//* all printed via LOCATE (no scroll), exit on SPACE - via FN_CORE2/3/4/23/24/25/26/28/30/31/32      *
//* directly through EXTBIO discovery, no BASIC interpreter involved. Works against RCX_INTERFACE in  *
//* ROM (CALSLT) or in mapped RAM (RCXRAM.COM), through the rcx_io library (see MSXgl/lib/rcx_io/).   *
//***************************************************************************************************
//* Datei: 8255.c                                                                                   *
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
				// 8255.ASM's WORKAREA)

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
			"8255 - UNAPI/EXTBIOS port of 8255.ASC (card type\r\n"
			"C, 82C55 PPI). Mode 0, Port A byte output, Port B\r\n"
			"byte input, Port C byte output - writes an\r\n"
			"incrementing byte to Port A and reads Port A/B/C\r\n"
			"back, toggles a Port C bit via SET/RESET, via\r\n"
			"FN_CORE2/3/4/23/24/25/26/28/30/31/32 directly,\r\n"
			"no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"8255 - UNAPI/EXTBIOS driver for RCX\r\n"
			"card type C (82C55 PPI). Port A as byte\r\n"
			"output, Port B as byte input, Port C as\r\n"
			"byte output with bit set/reset - writes\r\n"
			"an incrementing byte and reads it back.\r\n"
			"\r\n$");

	// static: &pioAddr is passed to Dos_ParseParam below - see the SDCC
	// SP-reset checklist for why a plain block-local here would be
	// addressed inconsistently once SP has moved.
	static u8 pioAddr;
	if (!Dos_ParseParam("/IO:", 252, &pioAddr))
	{
		DOS_StringOutput("Usage: 8255 /IO:<0..252>\r\n$");
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

	// Setup: RCX ISINIT/DEINIT/INIT, PPI INIT, PPI CTRL
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	Ppi_Init(pioAddr);
	Ppi_Ctrl(pioAddr, 0x82);	// Mode 0, Port A output, Port B input, Port C output

	DOS_CharOutput(12);	// CLS
	DOS_StringOutput("<SPACE> ends programme\r\n$");

	// static: outByte is read/written every MAINLOOP iteration - see the
	// SDCC SP-reset checklist for why a plain block-local surviving
	// across many intervening calls must not be trusted here.
	static u8 outByte = 0;

	for (;;)
	{
		Ppi_WriteA(pioAddr, outByte);
		u8 aRead = Ppi_ReadA(pioAddr);
		u8 bRead = Ppi_ReadB(pioAddr);

		// K = J AND 7 : IF (J AND 8) = 0 THEN PPI SET(Q,K) ELSE PPI RESET(Q,K)
		u8 bitIdx = outByte & 0x07;
		if ((outByte & 0x08) == 0)
			Ppi_SetBit(pioAddr, bitIdx);
		else
			Ppi_ResetBit(pioAddr, bitIdx);

		u8 cRead = Ppi_ReadC(pioAddr);

		Dos_PrLoc(3, 0);
		DOS_StringOutput("OUT: $");
		Dos_PrintHexNoLead(outByte);
		DOS_StringOutput(" A-RD: $");
		Dos_PrintHexNoLead(aRead);
		DOS_StringOutput("    $");

		Dos_PrLoc(4, 0);
		DOS_StringOutput("B-RD: $");
		Dos_PrintHexNoLead(bRead);
		DOS_StringOutput(" C-RD: $");
		Dos_PrintHexNoLead(cRead);
		DOS_StringOutput("    $");

		outByte++;

		// Non-blocking key poll
		if (Dos_ConsoleStatus() != 0)
		{
			if (Dos_ConsoleInput() == ' ')
				break;
		}
	}

	Rcx_Deinit();
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
