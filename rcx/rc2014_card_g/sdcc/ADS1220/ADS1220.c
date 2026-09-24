//***************************************************************************************************
//* HEADER - ADS1220.c - C/SDCC/MSXgl port of COM/CARD_G/ADS1220/ADS1220.ASM                        *
//***************************************************************************************************
//* Runs the same TEST A/B/C/D/F/G as the BASIC original, but via the rcx_io library's Ads_* calls    *
//* directly using EXTBIO discovery (see MSXgl/lib/rcx_io/). Works against RCX_INTERFACE in ROM       *
//* (CALSLT) or in mapped RAM (RCXRAM.COM, via the RAM helper).                                       *
//***************************************************************************************************
//* Datei: ADS1220.c                                                                                *
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
// READ-ONLY DATA
//===================================================================================================

// Channel select value per TEST A type index 0..11 - matches ADS1220.ASM's
// ADSCHANMAP.
static const u8 s_ChanMap[12] = { 0, 1, 2, 3, 0, 0, 0, 1, 1, 2, 1, 3 };

// Channel description strings per type index 0..11 - matches ADS1220.ASM's
// NMTAB/NM0..NM11.
static const c8* const s_NmTab[12] =
{
	"SE AIN0-GND$", "SE AIN1-GND$", "SE AIN2-GND$", "SE AIN3-GND$",
	"DE AIN0-AIN1$", "DE AIN0-AIN2$", "DE AIN0-AIN3$", "DE AIN1-AIN2$",
	"DE AIN1-AIN3$", "DE AIN2-AIN3$", "DE AIN1-AIN0$", "DE AIN3-AIN2$",
};

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
				// ADS1220.ASM's WORKAREA)

static u8 s_Er;		// running failed-check counter across all tests

static u8 s_ByBuf[4];		// current raw 4-byte MSX SNG float
static u8 s_FaBuf[12 * 4];	// TEST A's per-channel raw bytes, compared
				// again in TEST G

static u8 s_Fs;	// decoded float sign (0/1)
static i8 s_Fdp;	// decoded float decimal-point position (signed)
static u8 s_DgArr[6];	// decoded float digits

// ADS1220 card IO address - file-scope so every TEST function/DoStatus/
// Read4 can pass it on every Ads_* call, now that rcx_io no longer caches
// it.
static u8 s_AdsAddr;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Prints a byte (0..255) in decimal, no leading zeros - matches
// ADS1220.ASM's PRDECB.
void PrintDecByte(u8 value)
{
	if (value >= 100)
		DOS_CharOutput('0' + value / 100);
	if (value >= 10)
		DOS_CharOutput('0' + (value / 10) % 10);
	DOS_CharOutput('0' + value % 10);
}

//---------------------------------------------------------------------------------------------------
// Prints a signed byte in decimal (for FDP) - matches ADS1220.ASM's
// PRSDECB.
void PrintSignedDecByte(i8 value)
{
	if (value < 0)
	{
		DOS_CharOutput('-');
		value = -value;
	}
	PrintDecByte((u8)value);
}

//---------------------------------------------------------------------------------------------------
// Prints NM$(idx) - matches ADS1220.ASM's PRNAME.
void PrintName(u8 idx)
{
	DOS_StringOutput(s_NmTab[idx]);
}

//---------------------------------------------------------------------------------------------------
// Prints <count> bytes from buf as 2-digit hex, space-separated - matches
// ADS1220.ASM's PRHEXBUF2.
void PrintHexBuf2(u8* buf, u8 count)
{
	for (u8 i = 0; i < count; i++)
	{
		Dos_PrintHex2(buf[i]);
		DOS_CharOutput(' ');
	}
}

//---------------------------------------------------------------------------------------------------
// Splits a raw 4-byte MSX SNG float into s_Fs (sign)/s_Fdp (decimal-point
// position)/s_DgArr (6 digits) - matches ADS1220.ASM's DECODEFLOAT.
void DecodeFloat(u8* buf)
{
	s_Fs = (buf[0] & 0x80) ? 1 : 0;
	s_Fdp = (i8)((buf[0] & 0x7F) - 0x40);
	s_DgArr[0] = buf[1] >> 4;
	s_DgArr[1] = buf[1] & 0x0F;
	s_DgArr[2] = buf[2] >> 4;
	s_DgArr[3] = buf[2] & 0x0F;
	s_DgArr[4] = buf[3] >> 4;
	s_DgArr[5] = buf[3] & 0x0F;
}

