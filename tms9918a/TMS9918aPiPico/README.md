# TMS9918aPiPico

Firmware for a Raspberry Pi Pico (RP2040) that listens to all accesses of an MSX to the VDP
(ports 98h/99h) and sends them over USB to the TMS9918A emulator. The message format is
described in [DATA_FORMATS.md](../DATA_FORMATS.md).

## Bus connection

| Signal | Pin | Direction | Meaning |
| --- | --- | --- | --- |
| D0-D7 | GP0-GP7 | input | data bus |
| /CSW | GP8 | input | VDP write (active LOW) |
| /CSR | GP9 | input | VDP read (active LOW) |
| A0 | GP10 | input | 0 = port 98h, 1 = port 99h |
| /RES | GP11 | input | RESET of the MSX (active LOW) |
| VDP | GP12 | input | address 98h/99h present (GAL, without /IORQ), active HIGH |
| /RESMSX | GP20 | output to the 74HCT245 | reset the MSX (active LOW) |
| /WAIT | GP21 | output to the 74HCT245 | hold the current access (active LOW) |

The inputs of the RP2040 are not 5 V tolerant; the level shifting is done on the board.
/RESMSX and /WAIT are push-pull outputs with idle level HIGH. While the firmware is not running
(power-up, BOOTSEL, flashing) all pins of the RP2040 are inputs with pull-down: the MSX stays in
RESET. At start-up the firmware keeps /RESMSX LOW until the PIO and /WAIT are set up and then
releases the MSX; the LOW phase of /RES reports the RESET to the PC with `7`.
The Pico is powered by the MSX.

## Files

| File | Content |
| --- | --- |
| `TMS9918aPiPico.ino` | core 1: PIO, time stamps, /RES; core 0: formatting, USB, reset of the MSX |
| `vdpsniff.pio` | PIO program that captures the accesses |
| `vdpsniff.pio.h` | generated with `pioasm`: `pioasm -o c-sdk vdpsniff.pio vdpsniff.pio.h` |

Build with the Arduino-Pico core (earlephilhower), board "Raspberry Pi Pico"
(`rp2040:rp2040:rpipico`).

## Operation

1. VDP is HIGH while address 98h/99h is present, also during memory and refresh accesses. The
   PIO then waits for /CSW or /CSR LOW and samples D0-D7, /CSW, /CSR and A0 about 250-450 ns
   later (the VDP returns read data after at most 150 ns, data manual 5.5; /RD stays active for
   about 700 ns during an IO read). If its receive FIFO is nearly full when the address appears,
   it pulls /WAIT LOW, samples during the held access and releases /WAIT as soon as there is
   room. If no /CSW or /CSR follows within about 1.6 µs, it releases /WAIT immediately.
2. Core 1 adds the µs time stamp to every access and puts it into a ring buffer (16384 entries).
   A LOW phase of /RES (at least 50 µs) is queued as a RESET entry at the right position.
3. If the ring buffer is full, the access is held via /WAIT for at most 100 µs; after that
   accesses are dropped until the buffer is half empty again. This way the MSX is never held for
   long, even if no PC is reading.
4. Core 0 formats the entries (`3XX`-`6XX`, `7`, time mark `8TTTTTT` before an access after at
   least 64 µs) and sends them in blocks over USB. The emulator opens the port without DTR, so
   the firmware sends without flow control.
5. When the Pico receives `7` from the PC, it flushes all buffers, pulls /RESMSX LOW for 1 ms and
   waits 1 ms. The following LOW phase of /RES reports the RESET to the PC with `7`.
