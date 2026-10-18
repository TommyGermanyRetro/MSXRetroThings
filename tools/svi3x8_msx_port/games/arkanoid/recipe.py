"""Builds the MSX cartridge port of Arkanoid (Taito, SVI-318/328 cassette) as a 32 KiB ROM.

Source: the original SVI CAS. It holds a BASIC loader and three BLOAD files, each loaded at
#9000 with start code at #D000. They copy #9000-#CFFF to #0000 (SVI BIOS), #4000 and #8000
and start the machine with RAM in all pages. #4000-#BFFF is an MSX cartridge image with the
"AB" header; it uses the BIOS and takes the VDP ports from #0006/#0007. For the SVI the PSG
ports were changed and the code that enables the cartridge in page 2 was replaced by NOPs.
Both are restored here; the result is a plain 32 KiB ROM for #4000-#BFFF.
"""

import sviport

SOURCE = "Arkanoid (19xx)(Taito).cas"
OUTPUTS = ["Arkanoid (19xx)(Taito)(MSX).ROM"]

SVI_LOAD = 0x9000
ROM_BASE = 0x4000

# (address, original bytes, replacement bytes)
PATCHES = [
    (0x4030, "0000000000000000", "CD10402680CD2400"),  # CALL 4010h, LD H,80h, CALL ENASLT
    (0x4332, "D38C", "D3A1"),  # OUT (8Ch),A
    (0x4336, "D38C", "D3A1"),  # OUT (8Ch),A
    (0x433A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0x433E, "D388", "D3A0"),  # OUT (88h),A
    (0x4340, "DB90", "DBA2"),  # IN A,(90h)
    (0xB3B7, "D388", "D3A0"),  # OUT (88h),A
    (0xB3B9, "DB90", "DBA2"),  # IN A,(90h)
    (0xB3C3, "D388", "D3A0"),  # OUT (88h),A
    (0xB3C6, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xB58C, "D388", "D3A0"),  # OUT (88h),A
    (0xB590, "D38C", "D3A1"),  # OUT (8Ch),A
]


def rom_image(cas: bytes) -> bytearray:
    blocks = sviport.svi_blocks(cas)
    sviport.expect_structure(blocks, 8, [b"ARKAN ", b"ark1  ", b"ark2  ", b"ark3  "])
    # ark2 -> #4000, ark3 -> #8000
    parts = [sviport.bload_data(block, SVI_LOAD)[:0x4000] for block in (blocks[5], blocks[7])]
    return bytearray(parts[0] + parts[1])


def build(cas: bytes, asm: sviport.Assembler) -> dict:
    rom = rom_image(cas)
    if len(rom) != 0x8000 or rom[:2] != b"AB":
        raise sviport.PortError("unexpected cartridge image")
    sviport.apply_patches(rom, PATCHES, lambda address: address - ROM_BASE)
    print(f"init #{int.from_bytes(rom[2:4], 'little'):04X}, {len(PATCHES)} patches")
    return {OUTPUTS[0]: bytes(rom)}
