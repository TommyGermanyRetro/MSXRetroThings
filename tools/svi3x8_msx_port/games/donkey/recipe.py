"""Builds the MSX cartridge port of Donkey Kong (Buggy Software, SVI-318/328) as a 64 KiB ROM
with ASCII16 mapper.

Source: the original SVI CAS. It holds a BASIC loader and three BLOAD files, each loaded at
#9000 with start code at #D000. They copy #9000-#CFFF to #0000, #4000 and #8000, decrypt
#0000-#BFFF (XOR #5D), set VDP R1 and SP and jump to #3745 with RAM in all pages. The game
is the ColecoVision version with a BIOS replacement for the SVI hardware; #3745 copies #4000-
#7FFF to #C000-#FFFF. The decrypted image is patched (SVI I/O ports -> MSX ports, keyboard,
joysticks and triggers through the helper block DKHELP.ASM at #F000, PSG port A kept as
input, the copy to #C000 stops before the secondary slot register #FFFF). Bank 0 (DKROM.ASM)
holds the init code and the copy code (DKCOPY.ASM); banks 1-3 hold #0000, #4000 and #8000.
"""

import sviport

SOURCE = "Donkey Kong (19xx)(Buggy Software).cas"
OUTPUTS = ["Donkey Kong (19xx)(Buggy Software)(MSX)(ASCII16).ROM"]

SVI_LOAD = 0x9000
CRYPT_KEY = 0x5D
BANK_SIZE = 0x4000
HELP_ADDRESS = 0xF000               # runtime address, copied there from #7000

