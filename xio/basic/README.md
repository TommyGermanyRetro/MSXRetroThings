# XIO cartridge - BASIC test programs

## XIOIM2.ASC

Z80 PIO IM2 interrupt test against an SC103 card wired directly to the IO-Expander bus (no RCX
card involved, raw `_XIO OUT` register access): configures Port A as output and Port B in
bitcontrol mode (Mode 3, PB0 as interrupt input), installs a small Z80 machine code service
routine via `_XIO SET ADDR` on IM2 channel 8, and toggles PA0 in the main loop. Reports which PIO
channel fired the interrupt. IO address is asked for interactively.

## XIOPIC.ASC

PIC interrupt test: installs a Z80 machine code service routine via `_XIO SET ADDR` on PIC
channel 0 and enables just that channel via `_XIO SET MASK`. Reports whenever channel 0 fires -
the interrupt source is external (whatever is wired to that PIC input), this program only
exercises the PIC's own `SET ADDR`/`SET MASK` mechanism, no PIO/DUT setup of its own.

## XIOSLT.ASC

Low-level BIOS diagnostic tool, no XIO/UNAPI calls at all - reads MSX BIOS memory directly.
Asks for a main slot number (0..3), dumps the 8-byte MSX slot work area (`SLTWRK`) for that slot
as hex, then follows the pointer stored at that area's bytes 6+7 and dumps 96 (`&H60`) further
bytes from there. Useful to check where a UNAPI/EXTBIOS driver actually allocated its RAM work
area and what it currently contains.
