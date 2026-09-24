//***************************************************************************************************
//* HEADER - 8254I.c - C/SDCC/MSXgl port of COM/CARD_D/8254I/8254I.ASM                              *
//***************************************************************************************************
//* TIMER0/TIMER1 both MODE 2, TIMER0 0xFFFF, TIMER1 16, TIMER2 MODE 0 0xFFFF, all gated on, OUT0->  *
//* CLK1, OUT1->CLK2; OUT1 is ALSO wired to a PIC channel - TIMER2 is read/printed continuously via   *
//* LOCATE (no scroll), and every PIC interrupt prints "TIMER1 fired" plus a running interrupt count, *
//* also via LOCATE. Discovers both RCX_INTERFACE and XIO_IO_EXPANDER separately (two independent    *
//* UNAPI drivers, own slot/segment/entry each), via the rcx_io and xio_io libraries (see             *
//* MSXgl/lib/rcx_io/, MSXgl/lib/xio_io/).                                                            *
//***************************************************************************************************
//* Datei: 8254I.c                                                                                  *
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
			"8254I - UNAPI/EXTBIOS port of 8254I.ASC (card type\r\n"
			"D, 82C54), interrupt-driven display refresh via\r\n"
			"XIO_IO_EXPANDER, like RTCLCDI.COM. Wire OUT0 to\r\n"
			"CLK1, OUT1 to CLK2 AND to a PIC channel. TIMER2 is\r\n"
			"shown continuously; every PIC interrupt (from\r\n"
			"TIMER1) also shows a running interrupt count.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"8254I - UNAPI/EXTBIOS driver for\r\n"
			"RCX card type D (82C54 timer),\r\n"
			"interrupt-driven like RTCLCDI.\r\n"
			"Wire OUT0->CLK1, OUT1->CLK2 and\r\n"
			"to a PIC channel.\r\n"
			"\r\n$");

	// static: &timerAddr/&channel are passed to Dos_ParseParam below -
	// see the SDCC SP-reset checklist for why a plain block-local here
	// would be addressed inconsistently once SP has moved.
	static u8 timerAddr;
	static u8 channel;
	if (!Dos_ParseParam("/IO:", 251, &timerAddr) || !Dos_ParseParam("/INT:", 7, &channel))
	{
		DOS_StringOutput("Usage: 8254I /IO:<0..251> /INT:<0..7>\r\n$");
		Dos_PauseThenExit();
	}
	u8 maskVal = (u8)(1 << channel);

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

	// Setup: RCX ISINIT/DEINIT/INIT, TIMER INIT
	static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
					// 8254I.ASM's WORKAREA)
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	Timer_Init(timerAddr);	// BASIC does not check a Carry result here,
				// so neither does this port

	// Timer setup
	Timer_Ctrl(timerAddr, 0x0034);	// TIMER0, MODE 2, RW=11, BCD=0
	Timer_Ctrl(timerAddr, 0x0074);	// TIMER1, MODE 2, RW=11, BCD=0
	Timer_ResetGate(timerAddr, 7);
	Timer_Write0(timerAddr, 0xFFFF);
	Timer_Write1(timerAddr, 16);
	Timer_Write2(timerAddr, 0xFFFF);
	Timer_SetGate(timerAddr, 7);	// all three gates on, once, and never
					// toggled again

	// Activate the PIC channel only now - AFTER the timer setup is
	// fully done, not before: an early-activated XIO handler can
	// disturb in-flight RCX bus traffic.
	// On failure, MAINLOOP still runs below (matches 8254I.ASM: the
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
		// static: iCount is touched every MAINLOOP iteration - see the
		// SDCC SP-reset checklist for why this must not be a plain
		// block-local here.
		static u16 iCount = 0;

		for (;;)
		{
			u16 result = Timer_Read2(timerAddr);

			Dos_PrLoc(2, 0);
			DOS_StringOutput("TIMER2: $");
			Dos_PrintHexWordNoLead(result);
			DOS_StringOutput("      $");

			// Test and clear the interrupt bit atomically (DI/EI)
			DisableInterrupt();
			bool fired = (g_IntFlagsLo & maskVal) != 0;
			if (fired)
				g_IntFlagsLo &= ~maskVal;
			EnableInterrupt();

			if (fired)
			{
				iCount++;

				Dos_PrLoc(4, 0);
				DOS_StringOutput("TIMER1 fired  count=$");
				Dos_PrintDecWord(iCount);
				DOS_StringOutput("   $");
			}

			// Non-blocking key poll
			if (Dos_ConsoleStatus() != 0)
			{
				if (Dos_ConsoleInput() == ' ')
					break;
			}
		}

		Dos_PrLoc(6, 0);
		Xio_SetMask(0);
	}

	Xio_Deinit();
	Rcx_Deinit();
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
