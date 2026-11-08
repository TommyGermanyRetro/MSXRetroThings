//***************************************************************************************************
//* HEADER - DAC.c - C/SDCC/MSXgl port of COM/CARD_I/DAC/DAC.ASM                                    *
//***************************************************************************************************
//* Runs the same TEST A/B/C/D as DAC.ASC (RCX card type I, 2x MCP4822), but via the rcx_io          *
//* library's Dac_* calls directly using EXTBIO discovery (see MSXgl/lib/rcx_io/). Works against     *
//* RCX_INTERFACE in ROM (CALSLT) or in mapped RAM (RCXRAM.COM, via the RAM helper).                  *
//***************************************************************************************************
//* Datei: DAC.c                                                                                    *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 08/11/26                                                                                *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "rcx_io.h"
#include "dostools.h"

//===================================================================================================
// READ-ONLY DATA
//===================================================================================================

// TEST A values per step, TEST B values per channel, TEST C HOLD values per
// channel 0..2 - match DAC.ASM's TABA/TABB/TABC.
static const u16 s_TabA[4] = { 0, 1365, 2730, 4095 };
static const u16 s_TabB[4] = { 4095, 3095, 2095, 1095 };
static const u16 s_TabC[3] = { 1000, 2000, 3000 };

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0496];	// RCX INIT's RAM work area (same size as
				// DAC.ASM's WORKAREA)

static u8 s_Er;		// running failed-check counter across all tests

// File-scope instead of locals: kept in memory across every Dac_* call,
// never held in a bare register across a call by SDCC.
static u8  s_DacAddr;	// card IO address
static u8  s_K;		// channel 0..3
static u8  s_I;		// TEST A step 0..3
static u16 s_Val;	// value written
static u16 s_Read;	// value read back

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Prints a byte (0..255) in decimal, no leading zeros - matches DAC.ASM's
// PRDECB.
void PrintDecByte(u8 value)
{
	if (value >= 100)
		DOS_CharOutput('0' + value / 100);
	if (value >= 10)
		DOS_CharOutput('0' + (value / 10) % 10);
	DOS_CharOutput('0' + value % 10);
}

//---------------------------------------------------------------------------------------------------
// Compares s_Read against s_Val, prints "<msg> ch=K value= V read= R" and
// counts a mismatch - matches DAC.ASM's CMPVAL/PRFAILV.
void CheckVal(const c8* msg)
{
	if (s_Read == s_Val)
		return;

	s_Er++;
	DOS_StringOutput(msg);
	DOS_StringOutput(" ch=$");
	PrintDecByte(s_K);
	DOS_StringOutput(" value=$");
	Dos_PrintDecWord(s_Val);
	DOS_StringOutput(" read=$");
	Dos_PrintDecWord(s_Read);
	DOS_StringOutput("\r\n$");
}

//---------------------------------------------------------------------------------------------------
// Prints msg, then waits for a key - matches DAC.ASM's WAITKEY.
void WaitKey(const c8* msg)
{
	DOS_StringOutput(msg);
	DOS_StringOutput("\r\nPress a key.$");
	Dos_ConsoleInput();
	DOS_StringOutput("\r\n$");
}

//===================================================================================================
// TESTS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// TEST A: DAC SET + DAC GET with 0/1365/2730/4095 on every channel.
void TestA(void)
{
	for (s_K = 0; s_K < 4; s_K++)
	{
		for (s_I = 0; s_I < 4; s_I++)
		{
			s_Val = s_TabA[s_I];
			Dac_Set(s_DacAddr, s_K, s_Val);
			s_Read = Dac_Get(s_DacAddr, s_K);
			CheckVal("FAIL A$");
		}
	}
	DOS_StringOutput("Test A (SET/GET readback) done\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST B: DAC HOLD + DAC GET, the HOLD bit must not be read back.
void TestB(void)
{
	for (s_K = 0; s_K < 4; s_K++)
	{
		s_Val = s_TabB[s_K];
		Dac_Hold(s_DacAddr, s_K, s_Val);
		s_Read = Dac_Get(s_DacAddr, s_K);
		CheckVal("FAIL B$");
	}
	DOS_StringOutput("Test B (HOLD/GET readback) done\r\n\r\n$");
}

//---------------------------------------------------------------------------------------------------
// TEST C: all 0 V, 1/2/3 V with HOLD, channel 3 with SET = all outputs at
// once.
void TestC(void)
{
	for (s_K = 0; s_K < 4; s_K++)
		Dac_Set(s_DacAddr, s_K, 0);

	for (s_K = 0; s_K < 3; s_K++)
		Dac_Hold(s_DacAddr, s_K, s_TabC[s_K]);

	WaitKey("X101.2-5 must still be 0 V\r\n(HOLD).$");

	Dac_Set(s_DacAddr, 3, 4000);

	WaitKey("X101.2-5 now 1.000/2.000/\r\n3.000/4.000 V.$");
}

//---------------------------------------------------------------------------------------------------
// TEST D: channel 0 (1000) gain x1 -> 0.5 V, shutdown -> 0 V, back to
// gain x2.
void TestD(void)
{
	Dac_Conf(s_DacAddr, 0, 0);
	WaitKey("Gain x1: X101.2 must be 0.500 V.$");

	Dac_Conf(s_DacAddr, 0, 3);
	WaitKey("Shutdown: X101.2 must be 0 V.$");

	Dac_Conf(s_DacAddr, 0, 1);
	DOS_StringOutput("Gain x2: X101.2 must be 1.000 V\r\nagain.\r\n\r\n$");
}

//===================================================================================================
// MAIN LOOP
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Program entry point
void main(void)
{
	DOS_StringOutput("\x0C"
		"DAC (SDCC) - UNAPI/EXTBIOS test for\r\n"
		"RCX card type I (2x MCP4822).\r\n"
		"Runs TEST A/B/C/D via\r\n"
		"FN_CORE63..67 directly.\r\n"
		"\r\n$");

	if (!Dos_ParseParam("/IO:", 252, &s_DacAddr))
	{
		DOS_StringOutput("Usage: DAC /IO:<0..252>\r\n$");
		Dos_PauseThenExit();
	}

	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("RCX_INTERFACE found, slot=$");
	Dos_PrintHex2(g_RSlot);
	DOS_StringOutput(" seg=$");
	Dos_PrintHex2(g_RSeg);
	DOS_StringOutput("\r\n$");

	// Setup: RCX ISINIT/DEINIT/INIT, DAC INIT
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	if (!Dac_Init(s_DacAddr))
	{
		DOS_StringOutput("DAC INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	TestA();
	TestB();
	TestC();
	TestD();

	// SUMMARY + RCX DEINIT
	if (s_Er == 0)
		DOS_StringOutput("ALL READBACK CHECKS PASSED\r\n$");
	else
	{
		PrintDecByte(s_Er);
		DOS_StringOutput(" CHECK(S) FAILED\r\n$");
	}

	Rcx_Deinit();

	Bios_Exit(0);
}
