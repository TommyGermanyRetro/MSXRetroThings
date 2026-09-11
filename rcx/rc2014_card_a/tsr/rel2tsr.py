"""
REL-to-TSR packer: replaces the lost LT.COM (LinkTsr) tool for building MemMan TSR files.

Parses a MACRO-80 / LINK-80 relocatable (.REL) file (bit-stream format documented at
https://github.com/Konamiman/Nestor80/blob/master/docs/RelocatableFileFormat.md, the same
format Nestor80 writes for M80-relocatable builds), reconstructs the assembled code-segment
image (as if linked at address 0), and records every "relocatable value in code segment" item's
offset. Then emits a .TSR file: the assembled bytes with a 2-byte-length-prefixed relocation
table (entries = those offsets, terminated by 0x0000) spliced in right after byte 35 (the end of
the fixed 36-byte MST TSR header) - the exact format verified by disassembling the real tl.com
v2.42 loader (see project_memman_tl_disassembly.md).
"""
import sys


class BitReader:
    def __init__(self, data: bytes):
        self.data = data
        self.byte_pos = 0
        self.bit_pos = 0  # 0..7, 0 = MSB of current byte

    def read_bit(self) -> int:
        if self.byte_pos >= len(self.data):
            raise EOFError("unexpected end of REL file")
        byte = self.data[self.byte_pos]
        bit = (byte >> (7 - self.bit_pos)) & 1
        self.bit_pos += 1
        if self.bit_pos == 8:
            self.bit_pos = 0
            self.byte_pos += 1
        return bit

    def read_bits(self, n: int) -> int:
        v = 0
        for _ in range(n):
            v = (v << 1) | self.read_bit()
        return v

    def align_byte(self):
        if self.bit_pos != 0:
            self.bit_pos = 0
            self.byte_pos += 1

    def at_eof(self) -> bool:
        return self.byte_pos >= len(self.data)


SEG_ABS, SEG_CODE, SEG_DATA, SEG_COMMON = 'abs', 'code', 'data', 'common'
SEG_BITS = {0b01: SEG_CODE, 0b10: SEG_DATA, 0b11: SEG_COMMON}


class Segment:
    def __init__(self):
        self.image = bytearray()
        self.reloc_offsets = []  # offsets (into self.image) of 2-byte relocatable values

    def ensure(self, upto):
        if len(self.image) < upto:
            self.image.extend(b'\x00' * (upto - len(self.image)))

    def write_byte(self, pos, val):
        self.ensure(pos + 1)
        self.image[pos] = val & 0xFF

    def write_word_reloc(self, pos, val):
        self.ensure(pos + 2)
        self.image[pos] = val & 0xFF
        self.image[pos + 1] = (val >> 8) & 0xFF
        self.reloc_offsets.append(pos)


