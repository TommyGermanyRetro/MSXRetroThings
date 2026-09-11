# Card D (82C54 Timer) - .COM tools

UNAPI/EXTBIOS ports of the BASIC test programs (see `../basic/`) - call `RCX_INTERFACE` directly
via EXTBIO discovery, no BASIC interpreter involved. IO address is a command-line parameter
(`/IO:<0..251>`) instead of an interactive prompt.

## 8254.COM

Same timer cascade test as `8254.ASC`: Timer 0/1 in Mode 2, Timer 2 in Mode 0, all gated on,
Timer 2's live counter value shown as hex without scrolling.

## 8254I.COM

Same cascade as `8254.COM`, plus Timer 1's output wired to a PIC channel (`/INT:<0..7>`):
"TIMER1 fired" with a running interrupt count, alongside the live Timer 2 counter display.
