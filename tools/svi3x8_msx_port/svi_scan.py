"""Lists the files of an SVI-318/328 cassette and the SVI hardware accesses in their code.

    python svi_scan.py <game.cas>                       files of the cassette
    python svi_scan.py <game.cas> --block 3 [--base C000] [--patches]

For one block the scanner looks for the instruction patterns that address SVI hardware and shows
where they are, what they do on the SVI and what an MSX port does with them. With --patches it
prints the entries that only need a new port number as lines for a recipe's patch table. The
scan works on bytes, not on a disassembly: a hit inside data is possible, so every line is a
place to look at, not a verdict. The recipes check their patches against the original bytes.
"""

import argparse
import pathlib
import sys

import sviport

# SVI port -> (device, function, MSX port or None when the access needs more than a new number)
OUT_PORTS = {
    0x80: ("VDP", "data write", 0x98),
    0x81: ("VDP", "register/address write", 0x99),
    0x88: ("PSG", "register select", 0xA0),
    0x8C: ("PSG", "data write", 0xA1),
    0x96: ("PPI", "port C write: keyboard row, cassette", None),
    0x97: ("PPI", "control word", None),
}
IN_PORTS = {
    0x84: ("VDP", "data read", 0x98),
    0x85: ("VDP", "status read", 0x99),
    0x90: ("PSG", "data read", 0xA2),
    0x96: ("PPI", "port C read back", None),
    0x98: ("PPI", "port A read: triggers, cassette", None),
    0x99: ("PPI", "port B read: keyboard columns", None),
    0x9A: ("PPI", "port C read", None),
}
C_PORTS = {port: info for port, info in {**OUT_PORTS, **IN_PORTS}.items()}

# Z80 instructions that use port C (after LD C,n)
C_USERS = {
    bytes([0xED, 0xA3]): "OUTI", bytes([0xED, 0xB3]): "OTIR", bytes([0xED, 0xAB]): "OUTD",
    bytes([0xED, 0xBB]): "OTDR", bytes([0xED, 0xA2]): "INI", bytes([0xED, 0xB2]): "INIR",
    bytes([0xED, 0xAA]): "IND", bytes([0xED, 0xBA]): "INDR",
}
for register, code in zip("BCDEHLA", (0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x79)):
    C_USERS[bytes([0xED, code])] = f"OUT (C),{register}"
    C_USERS[bytes([0xED, code - 1])] = f"IN {register},(C)"
C_REACH = 24                        # bytes after LD C,n in which a port-C instruction counts


def blocks_of(cas: bytes) -> list:
    blocks = sviport.svi_blocks(cas)
    if not blocks:
        raise sviport.PortError("no SVI cassette blocks (16 x 55h + 7Fh) found")
    return blocks


def describe(blocks: list) -> None:
    """One line per block: header blocks with type and name, data blocks with BLOAD addresses."""
    kinds = {0xD0: "binary (BLOAD)", 0xD3: "tokenized BASIC (CLOAD)", 0xEA: "ASCII (LOAD)"}
    previous = None
    for index, block in enumerate(blocks):
        if len(block) >= 16 and block[0:10] == bytes([block[0]] * 10) and block[0] in kinds:
            previous = block[0]
            print(f"{index:3}  header  {kinds[block[0]]:24} name '{block[10:16].decode('ascii', 'replace')}'")
            continue
        line = f"{index:3}  data    {len(block):6} bytes"
        if previous == 0xD0 and len(block) >= 6:
            start, end, entry = (int.from_bytes(block[i:i + 2], "little") for i in (0, 2, 4))
            line += f"  BLOAD {start:04X}h-{end:04X}h, start {entry:04X}h (data from offset 6)"
        print(line)
        previous = None