//---------------------------------------------------------------------------------------------------
// Prints s_Fs/s_Fdp/s_DgArr as a decimal number (e.g. "784.788") - matches
// ADS1220.ASM's PRFLOAT.
void PrintFloat(void)
{
	if (s_Fs)
		DOS_CharOutput('-');

	if (s_Fdp < 0)
	{
		// DP<0: "0." + |DP| zeros + 6 digits
		DOS_StringOutput("0.$");
		for (i8 i = 0; i < -s_Fdp; i++)
			DOS_CharOutput('0');
		for (u8 i = 0; i < 6; i++)
			DOS_CharOutput('0' + s_DgArr[i]);
		return;
	}

	if (s_Fdp >= 6)
	{
		// DP>=6: all 6 digits, then zeros
		for (u8 i = 0; i < 6; i++)
			DOS_CharOutput('0' + s_DgArr[i]);
		for (u8 i = 0; i < (u8)(s_Fdp - 6); i++)
			DOS_CharOutput('0');
		return;
	}

	// 0<=DP<6: DP digits, then '.', then the remaining digits
	for (u8 i = 0; i < (u8)s_Fdp; i++)
		DOS_CharOutput('0' + s_DgArr[i]);
	DOS_CharOutput('.');
	for (u8 i = (u8)s_Fdp; i < 6; i++)
		DOS_CharOutput('0' + s_DgArr[i]);
}

//---------------------------------------------------------------------------------------------------
// Converts s_Fs/s_Fdp/s_DgArr (first 4 digits, approximated) into a signed
// value*10 (for the TEST G tolerance comparison, not for display) -
// matches ADS1220.ASM's TOFIXED10.
i16 ToFixed10(void)
{
	u16 val = 0;
	val = (u16)(val * 10 + s_DgArr[0]);
	val = (u16)(val * 10 + s_DgArr[1]);
	val = (u16)(val * 10 + s_DgArr[2]);
	val = (u16)(val * 10 + s_DgArr[3]);	// val = 4-digit number 0..9999

	i8 diff = (i8)(s_Fdp - 3);
	if (diff > 0)
	{
		for (u8 i = 0; i < (u8)diff; i++)
			val = (u16)(val * 10);
	}
	else if (diff < 0)
	{
		for (u8 i = 0; i < (u8)(-diff); i++)
			val = (u16)(val / 10);
	}

	i16 result = (i16)val;
	if (s_Fs)
		result = (i16)(-result);
	return result;
}

//---------------------------------------------------------------------------------------------------
// ADS STATUS(), split into *outChannel=rChannel (0..3), *outMCount=mCount
// (0..3) - matches ADS1220.ASM's DOSTATUS.
void DoStatus(u8* outChannel, u8* outMCount)
{
	u8 stat = Ads_GetStatus(s_AdsAddr);
	*outChannel = stat & 0x03;
	*outMCount = (u8)((stat >> 2) & 0x03);
}

//---------------------------------------------------------------------------------------------------
// 4x ADS READ() into buf - matches ADS1220.ASM's READ4.
void Read4(u8* buf)
{
	for (u8 i = 0; i < 4; i++)
		buf[i] = Ads_Read(s_AdsAddr);
}

