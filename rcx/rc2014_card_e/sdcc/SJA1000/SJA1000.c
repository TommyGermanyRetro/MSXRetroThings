//***************************************************************************************************
//* HEADER - SJA1000.c - C/SDCC/MSXgl port of COM/CARD_E/SJA1000/SJA1000.ASM                        *
//***************************************************************************************************
//* UNAPI/EXTBIOS driver for the SJA1000 CAN card. Uses two separate UNAPI drivers                  *
//* (RCX_INTERFACE for the CAN functions, XIO_IO_EXPANDER for the interrupt), each with its own      *
//* discovery/dispatch pair. Registers RCXROM's own CAN interrupt service routine (returned by       *
//* CAN GET ADDR) via XIO SET ADDR - no custom ISR stub needed, unlike XIOIM2/XIOPIC/RTCLCDI. CAN is  *
//* not implemented in RCXRAM.COM, so this only works against a real RCX ROM card.                   *
//***************************************************************************************************
//* Datei: SJA1000.c                                                                                 *
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
#define ROWRX	8	// screen row of the CAN RX display

// RCX INIT's RAM work area is a fixed page-3 address (rcx_io.h's
// RCXWORKAREA), not a program-local buffer: CAN INIT writes a ready-made
// interrupt routine directly into it, which then runs from actual
// hardware-interrupt context, where the MSX BIOS forces page 0 to system
// ROM.

//===================================================================================================
// DATA
//===================================================================================================

// Acceptance code & mask - CAN INIT only reads the even (low-byte) offsets,
// the high bytes are never read. Matches SJA1000.ASM's ACCBUF exactly.
static const u16 s_AccBuf[8] = { 0x00F0, 0x00B7, 0x00FF, 0x00FF, 0x00F0, 0x0007, 0x00FF, 0x00FF };

