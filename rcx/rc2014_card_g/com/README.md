# Card G (ADS1220, ATMEGA8) - .COM tool

UNAPI/EXTBIOS port of `../basic/ADS1220.ASC` - calls `RCX_INTERFACE` directly via EXTBIO
discovery, no BASIC interpreter involved. IO address is a command-line parameter
(`/IO:<0..252>`) instead of a fixed address.

## ADS1220.COM

Same TEST A-G bit-level suite as `ADS1220.ASC`: all 12 measurement types cross-checked between
manual byte decoding and native `SNG` float decoding, CMD/STATUS readback checks, RESULT-byte
wrap check, and `ADS GET` verified against the manual results.
