import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from svi_txt_to_cas import (
    ASCII_FILE_TYPE,
    ASCII_DATA_BLOCK_SIZE,
    BLOCK_HEADER,
    HEADER_TRAILER,
    build_cas,
    convert_file,
    default_program_name,
    parse_listing,
)


class SviTxtToCasTests(unittest.TestCase):
    def test_builds_ascii_svi_cas_with_two_blocks(self) -> None:
        image = build_cas("10 PRINT \"HALLO\"\n20 END\n", "TEST")

        first_block_start = len(BLOCK_HEADER)
        second_header_start = first_block_start + len(ASCII_FILE_TYPE) + 6 + len(HEADER_TRAILER)
        program_start = second_header_start + len(BLOCK_HEADER)
        self.assertEqual(image[:len(BLOCK_HEADER)], BLOCK_HEADER)
        self.assertEqual(image[first_block_start:first_block_start + 10], ASCII_FILE_TYPE)
        self.assertEqual(image[first_block_start + 10:first_block_start + 16], b"TEST  ")
        self.assertEqual(image[first_block_start + 16:second_header_start], HEADER_TRAILER)
        self.assertEqual(image[second_header_start:program_start], BLOCK_HEADER)
        text = b'10 PRINT "HALLO"\r\n20 END\r\n'
        self.assertEqual(image[program_start:], text + b"\x1a" * (ASCII_DATA_BLOCK_SIZE - len(text)) + b"\x00")

    def test_splits_ascii_program_data_into_256_byte_blocks(self) -> None:
        listing = "".join(f"{number} REM {number:03d} " + "X" * 20 + "\n" for number in range(10, 310, 10))
        image = build_cas(listing, "BLOCKS")
        positions: list[int] = []
        offset = 0
        while (offset := image.find(BLOCK_HEADER, offset)) >= 0:
            positions.append(offset)
            offset += len(BLOCK_HEADER)

        payloads = [
            image[positions[index] + len(BLOCK_HEADER):positions[index + 1]]
            for index in range(2, len(positions) - 1)
        ]
        payloads.append(image[positions[-1] + len(BLOCK_HEADER):])
        self.assertGreater(len(payloads), 1)
        self.assertTrue(all(payload.endswith(b"\x00") for payload in payloads))
        self.assertTrue(all(len(payload) == ASCII_DATA_BLOCK_SIZE + 1 for payload in payloads))
        self.assertTrue(payloads[-1].rstrip(b"\x00").endswith(b"\x1a"))

    def test_full_last_block_gets_an_extra_eof_block(self) -> None:
        text = "10 REM " + "X" * 247 + "\n"
        self.assertEqual(len(parse_listing(text)[0]) + 2, ASCII_DATA_BLOCK_SIZE)
        image = build_cas(text, "FULL")
        self.assertTrue(image.endswith(BLOCK_HEADER + b"\x1a" * ASCII_DATA_BLOCK_SIZE + b"\x00"))

    def test_ignores_blank_lines_and_normalizes_tabs(self) -> None:
        self.assertEqual(parse_listing("\n10\tPRINT 1\n\n20 END \t\n"), ["10 PRINT 1", "20 END"])

    def test_derives_a_six_character_program_name(self) -> None:
        self.assertEqual(default_program_name(Path("Hello World.txt")), "HELLOW")
        self.assertEqual(default_program_name(Path("äöü.txt")), "PROGRA")

    def test_rejects_unordered_or_unumbered_lines(self) -> None:
        for listing in ("20 END\n10 END\n", "PRINT 1\n"):
            with self.subTest(listing=listing):
                with self.assertRaises(ValueError):
                    parse_listing(listing)

    def test_rejects_non_ascii_source_and_empty_line_body(self) -> None:
        for listing in ("10 PRINT \"Grüße\"\n", "10\n"):
            with self.subTest(listing=listing):
                with self.assertRaises(ValueError):
                    build_cas(listing, "TEST")

    def test_converts_utf8_text_file(self) -> None:
        with TemporaryDirectory() as directory:
            source = Path(directory) / "demo.txt"
            output = Path(directory) / "demo.cas"
            source.write_text("10 PRINT 1\n", encoding="utf-8")

            size = convert_file(source, output, "DEMO")

            self.assertEqual(size, output.stat().st_size)
            self.assertTrue(output.read_bytes().startswith(BLOCK_HEADER + ASCII_FILE_TYPE))


if __name__ == "__main__":
    unittest.main()
