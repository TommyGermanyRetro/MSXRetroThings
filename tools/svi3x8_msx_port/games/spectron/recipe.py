"""Builds the MSX port of Spectron (Spectravideo, SVI-318/328) as CAS and ROM files.

Source: the original SVI CAS. Its GAME block holds the memory image the SVI loader copies
to #C000-#F7FF: block offsets #0000-#2FFF become #C000-#EFFF, offsets #3080-#387F become
#F000-#F7FF. The image is patched (SVI I/O ports -> MSX ports, keyboard and joystick trigger
through the page-0 helpers, stack below #FFFF), then saved as a BLOAD file together with the
boot code (templates/BOOT.ASM) and the helper block (SPHELP.ASM). A 16 KiB cartridge ROM is built
from the same patched game image, with its own ROM startup code (templates/ROM16.ASM).
"""

import sviport

SOURCE = "Spectron (1983)(Spectravideo).cas"
OUTPUTS = ["Spectron (1983)(Spectravideo)(MSX)[BLOAD'CAS-',R].cas", "Spectron (1983)(Spectravideo)(MSX).ROM"]

# (address, original bytes, replacement bytes)
PATCHES = [
    (0xC000, "310000", "31FEFF"),  # LD SP,0000h
    (0xC02D, "D388", "D3A0"),  # OUT (88h),A
    (0xC031, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC035, "D397", "0000"),  # OUT (97h),A
    (0xC040, "D381", "D399"),  # OUT (81h),A
    (0xC045, "D381", "D399"),  # OUT (81h),A
    (0xC063, "D388", "D3A0"),  # OUT (88h),A
    (0xC067, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC06B, "D388", "D3A0"),  # OUT (88h),A
    (0xC06F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xC112, "D381", "D399"),  # OUT (81h),A
    (0xC116, "D381", "D399"),  # OUT (81h),A
    (0xC11A, "D381", "D399"),  # OUT (81h),A
    (0xC11E, "D381", "D399"),  # OUT (81h),A
    (0xC125, "D381", "D399"),  # OUT (81h),A
    (0xC12B, "D381", "D399"),  # OUT (81h),A
    (0xC12F, "D380", "D398"),  # OUT (80h),A
    (0xC138, "D381", "D399"),  # OUT (81h),A
    (0xC13C, "D381", "D399"),  # OUT (81h),A
    (0xC145, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xC1A1, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xC1D6, "D381", "D399"),  # OUT (81h),A
    (0xC1DA, "D381", "D399"),  # OUT (81h),A
    (0xC1DE, "D381", "D399"),  # OUT (81h),A
    (0xC1E2, "D381", "D399"),  # OUT (81h),A
    (0xC296, "D381", "D399"),  # OUT (81h),A
    (0xC29A, "D381", "D399"),  # OUT (81h),A
    (0xC336, "D380", "D398"),  # OUT (80h),A
    (0xC522, "D381", "D399"),  # OUT (81h),A
    (0xC528, "D381", "D399"),  # OUT (81h),A
    (0xC532, "DB85", "DB99"),  # IN A,(85h)
    (0xC5F6, "310000", "31FEFF"),  # LD SP,0000h
    (0xC67B, "D381", "D399"),  # OUT (81h),A
    (0xC680, "D381", "D399"),  # OUT (81h),A
    (0xC686, "D380", "D398"),  # OUT (80h),A
    (0xC750, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xC769, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xC77E, "D381", "D399"),  # OUT (81h),A
    (0xC781, "D381", "D399"),  # OUT (81h),A
    (0xC786, "DB84", "DB98"),  # IN A,(84h)
    (0xC78A, "D381", "D399"),  # OUT (81h),A
    (0xC78F, "D381", "D399"),  # OUT (81h),A
    (0xC792, "D380", "D398"),  # OUT (80h),A
    (0xC796, "D381", "D399"),  # OUT (81h),A
    (0xC79B, "D381", "D399"),  # OUT (81h),A
    (0xC7A0, "D380", "D398"),  # OUT (80h),A
    (0xCD69, "D381", "D399"),  # OUT (81h),A
    (0xCD6C, "D381", "D399"),  # OUT (81h),A
    (0xCD71, "DB84", "DB98"),  # IN A,(84h)
    (0xCD82, "D381", "D399"),  # OUT (81h),A
    (0xCD87, "D381", "D399"),  # OUT (81h),A
    (0xCD8A, "D380", "D398"),  # OUT (80h),A
    (0xD191, "310000", "31FEFF"),  # LD SP,0000h
    (0xD28B, "D388", "D3A0"),  # OUT (88h),A
    (0xD28F, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD293, "D388", "D3A0"),  # OUT (88h),A
    (0xD297, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD558, "D388", "D3A0"),  # OUT (88h),A
    (0xD55A, "DB90", "DBA2"),  # IN A,(90h)
    (0xD566, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD572, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD57E, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD58A, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD664, "DB98CB67", "CD430000"),  # SVI trigger -> SVITRIG
    (0xD66C, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD6AF, "DB98CB67", "CD430000"),  # SVI trigger -> SVITRIG
    (0xD6B7, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xD6CC, "D388", "D3A0"),  # OUT (88h),A
    (0xD6D0, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD6D4, "D388", "D3A0"),  # OUT (88h),A
    (0xD6D8, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD6DC, "D388", "D3A0"),  # OUT (88h),A
    (0xD6E0, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD757, "D381", "D399"),  # OUT (81h),A
    (0xD75A, "D381", "D399"),  # OUT (81h),A
    (0xD75F, "DB84", "DB98"),  # IN A,(84h)
    (0xD767, "D388", "D3A0"),  # OUT (88h),A
    (0xD76B, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD76F, "D388", "D3A0"),  # OUT (88h),A
    (0xD773, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD799, "D388", "D3A0"),  # OUT (88h),A
    (0xD7A3, "DB90", "DBA2"),  # IN A,(90h)
    (0xD7A6, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD7AD, "DB90", "DBA2"),  # IN A,(90h)
    (0xD7B3, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD7B7, "D388", "D3A0"),  # OUT (88h),A
    (0xD7B9, "DB90", "DBA2"),  # IN A,(90h)
    (0xD7BE, "D388", "D3A0"),  # OUT (88h),A
    (0xD7C0, "DB90", "DBA2"),  # IN A,(90h)
    (0xD7C9, "D388", "D3A0"),  # OUT (88h),A
    (0xD7CC, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xD7D0, "D388", "D3A0"),  # OUT (88h),A
    (0xD7D3, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDBA2, "D381", "D399"),  # OUT (81h),A
    (0xDBA5, "D381", "D399"),  # OUT (81h),A
    (0xDBAA, "DB84", "DB98"),  # IN A,(84h)
    (0xDBB3, "DB84", "DB98"),  # IN A,(84h)
    (0xDDB8, "D381", "D399"),  # OUT (81h),A
    (0xDDBB, "D381", "D399"),  # OUT (81h),A
    (0xDDC0, "DB84", "DB98"),  # IN A,(84h)
    (0xDDC6, "D381", "D399"),  # OUT (81h),A
    (0xDDCB, "D381", "D399"),  # OUT (81h),A
    (0xDDD8, "D380", "D398"),  # OUT (80h),A
    (0xDE7E, "D388", "D3A0"),  # OUT (88h),A
    (0xDE83, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDE89, "D388", "D3A0"),  # OUT (88h),A
    (0xDE8D, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDE91, "D388", "D3A0"),  # OUT (88h),A
    (0xDE96, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDE9A, "D388", "D3A0"),  # OUT (88h),A
    (0xDE9E, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDEEE, "D388", "D3A0"),  # OUT (88h),A
    (0xDEF2, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDEF6, "D388", "D3A0"),  # OUT (88h),A
    (0xDEFA, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDF1D, "D388", "D3A0"),  # OUT (88h),A
    (0xDF22, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDF2D, "D388", "D3A0"),  # OUT (88h),A
    (0xDF32, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDF38, "D388", "D3A0"),  # OUT (88h),A
    (0xDF3D, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xDFAC, "D380", "D398"),  # OUT (80h),A
    (0xE031, "D381", "D399"),  # OUT (81h),A
    (0xE037, "D381", "D399"),  # OUT (81h),A
    (0xE056, "D388", "D3A0"),  # OUT (88h),A
    (0xE05A, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE05E, "D388", "D3A0"),  # OUT (88h),A
    (0xE062, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE08A, "310000", "31FEFF"),  # LD SP,0000h
    (0xE0B0, "D396DB99", "CD400000"),  # SVI keyboard -> SVIROW
    (0xE0DC, "0E81", "0E99"),  # LD C,81h
    (0xE116, "310000", "31FEFF"),  # LD SP,0000h
    (0xE12A, "310000", "31FEFF"),  # LD SP,0000h
    (0xE15E, "310000", "31FEFF"),  # LD SP,0000h
    (0xE17E, "D388", "D3A0"),  # OUT (88h),A
    (0xE180, "DB90", "DBA2"),  # IN A,(90h)
    (0xE189, "D388", "D3A0"),  # OUT (88h),A
    (0xE18B, "DB90", "DBA2"),  # IN A,(90h)
    (0xE195, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE1A5, "D388", "D3A0"),  # OUT (88h),A
    (0xE1AB, "DB90", "DBA2"),  # IN A,(90h)
    (0xE1B4, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE1BE, "D388", "D3A0"),  # OUT (88h),A
    (0xE1C2, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE1C6, "D388", "D3A0"),  # OUT (88h),A
    (0xE1CA, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE1CE, "D388", "D3A0"),  # OUT (88h),A
    (0xE1D3, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE2A1, "D388", "D3A0"),  # OUT (88h),A
    (0xE2A3, "DB90", "DBA2"),  # IN A,(90h)
    (0xE2A7, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE2AB, "D388", "D3A0"),  # OUT (88h),A
    (0xE2AD, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE2B1, "D388", "D3A0"),  # OUT (88h),A
    (0xE2B8, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE33C, "D388", "D3A0"),  # OUT (88h),A
    (0xE340, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE392, "D388", "D3A0"),  # OUT (88h),A
    (0xE395, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE399, "D388", "D3A0"),  # OUT (88h),A
    (0xE39D, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xE3A1, "D388", "D3A0"),  # OUT (88h),A
    (0xE3A5, "D38C", "D3A1"),  # OUT (8Ch),A
    (0xF111, "D381", "D399"),  # OUT (81h),A
    (0xF114, "D381", "D399"),  # OUT (81h),A
    (0xF119, "D380", "D398"),  # OUT (80h),A
    (0xF146, "0E81", "0E99"),  # LD C,81h
    (0xF194, "0E81", "0E99"),  # LD C,81h
    (0xF19F, "D380", "D398"),  # OUT (80h),A
    (0xF1A8, "0E81", "0E99"),  # LD C,81h
    (0xF1B1, "0E81", "0E99"),  # LD C,81h
    (0xF1C1, "D380", "D398"),  # OUT (80h),A
    (0xF1CB, "0E81", "0E99"),  # LD C,81h
    (0xF202, "D380", "D398"),  # OUT (80h),A
    (0xF210, "D380", "D398"),  # OUT (80h),A
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
    cassette, rom, helper = sviport.c000_port(asm, image, "SPHELP", b"SPECTR")
    print(f"{len(PATCHES)} patches, helper {helper} bytes")
    return dict(zip(OUTPUTS, (cassette, rom)))