def parse_rel(data: bytes, verbose=False):
    # Skip Nestor80 extended-format header if present (16 fixed bytes)
    EXT_MAGIC = bytes([0x85, 0xD3, 0x13, 0x92, 0xD4, 0xD5, 0x13, 0xD4,
                        0xA5, 0x00, 0x00, 0x13, 0x8F, 0xFF, 0xF0, 0x9E])
    if data[:16] == EXT_MAGIC:
        if verbose:
            print("Extended (Nestor80) REL header detected, skipping 16 bytes")
        data = data[16:]

    br = BitReader(data)
    segs = {SEG_CODE: Segment(), SEG_DATA: Segment()}
    common_segs = {}
    cur_seg = SEG_CODE
    cur_common_name = None
    lc = 0  # location counter within current segment
    symbols = {}  # name -> (seg, value) for "define public symbol" items
    program_name = None
    ended = False

    def cur_segment_obj():
        if cur_seg == SEG_COMMON:
            return common_segs.setdefault(cur_common_name, Segment())
        return segs[cur_seg]

    def read_symbol_bytes():
        legacy_len = br.read_bits(3)
        raw = bytes(br.read_bits(8) for _ in range(legacy_len))
        if legacy_len >= 2 and raw[0] == 0xFF:
            # extended symbol bytes field (Nestor80 extended REL format)
            len_bytes = raw[1:]
            actual_len = int.from_bytes(len_bytes, 'little')
            if actual_len >= 256:
                br.align_byte()
            return bytes(br.read_bits(8) for _ in range(actual_len))
        return raw

    while not ended:
        if br.at_eof():
            break
        marker = br.read_bit()
        if marker == 0:
            # absolute byte
            byte = br.read_bits(8)
            cur_segment_obj().write_byte(lc, byte)
            lc += 1
            continue

        # marker == 1: next 2 bits are a segment code. "00" means this is actually the
        # start of a link item ('100' prefix); '01'/'10'/'11' means a relocatable value
        # in the code/data/common segment respectively (both forms share the '1 0' start,
        # they only diverge on the second segment bit - '1 0 0' link item vs '1 0 1' code
        # segment relocatable value).
        segcode = br.read_bits(2)
        if segcode != 0b00:
            lo = br.read_bits(8)
            hi = br.read_bits(8)
            val = lo | (hi << 8)
            segname = SEG_BITS.get(segcode, SEG_ABS)
            if segname == cur_seg and segname in (SEG_CODE, SEG_DATA):
                cur_segment_obj().write_word_reloc(lc, val)
            elif segname == SEG_COMMON and cur_seg == SEG_COMMON:
                cur_segment_obj().write_word_reloc(lc, val)
            else:
                # relocatable value referencing a different segment than current LC segment
                # (rare for our simple TSR sources) - store as absolute low/high for now.
                cur_segment_obj().write_byte(lc, lo)
                cur_segment_obj().write_byte(lc + 1, hi)
                if verbose:
                    print(f"note: cross-segment relocatable value seg={segname} "
                          f"(cur_seg={cur_seg}) at lc={lc:#x}")
            lc += 2
            continue
        else:
            item_type = br.read_bits(4)

            has_value = item_type in range(5, 15)  # 5-14 per spec table (5-7 addr+sym, 8-14 addr only)
            has_symbol = item_type in list(range(0, 5)) + list(range(5, 8))  # 0-4 sym only, 5-7 addr+sym

            value = None
            if has_value:
                segbits = br.read_bits(2)
                lo = br.read_bits(8)
                hi = br.read_bits(8)
                value = (SEG_BITS.get(segbits, SEG_ABS), lo | (hi << 8))

            symbol = None
            if has_symbol:
                symbol = read_symbol_bytes()

            if item_type == 0:
                pass  # declare symbol - ignore (name in `symbol`)
            elif item_type == 1:
                cur_seg = SEG_COMMON
                cur_common_name = symbol.decode('ascii', 'backslashreplace')
                lc = 0
            elif item_type == 2:
                program_name = symbol.decode('ascii', 'backslashreplace') if symbol else None
                if verbose:
                    print(f"Program name: {program_name}")
            elif item_type == 3:
                pass  # library search request - ignore
            elif item_type == 4:
                # extension link item; symbol[0] is ext type
                if symbol and len(symbol) >= 1:
                    ext_type = symbol[0]
                    if verbose:
                        print(f"note: extension link item {ext_type:#x} ignored "
                              f"(external-symbol expressions not supported by this simple packer)")
                else:
                    raise NotImplementedError("extension link item with no symbol bytes")
            elif item_type == 5:
                pass  # define size of COMMON block - informational only
            elif item_type == 6:
                raise NotImplementedError("chain external - external symbols not supported")
            elif item_type == 7:
                name = symbol.decode('ascii', 'backslashreplace') if symbol else '?'
                symbols[name] = value
                if verbose:
                    print(f"Public symbol: {name} = {value}")
            elif item_type in (8, 9):
                raise NotImplementedError("external symbol reference items not supported")
            elif item_type == 10:
                pass  # define size of data segment - informational
            elif item_type == 11:
                segname, addr = value
                if segname == SEG_COMMON:
                    # shouldn't happen without a preceding 'select common' but handle gracefully
                    lc = addr
                else:
                    cur_seg = segname
                    lc = addr
                if verbose:
                    print(f"Set LC: seg={segname} addr={addr:#x}")
            elif item_type == 12:
                raise NotImplementedError("chain address - not supported")
            elif item_type == 13:
                pass  # define size of code segment - informational
            elif item_type == 14:
                br.align_byte()
                if verbose:
                    print(f"End of program (arg={value})")
            elif item_type == 15:
                ended = True
                if verbose:
                    print("End of file")
            else:
                raise NotImplementedError(f"unknown link item type {item_type}")

    return {
        'code': segs[SEG_CODE],
        'data': segs[SEG_DATA],
        'common': common_segs,
        'symbols': symbols,
        'program_name': program_name,
    }


