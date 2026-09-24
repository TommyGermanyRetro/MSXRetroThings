# Card G (ADS1220, ATMEGA8) - BASIC test program

## ADS1220.ASC

Full bit-level test of the ADS1220 4-channel 24-bit ADC via the `ADS INIT/MODE/CMD/STATUS/READ/
GET` commands:

- **TEST A** - all 12 measurement types (single-ended + differential), each fully read out and
  decoded (sign/exponent/BCD digits) two independent ways: manually from the raw `ADS READ`
  bytes, and via a native MSX `SNG` variable built through `VARPTR`/`POKE` - cross-checked
  against each other.
- **TEST B/C** - verifies the CMD register's channel-select and mCount-set readback bits.
- **TEST D** - confirms the reserved CMD command 3 is a true no-op.
- **TEST F** - confirms the RESULT byte counter (`mCount`) auto-increments and wraps correctly
  over 8 consecutive reads.
- **TEST G** - exercises the `ADS GET` convenience command (full measurement in one call) against
  the manually-assembled TEST A results, with a small tolerance for ADC noise.

Counts mismatches and prints `ALL BIT-LEVEL CHECKS PASSED` or the number of failures at the end.
