# Card B (Z80 PIO) - BASIC test programs

## Z80PIO.ASC

Basic loopback/counter test: configures Port A as output (Mode 0) and Port B as input (Mode 1),
writes an incrementing byte to Port A and reads Port B back in an endless loop, displaying both
values on screen (`<SPACE>` to exit). Needs Port A and Port B physically wired together to see
the written value echoed back.

## Z80PIOI.ASC

IM2 interrupt test: configures Port A as output and Port B in bitcontrol mode (Mode 3, PB0 as
interrupt input), installs a small Z80 machine code service routine via `XIO SET ADDR`, and
toggles PA0 on every main-loop pass. Reports which PIO channel fired the interrupt, with a
running per-channel counter shown on screen.
