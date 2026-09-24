//***************************************************************************************************
//* HEADER - RTCLCDI.c - C/SDCC/MSXgl port of COM/CARD_A/RTCLCDI/RTCLCDI.ASM                        *
//***************************************************************************************************
//* Like RTCLCD, plus interrupt-driven LCD refresh: on each PIC channel interrupt (standard XIO      *
//* handler, no custom ISR - see project_rtclcdi_xio_interrupt_pattern) the RTC is re-read and the   *
//* SEED LCD updated. Discovers BOTH RCX_INTERFACE and XIO_IO_EXPANDER separately (two independent  *
//* UNAPI drivers, own slot/segment/entry each) - via the rcx_io and xio_io libraries (see           *
//* MSXgl/lib/rcx_io/, MSXgl/lib/xio_io/). XIO_WORKAREA/XIO_INTFLAGS use the shared fixed page-3     *
//* addresses (0xC000/0xC100) every XIO-interrupt tool in this project uses - required because the   *
//* real ROM interrupt handler writes into them directly, outside this program's own transient       *
//* image (see project_rtclcdi_com_page0_interrupt_bug for why that matters).                        *
//***************************************************************************************************
//* Datei: RTCLCDI.c                                                                                 *
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

#define LINLEN		0xF3B0	// current active screen text width
#define LCDCHIP		0x7C	// PCF8584 slave address of the SEED LCD

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
					// RTCLCDI.ASM's WORKAREA)

// Shared between main(), WriteLcd() and Refresh() - static/file-scope so
// all three see the same buffers without pointer-passing, matching
// RTCLCDI.ASM's flat DATEBUF/TIMEBUF/data-register labels. ioAddr joins
// them here for the same reason: WriteLcd()'s I2c_Wrb() calls need it on
// every call now that rcx_io no longer caches the I2C bus address.
static u8 dateBuf[8];
static u8 timeBuf[8];
static u16 outBuf[9];
static u8 ioAddr;

// Low byte of the XIO PIC flag word XIO_INIT keeps updated on every PIC
// interrupt (bit0=channel0 ... bit7=channel7) - same convention as
// XIOPIC.c's g_IntFlagsLo. Fixed page-3 address (see header) - the real
// ROM interrupt handler writes here directly, not through this variable
// declaration, so it must not live inside this .COM's own transient image.
volatile u8 __at(XIO_INTFLAGS) g_IntFlagsLo;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Widen8 - copies 8 raw bytes from src into 8 words at dst[1..8] (dst[0] is
// left untouched, matches outBuf's "data register selector at [0]" layout)
// - low byte = the char, high byte = 0, matching CORE8's per-element
// stepping (see rcx_io.h) - mirrors RTCLCDI.ASM's own WIDEN8 subroutine.
// Kept as its own function on purpose - see RTCLCD.c/the SDCC SP-reset
// checklist for why a loop like this must never be inlined into main().
void Widen8(u16* dst, u8* src)
{
	for (u8 i = 0; i < 8; i++)
		dst[1 + i] = src[i];
}

//---------------------------------------------------------------------------------------------------
// Prompts once, then reads/validates DD/MM/YY (DD=01..31, MM=01..12,
// YY=any 2 digits) into dateOut[8], re-prompting (without repeating the
// outer prompt) on any violation - matches RTCLCDI.ASM's ASKNEWDATE
void AskNewDate(u8* dateOut)
{
	DOS_StringOutput("New date (DD/MM/YY, 8 characters): $");
	for (;;)
	{
		u8 line[8];
		u8 dd, mm, yy;
		Dos_Ask8Chars(line);

		if ((line[2] == '/') && (line[5] == '/') &&
			Dos_Digit2(&line[0], &dd) && (dd >= 1) && (dd <= 31) &&
			Dos_Digit2(&line[3], &mm) && (mm >= 1) && (mm <= 12) &&
			Dos_Digit2(&line[6], &yy))
		{
			for (u8 i = 0; i < 8; i++)
				dateOut[i] = line[i];
			return;
		}
		DOS_StringOutput("Invalid date - format DD/MM/YY,\r\nDD=01..31, MM=01..12.\r\n$");
	}
}

//---------------------------------------------------------------------------------------------------
// Prompts once, then reads/validates HH:MM:SS (HH=00..23, MM/SS=00..59)
// into timeOut[8], re-prompting (without repeating the outer prompt) on any
// violation - matches RTCLCDI.ASM's ASKNEWTIME
void AskNewTime(u8* timeOut)
{
	DOS_StringOutput("New time (HH:MM:SS, 8 characters): $");
	for (;;)
	{
		u8 line[8];
		u8 hh, mm, ss;
		Dos_Ask8Chars(line);

		if ((line[2] == ':') && (line[5] == ':') &&
			Dos_Digit2(&line[0], &hh) && (hh <= 23) &&
			Dos_Digit2(&line[3], &mm) && (mm <= 59) &&
			Dos_Digit2(&line[6], &ss) && (ss <= 59))
		{
			for (u8 i = 0; i < 8; i++)
				timeOut[i] = line[i];
			return;
		}
		DOS_StringOutput("Invalid time - format HH:MM:SS,\r\nHH=00..23, MM/SS=00..59.\r\n$");
	}
}

//---------------------------------------------------------------------------------------------------
// Positions the cursor on line 1 (DDRAM 0x00) and writes dateBuf, then
// line 2 (DDRAM 0x40) and writes timeBuf - shared by the initial LCD write
// and the interrupt-triggered refresh, matching RTCLCDI.ASM's WRITELCD.
void WriteLcd(void)
{
	static const u16 s_Line1Addr[2] = { 0x0080, 0x0080 };
	static const u16 s_Line2Addr[2] = { 0x0080, 0x00C0 };

	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_Line1Addr);
	outBuf[0] = 0x0040;
	Widen8(outBuf, dateBuf);
	I2c_Wrb(ioAddr, LCDCHIP, 9, (void*)outBuf);

	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_Line2Addr);
	outBuf[0] = 0x0040;
	Widen8(outBuf, timeBuf);
	I2c_Wrb(ioAddr, LCDCHIP, 9, (void*)outBuf);
}