# (runtime address, original bytes, replacement bytes); #C000-#FFFF is #4000-#7FFF of the image
PATCHES = [
    (0x1D57, "DB85", "DB99"),  # IN A,(85h)
    (0x3745, "010040", "01FF3F"),  # LD BC,4000h: copy stops before #FFFF
    (0xE0E5, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE0F1, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE0FD, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE109, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE120, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE12E, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE13F, "D388DB90", "CD03F000"),  # PSG R14 -> SVIJOY
    (0xE167, "DB98CB67", "CD06F000"),  # IN A,(98h) : BIT 4,A -> TRIG4
    (0xE194, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1A2, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1B0, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1BD, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1CA, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1D1, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1E6, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE1ED, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE238, "DB98CB67", "CD06F000"),  # IN A,(98h) : BIT 4,A -> TRIG4
    (0xE25B, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE267, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE273, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE27F, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE296, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE2A4, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE2B5, "D388DB90", "CD03F000"),  # PSG R14 -> SVIJOY
    (0xE2DD, "DB98CB6F", "CD09F000"),  # IN A,(98h) : BIT 5,A -> TRIG5
    (0xE314, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE322, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE330, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE33D, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE34A, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE351, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE366, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE36D, "D396DB99", "CD00F000"),  # SVI keyboard -> SVIROW
    (0xE3B8, "DB98CB6F", "CD09F000"),  # IN A,(98h) : BIT 5,A -> TRIG5
    (0xE40B, "D388", "D3A0"),  # OUT (88h),A
    (0xE40E, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE412, "D388", "D3A0"),  # OUT (88h),A
    (0xE415, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE42B, "D388", "D3A0"),  # OUT (88h),A
    (0xE42E, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE446, "D388", "D3A0"),  # OUT (88h),A
    (0xE448, "DB90", "DBA2"),  # IN A,(90h)
    (0xE45B, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE45F, "D388", "D3A0"),  # OUT (88h),A
    (0xE462, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE469, "D388", "D3A0"),  # OUT (88h),A
    (0xE46B, "DB90", "DBA2"),  # IN A,(90h)
    (0xE46F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE477, "D388", "D3A0"),  # OUT (88h),A
    (0xE47A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE495, "D388", "D3A0"),  # OUT (88h),A
    (0xE497, "DB90", "DBA2"),  # IN A,(90h)
    (0xE4AA, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE4AE, "D388", "D3A0"),  # OUT (88h),A
    (0xE4B1, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE4B8, "D388", "D3A0"),  # OUT (88h),A
    (0xE4BA, "DB90", "DBA2"),  # IN A,(90h)
    (0xE4BE, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE4C6, "D388", "D3A0"),  # OUT (88h),A
    (0xE4C9, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE4ED, "D388", "D3A0"),  # OUT (88h),A
    (0xE4EF, "DB90", "DBA2"),  # IN A,(90h)
    (0xE4F4, "D388", "D3A0"),  # OUT (88h),A
    (0xE4F6, "DB90", "DBA2"),  # IN A,(90h)
    (0xE510, "D388", "D3A0"),  # OUT (88h),A
    (0xE515, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE52B, "D388", "D3A0"),  # OUT (88h),A
    (0xE52D, "3EFC", "3EBC"),  # PSG R7 = FCh: port A stays input
    (0xE52F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE538, "DB85", "DB99"),  # IN A,(85h)
    (0xE54C, "D381", "D399"),  # OUT (81h),A
    (0xE551, "D381", "D399"),  # OUT (81h),A
    (0xE554, "D380", "D398"),  # OUT (80h),A
    (0xE55B, "DB85", "DB99"),  # IN A,(85h)
    (0xE562, "D381", "D399"),  # OUT (81h),A
    (0xE565, "D381", "D399"),  # OUT (81h),A
    (0xE56A, "D380", "D398"),  # OUT (80h),A
    (0xE574, "D381", "D399"),  # OUT (81h),A
    (0xE579, "D381", "D399"),  # OUT (81h),A
    (0xE58C, "0E80", "0E98"),  # LD C,80h: OUTI to the VDP
    (0xE59C, "D381", "D399"),  # OUT (81h),A
    (0xE5A1, "D381", "D399"),  # OUT (81h),A
    (0xE5A9, "D381", "D399"),  # OUT (81h),A
    (0xE5AC, "D381", "D399"),  # OUT (81h),A
    (0xE5B1, "0E80", "0E98"),  # LD C,80h: OUTI to the VDP
    (0xE5C5, "D381", "D399"),  # OUT (81h),A
    (0xE5C8, "D381", "D399"),  # OUT (81h),A
    (0xE5CC, "0E84", "0E98"),  # LD C,84h: INI from the VDP
    (0xE5DE, "DB85", "DB99"),  # IN A,(85h)
    (0xE6C4, "D381", "D399"),  # OUT (81h),A
    (0xE6C9, "D381", "D399"),  # OUT (81h),A
    (0xE6CE, "D380", "D398"),  # OUT (80h),A
    (0xE74F, "D381", "D399"),  # OUT (81h),A
    (0xE754, "D381", "D399"),  # OUT (81h),A
    (0xE75B, "D380", "D398"),  # OUT (80h),A
    (0xE765, "D381", "D399"),  # OUT (81h),A
    (0xE76A, "D381", "D399"),  # OUT (81h),A
    (0xE76F, "D380", "D398"),  # OUT (80h),A
]


# SVI keyboard matrix -> MSX matrix (international layout): per SVI row the (MSX row, MSX bit)
# of SVI bits 0-7, None = no MSX equivalent. SVI matrix as in openMSX Keyboard.cc
# (getSVIMapping).
KEYMAP = [
    [(0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7)],      # 0 1 2 3 4 5 6 7
    [(1, 0), (1, 1), (1, 7), (2, 0), (2, 2), (1, 3), (2, 3), (2, 4)],      # 8 9 : ' , = . /
    [(1, 2), (2, 6), (2, 7), (3, 0), (3, 1), (3, 2), (3, 3), (3, 4)],      # - A B C D E F G
    [(3, 5), (3, 6), (3, 7), (4, 0), (4, 1), (4, 2), (4, 3), (4, 4)],      # H I J K L M N O
    [(4, 5), (4, 6), (4, 7), (5, 0), (5, 1), (5, 2), (5, 3), (5, 4)],      # P Q R S T U V W
    [(5, 5), (5, 6), (5, 7), (1, 5), (1, 4), (1, 6), (7, 5), (8, 5)],      # X Y Z [ \ ] BS UP
    [(6, 0), (6, 1), (6, 2), (6, 4), (7, 2), (7, 4), (7, 7), (8, 4)],      # SHIFT CTRL LGRAPH RGRAPH ESC STOP ENTER LEFT
    [(6, 5), (6, 6), (6, 7), (7, 0), (7, 1), (8, 1), (8, 2), (8, 6)],      # F1 F2 F3 F4 F5 CLS INS DOWN
    [(8, 0), (7, 3), (8, 3), (6, 3), (7, 6), None, None, (8, 7)],          # SPACE TAB DEL CAPS SELECT PRINT - RIGHT
    [(9, 3), (9, 4), (9, 5), (9, 6), (9, 7), (10, 0), (10, 1), (10, 2)],   # keypad 0-7
    [(10, 3), (10, 4), (9, 1), (10, 5), (9, 0), (9, 2), (10, 7), (10, 6)],  # keypad 8 9 + - * / . ,
]


