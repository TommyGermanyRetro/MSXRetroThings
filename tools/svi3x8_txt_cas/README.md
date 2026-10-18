# SVI3x8 TXT-CAS Converter

Converts in both directions between SVI BASIC listings in TXT files and SVI-318/328 cassette images
(TXT → CAS: `svi_txt_to_cas.py`, CAS → TXT: `svi_cas_to_txt.py`, see below).

## TXT to CAS

Turns a line-numbered SVI BASIC listing from a TXT file into a raw SVI-318/328 CAS image. The CAS holds an
ASCII BASIC program of type `EA`: a file header block and program data blocks of at most 256 ASCII bytes
each, every block ending with a zero byte.

## Usage

```powershell
py .\svi_txt_to_cas.py .\MyProgram.txt
```

The output is called `MyProgram.cas` by default. An output path of your own and a BASIC cassette name (at
most six ASCII letters or digits) can be given:

```powershell
py .\svi_txt_to_cas.py .\MyProgram.txt --output .\MYPROG.cas --name MYPROG
```

Alternatively, drag a TXT file onto `TXT to CAS.bat`.

## Loading on the SVI-318/328

The TXT file becomes ASCII BASIC, not a tokenized `CSAVE` program, so BASIC loads it with `LOAD`, not with
`CLOAD`. The cassette name has at most six characters:

```basic
LOAD "CAS:MYPROG"
RUN
```

## Input Format

- BASIC lines are numbered and sorted in ascending order, for example `10 PRINT "HELLO"`.
- Empty lines are ignored; every numbered line must contain BASIC code.
- UTF-8 (with or without BOM) and Windows-1252 are read.
- The listing consists of ASCII characters. Umlauts and other non-ASCII characters have to be rewritten
  for the SVI before converting.
- Only Python 3 and its standard library are needed.

The block marker of the CAS is that of the raw SVI CAS format: 16 bytes `55h` followed by `7Fh`.

## The Other Way: CAS to TXT

`svi_cas_to_txt.py` reads BASIC programs from a raw SVI CAS image and writes them as numbered TXT listings.
Both ways of saving are supported:

- `CSAVE` (tokenized, type `D3`): the tokens are turned back into plain text with the keyword table of the
  SVI BASIC ROM (v1.11), including numeric constants (`&H`, `&O`, integer, single, double), `REM`, `DATA`,
  `ELSE` and `'`.
- `SAVE` (ASCII, type `EA`): the text is taken up to the end marker `1Ah`.

```powershell
py .\svi_cas_to_txt.py .\Game.cas
py .\svi_cas_to_txt.py .\Game.cas --output .\Game-Listing.txt
```

The output is called `Game.txt` by default. When the CAS holds several BASIC programs, they become
`Game_1_NAME.txt`, `Game_2_NAME.txt` and so on. Binary files (`BSAVE`, type `D0`) are skipped; a CAS without
BASIC gives an error message. Alternatively, drag a CAS file onto `CAS to TXT.bat`.

Notes:

- The listing shows spaces as they are stored in the program; tokenized programs were often typed without
  spaces (`FORI=1TO10`).
- The TXT file is written as UTF-8. Characters above `7Fh` (SVI graphics characters in strings) appear as
  Latin-1 characters; for the way back with `svi_txt_to_cas.py` they have to be replaced, since only ASCII
  is allowed there.
- MSX CAS files (header `1F A6 DE BA ...`) are a different format and are rejected.
- Checked with 121 BASIC programs of the TOSEC collection (without unknown tokens) and with the round trip
  `TXT -> CAS -> TXT`.

## Tests

```powershell
py -m unittest test_svi_txt_to_cas test_svi_cas_to_txt
```
