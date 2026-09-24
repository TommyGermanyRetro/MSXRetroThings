# RCXROM - RC2014 card driver ROM

## RCXROM.ASM / RCXROM.hex

The BIOS extension ROM that goes into the MSX cartridge slot: implements the generic `RCX
INIT/ISINIT/DEINIT/TABLE` bookkeeping plus the full per-card-type function set for card types
A through G (I2C/RTC, Z80 PIO, 82C55 PPI, 82C54 Timer, SJA1000 CAN, SPI, ADS1220 - see the main
[README.md](../../../README.md) for the complete command reference). Provides both classic BASIC
`CALL` statements and a KONAMIMAN-style UNAPI/EXTBIOS interface, so the same functions are also
callable directly from a `.COM` program without BASIC. Up to 16 RC2014 cards can be registered at
once, each with its own IO address and 16 bytes of private RAM.

`RCXROM.hex` is the ready-to-flash EEPROM image built from `RCXROM.ASM`.