def rows() -> str:
    """DKROWS.INC: one routine per SVI row for SVIROW in DKHELP.ASM; each MSX row is read once
    and the SVI row data is built in C (active low)."""
    out = []
    for row, keys in enumerate(KEYMAP):
        out.append(f"{f'ROW{row}:':<14}LD    C,#0FF")
        msx_rows = []
        for key in keys:
            if key is not None and key[0] not in msx_rows:
                msx_rows.append(key[0])
        identity = all(key == (msx_rows[0], bit) for bit, key in enumerate(keys))
        for msx_row in msx_rows:
            out += ["              IN    A,(#0AA)", "              AND   #0F0",
                    f"              OR    {msx_row}", "              OUT   (#0AA),A",
                    "              IN    A,(#0A9)"]
            if identity:
                out.append("              LD    C,A")
                continue
            out.append("              LD    B,A")
            for svi_bit, key in enumerate(keys):
                if key is not None and key[0] == msx_row:
                    out += [f"              BIT   {key[1]},B", "              JR    NZ,$+4",
                            f"              RES   {svi_bit},C"]
        out.append("              RET")
        out.append("")
    out.append("ROWTAB:       DEFW  " + ",".join(f"ROW{row}" for row in range(len(KEYMAP))))
    return "\n".join(out) + "\n"


def game_image(cas: bytes) -> bytearray:
    """#0000-#BFFF as the SVI loaders leave it, decrypted."""
    blocks = sviport.svi_blocks(cas)
    sviport.expect_structure(blocks, 8, [b"DKong ", b"dona  ", b"donb  ", b"donc  "])
    image = bytearray()
    for block in (blocks[3], blocks[5], blocks[7]):
        image += sviport.bload_data(block, SVI_LOAD)[:BANK_SIZE]
    return bytearray(byte ^ CRYPT_KEY for byte in image)


def image_offset(address: int) -> int:
    """Offset in the #0000-#BFFF image of a runtime address."""
    if address >= 0xC000:
        return address - 0x8000
    if 0x4000 <= address < 0x8000:
        raise sviport.PortError(f"{address:04X}: #4000-#7FFF is not game code at run time")
    return address


def build(cas: bytes, asm: sviport.Assembler) -> dict:
    image = game_image(cas)
    sviport.apply_patches(image, PATCHES, image_offset)
    asm.write("DKROWS.INC", rows())
    helper = asm.assemble("DKHELP")
    offset = image_offset(HELP_ADDRESS)
    if any(image[offset:offset + len(helper)]) or HELP_ADDRESS + len(helper) > 0xFFFF:
        raise sviport.PortError("helper area is not free")
    image[offset:offset + len(helper)] = helper
    copy = asm.assemble("DKCOPY")
    asm.write("DKROM.INC", f"COPYLEN:      EQU   {len(copy)}\n")
    bank0 = asm.assemble("DKROM")
    if len(bank0) > BANK_SIZE or bank0[:2] != b"AB":
        raise sviport.PortError("invalid bank 0")
    rom = bank0.ljust(BANK_SIZE, b"\xFF") + bytes(image)
    print(f"ASCII16, bank 0 init {len(bank0)} bytes, {len(PATCHES)} patches, "
          f"helper {len(helper)} bytes at #{HELP_ADDRESS:04X}")
    return {OUTPUTS[0]: rom}
