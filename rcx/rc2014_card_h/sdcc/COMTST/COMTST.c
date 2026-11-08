//***************************************************************************************************
//* HEADER - COMTST.c - C/SDCC/MSXgl port of COM/CARD_H/COMTST/COMTST.ASM                           *
//***************************************************************************************************
//* SC725 (RCX card type H) COM test without handshake: for every channel in s_Chans at 1200, 9600  *
//* and 19200 baud all 256 byte values are sent in 8 blocks of 32 and compared with what comes back  *
//* (loopback TXD-RXD on the tested port). Uses the rcx_io library: Com_Init (UNAPI 68), then the    *
//* RS-232C Extended BIOS found by Com_Discover (EXTBIO device 8). Works against the RCX ROM and      *
//* RCXRAM.COM.                                                                                      *
//***************************************************************************************************
//* File: COMTST.c                                                                                  *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 08/11/26                                                                                *
//***************************************************************************************************

#include "msxgl.h"
#include "dos.h"
#include "xio_io.h"
#include "rcx_io.h"
#include "dostools.h"

#define COM_IOADDR	0x80		// SC725 SIO base address
#define COM_FCB		((void*)0x9000)	// receive buffer, 0x8000 or higher (9 + 2 * 123 bytes)
#define COM_BUFLEN	123

// Channels to test: count, then 0 = port A, 1 = port B
static const u8 s_Chans[] = { 1, 1 };
static const u16 s_Rates[3] = { 1200, 9600, 19200 };

static u8 s_Par[13];	// 8 ASCII characters, rx baud, tx baud, time out
static u8 s_Ch;
static u8 s_Errs;
static u8 s_Tests;
static u8 s_Pass;
static u8 s_Blk;
static u8 s_I;
static u8 s_C;
static u8 s_R;
static u16 s_V;

//---------------------------------------------------------------------------------------------------
// SETCHN, INIT and OPEN (raw input/output) of channel s_Ch
//---------------------------------------------------------------------------------------------------
static bool InitCh(const c8* par8, u16 baud, u8 tout)
{
	for (s_I = 0; s_I < 8; s_I++)
		s_Par[s_I] = par8[s_I];
	s_Par[8] = (u8)baud;
	s_Par[9] = (u8)(baud >> 8);
	s_Par[10] = (u8)baud;
	s_Par[11] = (u8)(baud >> 8);
	s_Par[12] = tout;
	if (!Com_SetChn(s_Ch))
		return FALSE;
	if (!Com_Ini(s_Par))
		return FALSE;
	return Com_Open(COM_FCB, COM_BUFLEN, 4);
}

static void Result(void)
{
	s_Tests++;
	if (s_Errs == 0)
	{
		s_Pass++;
		DOS_StringOutput(" PASS\r\n$");
	}
	else
	{
		DOS_StringOutput(" FAIL$");
		Dos_PrintDecWord(s_Errs);
		DOS_StringOutput("\r\n$");
	}
}

//---------------------------------------------------------------------------------------------------
// XIO, RCX, COM INIT and the RS-232C jump table
//---------------------------------------------------------------------------------------------------
static void Setup(void)
{
	if (!Rcx_Discover() || (g_RCount == 0))
	{
		DOS_StringOutput("RCX_INTERFACE not found.\r\n$");
		Dos_PauseThenExit();
	}
	if (!Xio_Discover())
	{
		DOS_StringOutput("XIO_IO_EXPANDER not found.\r\n$");
		Dos_PauseThenExit();
	}
	if (Xio_IsInit())
		Xio_Deinit();
	if (!Xio_Init(XIO_WORKAREA, XIO_INTFLAGS))
	{
		DOS_StringOutput("XIO INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init(RCXWORKAREA))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	if (!Com_Init(COM_IOADDR))
	{
		DOS_StringOutput("COM INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}
	if (!Com_Discover())
	{
		DOS_StringOutput("No RS-232C (EXTBIO device 8).\r\n$");
		Dos_PauseThenExit();
	}
	DOS_StringOutput("RS-232C table $");
	Dos_PrintHex2(g_CSlot);
	DOS_StringOutput(" $");
	Dos_PrintHex2((u8)(g_CTable >> 8));
	Dos_PrintHex2((u8)g_CTable);
	DOS_StringOutput("\r\n$");
}

void main(void)
{
	DOS_StringOutput("SC725 COM test, no handshake\r\n$");
	s_Tests = 0;	// crt0 runs no GSINIT: statics are not zeroed
	s_Pass = 0;
	Setup();

	for (s_C = 1; s_C <= s_Chans[0]; s_C++)
	{
		s_Ch = s_Chans[s_C];
		for (s_R = 0; s_R < 3; s_R++)
		{
			s_Errs = 0;
			if (!InitCh("8N1NNNNN", s_Rates[s_R], 2))
				s_Errs++;
			else
			{
				for (s_Blk = 0; s_Blk < 8; s_Blk++)
				{
					for (s_I = 0; s_I < 32; s_I++)
						if (Com_SndChr((u8)(s_Blk * 32 + s_I)) != 0)
							s_Errs++;
					for (s_I = 0; s_I < 32; s_I++)
					{
						s_V = Com_GetChr();
						if (s_V & COM_ERR)
							s_Errs++;
						if ((u8)s_V != (u8)(s_Blk * 32 + s_I))
							s_Errs++;
					}
				}
				if (Com_Loc() != 0)
					s_Errs++;
				Com_Close();
			}
			DOS_StringOutput("COM$");
			DOS_CharOutput('0' + s_Ch);
			DOS_StringOutput(":$");
			Dos_PrintDecWord(s_Rates[s_R]);
			DOS_StringOutput(" baud 256 bytes$");
			Result();
		}
	}

	DOS_StringOutput("\r\n$");
	Dos_PrintDecWord(s_Pass);
	DOS_StringOutput(" of$");
	Dos_PrintDecWord(s_Tests);
	DOS_StringOutput(" PASS\r\n$");

	Rcx_Deinit();
	Xio_Deinit();
	DOS_Exit0();
}
