"""Builds the Spectravideo CP/M 2.24 system disk for the SVI-328 with an SVI-605B.

Parts:
- BIOS: the Spectravideo CP/M BIOS revision 2.24 source (MAC syntax with Z80.LIB) for a 59K system
  at E600h; not part of this folder, given with --bios.
- CCP and BDOS: the CP/M 2.2 sources of Digital Research (src/cpm22/OS2CCP.ASM, OS3BDOS.ASM)
  at D000h and D800h.
- Boot sector: the one of the Spectravideo CP/M 2.27 disk image given with --boot-disk, changed
  to load the BIOS to E600h.

Disk (single-sided, 40 tracks, 172032 bytes; reserved tracks 0-2 as in the BIOS DPB):
- track 0, sector 1 (FM, 128 bytes): boot sector, loaded by the BASIC ROM to C100h
- track 0, sectors 2-18 (FM) and track 1, sectors 1-4 (MFM, 256 bytes): BIOS from E600h
- track 1, sectors 13-17 and track 2, sectors 1-17: CCP and BDOS from D000h (read by the BIOS)
- from track 3: directory (64 entries, two 1 KiB blocks), the files in src/files and FORMAT.COM,
  assembled with pasmo from src/FORMAT.ASM.

zmac assembles the BIOS in its normal mode; four 8080 mnemonics share their name with a
different Z80 instruction there and are written in their Z80 form first: CPI n -> CP n,
RLC -> RLCA, RRC -> RRCA, JP addr (8080: jump if plus) -> JP P,addr. CCP and BDOS are plain
8080 code and use zmac's 8080 mode.

The BIOS patches the CCP's directory listing for 2 or 4 names per line: it replaces "push d;
ani 11b" at CCP+4B0h and "push psw; cpi 3" at CCP+4E5h with jumps into the BIOS. The
Digital Research CCP has exactly these instructions there; the build checks them.
"""

import argparse
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
BUILD = HERE / "build"

CCP_BASE = 0xD000
BDOS_BASE = 0xD800
BIOS_BASE = 0xE600

DISK_SIZE = 172032
FIRST_TRACK = 18 * 128
TRACK = 17 * 256
BLOCK = 1024
DIRECTORY_BLOCKS = 2
DATA_BLOCKS = 157


def plain_ascii(path: pathlib.Path) -> str:
    # WordStar marks single characters with bit 7; the assembler wants plain ASCII.
    return bytes(b & 0x7F for b in path.read_bytes()).decode("ascii").replace("\r\n", "\n")


def split_statements(source: str) -> str:
    """Puts each '!'-separated statement of the DR assemblers on a line of its own.

    Outside quotes '!' ends a statement and also a comment: in "cpi 6 ;done?! rnc" the RNC is
    an instruction.
    """
    out = []
    for line in source.split("\n"):
        parts = []
        current = ""
        quoted = False
        for char in line:
            if char == "'":
                quoted = not quoted
            if char == "!" and not quoted:
                parts.append(current)
                current = ""
            else:
                current += char
        parts.append(current)
        out.append(parts[0].rstrip())
        out.extend("\t" + part.strip() for part in parts[1:])
    return "\n".join(out)


def translate_8080(source: str) -> str:
    out = []
    for line in source.split("\n"):
        code, sep, comment = line.partition(";")
        # Labels start in column 1, instructions after white space.
        match = re.match(r"^((?:[A-Za-z_$?][\w$?]*:?)?\s+)([A-Za-z]+)(\b.*)$", code)
        if match:
            prefix, op, rest = match.groups()
            upper = op.upper()
            if upper == "CPI" and rest.strip():
                op = "CP"
            elif upper in ("RLC", "RRC") and not rest.strip():
                op = upper + "A"
            elif upper == "JP" and "," not in rest:
                op, rest = "JP", " P," + rest.strip()
            code = prefix + op + rest
        out.append(code + sep + comment)
    return "\n".join(out)


def assemble(zmac: str, name: str, source: str, mode8080: bool) -> dict:
    """Assembles with zmac and returns the bytes as {address: value}."""
    BUILD.mkdir(exist_ok=True)
    (BUILD / (name + ".asm")).write_text(source + "\n", encoding="ascii")
    (BUILD / "Z80.LIB").write_text("; zmac knows the Z80.LIB mnemonics itself.\n", encoding="ascii")
    args = [zmac, "--dri"] + (["-8"] if mode8080 else []) + ["-o", name + ".hex", "-o", name + ".lst", name + ".asm"]
    result = subprocess.run(args, cwd=BUILD, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"zmac failed for {name}")
    memory = {}
    for line in (BUILD / (name + ".hex")).read_text().splitlines():
        if not line.startswith(":"):
            continue
        record = bytes.fromhex(line[1:])
        count, address, kind = record[0], (record[1] << 8) | record[2], record[3]
        if kind == 0:
            for i in range(count):
                memory[address + i] = record[4 + i]
    return memory


def image(memory: dict, start: int, end: int) -> bytearray:
    return bytearray(memory.get(address, 0) for address in range(start, end))


def sector_offset(track: int, sector: int) -> int:
    if track == 0:
        return (sector - 1) * 128
    return FIRST_TRACK + (track - 1) * TRACK + (sector - 1) * 256