# Empirically confirmed (byte-diff against the real 1993-compiled savscr.tsr, built by the
# real LT.COM from the real unmodified savscr.mac): LT.COM links every MemMan TSR as if the
# whole module (byte 0 = the "MST TSR" magic) were loaded at address #4000 - the start of
# page 1, where TsrLoad/tl.com places TSR segments ("on every random place in page 1, #4000
# to #7FFF" per the MemMan docs). Every relocatable (code-segment) 16-bit value anywhere in
# the module - including the header's Base/Init/Kill/Talk fields and the Hooks: table's
# handler-address entries - gets this link base added directly into its stored value.
LINK_BASE = 0x4000
HEADER_LEN = 36


def build_tsr(code_image: bytearray, reloc_offsets, out_path):
    image = bytearray(code_image)
    reloc_offsets = sorted(set(reloc_offsets))

    def read_le16(off):
        return image[off] | (image[off + 1] << 8)

    def write_le16(off, val):
        image[off] = val & 0xFF
        image[off + 1] = (val >> 8) & 0xFF

    # Header field layout: DW version,Base,Init,Kill,Talk,TsrLen,IniLen (offsets 22..35)
    raw_base = read_le16(24)
    raw_init = read_le16(26)
    raw_inilen = read_le16(34)

    # Only offsets that fall within the assembled Base..(Init+IniLen) range get a runtime
    # REL-table entry - tl.com relocates the header fields (Base/Init/Kill/Talk) and the
    # Hooks: table's handler addresses through its own separate, dedicated logic instead
    # (confirmed: neither appear in the real file's table, both still carry the same +LINK_BASE
    # baked-in value as everything else).
    table_offsets = [off for off in reloc_offsets if raw_base <= off < (raw_init + raw_inilen)]

    for off in reloc_offsets:
        write_le16(off, read_le16(off) + LINK_BASE)

    # No explicit 0x0000 terminator is stored in the file itself - confirmed by byte-diffing
    # against the real savscr.tsr (its table length field covers exactly the real entries, no
    # extra pair). tl.com appends the terminator itself in RAM after reading the table (traced
    # in the disassembly at tl.com #0520-0523), so the loader never reads past what's declared
    # by the length field regardless.
    table_entries = bytearray()
    for off in table_offsets:
        table_entries += (off + LINK_BASE).to_bytes(2, 'little')

    table_len_field = (len(table_entries) + 2).to_bytes(2, 'little')

    out = bytearray()
    out += image[:HEADER_LEN]
    out += table_len_field
    out += table_entries
    out += image[HEADER_LEN:]

    with open(out_path, 'wb') as f:
        f.write(out)
    return out


if __name__ == '__main__':
    rel_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else rel_path.rsplit('.', 1)[0] + '.tsr'
    verbose = '-v' in sys.argv

    with open(rel_path, 'rb') as f:
        data = f.read()

    result = parse_rel(data, verbose=verbose)
    code = result['code']
    print(f"Code segment image length: {len(code.image)} bytes")
    print(f"Relocatable (code-seg) value slots: {len(code.reloc_offsets)}")
    if verbose:
        print("Reloc offsets:", sorted(code.reloc_offsets))
    print(f"Header (first 36 bytes): {bytes(code.image[:36])}")

    build_tsr(bytes(code.image), code.reloc_offsets, out_path)
    print(f"Wrote {out_path}")
