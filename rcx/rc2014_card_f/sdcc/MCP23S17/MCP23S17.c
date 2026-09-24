//***************************************************************************************************
//* HEADER - MCP23S17.c - C/SDCC/MSXgl port of COM/CARD_F/MCP23S17/MCP23S17.ASM                     *
//***************************************************************************************************
//* RCX INIT, SPI INIT/MODE on the card's SPI bus (/BUS:, 0..3), then a continuous loop that writes   *
//* an incrementing byte to MCP23S17 GPIO A and reads GPIO B back via SPI WRB/RDH - via EXTBIO         *
//* discovery, no BASIC interpreter involved. Works against RCX_INTERFACE in ROM (CALSLT) or in       *
//* mapped RAM (RCXRAM.COM), through the rcx_io library (see MSXgl/lib/rcx_io/).                      *
//***************************************************************************************************
//* Datei: MCP23S17.c                                                                               *
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

// MCP23S17 registers
#define MCP_ADDR	0x40	// MCP address (SPI opcode, write)
#define IODIRA		0x00
#define IODIRB		0x01
#define GPIOA		0x12
#define GPIOB		0x13

//===================================================================================================
// DATA
//===================================================================================================

static u8 s_Workarea[0x0330];	// RCX INIT's RAM work area (same size as
				// MCP23S17.ASM's WORKAREA)

// current PRINT column, see ZonePad - reproduces MSX-BASIC's comma-
// separated PRINT column-zone advance (fixed 14-column zones), only needed
// for the one PRINT statement in the main loop that uses commas rather
// than semicolons.
static u8 s_ColPos;

//===================================================================================================
// HELPER FUNCTIONS
//===================================================================================================

//---------------------------------------------------------------------------------------------------
// Pads with spaces from the current column to the next 14-column print
// zone boundary, always at least 1 space - matches MCP23S17.ASM's PRZONE.
void ZonePad(void)
{
	do
	{
		DOS_CharOutput(' ');
		s_ColPos++;
	} while ((s_ColPos % 14) != 0);
}

//---------------------------------------------------------------------------------------------------
// Prints a $-terminated string, advances the column by its length, then
// pads to the next zone - matches MCP23S17.ASM's PRZSTR.
void ZoneStr(const c8* str)
{
	DOS_StringOutput(str);

	u8 len = 0;
	while (str[len] != '$')
		len++;
	s_ColPos += len;

	ZonePad();
}

//---------------------------------------------------------------------------------------------------
// Prints a byte as 1-2 hex digits (no leading zero), advances the column
// by however many digits were printed, then pads to the next zone -
// matches MCP23S17.ASM's PRZHEXV.
void ZoneHexNoLead(u8 value)
{
	Dos_PrintHexNoLead(value);
	s_ColPos += (value >= 0x10) ? 2 : 1;

	ZonePad();
}

//---------------------------------------------------------------------------------------------------
// CRLF, resets the column to 0 - matches MCP23S17.ASM's PRZCRLF.
void ZoneCrlf(void)
{
	DOS_StringOutput("\r\n$");
	s_ColPos = 0;
}

//---------------------------------------------------------------------------------------------------
// Writes one MCP23S17 register via SPI WRB - K(0)=MCP_ADDR (write opcode),
// K(1)=register, K(2)=data. Matches MCP23S17.ASM's repeated KBUF/SPI WRB
// sequence.
void McpWriteReg(u8 spiAddr, u8 spiBus, u8 reg, u8 data)
{
	u16 buf[3];
	buf[0] = MCP_ADDR;
	buf[1] = reg;
	buf[2] = data;
	Spi_Wrb(spiAddr, spiBus, 3, buf);
}

//---------------------------------------------------------------------------------------------------
// Reads one MCP23S17 register via SPI RDH - K(0)=MCP_ADDR+1 (read opcode),
// K(1)=register, K(2) receives the result byte. Matches MCP23S17.ASM's
// SPI RDH(...,2,1,...) call (2 header bytes, 1 data byte).
u8 McpReadReg(u8 spiAddr, u8 spiBus, u8 reg)
{
	u16 buf[3];
	buf[0] = MCP_ADDR + 1;
	buf[1] = reg;
	buf[2] = 0;
	Spi_Rdh(spiAddr, spiBus, ((u16)2 << 8) | 1, buf);
	return (u8)buf[2];
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
			"MCP23S17 - UNAPI/EXTBIOS port of MCP23S17.ASC\r\n"
			"(card type F, SPI). Writes an incrementing byte\r\n"
			"to MCP23S17 GPIO A and reads GPIO B back via\r\n"
			"FN_CORE50/52/55/56 directly, no BASIC CALL.\r\n"
			"\r\n$");
	else
		DOS_StringOutput("\x0C"
			"MCP23S17 - UNAPI/EXTBIOS driver\r\n"
			"for RCX card type F (SPI) with an\r\n"
			"MCP23S17 GPIO expander.\r\n"
			"\r\n$");

	// static: &spiAddr/&spiBus are passed to Dos_ParseParam below - see
	// the SDCC SP-reset checklist for why a plain block-local here would
	// be addressed inconsistently once SP has moved.
	static u8 spiAddr;
	static u8 spiBus;
	if (!Dos_ParseParam("/IO:", 252, &spiAddr) || !Dos_ParseParam("/BUS:", 3, &spiBus))
	{
		DOS_StringOutput("Usage: MCP23S17 /IO:<0..252> /BUS:<0..3>\r\n$");
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

	// Setup: RCX ISINIT/DEINIT/INIT, SPI INIT/MODE
	if (Rcx_IsInit() == 1)
		Rcx_Deinit();
	if (!Rcx_Init((u16)(void*)s_Workarea))
	{
		DOS_StringOutput("RCX INIT failed (Carry).\r\n$");
		Dos_PauseThenExit();
	}

	// SPI INIT/MODE - no Carry check, not documented as a valid error
	// signal for either call
	Spi_Init(spiAddr);
	Spi_Mode(spiAddr, spiBus, 0, 0);

	// Set IODIR A/B
	McpWriteReg(spiAddr, spiBus, IODIRA, 0x00);	// Port A: all output
	McpWriteReg(spiAddr, spiBus, IODIRB, 0xFF);	// Port B: all input

	// Main loop - outByte is a single byte (0..255): natural 8-bit
	// wraparound on increment takes it straight from 255 back to 0, no
	// separate compare/reset needed.
	static u8 outByte = 0;

	for (;;)
	{
		McpWriteReg(spiAddr, spiBus, GPIOA, outByte);
		u8 inByte = McpReadReg(spiAddr, spiBus, GPIOB);

		// PRINT "OUT: ",HEX$(J),"IN: ",HEX$(I) - commas, not
		// semicolons, so this must reproduce BASIC's print-zone
		// column advance (14-column zones), see ZoneStr/ZoneHexNoLead
		ZoneStr("OUT: $");
		ZoneHexNoLead(outByte);
		ZoneStr("IN: $");
		ZoneHexNoLead(inByte);
		ZoneCrlf();

		outByte++;
	}
}
