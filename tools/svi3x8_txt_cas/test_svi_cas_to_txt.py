from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from svi_cas_to_txt import (
    SYNC,
    TYPE_ASCII,
    TYPE_TOKENIZED,
    convert_file,
    detokenize_program,
    read_programs,
)
from svi_txt_to_cas import build_cas


def tokenized_cas(program: bytes, name: bytes = b"TEST  ") -> bytes:
    return SYNC + b"\xd3" * 10 + name + b"\xff\x00" + SYNC + program + b"\x00" * 12


# Line 20 contains a single-precision constant whose mantissa holds 00h bytes.
SAMPLE = (
    b"\x0b\x80\x0a\x00\xbd\x0f\x0f\x2c\x12\x00"
    b"\x1f\x80\x14\x00\x91\x20\x22A\x22\x3b\xff\x86\x28\xf4\x1d\x41\x15\x00\x00\x29\x00"
    b"\x28\x80\x1e\x00\x89\x20\x1c\x00\x01\x00"
    b"\x00\x00"
)


class SviCasToTxtTests(unittest.TestCase):
    def test_detokenizes_csave_program(self) -> None:
        self.assertEqual(
            detokenize_program(SAMPLE),
            '10 COLOR15,1\r\n20 PRINT "A";ABS(-1.5)\r\n30 GOTO 256\r\n',
        )

    def test_reads_tokenized_header_and_name(self) -> None:
        programs = read_programs(tokenized_cas(SAMPLE, b"DEMO  "))
        self.assertEqual(len(programs), 1)
        self.assertEqual((programs[0].name, programs[0].file_type), ("DEMO", TYPE_TOKENIZED))

    def test_rem_data_and_else_handling(self) -> None:
        line = (
            b"\x0b\x80\x01\x00\x8f\x20X:PRINT\x00"
            b"\x0b\x80\x02\x00\x84\x31\x2c\x22\x3a\x22\x3a\x95\x00"
            b"\x0b\x80\x03\x00\x8b\x20\x11\xda\x3a\xa1\x12\x00"
            b"\x00\x00"
        )
        self.assertEqual(
            detokenize_program(line),
            '1 REM X:PRINT\r\n2 DATA1,":":ON\r\n3 IF 0THENELSE1\r\n',
        )

    def test_ascii_roundtrip_with_converter(self) -> None:
        source = '10 PRINT "HALLO"\r\n20 GOTO 10\r\n'
        programs = read_programs(build_cas(source, "ROUND"))
        self.assertEqual(programs[0].file_type, TYPE_ASCII)
        self.assertEqual(programs[0].name, "ROUND")
        self.assertEqual(programs[0].text, source)

    def test_convert_file_writes_txt(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            cas = Path(folder) / "demo.cas"
            cas.write_bytes(tokenized_cas(SAMPLE))
            written = convert_file(cas)
            self.assertEqual(written[0][0], cas.with_suffix(".txt"))
            self.assertTrue(cas.with_suffix(".txt").read_text(encoding="utf-8").startswith("10 COLOR15,1"))

    def test_rejects_non_svi_image(self) -> None:
        with self.assertRaises(ValueError):
            read_programs(b"\x1f\xa6\xde\xba\xcc\x13\x7d\x74" + b"\x00" * 32)


if __name__ == "__main__":
    unittest.main()
