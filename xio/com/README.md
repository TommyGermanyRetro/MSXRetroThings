# XIO cartridge - .COM tool

## XIOSLT.COM

Port of `../basic/XIOSLT.ASC` - same BIOS-only slot work area dump, no UNAPI/EXTBIOS discovery
needed at all (there is nothing to discover, it reads fixed MSX BIOS addresses directly). Main
slot number is a command-line parameter (`/SLOT:<0..3>`) instead of an interactive prompt.
