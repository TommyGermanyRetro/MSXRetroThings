# XIO cartridge - .COM tools

UNAPI/EXTBIOS ports of the BASIC test programs (see `../basic/`) - call `XIO_IO_EXPANDER`
directly via EXTBIO discovery, no BASIC interpreter involved.

## XIOIM2.COM

Same Z80 PIO IM2 interrupt test as `XIOIM2.ASC`: installs a Z80 machine code service routine via
`XIO SET ADDR` on IM2 channel 8, toggles PA0, reports which channel fired. IO address is a
command-line parameter (`/IO:<0..252>`) instead of an interactive prompt.

## XIOPIC.COM

Same PIC interrupt test as `XIOPIC.ASC`: installs a Z80 machine code service routine via
`XIO SET ADDR` on PIC channel 0, reports whenever it fires - no UNAPI parameters needed beyond
discovery, since the card address/interrupt source is external hardware, not configurable here.

## XIOSLT.COM

Port of `../basic/XIOSLT.ASC` - same BIOS-only slot work area dump, no UNAPI/EXTBIOS discovery
needed at all (there is nothing to discover, it reads fixed MSX BIOS addresses directly). Main
slot number is a command-line parameter (`/SLOT:<0..3>`) instead of an interactive prompt.
