# TMS9918A Emulator - Data Formats

The emulator reproduces a TMS9918A exactly. It is fed with the IO traffic of a real MSX with a
real VDP. A sender (the Raspberry Pi Pico in [TMS9918aPiPico](TMS9918aPiPico/README.md)) transfers
every access to the VDP ports 98h/99h to the PC, including the data the real VDP returns on reads.

## Interface

| Parameter | Value |
| --- | --- |
| Setting in the emulator | COM port |
| Connection | 1,000,000 baud, 8N1, no flow control |
| Encoding | ASCII, one line per message |
| Line end | `<LF>` (0Ah), no `<CR>` |

## Messages sender → PC

Format: `T H L <LF>`

- `T` = access type (ASCII digit)
- `H` `L` = data byte as two ASCII hex digits (`0`-`9`, `A`-`F`), upper nibble first

| `T` | Port | Direction | Data byte |
| --- | --- | --- | --- |
| `3` | 98h | read | VRAM value returned by the real VDP |
| `4` | 99h | read | status register returned by the real VDP |
| `5` | 98h | write | VRAM value written by the Z80 |
| `6` | 99h | write | register/address byte written by the Z80 |
| `7` | - | RESET | RESET of the MSX, data byte is ignored |
| `8` | - | time mark | optional, see time marks |

The RESET message is also valid without a data byte: `7<LF>`.

### Time marks

Format: `8 TTTTTT <LF>` (8 bytes), `TTTTTT` = sender time in µs as six ASCII hex digits (24 bits,
wraps after 16.7 s).

- The sender sends a time mark before an IO access if at least 64 µs (one display line) have
  passed since the last time mark.
- All following IO accesses up to the next time mark count as happening at this time.
- The time comes from a free-running µs counter of the sender; no relation to the VDP clock is
  needed.
- Without time marks the emulator works as described under "Frame flow without time marks".

Example:

```text
8012A40<LF>   time 76352 µs
6F4<LF>       OUT (99h),F4h
687<LF>       OUT (99h),87h
```

Lines of a different length, with `<CR>` or with an unknown access type are discarded and
counted.

Example: write F4h to register 7, then read the status during VBLANK:

```text
6F4<LF>    OUT (99h),F4h   value
687<LF>    OUT (99h),87h   register 7
49F<LF>    IN  A,(99h)     status 9Fh, F bit set
```

Bytes of the first line: `36 46 34 0A`.

## Messages PC → sender

| Message | Bytes | Meaning |
| --- | --- | --- |
| `7<LF>` | `37 0A` | reset the MSX |

The sender then triggers the RESET of the MSX and reports it back with the RESET message `7`.

## Synchronisation

### Starting point

1. Connection > Connect opens the interface, discards the receive buffer, resets the emulator
   and sends `7<LF>`.
2. All IO messages are discarded until the first RESET message `7`.
3. With the RESET message `7` the starting point is synchronised; from then on every message is
   processed.
4. A RESET of the MSX while connected also produces the message `7` and synchronises again.

### Frame flow without time marks

A read of the status register (`4`) with the F bit (bit 7) set marks the end of the active display
area (VBLANK) and triggers the frame output. Reads without the F bit trigger nothing.

### Frame flow with time marks

1. Frame period: PAL (313 lines) or NTSC (262 lines) from the intervals of the first 16 reads
   with the F bit: the period for which more intervals are an integer multiple to within one line
   is chosen (single delayed reads, e.g. while booting, do not disturb); one line takes 342 pixel
   clocks (63.695 µs).
2. Frame position: only reads with the F bit count, F is set by the VDP only. Interrupts from
   other sources (e.g. XIO, RCX) also cause a status read in the BIOS, but without the F bit.
   - Acquisition: the most frequent delay (classes of one line) of the first 32 reads with the
     F bit fixes the frame position; until then the status is not compared.
   - Tracking: a read at most 2 lines before the expected end of frame pulls the position
     forward, a read with the usual delay (below one line) moves it back by at most 0.5 µs,
     delayed reads (disabled interrupts) do not change it; after 300 delayed reads in a row the
     position is acquired again.
   - Period: from two reads with the usual delay at least 250 frames apart (compensates the
     crystal deviation between sender and VDP, up to 300 ppm).
3. Every display line is built at its time from the VRAM and register state valid then, its
   sprites are checked one line earlier (as the VDP does). Frame ends continue without status
   reads; the finished frame is output at the end of frame.
4. Long pauses without IO access are shortened: VRAM is unchanged, one complete frame and the last
   frame are sufficient.

## VDP ports

### Port 98h (VRAM data)

| Access | Effect |
| --- | --- |
| write | write the value to the VRAM address, also into the read-ahead buffer, address + 1 |
| read | return the read-ahead buffer, refill it from the VRAM address, address + 1 |

The VRAM address is 14 bits wide and wraps from 3FFFh to 0000h.

### Port 99h (register, address, status)

Writes come in pairs:

| Byte | Bit 7 | Bit 6 | Bits 5-0 | Meaning |
| --- | --- | --- | --- | --- |
| 1st | V7 | V6 | V5-V0 | register value or A7-A0 |
| 2nd | 1 | x | x x x R2 R1 R0 | write register R2-R0 with the value from byte 1 |
| 2nd | 0 | 1 | A13-A8 | set the VRAM address for writing |
| 2nd | 0 | 0 | A13-A8 | set the VRAM address for reading: fill the buffer, address + 1 |

