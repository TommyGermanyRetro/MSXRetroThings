"""Builds the MSX port of Armoured Assault (Spectravideo, SVI-318/328) as CAS and ROM files.

Source: the original SVI CAS. Its GAME block holds the memory image the SVI loader copies
to #C000-#F7FF: block offsets #0000-#2FFF become #C000-#EFFF, offsets #3080-#387F become
#F000-#F7FF. The image is patched (SVI I/O ports -> MSX ports, keyboard and joystick trigger
through the page-0 helpers, both joysticks, stack below #FFFF), then saved as a BLOAD file together with the
boot code (templates/BOOT.ASM) and the helper block (ARHELP.ASM). A 16 KiB cartridge ROM is built
from the same patched game image, with its own ROM startup code (templates/ROM16.ASM).
"""

import sviport

SOURCE = "Armoured Assaut (1983)(Spectravideo).cas"
OUTPUTS = ["Armoured Assaut (1983)(Spectravideo)(MSX)[BLOAD'CAS-',R].cas", "Armoured Assaut (1983)(Spectravideo)(MSX).ROM"]

# (address, original bytes, replacement bytes)
PATCHES = [
    (0xC018, "310000", "31FEFF"),  # LD SP,0000h
    (0xC062, "D381", "D399"),  # OUT (81h),A
    (0xC066, "D381", "D399"),  # OUT (81h),A
    (0xC06B, "310000", "31FEFF"),  # LD SP,0000h
    (0xC08A, "DB85", "DB99"),  # IN A,(85h)
    (0xC0CC, "310000", "31FEFF"),  # LD SP,0000h
    (0xC0F6, "310000", "31FEFF"),  # LD SP,0000h
    (0xC123, "3E0ED388DB9032C4F8", "CD460032C4F8000000"),  # Joystick 1+2 (PSG R14) -> SVIR14
    (0xC13A, "DB9832C5F8", "CD49000000"),  # triggers 1+2 (PPI port A) -> SVIPA, stores F8C5h
    (0xC337, "D388", "D3A0"),  # OUT (88h),A
    (0xC33A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC36B, "D388", "D3A0"),  # OUT (88h),A
    (0xC36E, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC420, "D388", "D3A0"),  # OUT (88h),A
    (0xC422, "DB90", "DBA2"),  # IN A,(90h)
    (0xC428, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC42C, "D388", "D3A0"),  # OUT (88h),A
    (0xC430, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC434, "D388", "D3A0"),  # OUT (88h),A
    (0xC438, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC43C, "D388", "D3A0"),  # OUT (88h),A
    (0xC43F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC443, "D388", "D3A0"),  # OUT (88h),A
    (0xC447, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC489, "0E88", "0EA0"),  # LD C,88h
    (0xC48F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC495, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC49B, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC4B1, "D388", "D3A0"),  # OUT (88h),A
    (0xC4B6, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC4BA, "D388", "D3A0"),  # OUT (88h),A
    (0xC4BE, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC4D7, "D388", "D3A0"),  # OUT (88h),A
    (0xC4DB, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC4DF, "D388", "D3A0"),  # OUT (88h),A
    (0xC4E2, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC4E6, "D388", "D3A0"),  # OUT (88h),A
    (0xC4EA, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC50C, "D381", "D399"),  # OUT (81h),A
    (0xC510, "D381", "D399"),  # OUT (81h),A
    (0xC701, "DB84", "DB98"),  # IN A,(84h)
    (0xC70D, "DB84", "DB98"),  # IN A,(84h)
    (0xC7AC, "0E81", "0E99"),  # LD C,81h
    (0xC7B5, "DB84", "DB98"),  # IN A,(84h)
    (0xC7C8, "DB84", "DB98"),  # IN A,(84h)
    (0xC9F8, "DB84", "DB98"),  # IN A,(84h)
    (0xCA4F, "0E81", "0E99"),  # LD C,81h
    (0xCA58, "DB84", "DB98"),  # IN A,(84h)
    (0xCAD3, "0E81", "0E99"),  # LD C,81h
    (0xCADF, "DB84", "DB98"),  # IN A,(84h)
    (0xCBC5, "0E81", "0E99"),  # LD C,81h
    (0xCBCD, "D380", "D398"),  # OUT (80h),A
    (0xCFDF, "D397", "0000"),  # OUT (97h),A
    (0xCFE3, "D388", "D3A0"),  # OUT (88h),A
    (0xCFE7, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xCFF7, "0E81", "0E99"),  # LD C,81h
    (0xD015, "0E81", "0E99"),  # LD C,81h
    (0xD02C, "0E81", "0E99"),  # LD C,81h
    (0xD26A, "0E81", "0E99"),  # LD C,81h
    (0xD271, "D380", "D398"),  # OUT (80h),A
    (0xD279, "0E81", "0E99"),  # LD C,81h
    (0xD27F, "0E80", "0E98"),  # LD C,80h
    (0xD292, "D388", "D3A0"),  # OUT (88h),A
    (0xD294, "DB90", "DBA2"),  # IN A,(90h)
    (0xD29A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2B4, "D388", "D3A0"),  # OUT (88h),A
    (0xD2B9, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2BD, "D388", "D3A0"),  # OUT (88h),A
    (0xD2C2, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2C6, "D388", "D3A0"),  # OUT (88h),A
    (0xD2CA, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2DB, "D388", "D3A0"),  # OUT (88h),A
    (0xD2E0, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2E4, "D388", "D3A0"),  # OUT (88h),A
    (0xD2E9, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD2ED, "D388", "D3A0"),  # OUT (88h),A
    (0xD2F1, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD30E, "D388", "D3A0"),  # OUT (88h),A
    (0xD310, "DB90", "DBA2"),  # IN A,(90h)
    (0xD316, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD4D9, "D388", "D3A0"),  # OUT (88h),A
    (0xD4DC, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD4E1, "D388", "D3A0"),  # OUT (88h),A
    (0xD4E4, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD4E9, "D388", "D3A0"),  # OUT (88h),A
    (0xD4EC, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD507, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD518, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD523, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE33D, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE349, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE355, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE361, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE375, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE391, "D381", "D399"),  # OUT (81h),A
    (0xE398, "D381", "D399"),  # OUT (81h),A
    (0xE39F, "D380", "D398"),  # OUT (80h),A
    (0xE3AC, "D381", "D399"),  # OUT (81h),A
    (0xE3B3, "D381", "D399"),  # OUT (81h),A
    (0xE3BA, "D380", "D398"),  # OUT (80h),A
    (0xE3DB, "0E81", "0E99"),  # LD C,81h
    (0xE405, "D380", "D398"),  # OUT (80h),A
    (0xE608, "D381", "D399"),  # OUT (81h),A
    (0xE611, "D381", "D399"),  # OUT (81h),A
    (0xE626, "D380", "D398"),  # OUT (80h),A
    (0xE694, "D381", "D399"),  # OUT (81h),A
    (0xE69A, "D381", "D399"),  # OUT (81h),A
    (0xE6A9, "D380", "D398"),  # OUT (80h),A
    (0xE76D, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE880, "0E81", "0E99"),  # LD C,81h
    (0xE899, "D380", "D398"),  # OUT (80h),A
    (0xE89F, "D380", "D398"),  # OUT (80h),A
    (0xE8A5, "D380", "D398"),  # OUT (80h),A
    (0xE8AB, "D380", "D398"),  # OUT (80h),A
    (0xE8BE, "D380", "D398"),  # OUT (80h),A
    (0xE8D8, "0E81", "0E99"),  # LD C,81h
    (0xE8E1, "DB84", "DB98"),  # IN A,(84h)
    (0xE8ED, "D388", "D3A0"),  # OUT (88h),A
    (0xE8EF, "DB90", "DBA2"),  # IN A,(90h)
    (0xE8F5, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE8F9, "D388", "D3A0"),  # OUT (88h),A
    (0xE8FD, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE901, "D388", "D3A0"),  # OUT (88h),A
    (0xE906, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE916, "D388", "D3A0"),  # OUT (88h),A
    (0xE918, "DB90", "DBA2"),  # IN A,(90h)
    (0xE91E, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE922, "D388", "D3A0"),  # OUT (88h),A
    (0xE926, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE92A, "D388", "D3A0"),  # OUT (88h),A
    (0xE92F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE93F, "D388", "D3A0"),  # OUT (88h),A
    (0xE941, "DB90", "DBA2"),  # IN A,(90h)
    (0xE947, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE94B, "D388", "D3A0"),  # OUT (88h),A
    (0xE94F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE953, "D388", "D3A0"),  # OUT (88h),A
    (0xE957, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE95B, "D388", "D3A0"),  # OUT (88h),A
    (0xE960, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE970, "D388", "D3A0"),  # OUT (88h),A
    (0xE972, "DB90", "DBA2"),  # IN A,(90h)
    (0xE978, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE97C, "D388", "D3A0"),  # OUT (88h),A
    (0xE980, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE984, "D388", "D3A0"),  # OUT (88h),A
    (0xE988, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE98C, "D388", "D3A0"),  # OUT (88h),A
    (0xE991, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE9A6, "D388", "D3A0"),  # OUT (88h),A
    (0xE9A8, "DB90", "DBA2"),  # IN A,(90h)
    (0xE9AE, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE9D2, "D388", "D3A0"),  # OUT (88h),A
    (0xE9D5, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE9D9, "D388", "D3A0"),  # OUT (88h),A
    (0xE9DC, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE9E2, "D388", "D3A0"),  # OUT (88h),A
    (0xE9E5, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE9FF, "D388", "D3A0"),  # OUT (88h),A
    (0xEA03, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEA27, "D388", "D3A0"),  # OUT (88h),A
    (0xEA29, "DB90", "DBA2"),  # IN A,(90h)
    (0xEA2F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEA59, "D388", "D3A0"),  # OUT (88h),A
    (0xEA5C, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEA60, "D388", "D3A0"),  # OUT (88h),A
    (0xEA63, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEA69, "D388", "D3A0"),  # OUT (88h),A
    (0xEA6C, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAAD, "D388", "D3A0"),  # OUT (88h),A
    (0xEAB1, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAB5, "D388", "D3A0"),  # OUT (88h),A
    (0xEAB7, "DB90", "DBA2"),  # IN A,(90h)
    (0xEABD, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAC1, "D388", "D3A0"),  # OUT (88h),A
    (0xEAC5, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAC9, "D388", "D3A0"),  # OUT (88h),A
    (0xEACD, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAD1, "D388", "D3A0"),  # OUT (88h),A
    (0xEAD4, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAD8, "D388", "D3A0"),  # OUT (88h),A
    (0xEADB, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAF0, "D388", "D3A0"),  # OUT (88h),A
    (0xEAF2, "DB90", "DBA2"),  # IN A,(90h)
    (0xEAF8, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEAFC, "D388", "D3A0"),  # OUT (88h),A
    (0xEB00, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB04, "D388", "D3A0"),  # OUT (88h),A
    (0xEB08, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB0C, "D388", "D3A0"),  # OUT (88h),A
    (0xEB0F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB13, "D388", "D3A0"),  # OUT (88h),A
    (0xEB15, "DB90", "DBA2"),  # IN A,(90h)
    (0xEB19, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB31, "D388", "D3A0"),  # OUT (88h),A
    (0xEB33, "DB90", "DBA2"),  # IN A,(90h)
    (0xEB39, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB3D, "D388", "D3A0"),  # OUT (88h),A
    (0xEB41, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB45, "D388", "D3A0"),  # OUT (88h),A
    (0xEB49, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB4D, "D388", "D3A0"),  # OUT (88h),A
    (0xEB50, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB54, "D388", "D3A0"),  # OUT (88h),A
    (0xEB56, "DB90", "DBA2"),  # IN A,(90h)
    (0xEB5A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB72, "D388", "D3A0"),  # OUT (88h),A
    (0xEB76, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB7A, "D388", "D3A0"),  # OUT (88h),A
    (0xEB7C, "DB90", "DBA2"),  # IN A,(90h)
    (0xEB82, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB86, "D388", "D3A0"),  # OUT (88h),A
    (0xEB8A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB8E, "D388", "D3A0"),  # OUT (88h),A
    (0xEB92, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB96, "D388", "D3A0"),  # OUT (88h),A
    (0xEB99, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEB9D, "D388", "D3A0"),  # OUT (88h),A
    (0xEBA0, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xEC1F, "D381", "D399"),  # OUT (81h),A
    (0xEC22, "D381", "D399"),  # OUT (81h),A
    (0xEC27, "D380", "D398"),  # OUT (80h),A
    (0xEC4F, "0E81", "0E99"),  # LD C,81h
    (0xEC94, "0E81", "0E99"),  # LD C,81h
    (0xEC9F, "D380", "D398"),  # OUT (80h),A
    (0xECA8, "0E81", "0E99"),  # LD C,81h
    (0xECB1, "0E81", "0E99"),  # LD C,81h
    (0xECC1, "D380", "D398"),  # OUT (80h),A
    (0xECCB, "0E81", "0E99"),  # LD C,81h
    (0xED02, "D380", "D398"),  # OUT (80h),A
    (0xED10, "D380", "D398"),  # OUT (80h),A
]


def game_image(cas: bytes) -> bytearray:
    blocks = sviport.svi_blocks(cas)
    names = [block[10:16] for block in blocks]
    if len(blocks) != 4 or names[2] != b"GAME  ":
        raise sviport.PortError(f"unexpected SVI CAS structure: {len(blocks)} blocks, names {names}")
    data = blocks[3]
    return bytearray(data[0x0000:0x3000] + data[0x3080:0x3880])


def build(cas: bytes, asm: sviport.Assembler) -> dict:
    image = game_image(cas)
    sviport.apply_patches(image, PATCHES, lambda address: address - 0xC000)
    cassette, rom, helper = sviport.c000_port(asm, image, "ARHELP", b"ARMOUR")
    print(f"{len(PATCHES)} patches, helper {helper} bytes")
    return dict(zip(OUTPUTS, (cassette, rom)))
