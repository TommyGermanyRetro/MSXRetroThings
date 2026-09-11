# XIO cartridge - BASIC test program

## XIOSLT.ASC

Low-level BIOS diagnostic tool, no XIO/UNAPI calls at all - reads MSX BIOS memory directly.
Asks for a main slot number (0..3), dumps the 8-byte MSX slot work area (`SLTWRK`) for that slot
as hex, then follows the pointer stored at that area's bytes 6+7 and dumps 96 (`&H60`) further
bytes from there. Useful to check where a UNAPI/EXTBIOS driver actually allocated its RAM work
area and what it currently contains.