def scan(data: bytes, base: int) -> list:
    """Returns (address, length, original hex, text, device/function, MSX bytes hex or None)."""
    hits = []
    for offset in range(len(data) - 1):
        code, port = data[offset], data[offset + 1]
        address = base + offset
        if code == 0xD3 and port in OUT_PORTS:
            device, function, msx = OUT_PORTS[port]
            hits.append((address, 2, f"D3{port:02X}", f"OUT ({port:02X}h),A", f"{device} {function}",
                         f"D3{msx:02X}" if msx else None))
        elif code == 0xDB and port in IN_PORTS:
            device, function, msx = IN_PORTS[port]
            hits.append((address, 2, f"DB{port:02X}", f"IN A,({port:02X}h)", f"{device} {function}",
                         f"DB{msx:02X}" if msx else None))
        elif code in (0x0E, 0x01) and port in C_PORTS and offset + (3 if code == 0x01 else 2) <= len(data):
            # LD C,n or LD BC,nnpp: the port goes to C for OUT (C),r, IN r,(C) or a block instruction
            length = 3 if code == 0x01 else 2
            original = data[offset:offset + length].hex().upper()
            text = f"LD C,{port:02X}h" if code == 0x0E else f"LD BC,{data[offset + 2]:02X}{port:02X}h"
            window = data[offset + length:offset + length + C_REACH]
            users = sorted({name for pattern, name in C_USERS.items() if pattern in window})
            device, function, msx = C_PORTS[port]
            if users:
                replacement = (bytes([code, msx]) + data[offset + 2:offset + length]).hex().upper() if msx else None
                hits.append((address, length, original, f"{text} ({', '.join(users)})", f"{device} {function}",
                             replacement))
            else:
                hits.append((address, length, original, text,
                             f"check: no port-C instruction in the next {C_REACH} bytes", None))
        elif code == 0x31 and port == 0x00 and offset + 2 < len(data) and data[offset + 2] == 0x00:
            hits.append((address, 3, "310000", "LD SP,0000h", "stack starts at FFFFh",
                         "31FEFF"))
    return hits


def main() -> int:
    parser = argparse.ArgumentParser(description="Lists the files of an SVI cassette and the SVI hardware accesses in one of its blocks.")
    parser.add_argument("cas", type=pathlib.Path, help="SVI cassette image")
    parser.add_argument("--block", type=int, help="index of the block to scan (see the list without --block)")
    parser.add_argument("--base", type=lambda text: int(text, 16),
                        help="address (hex) of the first scanned byte; default: the BLOAD start address of a "
                             "binary data block, else 0")
    parser.add_argument("--skip", type=lambda text: int(text, 0), default=None,
                        help="bytes to skip at the start of the block (default: 6 for BLOAD data, else 0)")
    parser.add_argument("--patches", action="store_true", help="print patch table lines for the plain port changes")
    args = parser.parse_args()
    try:
        blocks = blocks_of(args.cas.read_bytes())
    except (OSError, sviport.PortError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    if args.block is None:
        describe(blocks)
        return 0
    if not 0 <= args.block < len(blocks):
        parser.error(f"block {args.block} does not exist (0-{len(blocks) - 1})")
    block = blocks[args.block]
    header = blocks[args.block - 1] if args.block > 0 else b""
    bload = len(header) >= 16 and header[0:10] == bytes([0xD0] * 10)
    skip = args.skip if args.skip is not None else (6 if bload else 0)
    base = args.base if args.base is not None else (int.from_bytes(block[0:2], "little") if bload else 0)
    hits = scan(block[skip:], base)

    if args.patches:
        for address, _, original, text, _, msx in hits:
            if msx:
                print(f'    (0x{address:04X}, "{original}", "{msx}"),  # {text}')
            else:
                print(f'    # 0x{address:04X}: {original}  {text}: needs a helper routine')
        return 0
    print(f"block {args.block}: {len(block) - skip} bytes from offset {skip}, base {base:04X}h, {len(hits)} hits")
    for address, _, original, text, function, msx in hits:
        change = f"-> {msx}" if msx else "-> helper routine or removal"
        print(f"  {address:04X}h  {original:8} {text:34} {function:42} {change}")
    plain = sum(1 for hit in hits if hit[5])
    print(f"{plain} plain port changes, {len(hits) - plain} places that need more than a new port number")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
