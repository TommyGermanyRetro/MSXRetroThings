//***************************************************************************************************
//* HEADER - 8254.c - C/SDCC/MSXgl port of COM/CARD_D/8254/8254.ASM                                 *
//***************************************************************************************************
//* RCX INIT, TIMER INIT, TIMER0/TIMER1 both set to MODE 2 (Rate Generator) via TIMER CTRL, TIMER0    *
//* preset to 0xFFFF, TIMER1 to 16, TIMER2 (MODE 0) preset to 0xFFFF, all three gated on once - OUT0  *
//* wired externally to CLK1, OUT1 to CLK2 - then reads TIMER2 back in a loop, via EXTBIO discovery,  *
//* no BASIC interpreter involved. Works against RCX_INTERFACE in ROM (CALSLT) or in mapped RAM       *
//* (RCXRAM.COM), through the rcx_io library (see MSXgl/lib/rcx_io/).                                *
//***************************************************************************************************
//* Datei: 8254.c                                                                                   *
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
				// 8254.ASM's WORKAREA)

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
			"8254 - UNAPI/EXTBIOS port of 8254.ASC (card type\r\n"
			"D, 82C54). Wire OUT0 to CLK1, OUT1 to CLK2.\r\n"
			"TIMER0/1 MODE 2 (0xFFFF/16), TIMER2 MODE 0\r\n"
			"(0xFFFF), all gated on - then reads TIMER 2 back\r\n"
			"in a loop, via FN_CORE33/35/37/38/39/40/41/42\r\n"
			"directly, no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"8254 - UNAPI/EXTBIOS driver for RCX\r\n"
			"card type D (82C54 timer). Wire\r\n"
			"OUT0->CLK1, OUT1->CLK2. Reads\r\n"
			"TIMER 2 back in a loop.\r\n"
			"\r\n$");

	// static: &timerAddr is passed to Dos_ParseParam below - see the
	// SDCC SP-reset checklist for why a plain block-local here would be
	// addressed inconsistently once SP has moved.
	static u8 timerAddr;
	if (!Dos_ParseParam("/IO:", 251, &timerAddr))
	{
		DOS_StringOutput("Usage: 8254 /IO:<0..251>\r\n$");
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

	// Setup: RCX ISINIT/DEINIT/INIT, TIMER INIT
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// TIMER INIT(Q) - BASIC does not check a Carry result here, so
	// neither does this port
	Timer_Init(timerAddr);

	// Timer setup - TIMER0/TIMER1 are moved from the fixed MODE 0 that
	// TIMER INIT applies to all three counters into MODE 2 (Rate
	// Generator), so their OUT pulses repeatedly instead of a one-shot
	// transition - TIMER2 stays MODE 0.
	Timer_Ctrl(timerAddr, 0x0034);	// TIMER0, MODE 2, RW=11, BCD=0
	Timer_Ctrl(timerAddr, 0x0074);	// TIMER1, MODE 2, RW=11, BCD=0
	Timer_ResetGate(timerAddr, 7);
	Timer_Write0(timerAddr, 0xFFFF);
	Timer_Write1(timerAddr, 16);
	Timer_Write2(timerAddr, 0xFFFF);
	Timer_SetGate(timerAddr, 7);	// all three gates on, set once and never
					// toggled again

	// Main loop - no exit condition, matches 8254.ASM's MAINLOOP (plain
	// "JP MAINLOOP", no key poll)
	for (;;)
	{
		u16 result = Timer_Read2(timerAddr);

		Dos_PrintHexWordNoLead(result);
		DOS_StringOutput("\r\n$");
	}
}
