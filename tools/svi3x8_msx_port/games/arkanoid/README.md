# Arkanoid for MSX

MSX version of Arkanoid (Taito) from the SVI-318/328 cassette. The recipe builds a plain 32 KiB cartridge
(`4000h`-`BFFFh`, no mapper) from the original cassette `Arkanoid (19xx)(Taito).cas`. No assembler is needed.

```powershell
python port.py arkanoid --cas "Arkanoid (19xx)(Taito).cas"
```

## Parts

| File | Content |
| --- | --- |
| [recipe.py](recipe.py) | Takes the cartridge image from the cassette, applies the patch table and writes the ROM |

**Memory image:** the SVI version loads three BLOAD files to `9000h` and copies them to `0000h` (SVI BIOS),
`4000h` and `8000h`. `4000h`-`BFFFh` is an MSX cartridge image with `AB` header and initialization at `402Ah`.
It uses the BIOS routines and takes the VDP ports from `0006h`/`0007h`.

**Changes** (patch table, every site with its expected original bytes):

| SVI version | MSX |
| --- | --- |
| `4030h`: 8 × `NOP` | `CALL 4010h`, `LD H,80h`, `CALL 0024h`: also enables the cartridge in page 2 |
| PSG `OUT (88h)`, `OUT (8Ch)`, `IN (90h)` | `A0h`, `A1h`, `A2h` |

**Controls:** space starts and fires, cursor left/right move the paddle, joystick 1 works as well. The SVI
version does not support the Arkanoid paddle controller.
