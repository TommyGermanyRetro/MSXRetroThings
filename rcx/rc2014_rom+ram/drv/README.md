# RCXRAM - RAM-installable RCX_INTERFACE driver

## RCXRAM.ASM / RCXRAM.COM

A pure software alternative to the `RCXROM.ASM` cartridge: a `.COM` program that installs the
same `RCX_INTERFACE` UNAPI driver into mapped RAM at runtime instead of requiring a burned
EEPROM - useful for testing and development without the actual ROM hardware. Implements the same
card-type A-G function set as the ROM. It is UNAPI/EXTBIOS-only: BASIC `CALL` support relies on
BASIC's own boot-time scan of ROM header slots, which does not apply to a RAM segment, so RCXRAM
is only reachable from `.COM` programs, not from BASIC directly.