// TX buffer - 13 words. G(0..6) are fixed (Maerklin CS3 message header,
// matches SJA1000.ASM's TXBUF), G(7..9) are computed from the input in
// DoTx() below, G(10) is toggled there between 1 and 0, G(11)/G(12) stay 0.
static u16 s_TxBuf[13] = { 0x0086, 0x0000, 0x00B1, 0x00DA, 0x00E8, 0x0000, 0x0000,
                            0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

// RX buffer - 13 words. Can_Rx() fills both bytes of every element itself
// (low byte = data, high byte = 0), no pre-fill needed.
static u16 s_RxBuf[13];

// Row layout, computed once at startup depending on screen width - static/
// file-scope since shared between main() and DoRx()/DoTx() below.
static u8 s_Cols;	// 40 or 80
static u8 s_RxSpan;	// rows used by the CAN RX hex dump (1 at 80 columns, 2 at 40)
static u8 s_RowAddr;	// screen row of the ADDR prompt
static u8 s_RowDir;	// screen row of the DIR prompt
static u8 s_RowClr;	// screen row cleared before CAN TX
static u8 s_RowH1;	// screen row of the first hint line
static u8 s_RowH2;	// screen row of the second hint line

static u8 s_KeyResult;	// scratch for KeyPoll() below

// CAN card IO address - file-scope so DoRx()/DoTx() can also pass it on
// every Can_Rx/Can_Tx call, now that rcx_io no longer caches it.
static u8 s_CanAddr;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Non-blocking key poll via raw BDOS function 6 (Direct Console I/O,
// E=0xFF) - unlike Dos_ConsoleStatus+Dos_ConsoleInput (used by XIOIM2.c/
// XIOPIC.c/RTCLCDI.c), function 6 does not echo the character and returns
// the result (0 = none waiting) from a single call - matches SJA1000.ASM's
// own MAINLOOP_KEY exactly (BDOS function 6, not 0x0B+0x01).
static u8 KeyPoll(void)
{
	__asm
		ld	e, #0xFF
		ld	c, #6
		call	5
		ld	(_s_KeyResult), a
	__endasm;
	return s_KeyResult;
}

//---------------------------------------------------------------------------------------------------
// A=ASCII hex character -> value 0..15, or 0xFF if not a valid hex digit -
// matches SJA1000.ASM's HEXVAL (Carry -> 0xFF sentinel, since C has no
// flags register to return a second result through).
static u8 HexVal(u8 c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	if ((c >= 'A') && (c <= 'F'))
		return (u8)(c - 'A' + 10);
	if ((c >= 'a') && (c <= 'f'))
		return (u8)(c - 'a' + 10);
	return 0xFF;
}

//---------------------------------------------------------------------------------------------------
// Prints count hex bytes (low byte of each word at src, src+=1 per byte)
// separated by trailing spaces - matches SJA1000.ASM's DORX_BYTES.
static void DoRxBytes(u16* src, u8 count)
{
	for (u8 i = 0; i < count; i++)
	{
		Dos_PrintHex2((u8)src[i]);
		DOS_CharOutput(' ');
	}
}

//---------------------------------------------------------------------------------------------------
// CAN RX(CANADDR,RXBUF), prints "RX: " + 13 hex bytes, on one line at 80
// columns or split across two lines at 40 columns (s_RxSpan rows, set at
// startup) - matches SJA1000.ASM's DORX.
static void DoRx(void)
{
	Can_Rx(s_CanAddr, s_RxBuf);

	Dos_PrLoc(ROWRX, 0);
	DOS_StringOutput("RX: $");

	if (s_RxSpan == 1)
	{
		DoRxBytes(s_RxBuf, 13);
		return;
	}

	DoRxBytes(s_RxBuf, 7);	// 40 columns: 7 bytes fit next to "RX: "
	Dos_PrLoc(ROWRX + 1, 0);
	DoRxBytes(&s_RxBuf[7], 6);
}

//---------------------------------------------------------------------------------------------------
// Asks for a CAN target address (0x3000..0x33FF, 4 hex digits) and a
// direction (0=Red/1=Green), each with its own bounds check and retry,
// fills s_TxBuf, sends twice (G10=1, then G10=0) - matches SJA1000.ASM's
// DOTX_ADDR (which falls through DOTX_DIR into the send code, all one
// callable subroutine there too - kept as one function here for the same
// reason). The blank/clear PrLoc calls use column 0 (where BDOS's
// buffered-line INPUT always leaves the cursor after Enter, i.e. where a
// bare error message lands), the prompt PrLoc calls use column 1 (the
// indent the prompts are actually shown at).
static void DoTx(void)
{
	u16 addr;
	for (;;)
	{
		Dos_PrLoc(s_RowAddr, 0);
		DOS_StringOutput("                             $");
		Dos_PrLoc(s_RowAddr, 1);
		DOS_StringOutput("ADDR (3000...33ff): $");

		u8 inbuf[6];
		inbuf[0] = 4;
		Dos_ReadLine(inbuf);

		bool ok = (inbuf[1] == 4);
		addr = 0;
		for (u8 i = 0; ok && (i < 4); i++)
		{
			u8 d = HexVal(inbuf[2 + i]);
			if (d == 0xFF)
			{
				ok = FALSE;
				break;
			}
			addr = (u16)((addr << 4) | d);
		}
		if (ok && (addr >= 0x3000) && (addr <= 0x33FF))
			break;

		DOS_StringOutput("Please enter 3000..33FF.\r\n$");
	}

	s_TxBuf[7] = (u8)(addr >> 8);		// G(7) = high byte
	s_TxBuf[8] = (u8)(addr & 0x0F);	// G(8) = low nibble

	u8 dir;
	for (;;)
	{
		Dos_PrLoc(s_RowDir, 0);
		DOS_StringOutput("                             $");
		Dos_PrLoc(s_RowDir, 1);
		DOS_StringOutput("DIR (0=Red,1=Green): $");

		u8 inbuf[3];
		inbuf[0] = 1;
		Dos_ReadLine(inbuf);

		if (inbuf[1] != 0)
		{
			dir = HexVal(inbuf[2]);
			if ((dir != 0xFF) && (dir < 2))
				break;
		}

		DOS_StringOutput("Please enter 0 or 1.\r\n$");
	}
	s_TxBuf[9] = dir;	// G(9)

	Dos_PrLoc(s_RowClr, 0);
	DOS_StringOutput("                             $");

	s_TxBuf[10] = 1;	// G(10) = 1
	Can_Tx(s_CanAddr, s_TxBuf);

	s_TxBuf[10] = 0;	// G(10) = 0
	Can_Tx(s_CanAddr, s_TxBuf);
}

//---------------------------------------------------------------------------------------------------
// XIO DEINIT, RCX DEINIT, CLS, "Done." - matches SJA1000.ASM's CLEANUP,
// reached from the normal ESC exit and from a failed XIO SET ADDR/SET
// MASK (both of which leave RCX+XIO already initialized and needing this
// same cleanup, unlike the earlier discovery/init failures below which
// just print and return since nothing was set up yet at that point).
static void Cleanup(void)
{
	Xio_Deinit();
	Rcx_Deinit();
	DOS_ClearScreen();
	DOS_StringOutput("Done.\r\n$");
}

//===================================================================================================
// MAIN LOOP
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Program entry point
void main(void)
{
	// Screen width: reads the currently active column count (LINLEN),
	// never changes it - 40-column layout below the 41-column threshold,
	// 80-column layout at or above it. Row layout below ROWRX depends on
	// how many rows the CAN RX hex dump needs (s_RxSpan).
	s_Cols = (*(u8*)LINLEN >= 41) ? 80 : 40;

	s_RxSpan = (s_Cols == 80) ? 1 : 2;
	u8 rowBase = (u8)(ROWRX + 2 + s_RxSpan);
	s_RowAddr = rowBase;
	s_RowDir = (u8)(rowBase + 1);
	s_RowClr = (u8)(rowBase + 2);
	s_RowH1 = (u8)(rowBase + 2 + 4);
	s_RowH2 = (u8)(s_RowH1 + 1);

	if (s_Cols == 80)
		DOS_StringOutput("\x0C"
			"SJA1000 - UNAPI/EXTBIOS port of SJA1000.ASC\r\n"
			"(card type E, CAN). CAN messages are received\r\n"
			"via interrupt (RCX's own ISR registered through\r\n"
			"XIO SET ADDR).\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"SJA1000 - UNAPI/EXTBIOS driver\r\n"
			"for RCX card type E (CAN).\r\n"
			"CAN messages arrive via a\r\n"
			"hardware interrupt (RCX ISR).\r\n"
			"\r\n$");

	// static: &canChan is passed to Dos_ParseParam below - see RTCLCD.c/
	// the SDCC SP-reset checklist for why a plain block-local here would
	// be addressed inconsistently once SP has moved. s_CanAddr itself is
	// file-scope (see the DATA section above) so DoRx()/DoTx() can see it
	// too.
	static u8 canChan;

	if (!Dos_ParseParam("/IO:", 254, &s_CanAddr))
		s_CanAddr = Dos_AskDecimal("CAN card, IO address (0..254): $", 254);
	if (!Dos_ParseParam("/INT:", 7, &canChan))
		canChan = Dos_AskDecimal("PIC INT channel (0..7): $", 7);

	// Discover RCX_INTERFACE, then XIO_IO_EXPANDER - same order as
	// SJA1000.ASM (setup below happens XIO-then-RCX, the opposite order).
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

	// Setup: XIO ISINIT/DEINIT/INIT (before RCX)
	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// Setup: RCX ISINIT/DEINIT/INIT
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init(RCXWORKAREA))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// CAN INIT(CANADDR,ACC)
	if (!Can_Init(s_CanAddr, (void*)s_AccBuf))
	{
		DOS_StringOutput("CAN INIT failed - no SJA1000 found at\r\n"
			"CANADDR, or already installed.\r\n$");
		Dos_PauseThenExit();
	}

	// CAN GET ADDR + XIO SET ADDR/MASK - registers RCX ROM's own CAN
	// interrupt service routine (not a custom stub) for the chosen PIC
	// channel, then activates it.
	u16 intAddr = Can_GetIntAddr(s_CanAddr);
	if (!Xio_SetAddr(canChan, intAddr))
	{
		DOS_StringOutput("XIO SET ADDR failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	u16 maskVal = (u16)(1 << canChan);
	if (!Xio_SetMask(maskVal))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// CLS before the interactive CAN monitor screen - the discovery/setup
	// text above is no longer needed once we get here (not in
	// SJA1000.ASM, added on explicit request after hardware testing).
	DOS_ClearScreen();

	Dos_PrLoc(s_RowH1, 0);
	DOS_StringOutput((s_Cols == 80)
		? "Key '1' starts the switching command sequence\r\n$"
		: "Key '1' sends a command\r\n$");
	Dos_PrLoc(s_RowH2, 0);
	DOS_StringOutput("ESC exits the programme\r\n$");

	// Main loop: CAN CHECK, on a new message CAN RX + print, key 1 =
	// send, ESC = exit - matches SJA1000.ASM's MAINLOOP.
	for (;;)
	{
		if (Can_Check(s_CanAddr))
			DoRx();

		u8 key = KeyPoll();
		if (key == 27)		// ESC
			break;
		if (key == '1')
			DoTx();
	}

	Cleanup();
	Bios_Exit(0);
}