Reading returns the status register. Reading or writing port 98h and reading port 99h reset the
byte sequence: the next byte written to port 99h is byte 1 again.

### Comparison with the real VDP

Every VRAM value received with `3` is compared with the emulator's read-ahead buffer. Mismatches
are counted and the first eight are logged (VRAM address, expected value, real value, previous IO
access). The emulator's VRAM content is not corrected.

## Registers

| Reg | Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | - | - | - | - | - | - | M3 | EXTVID |
| 1 | 4/16K | BL | IE | M1 | M2 | - | SIZE | MAG |
| 2 | - | - | - | - | PN13 | PN12 | PN11 | PN10 |
| 3 | CT13 | CT12 | CT11 | CT10 | CT9 | CT8 | CT7 | CT6 |
| 4 | - | - | - | - | - | PG13 | PG12 | PG11 |
| 5 | - | SA13 | SA12 | SA11 | SA10 | SA9 | SA8 | SA7 |
| 6 | - | - | - | - | - | SG13 | SG12 | SG11 |
| 7 | TC3 | TC2 | TC1 | TC0 | BD3 | BD2 | BD1 | BD0 |

### Screen modes

| M1 | M2 | M3 | Mode | MSX BASIC |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | Graphics I | SCREEN 1 |
| 0 | 0 | 1 | Graphics II | SCREEN 2 |
| 0 | 1 | 0 | Multicolor | SCREEN 3 |
| 1 | 0 | 0 | Text | SCREEN 0 |
| other | - | - | undocumented mixed modes | - |

### Table addresses

| Table | Address (M3 = 0) | Address (M3 = 1) |
| --- | --- | --- |
| Name table | (R2 AND 0Fh) × 400h | (R2 AND 0Fh) × 400h |
| Colour table | R3 × 40h | (R3 AND 80h) × 40h, bits 6-0 = mask |
| Pattern generator | (R4 AND 07h) × 800h | (R4 AND 04h) × 800h, bits 1-0 = mask |
| Sprite attribute | (R5 AND 7Fh) × 80h | (R5 AND 7Fh) × 80h |
| Sprite pattern | (R6 AND 07h) × 800h | (R6 AND 07h) × 800h |

### 4K mode

With register 1 bit 7 = 0 the VDP puts the address onto the DRAMs differently. Physical VRAM
address from VDP address A:

| VRAM bit | 6-0 | 12-7 | 13 |
| --- | --- | --- | --- |
| Source | A12, A5-A0 | A11-A6 | A13 |

## Status register

| Bit 7 | Bit 6 | Bit 5 | Bits 4-0 |
| --- | --- | --- | --- |
| F | 5S | C | number of the fifth sprite |

- F: end of the active display area reached (VBLANK), with IE = 1 INT is active
- 5S: fifth sprite on a line, number in bits 4-0 (sprite of the earliest line); only set while F
  and 5S are clear
- Bits 4-0 without 5S: last sprite processed (position of the D0h terminator, otherwise 31)
- C: collision of two of the at most 4 visible sprites of a line, only within x = 0..255, also
  with colour 0
- Display lines 0 to 192 are checked, only with the display enabled and not in text mode
- Reading clears F, 5S and C, bits 4-0 are retained

### Comparison with the real VDP (status)

With time marks every status read is compared, also reads in the middle of the frame (polling,
interrupts of other sources); the log names the display line of the read. Limits:

- Where within a line the VDP checks the sprites is not known; a read in exactly the line of a
  collision or 5S event may differ.
- If text mode was active or the display blanked since the last status read, only F and C are
  compared. The real VDP then returns 5S set and in bits 4-0 a value that does not follow from
  the sprite attribute table (measured on an SVI-728: 8 to 14 in text mode, depending on the
  screen content; 17 with the display blanked); the value stays until the next read.
- Without 5S, F, 5S and C are compared. Bits 4-0 then show the entry the sprite check of the
  current line had reached at the time of the read (measured on an SVI-728: changing values 0 to
  31 with an unchanged sprite attribute table without D0h terminator). This would require the
  time of the read to within one pixel.
- The frame position is derived from the usual delay of the VBLANK read; the shift by one line
  compensates the delay of the BIOS interrupt (about 58 µs). If a driver in H.KEYI permanently
  extends this delay, the line assignment shifts accordingly.

Without time marks the emulator builds the status register from the frame at the time of the
VBLANK on every read with the F bit set. If the status register was already read since the last
VBLANK, the time of the reset within the frame is unknown; such frames are counted as not
comparable. Mismatches are marked with "SAT changed during frame" (sprite attribute table written
while the frame was output) and "5S expected only" (typical for a frame without status read in
which F stayed set and the VDP suppressed 5S).

## RESET

A RESET clears registers 0 and 1: the display is blanked, 4K mode is active, the interrupt is off.
VRAM and registers 2 to 7 are retained. The emulator's diagnostic counters restart.

## Sources

- `Doku/TMS9918A_DataManual.pdf` - TI TMS9918A/TMS9928A/TMS9929A Video Display Processors Data
  Manual
- `Doku/tms9918a.txt` - Sean Young, TMS9918A including undocumented features
