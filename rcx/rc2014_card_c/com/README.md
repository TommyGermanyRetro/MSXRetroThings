# Card C (82C55 PPI) - .COM tools

UNAPI/EXTBIOS ports of the BASIC test programs (see `../basic/`) - call `RCX_INTERFACE` directly
via EXTBIO discovery, no BASIC interpreter involved. IO address is a command-line parameter
(`/IO:<0..252>`) instead of an interactive prompt.

## 8255.COM

Same Mode 0 test as `8255.ASC`: Port A output, Port B/C input, incrementing byte written and
read back, plus `PPI SET`/`PPI RESET` bit-toggle test on Port C.

## 8255I.COM

Same Mode 1 strobed-input interrupt test as `8255I.ASC`: the 82C55's own `INTR B` on PC0 is
reported via XIO's standard interrupt handler (`/INT:<0..7>` selects the channel), no custom
service routine needed.