//===================================================================================================
// TESTS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// TEST A: MODE 0..11, start conversion, select channel, read+decode
// RESULT - matches ADS1220.ASM's TEST A (ADS1220.ASC line 210-380).
void TestA(void)
{
	for (u8 t = 0; t < 12; t++)
	{
		Ads_SetMode(s_AdsAddr, t);
		Ads_Cmd(s_AdsAddr, 0x80);	// start conversion

		u8 chan = s_ChanMap[t];
		Ads_Cmd(s_AdsAddr, chan);	// select channel

		u8 rChan, mCount;
		DoStatus(&rChan, &mCount);

		if ((rChan != chan) || (mCount != 0))
		{
			s_Er++;
			DOS_StringOutput("FAIL A cmd readback type $");
			PrintDecByte(t);
			DOS_StringOutput("\r\n$");
		}

		Read4(s_ByBuf);		// reads 4 RESULT bytes into s_ByBuf
		DecodeFloat(s_ByBuf);	// s_ByBuf -> s_Fs/s_Fdp/s_DgArr

		// FA(T) = BYBUF, for TEST G
		for (u8 i = 0; i < 4; i++)
			s_FaBuf[t * 4 + i] = s_ByBuf[i];

		PrintName(t);
		DOS_StringOutput(" ch=$");
		PrintDecByte(chan);
		DOS_StringOutput(" by=$");
		PrintHexBuf2(s_ByBuf, 4);
		DOS_StringOutput(" S=$");
		PrintDecByte(s_Fs);
		DOS_StringOutput(" EX=$");
		PrintDecByte(s_ByBuf[0] & 0x7F);
		DOS_StringOutput(" DP=$");
		PrintSignedDecByte(s_Fdp);
		DOS_StringOutput(" DG=$");
		for (u8 i = 0; i < 6; i++)
			DOS_CharOutput('0' + s_DgArr[i]);
		DOS_StringOutput(" F=$");
		PrintFloat();
		DOS_StringOutput("\r\n$");
	}

	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST B: CMD cmd0 (channel select 0..3) readback - matches ADS1220.ASM's
// TEST B (ADS1220.ASC line 410-470).
void TestB(void)
{
	for (u8 t = 0; t < 4; t++)
	{
		Ads_Cmd(s_AdsAddr, t);
		u8 rChan, mCount;
		DoStatus(&rChan, &mCount);

		DOS_StringOutput("cmd0 ch=$");
		PrintDecByte(t);
		DOS_StringOutput(" -> rChannel=$");
		PrintDecByte(rChan);
		DOS_StringOutput(" mCount=$");
		PrintDecByte(mCount);
		DOS_StringOutput("\r\n$");

		if ((rChan != t) || (mCount != 0))
		{
			s_Er++;
			DOS_StringOutput("FAIL B ch $");
			PrintDecByte(t);
			DOS_StringOutput("\r\n$");
		}
	}

	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST C: CMD cmd1 (set mCount) readback - matches ADS1220.ASM's TEST C
// (ADS1220.ASC line 510-580).
void TestC(void)
{
	for (u8 t = 0; t < 4; t++)
	{
		u8 cmdVal = (u8)((t << 2) | 0x40);
		Ads_Cmd(s_AdsAddr, cmdVal);
		u8 rChan, mCount;
		DoStatus(&rChan, &mCount);

		DOS_StringOutput("cmd1 cnt=$");
		PrintDecByte(t);
		DOS_StringOutput(" mCount=$");
		PrintDecByte(mCount);
		DOS_StringOutput("\r\n$");

		if (mCount != t)
		{
			s_Er++;
			DOS_StringOutput("FAIL C cnt $");
			PrintDecByte(t);
			DOS_StringOutput("\r\n$");
		}
	}

	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST D: CMD cmd3 (reserved) must not change state - matches
// ADS1220.ASM's TEST D (ADS1220.ASC line 610-670).
void TestD(void)
{
	Ads_Cmd(s_AdsAddr, 1);
	Ads_Cmd(s_AdsAddr, 0xC0);
	u8 rChan, mCount;
	DoStatus(&rChan, &mCount);

	DOS_StringOutput("cmd3 (reserved) -> rChannel=$");
	PrintDecByte(rChan);
	DOS_StringOutput(" mCount=$");
	PrintDecByte(mCount);
	DOS_StringOutput(" (expect 1 0)$");

	if ((rChan != 1) || (mCount != 0))
	{
		s_Er++;
		DOS_StringOutput("FAIL D state changed$");
	}

	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST F: RESULT auto-increment + wrap x2 - matches ADS1220.ASM's TEST F
// (ADS1220.ASC line 810-890).
void TestF(void)
{
	Ads_Cmd(s_AdsAddr, 2);

	for (u8 k = 0; k < 8; k++)
	{
		u8 readByte = Ads_Read(s_AdsAddr);	// reads 1 byte
		u8 rChan, mCount;
		DoStatus(&rChan, &mCount);	// mCount after the read

		u8 expect = (u8)(((k & 0x03) + 1) & 0x03);

		DOS_StringOutput("read $");
		PrintDecByte(k);
		DOS_StringOutput(" byte=$");
		Dos_PrintHexNoLead(readByte);
		DOS_StringOutput(" mCount-after=$");
		PrintDecByte(mCount);
		DOS_StringOutput(" (expect $");
		PrintDecByte(expect);
		DOS_StringOutput(")$");
		DOS_StringOutput("\r\n$");

		if (mCount != expect)
		{
			s_Er++;
			DOS_StringOutput("FAIL F wrap read $");
			PrintDecByte(k);
			DOS_StringOutput("\r\n$");
		}
	}

	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST G: ADS GET vs TEST A - path 1 (individual commands, TEST A) vs
// path 2 (convenience command), tolerance instead of exact comparison -
// matches ADS1220.ASM's TEST G (ADS1220.ASC line 920-970).
void TestG(void)
{
	for (u8 t = 0; t < 12; t++)
	{
		Ads_Get(s_AdsAddr, t, s_ByBuf);

		DecodeFloat(s_ByBuf);
		i16 gVal10 = ToFixed10();

		PrintName(t);
		DOS_StringOutput(" GET=$");
		PrintFloat();

		DOS_StringOutput(" (TestA=$");

		// load and decode FA(T) (reverse direction from the store in
		// TEST A)
		for (u8 i = 0; i < 4; i++)
			s_ByBuf[i] = s_FaBuf[t * 4 + i];

		DecodeFloat(s_ByBuf);
		PrintFloat();
		DOS_StringOutput(")$");
		DOS_StringOutput("\r\n$");

		i16 testAVal10 = ToFixed10();

		i16 diff = (i16)(testAVal10 - gVal10);
		u16 absDiff = (diff < 0) ? (u16)(-diff) : (u16)diff;

		if (absDiff >= 20)	// tolerance 2.0 -> *10 = 20
		{
			s_Er++;
			DOS_StringOutput("FAIL G tolerance type $");
			PrintDecByte(t);
			DOS_StringOutput("\r\n$");
		}
	}

	DOS_StringOutput("\r\n$");
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
			"ADS1220 - UNAPI/EXTBIOS port of ADS1220.ASC\r\n"
			"(card type G, ADS1220). Runs the same TEST A/B/\r\n"
			"C/D/F/G, but via FN_CORE57..62 directly instead\r\n"
			"of BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"ADS1220 - UNAPI/EXTBIOS driver\r\n"
			"for RCX card type G (ADS1220).\r\n"
			"Runs TEST A/B/C/D/F/G via\r\n"
			"FN_CORE57..62 directly.\r\n"
			"\r\n$");

	// s_AdsAddr is file-scope (see the DATA section above) so every TEST
	// function can see it too.
	if (!Dos_ParseParam("/IO:", 252, &s_AdsAddr))
	{
		DOS_StringOutput("Usage: ADS1220 /IO:<0..252>\r\n$");
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

	// Setup: RCX ISINIT/DEINIT/INIT, ADS INIT
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	if (!Ads_Init(s_AdsAddr))
	{
		DOS_StringOutput("ADS INIT failed - no card at ADSADDR.\r\n$");
		Dos_PauseThenExit();
	}

	TestA();
	TestB();
	TestC();
	TestD();
	TestF();
	TestG();

	// SUMMARY + RCX DEINIT
	if (s_Er == 0)
		DOS_StringOutput("ALL BIT-LEVEL CHECKS PASSED\r\n$");
	else
	{
		PrintDecByte(s_Er);
		DOS_StringOutput(" CHECK(S) FAILED\r\n$");
	}

	Rcx_Deinit();

	Bios_Exit(0);
}
