//***************************************************************************************************
//* HEADER - Z80PIOI.c - C/SDCC/MSXgl port of COM/CARD_B/Z80PIOI/Z80PIOI.ASM                        *
//***************************************************************************************************
//* Installs a Z80 ISR stub via IM2 (XIO SET ADDR, channel INT+8) at XIO_ISRCODE (own fixed page-3  *
//* area), toggles PA0, and reports which PIO channel triggered the interrupt (INTFLAGS high byte,  *
//* bound via XIO INIT and updated by the ROM on every IM2 interrupt, independent of the custom      *
//* ISR). PIO registers themselves are accessed via the RCX PIO library functions (Pio_Init/CtrlA/   *
//* WriteA/CtrlB), not raw XIO Out - but via the xio_io and rcx_io libraries directly through EXTBIO *
//* discovery, no BASIC interpreter involved. Discovers both RCX_INTERFACE and XIO_IO_EXPANDER       *
//* separately (two independent UNAPI drivers, own slot/segment/entry each). Uses RC2014 SC103 as   *
//* DUT.                                                                                             *
//***************************************************************************************************
//* Datei: Z80PIOI.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "memory.h"
#include "xio_io.h"
#include "rcx_io.h"
#include "dostools.h"

//===================================================================================================
// DEFINE
//===================================================================================================

#define LINLEN	0xF3B0	// current active screen text width

//===================================================================================================
// READ-ONLY DATA
//===================================================================================================

// Z80 ISR stub copied to XIO_ISRCODE: positions to row 4/col 0 via the
// console's ESC-Y code, blanks it with 13 spaces, repositions, prints
// "ISR has fired", then RETI (IM2 interrupt end - unlike XIOPIC's PIC
// service routine, an IM2 handler ends with RETI, not RET). Bytes send
// each character through the fixed BIOS CHPUT vector (0x00A2). Identical
// bytes to Z80PIOI.ASM's ISRDATA.
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
// interrupt (bit0=channel8 ... bit7=channel15), independent of the custom
// ISR stub above - unlike XIOPIC.c's g_IntFlagsLo (PIC channels 0-7, low
// byte), Z80 PIO uses real IM2 vectoring, whose pending bits the ROM tracks
// in the flag word's HIGH byte instead.
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
	// Screen width: reads the currently active column count (LINLEN),
	// never changes it - 40-column banner below the 41-column threshold,
	// 80-column banner at or above it
	u8 cols = (*(u8*)LINLEN >= 41) ? 80 : 40;

	if (cols == 80)
		DOS_StringOutput("\x0C"
			"Z80PIOI - UNAPI/EXTBIOS port of Z80PIOI.ASC (card\r\n"
			"type B, Z80 PIO, IM2 test routine). Installs a Z80\r\n"
			"ISR stub via XIO SET ADDR, toggles PA0 and reports\r\n"
			"which PIO channel fired the interrupt, directly,\r\n"
			"no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"Z80PIOI - UNAPI/EXTBIOS driver for RCX\r\n"
			"card type B (Z80 PIO), IM2 test routine.\r\n"
			"Installs a Z80 ISR stub, toggles PA0 and\r\n"
			"reports which PIO channel fired.\r\n"
			"\r\n$");

	// static: &pioAddr/&intChan are passed to Dos_ParseParam below - see
	// the SDCC SP-reset checklist for why a plain block-local here would
	// be addressed inconsistently once SP has moved.
	static u8 pioAddr;
	static u8 intChan;
	if (!Dos_ParseParam("/IO:", 252, &pioAddr) || !Dos_ParseParam("/INT:", 7, &intChan))
	{
		DOS_StringOutput("Usage: Z80PIOI /IO:<0..252> /INT:<0..7>\r\n$");
		Dos_PauseThenExit();
	}

	// XIOCHAN = INTCHAN + 8 (IM2 channel), VECTORBYTE = INTCHAN * 2 (Z80
	// PIO INT-vector byte)
	u8 xioChan = intChan + 8;
	u8 vectorByte = intChan * 2;

	if (!Xio_Discover())
	{
		DOS_StringOutput("XIO_IO_EXPANDER not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("XIO_IO_EXPANDER found, slot=$");
	Dos_PrintHex2(g_XSlot);
	DOS_StringOutput("\r\n$");

	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("RCX_INTERFACE found, slot=$");
	Dos_PrintHex2(g_RSlot);
	DOS_StringOutput("\r\n$");

	// Setup: XIO ISINIT/DEINIT/INIT
	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] is installed\r\n$");

	// Setup: RCX ISINIT/DEINIT/INIT
	static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
					// Z80PIOI.ASM's WORKAREA)
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// PIO setup via SC103, using the RCX PIO library functions
	Pio_Init(pioAddr);
	Pio_CtrlA(pioAddr, 0x0F);	// Port A as output
	Pio_WriteA(pioAddr, 0);		// Port A = 0
	Pio_CtrlB(pioAddr, 0xCF);	// Port B as MODE 3
	Pio_CtrlB(pioAddr, 0x01);	// PB0 as Input
	Pio_CtrlB(pioAddr, vectorByte);	// Port B INT-vector 0,2,4...14
	Pio_CtrlB(pioAddr, 0xB7);	// Port B INT-control via PB0
	Pio_CtrlB(pioAddr, 254);

	// Install the ISR stub and register it for the selected IM2 channel
	Mem_Copy(g_IsrData, (void*)XIO_ISRCODE, sizeof(g_IsrData));
	if (!Xio_SetAddr(xioChan, XIO_ISRCODE))
	{
		DOS_StringOutput("XIO SET ADDR failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// Interrupt mask: IM2 0x00xx ... 0xFFxx
	if (!Xio_SetMask(0xFF00))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("[XIO] mask for IM2 is set to &H$");
	Dos_PrintHexWordNoLead(0xFF00);
	DOS_StringOutput("\r\n$");

	DOS_CharOutput(12);	// CLS
	Dos_PrLoc(0, 0);
	DOS_StringOutput("<SPACE> ends programme, waiting for INT\r\n$");

	// static: outByte/counters are touched every MAINLOOP iteration - see
	// the SDCC SP-reset checklist for why these must not be plain
	// block-locals here. counters zeroed with an explicit runtime loop
	// rather than an initializer list, since this project's MSXgl crt0
	// never calls SDCC's generated GSINIT startup.
	static u8 outByte = 0;
	static u16 counters[8];
	for (u8 i = 0; i < 8; i++)
		counters[i] = 0;

	for (;;)
	{
		// Snapshot + clear INTFLAGS high byte atomically (DI/EI)
		DisableInterrupt();
		u8 snap = g_IntFlagsHi;
		g_IntFlagsHi = 0;
		EnableInterrupt();

		for (u8 ch = 0; ch < 8; ch++)
		{
			if (snap & (1 << ch))
				ReportChannel(ch, counters);
		}

		// Toggles PA0 with the current output byte
		Pio_WriteA(pioAddr, outByte);
		outByte ^= 1;

		// Non-blocking key poll
		if (Dos_ConsoleStatus() != 0)
		{
			if (Dos_ConsoleInput() == ' ')
				break;
		}
	}

	Pio_CtrlB(pioAddr, 0x07);
	Xio_SetMask(0);
	Xio_SetAddr(xioChan, 0);
	Rcx_Deinit();
	Xio_Deinit();

	Dos_PrLoc(6, 0);
	DOS_StringOutput("DONE\r\n$");

	Bios_Exit(0);
}
