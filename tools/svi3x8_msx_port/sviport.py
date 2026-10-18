"""Shared building blocks for porting SVI-318/328 cassette games to MSX.

A port recipe (games/<name>/recipe.py) takes the memory image apart as the SVI loader builds it,
patches it with a table of (address, original bytes, replacement bytes), assembles its helper and
startup code with pasmo and writes MSX cassette and cartridge files. Everything a recipe does with
the image goes through this module, so that every port checks the original bytes in the same way.
"""

import pathlib
import shutil
import subprocess
import sys

SVI_HEADER = bytes([0x55] * 16 + [0x7F])
MSX_MARKER = bytes.fromhex("1FA6DEBACC137D74")
MSX_BINARY = 0xD0
TEMPLATES = pathlib.Path(__file__).resolve().parent / "templates"


class PortError(Exception):
    """A source that does not look as the recipe expects, or a build step that failed."""


def svi_blocks(cas: bytes) -> list:
    """Splits an SVI CAS image at its 16 x #55 + #7F block headers."""
    blocks = []
    position = 0
    while True:
        start = cas.find(SVI_HEADER, position)
        if start < 0:
            break
        end = cas.find(SVI_HEADER, start + len(SVI_HEADER))
        blocks.append(cas[start + len(SVI_HEADER):end if end >= 0 else len(cas)])
        if end < 0:
            break
        position = end
    return blocks


def file_names(blocks: list) -> list:
    """Names of the files of an SVI CAS: bytes 10-15 of every header block (every second block)."""
    return [block[10:16] for block in blocks[0::2]]


def expect_structure(blocks: list, count: int, names: list, step: int = 2) -> None:
    """Checks the number of blocks and the file names (from every step-th block)."""
    found = [block[10:16] for block in blocks[0::step]] if step else []
    if len(blocks) != count or found != names:
        raise PortError(f"unexpected SVI CAS structure: {len(blocks)} blocks, names {found}")


def bload_data(block: bytes, load: int) -> bytes:
    """Data of an SVI BLOAD block after checking its load address (start, end, entry, data)."""
    if int.from_bytes(block[0:2], "little") != load:
        raise PortError("unexpected SVI load address")
    return block[6:]


def apply_patches(image: bytearray, patches: list, offset_of) -> None:
    """Applies (address, original hex, replacement hex) entries; offset_of maps an address to an
    offset in the image. Every original byte sequence must be found unchanged."""
    for address, original, replacement in patches:
        offset = offset_of(address)
        old = bytes.fromhex(original)
        new = bytes.fromhex(replacement)
        if len(old) != len(new) or image[offset:offset + len(old)] != old:
            raise PortError(f"patch {address:04X}: expected {original}, found "
                            f"{image[offset:offset + len(old)].hex().upper()}")
        image[offset:offset + len(new)] = new


class Assembler:
    """Runs pasmo in a work folder that holds copies of the recipe's and the shared sources."""

    def __init__(self, pasmo: str, work: pathlib.Path, sources: list):
        self.pasmo = pasmo
        self.work = work
        work.mkdir(parents=True, exist_ok=True)
        for source in sources:
            shutil.copyfile(source, work / source.name)

    def write(self, name: str, content) -> None:
        """Writes a generated include (text, CRLF) or binary file into the work folder."""
        path = self.work / name
        if isinstance(content, str):
            path.write_bytes(content.replace("\r\n", "\n").replace("\n", "\r\n").encode("ascii"))
        else:
            path.write_bytes(bytes(content))

    def assemble(self, name: str) -> bytes:
        """Assembles <name>.ASM in the work folder to <name>.BIN and returns its bytes."""
        try:
            result = subprocess.run([self.pasmo, "--bin", f"{name}.ASM", f"{name}.BIN"], cwd=self.work,
                                    capture_output=True, text=True)
        except FileNotFoundError as error:
            raise PortError(f"pasmo not found: {self.pasmo}") from error
        if result.returncode != 0:
            sys.stderr.write(result.stdout + result.stderr)
            raise PortError(f"pasmo failed for {name}.ASM")
        return (self.work / f"{name}.BIN").read_bytes()


def msx_bload_cas(name: bytes, load: int, start: int, body: bytes) -> bytes:
    """MSX cassette image of one BLOAD file: header block with the name, data block with start,
    end and entry address."""
    if len(name) != 6:
        raise PortError("an MSX cassette name has six characters")
    end = load + len(body) - 1
    header = MSX_MARKER + bytes([MSX_BINARY] * 10) + name
    data = (MSX_MARKER + load.to_bytes(2, "little") + end.to_bytes(2, "little")
            + start.to_bytes(2, "little") + body)
    return header + data


def cartridge(rom: bytes, size: int, entry: int = None) -> bytes:
    """Fills a cartridge image to its size with #FF and checks the "AB" header and the entry."""
    if len(rom) > size:
        raise PortError(f"cartridge image is too large: {len(rom)} bytes")
    if rom[:2] != b"AB" or (entry is not None and int.from_bytes(rom[2:4], "little") != entry):
        raise PortError("invalid cartridge ROM header or startup address")
    return rom.ljust(size, b"\xFF")


C000_LOAD = 0x8000
C000_BOOT = 0xB800
C000_ROM_ENTRY = 0x4010


def c000_port(asm: Assembler, image: bytes, helper_source: str, cas_name: bytes) -> tuple:
    """Cassette and 16 KiB cartridge for a game that runs at #C000 with a page-0 helper block
    (templates BOOT.ASM and ROM16.ASM). Returns (cassette image, ROM image, helper size)."""
    helper = asm.assemble(helper_source)
    asm.write("HELP.INC", f"HELPLEN:      EQU   {len(helper)}\n")
    asm.write("HELP.BIN", helper)
    boot = asm.assemble("BOOT")
    if C000_LOAD + len(image) != C000_BOOT:
        raise PortError("game image does not end at the boot address")
    cas = msx_bload_cas(cas_name, C000_LOAD, C000_BOOT, bytes(image) + boot + helper)
    asm.write("GAME.BIN", image)
    rom = cartridge(asm.assemble("ROM16"), 0x4000, C000_ROM_ENTRY)
    return cas, rom, len(helper)
