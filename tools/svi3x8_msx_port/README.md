# SVI3x8-MSX-PORT

Ports Spectravideo SVI-318/328 cassette games to MSX: from the original SVI cassette image it builds MSX
cassette files (`BLOAD"CAS:",R`) and cartridge ROMs. The SVI-318/328 is the predecessor of MSX and has the
same CPU, video chip and sound chip, but at other I/O ports and with another keyboard and joystick
wiring; a port moves every hardware access to the MSX ports and replaces keyboard and joystick reading
with small helper routines.

| Game | Recipe | Output |
| --- | --- | --- |
| Alien 8 (Ultimate, 1985) | [games/alien8](games/alien8/README.md) | 32 KiB ROM |
| Arkanoid (Taito) | [games/arkanoid](games/arkanoid/README.md) | 32 KiB ROM |
| Armoured Assault (Spectravideo, 1983) | [games/armoured](games/armoured/README.md) | cassette and 16 KiB ROM |
| Donkey Kong (Buggy Software) | [games/donkey](games/donkey/README.md) | 64 KiB ROM with ASCII16 mapper |
| Spectron (Spectravideo, 1983) | [games/spectron](games/spectron/README.md) | cassette and 16 KiB ROM |

## Usage

```powershell
python port.py --list
python port.py spectron --cas "Spectron (1983)(Spectravideo).cas" --rom-dir out --cas-dir out
python port.py all --source-dir <folder with the SVI cassettes> --rom-dir out --cas-dir out
```

`--list` shows each recipe with the cassette file it expects and the files it writes. With `--source-dir`
(may be given several times) the tool looks for that file name in the folders. `--pasmo` names the
assembler when pasmo is not on the PATH; `--work <folder>` keeps the work files (assembler sources,
generated includes, intermediate binaries) instead of a temporary folder.

Needs Python 3 and [pasmo](https://pasmo.speccy.org/). The original cassettes are not part of this tool:
each port is a description of changes, applied to your own copy of the game.

## How a Port Works

Each recipe in `games/<name>/recipe.py`:

1. takes the cassette apart (`sviport.svi_blocks`) and builds the memory image the SVI loader leaves behind,
   checking the file names and load addresses on the way;
2. applies its patch table: every entry is (address, original bytes, replacement bytes), and the build stops
   when an original byte sequence is not found, so a different version of the game is never patched blindly;
3. assembles its helper block and startup code with pasmo and writes the cassette and cartridge files.

| SVI-318/328 | MSX | Typical change |
| --- | --- | --- |
| VDP `80h`/`81h` (write), `84h`/`85h` (read) | `98h`/`99h` | port numbers in `OUT (n)`, `IN (n)`, `LD C,n` |
| PSG `88h` (address), `8Ch` (write), `90h` (read) | `A0h`, `A1h`, `A2h` | port numbers |
| PSG register 7 with port A as output | port A stays input | mask in the value written to R7 |
| PPI control word `OUT (97h)` | not needed | removed |
| keyboard: row select `OUT (96h)`, read `IN (99h)` | other matrix, `AAh`/`A9h` | `CALL` to a helper routine that builds the SVI row from the MSX matrix |
| joysticks and triggers in PSG R14 and PPI port A | PSG R14/R15 | helper routines |
| memory up to `FFFFh` | `FFFFh` is the secondary slot register | stack, tables or copies moved below `FFFFh` |

Shared parts: [sviport.py](sviport.py) (cassette blocks, patch engine, pasmo, MSX cassette and cartridge
files) and [templates](templates) with the boot code (`BOOT.ASM`) and the 16 KiB cartridge (`ROM16.ASM`) for
games that run at `C000h` with a page-0 helper block. Helper blocks, which read keyboard and joysticks the way
each game expects, stay with the game.

## Finding the Hardware Accesses: svi_scan.py

```powershell
python svi_scan.py "Alien 8 (1985)(Ultimate).cas"                  # files of the cassette
python svi_scan.py "Alien 8 (1985)(Ultimate).cas" --block 3          # SVI hardware accesses in block 3
python svi_scan.py "Alien 8 (1985)(Ultimate).cas" --block 3 --base 3F00 --patches
```

Without `--block` the scanner lists the files of the cassette: header blocks with type and name, data blocks
with their BLOAD start, end and entry address. With `--block` it searches that block for `OUT (n),A` and
`IN A,(n)` on SVI ports, for `LD C,n` and `LD BC,nnpp` with an SVI port followed by `OUT (C)`, `IN (C)` or a
block instruction, and for `LD SP,0000h`. Each hit shows the SVI device and the MSX change: a new port number,
or a helper routine (keyboard, triggers, PPI). The addresses count from the BLOAD start address; when the
loader moves the code, `--base` gives the address the first byte has at run time (`--skip` sets the bytes to
leave out at the start of the block). `--patches` prints the hits as lines for a patch table: plain port
changes as entries, the rest as comments.

The scanner works on bytes, not on a disassembly. A hit inside a table is possible, and what a game writes
as data (for example the masks it writes to PSG register 7) is not found; every line is a place to look at.
For Spectron and Armoured Assault it finds every site of their patch tables, and for nearly all plain port
changes it proposes exactly the replacement the recipe uses.

## Adding a Game

Start with `svi_scan.py` on the cassette, then create `games/<name>/recipe.py` with `SOURCE` (file name of the original cassette), `OUTPUTS` (file names of
the results), a patch table and `build(cas, asm)` that returns the files as a dictionary; put the game's own
assembler sources next to it. The recipes of the five games show the three layouts: a game image at
`C000h` (Spectron, Armoured Assault), a game for `4000h`-`BFFFh` with RAM in all pages (Alien 8), and a
banked game copied from an ASCII16 ROM (Donkey Kong).
