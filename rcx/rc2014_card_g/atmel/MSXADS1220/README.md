# MSXADS1220 - ATMEGA8 firmware for Card G

## MSXADS1220.ino

Runs on the card's ATMEGA8 and drives the ADS1220 24-bit ADC via Wolfgang Ewald's `ADS1220_WE`
library, exposed to the RC2014 bus as a tiny memory-mapped peripheral with 4 registers (selected
by 2 address lines, `/WAIT` held low via a 74123 monostable while each access is handled):

- **Register 0 (MODE)** - write selects one of the 12 measurement types (single-ended AIN0..3 vs
  GND, or one of 8 differential AIN pairs) and programs the ADS1220's input mux accordingly.
- **Register 1 (CMD)** - write with command 0 selects the read-back channel, command 1 resets the
  result-byte counter (`mCount`), command 2 triggers an actual conversion
  (`ads.getVoltage_mV()`) and converts the result from binary float into the 4-byte MSX float
  layout described below; read returns the current read channel and `mCount`.
- **Register 2** - unused/not connected.
- **Register 3 (RESULT)** - read returns one of the 4 result bytes for the selected channel;
  `mCount` auto-increments after each read and wraps from 4 back to 0.

Each conversion result is stored as 4 bytes matching the MSX's own floating point format
directly, so the ROM's `ADS READ`/`ADS GET` can hand it to BASIC as a native `SNG` value without
any conversion on the MSX side: byte 0 is sign (bit 7) + exponent (bit 6..0, `0x40` + decimal
point position), bytes 1-3 are the 6 mantissa digits as BCD nibbles. Uses a fixed-size formatting
buffer instead of Arduino's `String` class, since `String` was observed to fragment/exhaust the
ATmega8's 1&nbsp;KB heap after a handful of measurements.

This is the firmware counterpart to the ROM's `ADS INIT/MODE/CMD/STATUS/READ/GET` commands (see
the main [README.md](../../../../README.md)) - each of those ends up as one or more raw register
accesses following exactly this Mode/Cmd/Result sequence.
