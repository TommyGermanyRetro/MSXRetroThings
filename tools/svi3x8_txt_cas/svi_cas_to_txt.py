#!/usr/bin/env python3
"""Convert BASIC programs from an SVI cassette image (CSAVE or SAVE) into TXT listings."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


SYNC = b"\x55" * 16 + b"\x7f"
TYPE_TOKENIZED = 0xD3
TYPE_BINARY = 0xD0
TYPE_ASCII = 0xEA
FILE_TYPES = (TYPE_TOKENIZED, TYPE_BINARY, TYPE_ASCII)
HEADER_SIZE = 18
EOF_MARKER = 0x1A

# Keyword table extracted from the SVI-318/328 BASIC ROM (v1.11).
KEYWORDS: dict[int, str] = {
    0x81: "END", 0x82: "FOR", 0x83: "NEXT", 0x84: "DATA", 0x85: "INPUT", 0x86: "DIM",
    0x87: "READ", 0x88: "LET", 0x89: "GOTO", 0x8A: "RUN", 0x8B: "IF", 0x8C: "RESTORE",
    0x8D: "GOSUB", 0x8E: "RETURN", 0x8F: "REM", 0x90: "STOP", 0x91: "PRINT", 0x92: "CLEAR",
    0x93: "LIST", 0x94: "NEW", 0x95: "ON", 0x96: "WAIT", 0x97: "DEF", 0x98: "POKE",
    0x99: "CONT", 0x9A: "CSAVE", 0x9B: "CLOAD", 0x9C: "OUT", 0x9D: "LPRINT", 0x9E: "LLIST",
    0x9F: "CLS", 0xA0: "WIDTH", 0xA1: "ELSE", 0xA2: "TRON", 0xA3: "TROFF", 0xA4: "SWAP",
    0xA5: "ERASE", 0xA6: "ERROR", 0xA7: "RESUME", 0xA8: "DELETE", 0xA9: "AUTO", 0xAA: "RENUM",
    0xAB: "DEFSTR", 0xAC: "DEFINT", 0xAD: "DEFSNG", 0xAE: "DEFDBL", 0xAF: "LINE", 0xB0: "OPEN",
    0xB1: "FIELD", 0xB2: "GET", 0xB3: "PUT", 0xB4: "CLOSE", 0xB5: "LOAD", 0xB6: "MERGE",
    0xB7: "FILES", 0xB8: "LSET", 0xB9: "RSET", 0xBA: "SAVE", 0xBB: "LFILES", 0xBC: "CIRCLE",
    0xBD: "COLOR", 0xBE: "DRAW", 0xBF: "PAINT", 0xC0: "BEEP", 0xC1: "PLAY", 0xC2: "PSET",
    0xC3: "PRESET", 0xC4: "SOUND", 0xC5: "SCREEN", 0xC6: "VPOKE", 0xC7: "KEY", 0xC8: "CLICK",
    0xC9: "SWITCH", 0xCA: "MAX", 0xCB: "MON", 0xCC: "MOTOR", 0xCD: "BLOAD", 0xCE: "BSAVE",
    0xCF: "MDM", 0xD0: "DIAL", 0xD1: "DSKO$", 0xD2: "SET", 0xD3: "NAME", 0xD4: "KILL",
    0xD5: "IPL", 0xD6: "COPY", 0xD7: "CMD", 0xD8: "LOCATE", 0xD9: "TO", 0xDA: "THEN",
    0xDB: "TAB(", 0xDC: "STEP", 0xDD: "USR", 0xDE: "FN", 0xDF: "SPC(", 0xE0: "NOT",
    0xE1: "ERL", 0xE2: "ERR", 0xE3: "STRING$", 0xE4: "USING", 0xE5: "INSTR", 0xE6: "'",
    0xE7: "VARPTR", 0xE8: "CSRLIN", 0xE9: "ATTR$", 0xEA: "DSKI$", 0xEB: "OFF", 0xEC: "INKEY$",
    0xED: "POINT", 0xEE: "SPRITE", 0xEF: "TIME",
    0xF0: ">", 0xF1: "=", 0xF2: "<", 0xF3: "+", 0xF4: "-", 0xF5: "*", 0xF6: "/", 0xF7: "^",
    0xF8: "AND", 0xF9: "OR", 0xFA: "XOR", 0xFB: "EQV", 0xFC: "IMP", 0xFD: "MOD", 0xFE: "\\",
}

# Functions are stored as FF followed by (index | 80h).
FUNCTIONS: dict[int, str] = {
    0x01: "LEFT$", 0x02: "RIGHT$", 0x03: "MID$", 0x04: "SGN", 0x05: "INT", 0x06: "ABS",
    0x07: "SQR", 0x08: "RND", 0x09: "SIN", 0x0A: "LOG", 0x0B: "EXP", 0x0C: "COS",
    0x0D: "TAN", 0x0E: "ATN", 0x0F: "FRE", 0x10: "INP", 0x11: "POS", 0x12: "LEN",
    0x13: "STR$", 0x14: "VAL", 0x15: "ASC", 0x16: "CHR$", 0x17: "PEEK", 0x18: "VPEEK",
    0x19: "SPACE$", 0x1A: "OCT$", 0x1B: "HEX$", 0x1C: "LPOS", 0x1D: "BIN$", 0x1E: "CINT",
    0x1F: "CSNG", 0x20: "CDBL", 0x21: "FIX", 0x22: "STICK", 0x23: "STRIG", 0x24: "PDL",
    0x25: "PAD", 0x26: "DSKF", 0x27: "FPOS", 0x28: "CVI", 0x29: "CVS", 0x2A: "CVD",
    0x2B: "EOF", 0x2C: "LOC", 0x2D: "LOF", 0x2E: "MKI$", 0x2F: "MKS$", 0x30: "MKD$",
}

FILE_NAME_PATTERN = re.compile(r"[^A-Za-z0-9_-]")


@dataclass(frozen=True)
class CasProgram:
    name: str
    file_type: int
    text: str | None


def split_blocks(image: bytes) -> list[bytes]:
    parts = image.split(SYNC)
    if len(parts) < 2 or parts[0].strip(b"\x55\x00"):
        raise ValueError("The file is not an SVI CAS image (sync sequence 55h x16 + 7Fh missing).")
    return parts[1:]


def parse_header(block: bytes) -> tuple[int, str] | None:
    if len(block) < HEADER_SIZE or block[0] not in FILE_TYPES:
        return None
    if any(byte != block[0] for byte in block[:10]) or block[16:18] != b"\xff\x00":
        return None
    return block[0], block[10:16].decode("latin-1").rstrip()


def format_bcd(data: bytes, force_suffix: bool) -> str:
    exponent = (data[0] & 0x7F) - 64
    negative = bool(data[0] & 0x80)
    digits = "".join(f"{byte:02x}" for byte in data[1:]).rstrip("0")
    if not digits:
        return "0"
    if 0 < exponent <= 15:
        body = (
            digits + "0" * (exponent - len(digits))
            if exponent >= len(digits)
            else f"{digits[:exponent]}.{digits[exponent:]}"
        )
    elif -6 < exponent <= 0:
        body = "." + "0" * -exponent + digits
    else:
        mantissa = digits[0] + ("." + digits[1:] if len(digits) > 1 else "")
        body = f"{mantissa}E{exponent - 1:+03d}"
    if force_suffix and len(digits) <= 6 and "E" not in body:
        body += "#"
    return ("-" if negative else "") + body


def detokenize_line(data: bytes) -> str:
    out: list[str] = []
    pos = 0
    in_string = False
    raw_until_colon = False

    while pos < len(data):
        byte = data[pos]
        pos += 1

        if in_string:
            out.append(chr(byte))
            in_string = byte != 0x22
            continue
        if byte == 0x22:
            out.append('"')
            in_string = True
            continue
        if raw_until_colon:
            out.append(chr(byte))
            continue

        if byte == 0x3A and data[pos:pos + 2] == b"\x8f\xe6":
            out.append("'")
            pos += 2
            raw_until_colon = True
        elif byte == 0x3A and data[pos:pos + 1] == b"\xa1":
            out.append("ELSE")
            pos += 1
        elif byte == 0x8F:
            out.append("REM")
            raw_until_colon = True
        elif byte == 0x84:
            out.append("DATA")
            pos = copy_data_statement(data, pos, out)
        elif byte == 0x0B and pos + 2 <= len(data):
            out.append(f"&O{int.from_bytes(data[pos:pos + 2], 'little'):o}")
            pos += 2
        elif byte == 0x0C and pos + 2 <= len(data):
            out.append(f"&H{int.from_bytes(data[pos:pos + 2], 'little'):X}")
            pos += 2
        elif byte in (0x0D, 0x0E) and pos + 2 <= len(data):
            out.append(str(int.from_bytes(data[pos:pos + 2], "little")))
            pos += 2
        elif byte == 0x0F and pos < len(data):
            out.append(str(data[pos]))
            pos += 1
        elif 0x11 <= byte <= 0x1A:
            out.append(str(byte - 0x11))
        elif byte == 0x1C and pos + 2 <= len(data):
            out.append(str(struct.unpack_from("<h", data, pos)[0]))
            pos += 2
        elif byte == 0x1D and pos + 4 <= len(data):
            out.append(format_bcd(data[pos:pos + 4], force_suffix=False))
            pos += 4
        elif byte == 0x1F and pos + 8 <= len(data):
            out.append(format_bcd(data[pos:pos + 8], force_suffix=True))
            pos += 8
        elif byte == 0xFF and pos < len(data):
            function = FUNCTIONS.get(data[pos] & 0x7F)
            out.append(function if function is not None else f"<FF{data[pos]:02X}>")
            pos += 1
        elif byte >= 0x80:
            out.append(KEYWORDS.get(byte, f"<{byte:02X}>"))
        else:
            out.append(chr(byte))

    return "".join(out)


def copy_data_statement(data: bytes, pos: int, out: list[str]) -> int:
    in_string = False
    while pos < len(data):
        byte = data[pos]
        if byte == 0x22:
            in_string = not in_string
        elif byte == 0x3A and not in_string:
            break
        out.append(chr(byte))
        pos += 1
    return pos


def detokenize_program(data: bytes) -> str:
    lines: list[str] = []
    pos = 0
    while pos + 4 <= len(data):
        if data[pos:pos + 2] == b"\x00\x00":
            break
        line_number = int.from_bytes(data[pos + 2:pos + 4], "little")
        pos += 4
        start = pos
        pos = find_line_end(data, pos)
        lines.append(f"{line_number} {detokenize_line(data[start:pos])}")
        pos += 1
    if not lines:
        raise ValueError("The tokenized BASIC program contains no lines.")
    return "\r\n".join(lines) + "\r\n"


def find_line_end(data: bytes, pos: int) -> int:
    # Numeric constants may contain 00h, so skip their payload bytes.
    skip = {0x0B: 2, 0x0C: 2, 0x0D: 2, 0x0E: 2, 0x0F: 1, 0x1C: 2, 0x1D: 4, 0x1F: 8}
    in_string = False
    while pos < len(data):
        byte = data[pos]
        if byte == 0x00:
            break
        if byte == 0x22:
            in_string = not in_string
        pos += 1
        if not in_string and byte in skip:
            pos += skip[byte]
    return min(pos, len(data))


def ascii_program(data: bytes) -> str:
    end = data.find(bytes([EOF_MARKER]))
    body = data if end < 0 else data[:end]
    text = body.replace(b"\x00", b"").decode("latin-1")
    return "\r\n".join(text.replace("\r\n", "\n").replace("\r", "\n").split("\n")).rstrip("\r\n") + "\r\n"


def read_programs(image: bytes) -> list[CasProgram]:
    blocks = split_blocks(image)
    programs: list[CasProgram] = []
    index = 0
    while index < len(blocks):
        header = parse_header(blocks[index])
        index += 1
        if header is None:
            continue
        file_type, name = header
        payload = bytearray()
        while index < len(blocks) and parse_header(blocks[index]) is None:
            payload += blocks[index]
            index += 1

        if file_type == TYPE_TOKENIZED:
            text: str | None = detokenize_program(bytes(payload))
        elif file_type == TYPE_ASCII:
            text = ascii_program(bytes(payload))
        else:
            text = None
        programs.append(CasProgram(name, file_type, text))
    return programs


def output_paths(source: Path, programs: list[CasProgram], explicit: Path | None) -> list[Path]:
    if explicit is not None and len(programs) == 1:
        return [explicit]
    if len(programs) == 1:
        return [source.with_suffix(".txt")]
    paths: list[Path] = []
    for number, program in enumerate(programs, start=1):
        safe_name = FILE_NAME_PATTERN.sub("_", program.name) or "PROGRAM"
        paths.append(source.with_name(f"{source.stem}_{number}_{safe_name}.txt"))
    return paths


def convert_file(source: Path, explicit_output: Path | None = None) -> list[tuple[Path, CasProgram]]:
    programs = [program for program in read_programs(source.read_bytes()) if program.text is not None]
    if not programs:
        raise ValueError("The CAS file contains no BASIC program (CSAVE or SAVE file).")
    written: list[tuple[Path, CasProgram]] = []
    for path, program in zip(output_paths(source, programs, explicit_output), programs):
        assert program.text is not None
        path.write_text(program.text, encoding="utf-8", newline="")
        written.append((path, program))
    return written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Converts BASIC programs (CSAVE/SAVE) from an SVI CAS image into TXT listings."
    )
    parser.add_argument("cas", type=Path, help="SVI CAS file")
    parser.add_argument(
        "-o", "--output", type=Path, help="TXT output file (only when the CAS holds exactly one BASIC program)"
    )
    args = parser.parse_args(argv)

    try:
        written = convert_file(args.cas, args.output)
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    for path, program in written:
        kind = "CSAVE (tokenized)" if program.file_type == TYPE_TOKENIZED else "SAVE (ASCII)"
        print(f"TXT written: {path} (BASIC name: {program.name}, {kind})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
