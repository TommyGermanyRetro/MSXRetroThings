# dostools - shared MSX-DOS console helpers (SDCC/MSXgl)

Precompiled SDCC library (`dostools.h`/`dostools.c`/`dostools.s` - the raw BDOS console wrappers
assembled with `sdasz80`, the rest compiled with SDCC, both archived together with `sdar` into
`dostools.lib`) providing generic MSX-DOS console I/O and print/input helpers, not tied to any
UNAPI driver. Every function name is `Dos_`-prefixed.

## Raw BDOS wrappers

- `Dos_ReadLine(u8* buf)` - BDOS function `0x0A` (buffered console input): `buf[0]` must already
  hold the max length before the call, `buf[1]` becomes the actual typed length, `buf[2..]` the
  characters (no terminator).
- `Dos_ConsoleStatus(void)` - BDOS function `0x0B`, non-zero if a key is waiting.
- `Dos_ConsoleInput(void)` - BDOS function `0x01`, blocking read (with echo) of one waiting
  character.

## Print helpers

- `Dos_PrintHexNibble(u8 value)` - one hex digit (0-F, uppercase).
- `Dos_PrintHex2(u8 value)` - two hex digits, no trailing space.
- `Dos_PrintHexNoLead(u8 value)` - 1-2 hex digits, no leading zero (matches plain BASIC `HEX$()`
  on a byte).
- `Dos_PrintHexWordNoLead(u16 value)` - 1-4 hex digits, no leading zero (matches plain BASIC
  `HEX$()` on a 16-bit integer, e.g. `HEX$(256)="100"`, 3 digits, not 4).
- `Dos_PrintRaw(u8* buf, u8 count)` - prints raw characters, not `$`-terminated.
- `Dos_PrLoc(u8 row, u8 col)` - console `ESC Y` cursor positioning (row/col 0-based, top-left is
  `0,0`).
- `Dos_PrintDecWord(u16 value)` - positive decimal, one leading space, no leading zeros.
- `Dos_PauseThenExit(void)` - prints a "press a key" prompt, waits for one, then exits via
  `Bios_Exit(0)` - so a message printed right before this stays on screen instead of being wiped
  by MSX-DOS's screen-clear on return.

## Interactive input helpers

- `Dos_AskDecimal(const c8* prompt, u8 maxVal)` - prints `prompt`, reads a line, re-prompts until
  the input parses as a decimal value `0..maxVal`.
- `Dos_Ask8Chars(u8* out)` - reads a line, re-prompts until exactly 8 characters were typed.
- `Dos_AskYesNo(void)` - reads one character, re-prompts until it is `Y`/`y`/`N`/`n`.
- `Dos_Digit2(u8* p, u8* val)` - parses 2 ASCII digit characters at `p` into `*val` (0..99),
  returns `FALSE` if either character isn't a digit.

## Command-line parsing

- `Dos_ParseParam(const c8* needle, u16 maxVal, u8* out)` - case-insensitive search for `needle`
  (e.g. `"/IO:"`) in the MSX-DOS command tail, then parses the decimal digits right after it and
  range-checks `0..maxVal`; returns `FALSE` both when `needle` is missing entirely and when its
  value is out of range.

## Rebuilding

```sh
cd MSXgl/lib/dostools
sdasz80 -o -l -s dostools.s
# dostools.c must be compiled from inside an SDCC project folder that already has the Target/
# Machine defines and engine include paths set up. The resulting dostools.rel (asm) and the .c
# compile's own dostools.rel (renamed dostools_impl.rel to avoid overwriting) are archived together:
sdar -rc dostools.lib dostools.rel dostools_impl.rel
```
