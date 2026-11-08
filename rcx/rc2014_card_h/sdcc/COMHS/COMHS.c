//***************************************************************************************************
//* HEADER - COMHS.c - C/SDCC/MSXgl port of COM/CARD_H/COMHS/COMHS.ASM                             *
//***************************************************************************************************
//* SC725 (RCX card type H) COM test with CTS-RTS handshake: for every channel in s_Chans at 9600   *
//* baud characters are sent without reading until RTS stops the sender (time out), the buffer is   *
//* read and checked (no overflow), then the rest of 200 characters is sent and read back. Loopback  *
//* TXD-RXD and RTS-CTS on the tested port. Also calls STAT, EOF, LOF, BACKUP and DTR, so every      *
//* RS-232C entry of the rcx_io library is used. Works against the RCX ROM and RCXRAM.COM.           *
//***************************************************************************************************
//* File: COMHS.c                                                                                   *
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
#define COM_TOTAL	200

// Channels to test: count, then 0 = port A, 1 = port B
static const u8 s_Chans[] = { 1, 1 };

static u8 s_Par[13];	// 8 ASCII characters, rx baud, tx baud, time out
static u8 s_Ch;
static u8 s_Errs;
static u8 s_Tests;
static u8 s_Pass;
static u8 s_I;
static u8 s_C;
static u8 s_Sent;
static u16 s_Loc;
static u16 s_V;

static u8 HsChar(u8 n)
{
	return (u8)('A' + n % 26);
}

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

static void PrintCh(void)
{
	DOS_StringOutput("COM$");
	DOS_CharOutput('0' + s_Ch);
	DOS_StringOutput(":$");
}

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
	DOS_StringOutput("SC725 COM test, CTS-RTS\r\n$");
	s_Tests = 0;	// crt0 runs no GSINIT: statics are not zeroed
	s_Pass = 0;
	Setup();

	for (s_C = 1; s_C <= s_Chans[0]; s_C++)
	{
		s_Ch = s_Chans[s_C];

		// Stop by RTS before the buffer overflows
		s_Errs = 0;
		if (!InitCh("8N1NHNNN", 9600, 1))
			s_Errs++;
		Com_Dtr(1);
		for (s_Sent = 0; s_Sent < COM_TOTAL; s_Sent++)
			if (Com_SndChr(HsChar(s_Sent)) == COM_TIMEOUT)
				break;
		if (s_Sent == COM_TOTAL)
			s_Errs++;
		s_Loc = Com_Loc();
		if ((s_Loc != s_Sent) || (s_Loc >= COM_BUFLEN))
			s_Errs++;
		if (Com_Lof() != COM_BUFLEN - s_Loc)
			s_Errs++;
		if (Com_Eof())
			s_Errs++;
		for (s_I = 0; s_I < (u8)s_Loc; s_I++)
		{
			s_V = Com_GetChr();
			if ((s_V & COM_ERR) || ((u8)s_V != HsChar(s_I)))
				s_Errs++;
		}
		if (Com_Stat() & 0x8000)
			s_Errs++;
		PrintCh();
		DOS_StringOutput(" stop after$");
		Dos_PrintDecWord(s_Sent);
		DOS_StringOutput(" LOC$");
		Dos_PrintDecWord(s_Loc);
		Result();

		// Rest, one character at a time; BACKUP once
		s_Errs = 0;
		for (s_I = s_Sent; s_I < COM_TOTAL; s_I++)
		{
			if (Com_SndChr(HsChar(s_I)) != 0)
				s_Errs++;
			s_V = Com_GetChr();
			if ((s_V & COM_ERR) || ((u8)s_V != HsChar(s_I)))
				s_Errs++;
		}
		Com_Backup('#');
		if ((u8)Com_GetChr() != '#')
			s_Errs++;
		Com_Close();
		PrintCh();
		DOS_StringOutput(" rest$");
		Dos_PrintDecWord(COM_TOTAL - s_Sent);
		DOS_StringOutput(" chars, BACKUP$");
		Result();
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
