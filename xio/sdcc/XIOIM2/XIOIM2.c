//***************************************************************************************************
//* HEADER - XIOIM2.c - C/SDCC/MSXgl port of BASIC/XIOIM2/20260913/XIOIM2.ASC                       *
//***************************************************************************************************
//* Z80 PIO IM2 interrupt test against an SC103 card wired directly to the IO-Expander bus (no RCX  *
//* card involved, raw XIO OUT register access): configures Port A as output and Port B in          *
//* bitcontrol mode (Mode 3, PB0 as interrupt input), installs a small Z80 machine code ISR stub via*
//* XIO SET ADDR on the chosen IM2 channel, and toggles PA0 in the main loop. Reports which PIO     *
//* channel fired the interrupt.                                                                    *
//*                                                                                                 *
//* There is no BASIC interpreter here, so unlike the original .ASC the XIO_IO_EXPANDER UNAPI driver*
//* must be located explicitly through EXTBIO before any XIO call can be made (see xio_io.s).       *
//***************************************************************************************************
//* Datei: XIOIM2.c                                                                                 *
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
// "ISR has fired", then RETI (IM2 interrupt end). Bytes send each
// character through the fixed BIOS CHPUT vector (0x00A2). Identical
// bytes to BASIC/XIOIM2/20260913/XIOIM2.ASC's DATA statements.
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
	0xED, 0x4D,
};

//===================================================================================================
// MEMORY DATA
//===================================================================================================

// High byte of the IM2 flag word XIO_INIT keeps updated on every IM2
// interrupt (bit0=channel0 ... bit7=channel7), independent of the custom
// ISR stub above.
volatile u8 __at(XIO_INTFLAGS + 1) g_IntFlagsHi;

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

	// volatile: SDCC can otherwise keep these live in a register across
	// the intervening calls below and lose the value.
	volatile u8 pioAddr = Dos_AskDecimal("PIO card, IO address (0..252): $", 252);
	volatile u8 pioA2 = pioAddr + 2;	// Port A control
	volatile u8 pioA3 = pioAddr + 3;	// Port B control

	u8 imChan = Dos_AskDecimal("IM2 channel (0..7): $", 7);
	u8 xioChan = imChan + 8;
	// volatile: see pioAddr above - crosses the same gap of intervening
	// calls before it is used at the Z80 PIO setup step below.
	volatile u8 pioVector = imChan * 2;	// Z80 PIO INT-vector byte

	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] is installed\r\n$");

	// PIO setup via SC103, raw XIO OUT
	Xio_Out(pioA2, 0x0F);	// Port A as output
	Xio_Out(pioAddr, 0);	// Port A = 0
	Xio_Out(pioA3, 0xCF);	// Port B as MODE 3
	Xio_Out(pioA3, 1);
	Xio_Out(pioA3, pioVector);	// INT vector 0,2,4...14
	Xio_Out(pioA3, 0xB7);	// INT control via PB0
	Xio_Out(pioA3, 254);

	// Install the ISR stub and register it for the chosen IM2 channel
	Mem_Copy(g_IsrData, (void*)XIO_ISRCODE, sizeof(g_IsrData));
	if (!Xio_SetAddr(xioChan, XIO_ISRCODE))
	{
		DOS_StringOutput("XIO SET ADDR failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// IM2 mask: all 8 channels active (high byte of the mask word)
	if (!Xio_SetMask(0xFF00))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] mask for IM2 is set to &HFF00\r\n$");

	DOS_ClearScreen();
	Dos_PrLoc(0, 0);
	DOS_StringOutput("<SPACE> ends programme, waiting for INT\r\n$");

	u16 counters[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	// volatile: see pioAddr above.
	volatile u8 outByte = 0;
	for (;;)
	{
		// The ROM ORs each channel's bit into g_IntFlagsHi as interrupts
		// arrive; clear only the reported channel's own bit so a bit set
		// by a concurrent interrupt is not lost.
		for (u8 ch = 0; ch < 8; ch++)
		{
			if (g_IntFlagsHi & (1 << ch))
			{
				ReportChannel(ch, counters);
				g_IntFlagsHi &= ~(1 << ch);
			}
		}

		Xio_Out(pioAddr, outByte);	// toggle PA0
		outByte ^= 1;

		// Non-blocking key poll
		if (Dos_ConsoleStatus() != 0)
		{
			if (Dos_ConsoleInput() == ' ')
				break;
		}
	}

	Xio_SetMask(0);
	Xio_SetAddr(xioChan, 0);
	Xio_Deinit();
	Dos_PrLoc(6, 0);
	DOS_StringOutput("DONE\r\n$");

	Bios_Exit(0);
}
