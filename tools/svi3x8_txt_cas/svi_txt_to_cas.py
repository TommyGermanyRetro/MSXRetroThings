#!/usr/bin/env python3
"""Convert a numbered SVI BASIC listing into a raw SVI cassette image."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


BLOCK_HEADER = b"\x55" * 16 + b"\x7f"
ASCII_FILE_TYPE = b"\xea" * 10
HEADER_TRAILER = b"\xff\x00"
ASCII_DATA_BLOCK_SIZE = 256
MAX_CAS_SIZE = 16 * 1024 * 1024
MAX_LINE_NUMBER = 65529
LINE_PATTERN = re.compile(r"^\s*(\d+)\s*(.*)$")


def read_source(path: Path) -> str:
    raw = path.read_bytes()
    try:
        return raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        try:
            return raw.decode("cp1252")
        except UnicodeDecodeError as error:
            raise ValueError("The TXT file is neither UTF-8 nor Windows-1252.") from error


def parse_listing(text: str) -> list[str]:
    lines: list[str] = []
    previous_number = -1

    for source_line, raw_line in enumerate(text.splitlines(), start=1):
        if not raw_line.strip():
            continue
        raw_line = raw_line.replace("\t", " ")
        match = LINE_PATTERN.fullmatch(raw_line)
        if match is None:
            raise ValueError(f"Line {source_line}: a BASIC line number is expected.")

        line_number = int(match.group(1))
        if line_number > MAX_LINE_NUMBER:
            raise ValueError(
                f"Line {source_line}: line number {line_number} is greater than "
                f"{MAX_LINE_NUMBER}."
            )
        if line_number <= previous_number:
            raise ValueError(
                f"Line {source_line}: BASIC line numbers must be unique "
                "and in ascending order."
            )

        content = match.group(2).rstrip()
        if not content:
            raise ValueError(f"Line {source_line}: BASIC code is missing after the line number.")
        basic_line = f"{line_number} {content}"
        try:
            encoded_line = basic_line.encode("ascii")
        except UnicodeEncodeError as error:
            raise ValueError(
                f"Line {source_line}: SVI BASIC listings may contain ASCII characters only."
            ) from error
        if len(encoded_line) > 255:
            raise ValueError(f"Line {source_line}: a BASIC line may be at most 255 bytes long.")
        if any(byte < 0x20 or byte > 0x7E for byte in encoded_line):
            raise ValueError(
                f"Line {source_line}: control characters are not allowed in the BASIC listing."
            )

        lines.append(basic_line)
        previous_number = line_number

    if not lines:
        raise ValueError("The TXT file contains no numbered BASIC lines.")
    return lines


def default_program_name(source_path: Path) -> str:
    name = re.sub(r"[^A-Z0-9]", "", source_path.stem.upper())
    return name[:6] or "PROGRA"


def validate_program_name(name: str) -> bytes:
    if not 1 <= len(name) <= 6 or not name.isascii() or not name.isalnum():
        raise ValueError("The BASIC file name must consist of 1 to 6 ASCII letters or digits.")
    return name.encode("ascii").ljust(6, b" ")


def build_cas(text: str, program_name: str) -> bytes:
    name_bytes = validate_program_name(program_name)
    listing = parse_listing(text)
    program_data = ("\r\n".join(listing) + "\r\n").encode("ascii")
    # ASCII files end with 1Ah (EOF); the last block is padded with it to full size.
    program_data += b"\x1a" * (ASCII_DATA_BLOCK_SIZE - len(program_data) % ASCII_DATA_BLOCK_SIZE)
    header_data = ASCII_FILE_TYPE + name_bytes + HEADER_TRAILER
    data_blocks = b"".join(
        BLOCK_HEADER + program_data[offset:offset + ASCII_DATA_BLOCK_SIZE] + b"\x00"
        for offset in range(0, len(program_data), ASCII_DATA_BLOCK_SIZE)
    )
    image = BLOCK_HEADER + header_data + data_blocks
    if len(image) > MAX_CAS_SIZE:
        raise ValueError(f"The CAS image exceeds {MAX_CAS_SIZE // (1024 * 1024)} MiB.")
    return image


def convert_file(source_path: Path, output_path: Path, program_name: str) -> int:
    if source_path.resolve() == output_path.resolve():
        raise ValueError("Input and output file must not be the same.")
    image = build_cas(read_source(source_path), program_name)
    output_path.write_bytes(image)
    return len(image)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Converts a numbered SVI BASIC TXT listing into a raw SVI CAS image."
    )
    parser.add_argument("txt", type=Path, help="TXT file with numbered BASIC lines")
    parser.add_argument(
        "-o", "--output", type=Path, help="CAS output file (default: same name as the TXT file)"
    )
    parser.add_argument(
        "-n", "--name", help="BASIC cassette name (1-6 ASCII letters/digits; default: TXT file name)"
    )
    args = parser.parse_args(argv)

    output_path = args.output or args.txt.with_suffix(".cas")
    program_name = args.name or default_program_name(args.txt)
    try:
        size = convert_file(args.txt, output_path, program_name)
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    print(f"CAS written: {output_path} ({size} bytes, BASIC name: {program_name})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