def write_files(disk: bytearray, files: list) -> list:
    """Writes CP/M 2.2 directory entries (user 0) and data blocks; returns (name, KiB) pairs."""
    data_start = sector_offset(3, 1)
    next_block = DIRECTORY_BLOCKS
    entries = []
    listing = []
    for path in files:
        content = path.read_bytes()
        records = (len(content) + 127) // 128
        content += b"\x1a" * (records * 128 - len(content))
        name, _, ext = path.name.upper().partition(".")
        blocks = []
        for start in range(0, len(content), BLOCK):
            if next_block > DATA_BLOCKS - 1:
                raise SystemExit("Disk full")
            offset = data_start + next_block * BLOCK
            chunk = content[start:start + BLOCK]
            disk[offset:offset + len(chunk)] = chunk
            blocks.append(next_block)
            next_block += 1
        for extent in range(0, max(1, (len(blocks) + 15) // 16)):
            extent_blocks = blocks[extent * 16:(extent + 1) * 16]
            extent_records = min(records - extent * 128, 128)
            entry = bytearray(32)
            entry[1:9] = name.ljust(8).encode("ascii")
            entry[9:12] = ext.ljust(3).encode("ascii")
            entry[12] = extent
            entry[15] = extent_records
            entry[16:16 + len(extent_blocks)] = bytes(extent_blocks)
            entries.append(entry)
        listing.append((path.name.upper(), len(blocks)))
    if len(entries) > 64:
        raise SystemExit("Directory full")
    for index, entry in enumerate(entries):
        disk[data_start + index * 32:data_start + index * 32 + 32] = entry
    return listing


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Builds the Spectravideo CP/M 2.24 system disk for the SVI-328 with an SVI-605B.")
    parser.add_argument("--bios", type=pathlib.Path, default=HERE / "src" / "SVI328BIOS224.ASM",
                        help="Spectravideo CP/M BIOS 2.24 source (default: src/SVI328BIOS224.ASM)")
    parser.add_argument("--boot-disk", type=pathlib.Path, default=HERE / "src" / "CPM227DS.DSK",
                        help="Spectravideo CP/M 2.27 disk image for the boot sector (default: src/CPM227DS.DSK)")
    parser.add_argument("--output", type=pathlib.Path, default=HERE / "CPM224SS.DSK",
                        help="disk image to write (default: CPM224SS.DSK next to this script)")
    parser.add_argument("--zmac", default="zmac", help="zmac executable (default: zmac on the PATH)")
    parser.add_argument("--pasmo", default="pasmo", help="pasmo executable (default: pasmo on the PATH)")
    args = parser.parse_args()
    for path, what in ((args.bios, "BIOS source"), (args.boot_disk, "CP/M 2.27 disk image")):
        if not path.is_file():
            parser.error(f"{what} not found: {path}")
    return args


def main() -> None:
    args = parse_arguments()
    ccp_source = plain_ascii(HERE / "src" / "cpm22" / "OS2CCP.ASM").replace("\torg\t000h", f"\torg\t0{CCP_BASE:X}h")
    bdos_source = plain_ascii(HERE / "src" / "cpm22" / "OS3BDOS.ASM").replace("\torg\t0800h", f"\torg\t0{BDOS_BASE:X}h")
    ccp = assemble(args.zmac, "ccp", split_statements(ccp_source), mode8080=True)
    bdos = assemble(args.zmac, "bdos", split_statements(bdos_source), mode8080=True)
    system = image({**ccp, **bdos}, CCP_BASE, BIOS_BASE)
    bios_source = plain_ascii(args.bios)
    bios = assemble(args.zmac, "bios", translate_8080(bios_source), mode8080=False)
    for offset, expected in ((0x4B0, "d5e603"), (0x4E5, "f5fe03")):
        assert bytes(system[offset:offset + 3]) == bytes.fromhex(expected), hex(offset)
        system[offset:offset + 3] = bytes(bios[CCP_BASE + offset + i] for i in range(3))
    bios_code = [a for a in bios if a >= BIOS_BASE]
    bios_image = image(bios, BIOS_BASE, max(bios_code) + 1)
    bios_sectors = 17 + 4
    bios_room = 17 * 128 + 4 * 256
    if len(bios_image) > bios_room:
        raise SystemExit(f"BIOS ({len(bios_image)} bytes) does not fit into {bios_room} bytes")
    bios_image += bytes(bios_room - len(bios_image))

    boot = bytearray(args.boot_disk.read_bytes()[:128])
    assert boot[0x20] == 0x0C and boot[0x25:0x27] == b"\x00\xd6" and boot[0x37:0x39] == b"\x00\xd6"
    boot[0x20] = 4
    boot[0x25:0x27] = BIOS_BASE.to_bytes(2, "little")
    boot[0x37:0x39] = BIOS_BASE.to_bytes(2, "little")

    disk = bytearray(b"\xe5" * DISK_SIZE)
    disk[0:128] = boot
    disk[sector_offset(0, 2):sector_offset(0, 2) + 17 * 128] = bios_image[:17 * 128]
    disk[sector_offset(1, 1):sector_offset(1, 1) + 4 * 256] = bios_image[17 * 128:]
    disk[sector_offset(1, 13):sector_offset(1, 13) + 5 * 256] = system[:5 * 256]
    disk[sector_offset(2, 1):sector_offset(2, 1) + 17 * 256] = system[5 * 256:]
    result = subprocess.run([args.pasmo, "--bin", str(HERE / "src" / "FORMAT.ASM"), str(BUILD / "FORMAT.COM")],
                            capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit("pasmo failed for FORMAT.ASM")
    files = sorted(list((HERE / "src" / "files").glob("*.*")) + [BUILD / "FORMAT.COM"], key=lambda f: f.name.upper())
    listing = write_files(disk, files)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(disk)
    print(f"BIOS {len(bios_code)} bytes at {BIOS_BASE:04X}h ({bios_sectors} sectors), CCP+BDOS {len(system)} bytes")
    print("Files: " + ", ".join(f"{name} {kib}K" for name, kib in listing))
    print(f"Disk: {args.output}")


if __name__ == "__main__":
    main()
