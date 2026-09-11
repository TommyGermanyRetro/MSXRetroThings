# Card B (Z80 PIO) - .COM tools

UNAPI/EXTBIOS ports of the BASIC test programs (see `../basic/`) - call `RCX_INTERFACE` directly
via EXTBIO discovery, no BASIC interpreter involved. IO address is a command-line parameter
(`/IO:<0..252>`) instead of an interactive prompt.

## Z80PIO.COM

Same loopback/counter test as `Z80PIO.ASC`: Port A output / Port B input, incrementing byte
written and read back endlessly, shown on screen without scrolling (`<SPACE>` to exit).

## Z80PIOI.COM

Same IM2 interrupt test as `Z80PIOI.ASC`: installs a Z80 machine code service routine via
`XIO SET ADDR` (`/INT:<0..7>` selects the channel), toggles PA0, reports which channel fired
with a running per-channel counter.
