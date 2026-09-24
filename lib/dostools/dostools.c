//***************************************************************************************************
//* HEADER - dostools.c - Generic MSX-DOS/console helpers shared across this project's SDCC ports    *
//***************************************************************************************************
//* Print/input utilities used identically by more than one SDCC port in this project (XIOIM2/       *
//* XIOPIC/XIOSLT/RCXSLT/RTCLCD/RTCLCDI) - none of these touch XIO_IO_EXPANDER or RCX_INTERFACE       *
//* directly, so they do not belong in xio_io/rcx_io. Extracted 2026-09-16 after the same functions   *
//* had accumulated as identical copy-pasted code across six separate .c files.                       *
//***************************************************************************************************
//* Datei: dostools.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************

#include "dostools.h"
#include "msxgl.h"
#include "dos.h"

#define CMDTAIL 0x0080	// MSX-DOS command tail: length byte, followed by
				// the raw text at CMDTAIL+1

//---------------------------------------------------------------------------------------------------
// Prints one hex digit (0-F, uppercase)
void Dos_PrintHexNibble(u8 value)
{
	u8 n = value & 0x0F;
	DOS_CharOutput((n < 10) ? ('0' + n) : ('A' + n - 10));
}

//---------------------------------------------------------------------------------------------------
// Prints a byte as two hex digits, no trailing space - matches the various
// ASM originals' PRHEX2 exactly (callers that need a separator print it
// themselves, e.g. XIOSLT/RCXSLT's DumpBytes).
void Dos_PrintHex2(u8 value)
{
	Dos_PrintHexNibble(value >> 4);
	Dos_PrintHexNibble(value);
}

//---------------------------------------------------------------------------------------------------
// Prints a byte as 1-2 hex digits, no leading zero - matches the various ASM
// originals' PRHEXV (plain BASIC HEX$() on a byte).
void Dos_PrintHexNoLead(u8 value)
{
	u8 hi = value >> 4;
	if (hi != 0)
		Dos_PrintHexNibble(hi);
	Dos_PrintHexNibble(value);
}

//---------------------------------------------------------------------------------------------------
// Prints a word as 1-4 hex digits, no leading zero - matches the various ASM
// originals' PRHEXVW (plain BASIC HEX$() on a 16-bit integer, e.g.
// HEX$(256)="100", 3 digits, not 4).
void Dos_PrintHexWordNoLead(u16 value)
{
	u8 hi = (u8)(value >> 8);
	if (hi == 0)
	{
		Dos_PrintHexNoLead((u8)value);
		return;
	}
	Dos_PrintHexNoLead(hi);
	Dos_PrintHex2((u8)value);
}

//---------------------------------------------------------------------------------------------------
// Prints <count> raw bytes from buf as characters - not $-terminated, since
// GET TIME/DATE and similar ROM calls fill a fixed-length ASCII buffer
// with no terminator.
void Dos_PrintRaw(u8* buf, u8 count)
{
	for (u8 i = 0; i < count; i++)
		DOS_CharOutput(buf[i]);
}

//---------------------------------------------------------------------------------------------------
// Positions the cursor via the MSX-DOS console's "ESC Y" code (row/col + 32,
// both 0-based, row 0/col 0 is top-left).
void Dos_PrLoc(u8 row, u8 col)
{
	DOS_CharOutput(27);
	DOS_CharOutput('Y');
	DOS_CharOutput(row + 32);
	DOS_CharOutput(col + 32);
}

//---------------------------------------------------------------------------------------------------
// Prints a positive word in decimal, one leading space (reserved sign
// column) then the digits with no leading zeros - matches plain BASIC
// PRINT of a non-negative integer.
void Dos_PrintDecWord(u16 value)
{
	c8 digits[5];
	u8 count = 0;

	DOS_CharOutput(' ');
	do
	{
		digits[count++] = '0' + (value % 10);
		value /= 10;
	} while (value > 0);

	while (count > 0)
		DOS_CharOutput(digits[--count]);
}

//---------------------------------------------------------------------------------------------------
// Waits for a keypress before exiting, so an error message printed right
// before this is still on screen when control returns to MSX-DOS (which
// clears the screen on return).
void Dos_PauseThenExit(void)
{
	DOS_StringOutput("Press a key to return to DOS...$");
	Dos_ConsoleInput();
	Bios_Exit(0);
}

