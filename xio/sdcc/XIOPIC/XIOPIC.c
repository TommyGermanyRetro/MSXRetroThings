//***************************************************************************************************
//* HEADER - XIOPIC.c - C/SDCC/MSXgl port of BASIC/XIOPIC/20260913/XIOPIC.ASC                       *
//***************************************************************************************************
//* PIC interrupt test: installs a Z80 machine code ISR stub on PIC channel 0's vector and enables  *
//* all 8 PIC channels (0..7) via XIO SET MASK. Reports whichever channel fires with a running      *
//* per-channel counter at a fixed screen position - the interrupt source is external (whatever is  *
//* wired to that PIC input), this program only exercises the PIC's own SET ADDR/SET MASK           *
//* mechanism, no PIO/DUT setup of its own.                                                         *
//*                                                                                                 *
//* There is no BASIC interpreter here, so unlike the original .ASC the XIO_IO_EXPANDER UNAPI driver*
//* must be located explicitly through EXTBIO before any XIO call can be made (see xio_io.s).       *
//***************************************************************************************************
//* Datei: XIOPIC.c                                                                                 *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
//***************************************************************************************************
//* VERSION: 13/09/26                                                                               *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "memory.h"
#include "xio_io.h"
#include "dostools.h"

//===================================================================================================
// READ-ONLY DATA
//===================================================================================================

// Z80 ISR stub copied to XIO_ISRCODE: positions to row 4/col 0 via the
// console's ESC-Y code, blanks it with 13 spaces, repositions, prints
// "ISR has fired", then RET (PIC interrupt end - unlike Z80/IM2, a PIC
// service routine ends with RET, not RETI). Bytes send each character
// through the fixed BIOS CHPUT vector (0x00A2). Identical bytes to
// BASIC/XIOPIC/20260913/XIOPIC.ASC's DATA statements.
const u8 g_IsrData[] =
{
	0x3E, 0x1B, 0xCD, 0xA2, 0x00,
	0x3E, 0x59, 0xCD, 0xA2, 0x00,
	0x3E, 0x24, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x1B, 0xCD, 0xA2, 0x00,
	0x3E, 0x59, 0xCD, 0xA2, 0x00,
	0x3E, 0x24, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x49, 0xCD, 0xA2, 0x00,
	0x3E, 0x53, 0xCD, 0xA2, 0x00,
	0x3E, 0x52, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x68, 0xCD, 0xA2, 0x00,
	0x3E, 0x61, 0xCD, 0xA2, 0x00,
	0x3E, 0x73, 0xCD, 0xA2, 0x00,
	0x3E, 0x20, 0xCD, 0xA2, 0x00,
	0x3E, 0x66, 0xCD, 0xA2, 0x00,
	0x3E, 0x69, 0xCD, 0xA2, 0x00,
	0x3E, 0x72, 0xCD, 0xA2, 0x00,
	0x3E, 0x65, 0xCD, 0xA2, 0x00,
	0x3E, 0x64, 0xCD, 0xA2, 0x00,
	0xC9,
};

//===================================================================================================
// MEMORY DATA
//===================================================================================================

// Low byte of the IM2/PIC flag word XIO_INIT keeps updated on every PIC
// interrupt (bit0=channel0 ... bit7=channel7), independent of the custom
// ISR stub above.
volatile u8 __at(XIO_INTFLAGS) g_IntFlagsLo;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// LOCATEs to row 2/col 0, prints "Here channel N, count: " plus the
// per-channel counter (incremented here), padded to overwrite any longer
// previous text at the same position.
void ReportChannel(u8 channel, u16* counters)
{
	Dos_PrLoc(2, 0);
	DOS_StringOutput("Here channel $");
	DOS_CharOutput('0' + channel);
	DOS_StringOutput(", count: $");
	counters[channel]++;
	Dos_PrintDecWord(counters[channel]);
	DOS_StringOutput("    $");
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

	DOS_ClearScreen();

	if (!Xio_Discover())
	{
		DOS_StringOutput("XIO_IO_EXPANDER not found (count=$");
		DOS_CharOutput('0' + (g_XCount % 10));
		DOS_StringOutput(").\r\n$");
		Dos_PauseThenExit();
	}

	DOS_StringOutput("XIO_IO_EXPANDER found, slot=$");
	Dos_PrintHexNibble(g_XSlot >> 4);
	Dos_PrintHexNibble(g_XSlot);
	DOS_StringOutput(" seg=$");
	Dos_PrintHexNibble(g_XSeg >> 4);
	Dos_PrintHexNibble(g_XSeg);
	DOS_StringOutput("\r\n$");

	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] is installed\r\n$");

	// Install the ISR stub and register it for PIC channel 0 (fixed,
	// matching the BASIC original)
	Mem_Copy(g_IsrData, (void*)XIO_ISRCODE, sizeof(g_IsrData));
	if (!Xio_SetAddr(0, XIO_ISRCODE))
	{
		DOS_StringOutput("XIO SET ADDR failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// PIC mask: all 8 channels active (low byte of the mask word)
	if (!Xio_SetMask(0x00FF))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] mask for PIC is set to &H00FF\r\n$");

	DOS_ClearScreen();
	Dos_PrLoc(0, 0);
	DOS_StringOutput("<SPACE> ends programme, waiting for INT\r\n$");

	// static: a plain block-local declaration here got its zero-init
	// folded into the loop's own back-edge by SDCC, so it re-ran on
	// every iteration instead of once - static gives it a fixed
	// address instead. Zeroed with an explicit runtime loop rather
	// than an initializer list, since none of this project's MSXgl
	// crt0 variants call SDCC's generated GSINIT startup that would
	// normally run a static initializer before main().
	static u16 counters[8];
	for (u8 i = 0; i < 8; i++)
		counters[i] = 0;

	for (;;)
	{
		// The ROM ORs each channel's bit into g_IntFlagsLo as interrupts
		// arrive; clear only the reported channel's own bit so a bit set
		// by a concurrent interrupt is not lost.
		for (u8 ch = 0; ch < 8; ch++)
		{
			if (g_IntFlagsLo & (1 << ch))
			{
				ReportChannel(ch, counters);
				g_IntFlagsLo &= ~(1 << ch);
			}
		}

		// Non-blocking key poll
		if (Dos_ConsoleStatus() != 0)
		{
			if (Dos_ConsoleInput() == ' ')
				break;
		}
	}

	Xio_SetMask(0);
	Xio_SetAddr(0, 0);
	Xio_Deinit();
	Dos_PrLoc(6, 0);
	DOS_StringOutput("DONE\r\n$");

	Bios_Exit(0);
}
