# RC2014 cartridge - card-independent .COM tools

UNAPI/EXTBIOS ports of the card-independent BASIC test programs (see `../basic/`) - call
`RCX_INTERFACE` directly via EXTBIO discovery, no BASIC interpreter involved.

## RCXSLT.COM

Port of `../basic/RCXSLT.ASC` - same BIOS-only slot work area dump, no UNAPI/EXTBIOS discovery
needed at all (there is nothing to discover, it reads fixed MSX BIOS addresses directly). Main
slot number is a command-line parameter (`/SLOT:<0..3>`) instead of an interactive prompt.
Card-independent counterpart to the XIO cartridge's [`XIOSLT.COM`](../../../xio/com/XIOSLT/XIOSLT.COM).
