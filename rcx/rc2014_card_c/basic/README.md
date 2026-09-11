# Card C (82C55 PPI) - BASIC test programs

## 8255.ASC

Mode 0 test: Port A output, Port B input, Port C output. Writes an incrementing byte to Port A
and reads it back on Port B/C (wired together for the test), also exercises `PPI SET`/`PPI RESET`
to toggle a single Port C bit via the 82C55's bit-set/reset mode. Values shown on screen without
scrolling (`<SPACE>` to exit).

## 8255I.ASC

Interrupt variant: runs Port B in Mode 1 (strobed input). The 82C55 itself raises `INTR B` on
PC0 when a byte is strobed in - reported through XIO's standard interrupt handler, no custom
service routine needed (PC4 is jumpered to PC2 to generate the strobe pulse from software).
