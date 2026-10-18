# Alien 8 for MSX

MSX version of Alien 8 (Ultimate, 1985) for the SVI-318/328. The recipe builds a plain 32 KiB cartridge
(`4000h`-`BFFFh`, no mapper) from the original cassette `Alien 8 (1985)(Ultimate).cas`. The game needs
64 KB of RAM; insert the ROM as a module and the game starts after switching on.

```powershell
python port.py alien8 --cas "Alien 8 (1985)(Ultimate).cas"
```

## Parts

| File | Content |
| --- | --- |
| [recipe.py](recipe.py) | Builds the memory image from the cassette, applies the patch table, assembles and writes the ROM |
| [A8ROM.ASM](A8ROM.ASM) | ROM with `AB` header; the initialization at `4010h` switches page 2 to the ROM, determines the slot settings and starts the copy code |
| [A8COPY.ASM](A8COPY.ASM) | Copy code at `E000h`: copies helper block and game through a buffer in page 0 into RAM, clears `B34Ah`-`BFFFh`, selects joystick port 1 and jumps to `4000h` |
| [A8HELP.ASM](A8HELP.ASM) | Helper block at `0200h`: SVI keyboard rows from the MSX matrix (`SVIROW`), shadow value of SVI PPI port C |

**Memory image:** the SVI version loads two BLOAD files to `8700h`. The first copies `8700h`-`C7FFh` to
`3F00h`-`7FFFh` of the lower RAM bank, the second `8800h`-`C7FFh` to `8000h`-`BFFFh` and starts the game at
`4000h` with RAM in all pages. The ROM holds the patched image `3F00h`-`B349h`; the rest up to `BFFFh` is
zeros, which the copy code clears.

**Hardware changes** (patch table, every site with its expected original bytes):

| SVI-318/328 | MSX |
| --- | --- |
| VDP `OUT (80h)`/`OUT (81h)`, `IN (85h)`, `LD C,80h`/`81h` for `OUT (C)` | `98h`/`99h` |
| PSG `OUT (88h)`, `OUT (8Ch)`, `IN (90h)` | `A0h`, `A1h`, `A2h` |
| trigger `IN A,(98h)` | `IN A,(A2h)`, PSG register 14 bit 4 |
| PSG register 7 with `OR F0h` | `OR B0h`, port A stays input |
| keyboard `OUT (96h)` + `IN (99h)`, read back `IN (96h)` | helper block (`KEYR`, `KEYC`, `KEYS`) |
| shift tables `F200h`-`FFFFh` | `2200h`-`2FFFh`, so that the game never writes `FFFFh` (secondary slot register) |

The game runs with interrupts disabled and calls no ROM routines; the MSX BIOS is not used after the start.

**Controls:** in the menu `1` selects keyboard, `2` joystick, `3` directional control, `0` starts. Keyboard
mode with the letter keys as on the SVI-328, joystick 1 with trigger A.
