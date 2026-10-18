"""Builds the MSX cartridge port of Alien 8 (Ultimate, SVI-318/328) as a 32 KiB ROM.

Source: the original SVI CAS. It holds a BASIC loader and two BLOAD files, both loaded at
#8700. The start code of the first copies #8700-#C7FF to #3F00-#7FFF of the lower RAM bank,
the second copies #8800-#C7FF to #8000-#BFFF and starts the game at #4000 with RAM in all
pages. The combined image #3F00-#BFFF is patched (SVI I/O ports -> MSX ports, keyboard
through the page-0 helper block, PSG port A kept as input, bit-shift tables moved from
#F200-#FFFF to #2200-#2FFF so that the game never writes the secondary slot register #FFFF).
Its trailing zeros are left out; the copy code clears that range. The ROM (A8ROM.ASM) holds
the init code, the copy code (A8COPY.ASM), the helper block (A8HELP.ASM) and the image.
"""

import sviport

SOURCE = "Alien 8 (1985)(Ultimate).cas"
OUTPUTS = ["Alien 8 (1985)(Ultimate)(MSX).ROM"]

SVI_LOAD = 0x8700
GAME_BASE = 0x3F00                  # #3F00-#BFFF after both SVI loaders
GAME_END = 0xC000

# (address, original bytes, replacement bytes)
PATCHES = [
    (0x3F03, "D388", "D3A0"),  # OUT (88h),A: PSG register 14
    (0x3F05, "DB90", "DBA2"),  # IN A,(90h): joystick 1 directions
    (0x3F0A, "DB98", "DBA2"),  # IN A,(98h): trigger, MSX PSG R14 bit 4
    (0x3F50, "D396DB99C9", "C300020000"),  # keyboard row -> KEYR
    (0x3F80, "D396E61FFE152803DB99C9DB99CBFFC9", "C30302" + "00" * 13),  # -> KEYC
    (0x400A, "F6F0", "F6B0"),  # PSG R7: port A stays input
    (0x400E, "D388", "D3A0"),  # OUT (88h),A
    (0x403F, "DB85", "DB99"),  # IN A,(85h)
    (0x491C, "D38C", "D3A1"),  # OUT (8Ch),A
    (0x4925, "018108", "019908"),  # LD BC,0881h: OUTI/OUT (C) to the VDP
    (0x4940, "D381", "D399"),  # OUT (81h),A
    (0x4945, "D381", "D399"),  # OUT (81h),A
    (0x494E, "018003", "019803"),  # LD BC,0380h: OUT (C),A to the VDP
    (0x4985, "0E80", "0E98"),  # LD C,80h: OUT (C),B to the VDP
    (0x49BC, "0E81", "0E99"),  # LD C,81h: OUT (C),A to the VDP
    (0x5503, "4FDB96E6F0B1CD803F", "CD0602" + "00" * 6),  # port C read back -> KEYS
    (0x6B4B, "D380", "D398"),  # OUT (80h),A
    (0x6C8D, "D380", "D398"),  # OUT (80h),A
    (0x6CCE, "26FF", "262F"),  # LD H,FFh: shift tables at #2200-#2FFF
    (0x6D50, "F6F0", "F620"),  # OR F0h: shift table page
]


def game_image(cas: bytes) -> bytearray:
    blocks = sviport.svi_blocks(cas)
    sviport.expect_structure(blocks, 6, [b"ALIEN8", b"ali1  ", b"ali2  "])
    ali1, ali2 = (sviport.bload_data(block, SVI_LOAD) for block in (blocks[3], blocks[5]))
    # #8700-#C7FF of ali1 -> #3F00-#7FFF, #8800-#C7FF of ali2 -> #8000-#BFFF
    return bytearray(ali1[0x0000:0x4100] + ali2[0x0100:0x4100])


def include(helper: int, game: int, copy: int) -> str:
    return f"HELPLEN:      EQU   {helper}\nGAMELEN:      EQU   {game}\nCOPYLEN:      EQU   {copy}\n"


def build(cas: bytes, asm: sviport.Assembler) -> dict:
    image = game_image(cas)
    if len(image) != GAME_END - GAME_BASE:
        raise sviport.PortError("unexpected game image size")
    sviport.apply_patches(image, PATCHES, lambda address: address - GAME_BASE)
    game = bytes(image).rstrip(b"\x00")
    asm.write("A8GAME.BIN", game)
    helper = asm.assemble("A8HELP")
    asm.write("A8ROM.INC", include(len(helper), len(game), 0))
    copy = asm.assemble("A8COPY")
    asm.write("A8ROM.INC", include(len(helper), len(game), len(copy)))
    rom = asm.assemble("A8ROM")
    free = 0x8000 - len(rom)
    rom = sviport.cartridge(rom, 0x8000)
    print(f"game #{GAME_BASE:04X}-#{GAME_BASE + len(game) - 1:04X}, {len(PATCHES)} patches, "
          f"helper {len(helper)} bytes, copy code {len(copy)} bytes, {free} bytes free")
    return {OUTPUTS[0]: rom}