//---------------------------------------------------------------------------------------------------
// Re-reads the RTC and rewrites both LCD lines - called from main()'s
// polling loop when the selected PIC channel's interrupt bit was set,
// matching RTCLCDI.ASM's REFRESH.
void Refresh(void)
{
	DOS_StringOutput("INT detected, refreshing:\r\n$");

	Rtc_GetTime(ioAddr, timeBuf);
	Rtc_GetDate(ioAddr, dateBuf);

	DOS_StringOutput("Current date: $");
	Dos_PrintRaw(dateBuf, 8);
	DOS_StringOutput("\r\n$");
	DOS_StringOutput("Current time: $");
	Dos_PrintRaw(timeBuf, 8);
	DOS_StringOutput("\r\n$");

	WriteLcd();
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
			"RTCLCDI - UNAPI/EXTBIOS port of RTCLCDI.ASC\r\n"
			"(card type A, I2C+RTC). Reads/shows the RTC,\r\n"
			"optionally sets it, writes to the SEED LCD, then\r\n"
			"refreshes it on a PIC interrupt via XIO_IO_EXPANDER.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"RTCLCDI - UNAPI/EXTBIOS driver\r\n"
			"for RCX card type A (I2C+RTC),\r\n"
			"interrupt-driven LCD refresh\r\n"
			"via XIO_IO_EXPANDER.\r\n"
			"\r\n$");

	// static: &channel is passed to Dos_ParseParam below - see RTCLCD.c/
	// the SDCC SP-reset checklist for why a plain block-local here would
	// be addressed inconsistently once SP has moved. ioAddr itself is
	// now file-scope (see the DATA section above) so WriteLcd()/Refresh()
	// can see it too.
	static u8 channel;
	static u8 maskVal;

	if (!Dos_ParseParam("/IO:", 254, &ioAddr))
		ioAddr = Dos_AskDecimal("I2C card, IO address (0..254): $", 254);

	if (!Dos_ParseParam("/INT:", 7, &channel))
		channel = Dos_AskDecimal("PIC INT channel (0..7): $", 7);
	maskVal = (u8)(1 << channel);

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

	// Setup: XIO ISINIT/DEINIT/INIT - binds the polling flag word only,
	// does NOT activate the channel yet (Xio_SetMask happens much later,
	// after the LCD is fully initialized - see below and
	// project_rtclcdi_xio_interrupt_pattern: an early-activated XIO
	// handler can disturb in-flight RCX bus traffic).
	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// Setup: RCX ISINIT/DEINIT/INIT, I2C INIT
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	I2c_Init(ioAddr);

	// Read + show current date/time
	Rtc_GetTime(ioAddr, timeBuf);
	Rtc_GetDate(ioAddr, dateBuf);

	DOS_StringOutput("Current date: $");
	Dos_PrintRaw(dateBuf, 8);
	DOS_StringOutput("\r\n$");
	DOS_StringOutput("Current time: $");
	Dos_PrintRaw(timeBuf, 8);
	DOS_StringOutput("\r\n$");

	// Optionally change date/time
	DOS_StringOutput("Change date (Y/N)? $");
	if (Dos_AskYesNo())
	{
		AskNewDate(dateBuf);
		Rtc_SetDate(ioAddr, dateBuf);
	}

	DOS_StringOutput("Change time (Y/N)? $");
	if (Dos_AskYesNo())
	{
		AskNewTime(timeBuf);
		Rtc_SetTime(ioAddr, timeBuf);
	}

	// SEED I2C LCD init sequence, then date on line 1 / time on line 2
	static const u16 s_FuncSet[2] = { 0x0080, 0x0028 };
	static const u16 s_ClrDisp[2] = { 0x0080, 0x0001 };
	static const u16 s_DispOn[2]  = { 0x0080, 0x000F };

	for (u8 i = 0; i < 4; i++)
		I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_FuncSet);
	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_ClrDisp);
	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_DispOn);

	WriteLcd();

	DOS_StringOutput("LCD sequence sent.\r\n$");

	// Activate the PIC channel only now - after the LCD is fully
	// initialized and the first date/time write is done, not before.
	if (!Xio_SetMask(maskVal))
	{
		DOS_StringOutput("XIO SET MASK failed (Carry).\r\n$");
	}
	else
	{
		DOS_StringOutput("<SPACE> ends programme, waiting for INT\r\n$");

		for (;;)
		{
			// The ROM ORs the fired channel's bit into g_IntFlagsLo;
			// clear only the selected channel's own bit so a bit set
			// by a concurrent interrupt on another channel is not lost.
			if (g_IntFlagsLo & maskVal)
			{
				g_IntFlagsLo &= ~maskVal;
				Refresh();
			}

			// Non-blocking key poll
			if (Dos_ConsoleStatus() != 0)
			{
				if (Dos_ConsoleInput() == ' ')
					break;
			}
		}

		Xio_SetMask(0);
	}

	Xio_Deinit();
	Rcx_Deinit();
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