//---------------------------------------------------------------------------------------------------
// Dos_Print8CharsError - kept as its own function on purpose (not exposed
// in dostools.h, only used internally by Dos_Ask8Chars below): inlined
// directly in Dos_Ask8Chars' loop, SDCC would cache this string's address
// in a register across the intervening Dos_ReadLine/DOS_StringOutput("\r\n$")
// calls, protected by push/pop only around Dos_ReadLine - a real
// function-call boundary removes the shared register to cache in the first
// place (see feedback_sdcc_sp_reset_vs_sp_relative_locals).
static void Dos_Print8CharsError(void)
{
	DOS_StringOutput("Please enter exactly 8 characters.\r\n$");
}

//---------------------------------------------------------------------------------------------------
// Reads a line into out[8] via BDOS function 0x0A, re-prompting until at
// least 8 characters were typed.
void Dos_Ask8Chars(u8* out)
{
	u8 inbuf[22];
	for (;;)
	{
		inbuf[0] = 20;
		Dos_ReadLine(inbuf);
		DOS_StringOutput("\r\n$");

		if (inbuf[1] >= 8)
		{
			for (u8 i = 0; i < 8; i++)
				out[i] = inbuf[2 + i];
			return;
		}
		Dos_Print8CharsError();
	}
}

//---------------------------------------------------------------------------------------------------
// Reads exactly 1 character, returns TRUE for Y/y, FALSE for N/n,
// re-prompts on anything else.
bool Dos_AskYesNo(void)
{
	u8 inbuf[3];
	for (;;)
	{
		inbuf[0] = 1;
		Dos_ReadLine(inbuf);
		DOS_StringOutput("\r\n$");

		if (inbuf[1] == 1)
		{
			u8 ch = inbuf[2];
			if ((ch == 'Y') || (ch == 'y'))
				return TRUE;
			if ((ch == 'N') || (ch == 'n'))
				return FALSE;
		}
		DOS_StringOutput("Please enter Y or N.\r\n$");
	}
}

//---------------------------------------------------------------------------------------------------
// p points at 2 ASCII digit characters - returns TRUE and *val=0..99, or
// FALSE if either character is not '0'..'9'.
bool Dos_Digit2(u8* p, u8* val)
{
	if ((p[0] < '0') || (p[0] > '9'))
		return FALSE;
	if ((p[1] < '0') || (p[1] > '9'))
		return FALSE;
	*val = (u8)((p[0] - '0') * 10 + (p[1] - '0'));
	return TRUE;
}

//---------------------------------------------------------------------------------------------------
// Prompts for a decimal value in [0..maxVal], re-asking on invalid input.
u8 Dos_AskDecimal(const c8* prompt, u8 maxVal)
{
	u8 buf[16];
	for (;;)
	{
		DOS_StringOutput(prompt);

		buf[0] = 14;
		Dos_ReadLine(buf);
		DOS_StringOutput("\r\n$");

		u8 len = buf[1];
		bool valid = (len > 0);
		u16 val = 0;
		for (u8 i = 0; i < len; i++)
		{
			c8 c = buf[2 + i];
			if ((c < '0') || (c > '9'))
			{
				valid = FALSE;
				break;
			}
			val = (val * 10) + (c - '0');
		}

		if (valid && (val <= maxVal))
			return (u8)val;

		DOS_StringOutput("Please enter a valid value.\r\n$");
	}
}

//---------------------------------------------------------------------------------------------------
// Case-insensitive search for <needle> in the MSX-DOS command tail, then
// parses the decimal digits right after it and range-checks 0..maxVal -
// generalizes what used to be separate ParseIo/ParseInt copies (only the
// needle string and range differed between them). Returns FALSE both when
// <needle> is missing entirely and when its value is out of range -
// callers fall back to Dos_AskDecimal either way.
bool Dos_ParseParam(const c8* needle, u16 maxVal, u8* out)
{
	u8 len = *(u8*)CMDTAIL;
	u8* text = (u8*)(CMDTAIL + 1);
	u8* end = text + len;
	u8* found = 0;

	for (u8* s = text; s < end; s++)
	{
		u8* a = s;
		const c8* n = needle;
		while (*n)
		{
			if (a >= end)
				break;
			u8 c = *a;
			if ((c >= 'a') && (c <= 'z'))
				c -= 0x20;
			if (c != *n)
				break;
			a++;
			n++;
		}
		if (*n == 0)
		{
			found = a;
			break;
		}
	}
	if (!found)
		return FALSE;

	u16 val = 0;
	u8 digits = 0;
	u8* q = found;
	while ((q < end) && (*q >= '0') && (*q <= '9'))
	{
		val = (u16)(val * 10 + (*q - '0'));
		if (val > 999)	// generic overflow guard, well above any real maxVal used in this project
			return FALSE;
		digits++;
		q++;
	}
	if ((digits == 0) || (val > maxVal))
		return FALSE;

	*out = (u8)val;
	return TRUE;
}
