//***************************************************************************************************
//* HEADER - dostools.h - Generic MSX-DOS/console helpers shared across this project's SDCC ports    *
//***************************************************************************************************
//* Plain MSX-DOS console I/O and small print/input utilities, not tied to XIO_IO_EXPANDER or        *
//* RCX_INTERFACE - see dostools.c/dostools.s for the implementation.                                *
//***************************************************************************************************
//* Datei: dostools.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                          *
//***************************************************************************************************
//* VERSION: 23/09/26                                                                                *
//***************************************************************************************************
#pragma once

#include "core.h"

// Direct BDOS wrappers (dostools.s) - not tied to any UNAPI driver.
void Dos_ReadLine(u8* buf);
u8   Dos_ConsoleStatus(void);
c8   Dos_ConsoleInput(void);

// Print helpers (dostools.c).
void Dos_PrintHexNibble(u8 value);
void Dos_PrintHex2(u8 value);		// two hex digits, no trailing space
void Dos_PrintHexNoLead(u8 value);	// 1-2 hex digits, no leading zero
void Dos_PrintHexWordNoLead(u16 value);// 1-4 hex digits, no leading zero
void Dos_PrintRaw(u8* buf, u8 count);	// not $-terminated
void Dos_PrLoc(u8 row, u8 col);	// console "ESC Y" cursor positioning
void Dos_PrintDecWord(u16 value);	// positive decimal, leading space

// Waits for a keypress, then exits - so an error message printed right
// before this is still on screen when control returns to MSX-DOS (which
// clears the screen on return).
void Dos_PauseThenExit(void);

// Interactive input helpers (dostools.c).
u8   Dos_AskDecimal(const c8* prompt, u8 maxVal);
void Dos_Ask8Chars(u8* out);
bool Dos_AskYesNo(void);
bool Dos_Digit2(u8* p, u8* val);

// Case-insensitive search for <needle> (e.g. "/IO:") in the MSX-DOS command
// tail, then parses the decimal digits right after it and range-checks
// 0..maxVal - generalizes the *.ASM ports' PARSEIO/PARSEINT/FINDTOK/
// PARSEDEC pattern into one function.
bool Dos_ParseParam(const c8* needle, u16 maxVal, u8* out);
