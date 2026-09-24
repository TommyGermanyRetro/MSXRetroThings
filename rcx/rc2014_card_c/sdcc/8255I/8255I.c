//***************************************************************************************************
//* HEADER - 8255I.c - C/SDCC/MSXgl port of COM/CARD_C/8255I/8255I.ASM                              *
//***************************************************************************************************
//* Mode 0, Port A output, Port C upper output; Group B is Mode 1 (Port B strobed input) so the      *
//* 82C55 itself raises INTR B on PC0 - fed to the PIC via the XIO standard handler (INTFLAGS         *
//* bitmask, no custom ISR, no XIO SET ADDR). PC4 is toggled via PPI SET/RESET each loop pass - wire  *
//* it to PC2 (STB B) to trigger the interrupt. Discovers both RCX_INTERFACE and XIO_IO_EXPANDER      *
//* separately (two independent UNAPI drivers, own slot/segment/entry each), via the rcx_io and       *
//* xio_io libraries (see MSXgl/lib/rcx_io/, MSXgl/lib/xio_io/).                                      *
//***************************************************************************************************
//* Datei: 8255I.c                                                                                  *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "xio_io.h"
#include "rcx_io.h"
#include "dostools.h"

//===================================================================================================
// DEFINE
//===================================================================================================

#define LINLEN	0xF3B0	// current active screen text width

//===================================================================================================
// MEMORY DATA
//===================================================================================================

// Low byte of the XIO PIC flag word XIO_INIT keeps updated on every PIC
// interrupt (bit0=channel0 ... bit7=channel7) - same convention as
// XIOPIC.c's g_IntFlagsLo. Fixed page-3 address (see header) - the real
// ROM interrupt handler writes here directly, not through this variable
// declaration, so it must not live inside this .COM's own transient image.
volatile u8 __at(XIO_INTFLAGS) g_IntFlagsLo;

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
			"8255I - UNAPI/EXTBIOS port of 8255I.ASC (card\r\n"
			"type C, 82C55 PPI, interrupt test). Port B Mode 1\r\n"
			"(strobed input) - the 82C55 itself raises INTR B\r\n"
			"on PC0, reported via the XIO standard handler (no\r\n"
			"custom ISR), directly, no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"8255I - UNAPI/EXTBIOS driver for RCX\r\n"
			"card type C (82C55 PPI), interrupt test.\r\n"
			"Port B Mode 1 - the 82C55 itself raises\r\n"
			"INTR B on PC0.\r\n"
			"\r\n$");

	// static: &pioAddr/&intChan are passed to Dos_ParseParam below - see
	// the SDCC SP-reset checklist for why a plain block-local here would
	// be addressed inconsistently once SP has moved.
	static u8 pioAddr;
	static u8 intChan;
	if (!Dos_ParseParam("/IO:", 252, &pioAddr) || !Dos_ParseParam("/INT:", 7, &intChan))
	{
		DOS_StringOutput("Usage: 8255I /IO:<0..252> /INT:<0..7>\r\n$");
		Dos_PauseThenExit();
	}
	u8 maskVal = (u8)(1 << intChan);

	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("RCX_INTERFACE found, slot=$");
	Dos_PrintHex2(g_RSlot);
	DOS_StringOutput("\r\n$");

	if (!Xio_Discover())
	{
		DOS_StringOutput("XIO_IO_EXPANDER not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("XIO_IO_EXPANDER found, slot=$");
	Dos_PrintHex2(g_XSlot);
	DOS_StringOutput("\r\n$");

	// Setup: XIO ISINIT/DEINIT/INIT
	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// Setup: RCX ISINIT/DEINIT/INIT
	static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
					// 8255I.ASM's WORKAREA)
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// PPI setup: Group A Mode 0 (Port A out, PC-hi out), Group B Mode 1
	// (Port B in, INTR B on PC0)
	Ppi_Init(pioAddr);
	Ppi_Ctrl(pioAddr, 0x87);
	Ppi_SetBit(pioAddr, 2);	// enable INTE B (BSR on PC2)

	// Activate the PIC channel only now - AFTER the PPI setup is fully
	// done, not before: an early interrupt could otherwise land
	// mid-setup.
	// On failure, MAINLOOP still runs below (matches 8255I.ASM: the
	// error path falls straight through into MAINLOOP without a CLS or
	// wait message - the interrupt bit just never gets set, so REFRESH
	// never fires).
	if (!Xio_SetMask(maskVal))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
	}
	else
	{
		DOS_CharOutput(12);	// CLS
		Dos_PrLoc(0, 0);
		DOS_StringOutput("<SPACE> ends programme, waiting for INT\r\n$");
	}

	{
		// static: outByte/iCount are touched every MAINLOOP iteration -
		// see the SDCC SP-reset checklist for why these must not be
		// plain block-locals here.
		static u8 outByte = 0;
		static u16 iCount = 0;

		for (;;)
		{
			Ppi_WriteA(pioAddr, outByte);
			u8 aRead = Ppi_ReadA(pioAddr);

			// Toggle PC4 - wire to PC2 (STB B) to trigger
			if ((outByte & 1) == 0)
				Ppi_SetBit(pioAddr, 4);
			else
				Ppi_ResetBit(pioAddr, 4);

			Dos_PrLoc(2, 0);
			DOS_StringOutput("OUT: $");
			Dos_PrintHexNoLead(outByte);
			DOS_StringOutput(" A-RD: $");
			Dos_PrintHexNoLead(aRead);
			DOS_StringOutput("    $");

			// Test and clear the interrupt bit atomically (DI/EI)
			DisableInterrupt();
			bool fired = (g_IntFlagsLo & maskVal) != 0;
			if (fired)
				g_IntFlagsLo &= ~maskVal;
			EnableInterrupt();

			if (fired)
			{
				// _PPI B RD (also clears INTR B/IBF B in the
				// 82C55 itself)
				u8 bRead = Ppi_ReadB(pioAddr);
				iCount++;

				Dos_PrLoc(3, 0);
				DOS_StringOutput("INT fired, B-RD: $");
				Dos_PrintHexWordNoLead(bRead);
				DOS_StringOutput(" count:$");
				Dos_PrintDecWord(iCount);
				DOS_StringOutput("   $");
			}

			outByte++;

			// Non-blocking key poll
			if (Dos_ConsoleStatus() != 0)
			{
				if (Dos_ConsoleInput() == ' ')
					break;
			}
		}

		Ppi_ResetBit(pioAddr, 2);
		Xio_SetMask(0);
	}

	Rcx_Deinit();
	Xio_Deinit();

	Dos_PrLoc(5, 0);
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
