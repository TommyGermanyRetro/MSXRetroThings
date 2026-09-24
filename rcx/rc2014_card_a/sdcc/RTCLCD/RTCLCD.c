//***************************************************************************************************
//* HEADER - RTCLCD.c - C/SDCC/MSXgl port of COM/RTCLCD/RTCLCD.ASM                                  *
//***************************************************************************************************
//* Reads/shows the PCF8583 RTC date/time, optionally sets them, then writes both to the SEED I2C   *
//* LCD, via FN_CORE2/4/6/8/12..15 directly through EXTBIO discovery - no BASIC interpreter          *
//* involved. Works against RCX_INTERFACE in ROM (CALSLT) or in mapped RAM (RCXRAM.COM, via the RAM *
//* helper), through the rcx_io library (see MSXgl/lib/rcx_io/).                                    *
//***************************************************************************************************
//* Datei: RTCLCD.c                                                                                 *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                         *
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

#define LINLEN		0xF3B0	// current active screen text width
#define LCDCHIP		0x7C	// PCF8584 slave address of the SEED LCD

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
					// RTCLCD.ASM's WORKAREA)

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Widen8 - copies 8 raw bytes from src into 8 words at dst[1..8] (dst[0] is
// left untouched, matches outBuf's "data register selector at [0]" layout)
// - low byte = the char, high byte = 0, matching CORE8's per-element
// stepping (see rcx_io.h) - mirrors RTCLCD.ASM's own WIDEN8 subroutine.
// Pulled out into its own function on purpose: written inline in main()
// before, SDCC computed &outBuf[1+i] into scratch stack slots but then
// re-read the address via an SP-relative "peek" (POP/PUSH) that silently
// assumed SP had not moved since main()'s prologue - not safe to trust
// given how many calls precede this point in main(). A real function-call
// boundary removes that risk entirely (same idea as Dos_Print8CharsError).
void Widen8(u16* dst, u8* src)
{
	for (u8 i = 0; i < 8; i++)
		dst[1 + i] = src[i];
}

//---------------------------------------------------------------------------------------------------
// Prompts once, then reads/validates DD/MM/YY (DD=01..31, MM=01..12,
// YY=any 2 digits) into dateOut[8], re-prompting (without repeating the
// outer prompt) on any violation - matches RTCLCD.ASM's ASKNEWDATE
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
// violation - matches RTCLCD.ASM's ASKNEWTIME
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
			"RTCLCD - UNAPI/EXTBIOS port of RTCLCD.ASC\r\n"
			"(card type A, I2C+RTC). Reads/shows the RTC,\r\n"
			"optionally sets it, then writes to the SEED LCD -\r\n"
			"via FN_CORE2/4/6/8/12..15 directly, no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"RTCLCD - UNAPI/EXTBIOS driver\r\n"
			"for RCX card type A (I2C+RTC).\r\n"
			"Reads/sets RTC, writes to the\r\n"
			"SEED I2C LCD.\r\n"
			"\r\n$");

	// static: SDCC computed &ioAddr for the Dos_ParseParam call below SP-relative
	// (ADD HL,SP, evaluated after the SP reset above), while every direct
	// read/write of ioAddr elsewhere used IX-relative addressing - the two
	// no longer agree on where ioAddr lives once SP has moved, so a value
	// Dos_ParseParam wrote through the pointer would never be seen by the rest
	// of main(). Same root cause as dateBuf/timeBuf/outBuf below - static
	// gives it a fixed address, sidestepping both addressing modes.
	static u8 ioAddr;
	if (!Dos_ParseParam("/IO:", 254, &ioAddr))
		ioAddr = Dos_AskDecimal("I2C card, IO address (0..254): $", 254);

	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}

	DOS_StringOutput("RCX_INTERFACE found, slot=$");
	Dos_PrintHex2(g_RSlot);
	DOS_StringOutput("\r\n$");

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
	// static: a plain block-local array here is addressed SP-relative by
	// SDCC (since it is later indexed with a variable loop index below),
	// and the inline SP reset above moves SP out from under that
	// addressing after the compiler already fixed the offsets - the
	// buffer ends up living just a few bytes above the live call stack,
	// so it gets clobbered by the very next CALL's return address.
	// static gives it a fixed address instead, immune to SP entirely
	// (same reasoning as XIOPIC.c's static counters[]).
	static u8 dateBuf[8];
	static u8 timeBuf[8];
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

	// SEED I2C LCD init sequence, then date on line 1 / time on line 2 -
	// each word element's low byte is the char sent, high byte 0 (CORE8
	// steps its data pointer by 2 per element - see rcx_io.s), matching
	// how RTCLCD.ASC's BASIC integer array B() works
	static const u16 s_FuncSet[2]   = { 0x0080, 0x0028 };
	static const u16 s_ClrDisp[2]   = { 0x0080, 0x0001 };
	static const u16 s_DispOn[2]    = { 0x0080, 0x000F };
	static const u16 s_Line1Addr[2] = { 0x0080, 0x0080 };
	static const u16 s_Line2Addr[2] = { 0x0080, 0x00C0 };
	// static for the same SP-vs-IX addressing reason as dateBuf/timeBuf
	// above (outBuf[1+i] uses a variable loop index too).
	static u16 outBuf[9];

	for (u8 i = 0; i < 4; i++)
		I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_FuncSet);
	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_ClrDisp);
	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_DispOn);

	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_Line1Addr);
	outBuf[0] = 0x0040;
	Widen8(outBuf, dateBuf);
	I2c_Wrb(ioAddr, LCDCHIP, 9, (void*)outBuf);

	I2c_Wrb(ioAddr, LCDCHIP, 2, (void*)s_Line2Addr);
	outBuf[0] = 0x0040;
	Widen8(outBuf, timeBuf);
	I2c_Wrb(ioAddr, LCDCHIP, 9, (void*)outBuf);

	DOS_StringOutput("LCD sequence sent.\r\n$");

	Rcx_Deinit();
	DOS_StringOutput("Done.\r\n$");

	Bios_Exit(0);
}
