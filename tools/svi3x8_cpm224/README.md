# SVI3x8-CPM224

Builds a bootable Spectravideo CP/M 2.24 system disk for the SVI-328 with an SVI-605B: `CPM224SS.DSK`
(single-sided, 172032 bytes).

## Sources

| Part | Source | Address |
| --- | --- | --- |
| BIOS | Spectravideo CP/M BIOS revision 2.24 (H. Kawazoe, J. Suzuki, T. Kennedy, M.H. Kam), MAC syntax with Z80.LIB. **Not included**: Spectravideo code, supplied by you (see [What You Supply](#what-you-supply)) | `E600h` |
| CCP, BDOS | [src/cpm22/OS2CCP.ASM](src/cpm22/OS2CCP.ASM), [src/cpm22/OS3BDOS.ASM](src/cpm22/OS3BDOS.ASM): CP/M 2.2 by Digital Research, from [src/cpm22/cpm2-plm.zip](src/cpm22/cpm2-plm.zip) of the Unofficial CP/M Web Site | `D000h`, `D800h` |
| Boot sector | Track 0, sector 1 of a Spectravideo CP/M 2.27 disk for the SVI-608M, changed to load the BIOS to `E600h`. **Not included**: supplied by you | `C100h` |
| Programs | [src/files](src/files): ASM, DDT, DUMP, ED, LOAD, PIP, STAT, SUBMIT, XSUB from the same archive | |
| FORMAT.COM | [src/FORMAT.ASM](src/FORMAT.ASM), assembled with pasmo: formats a single-sided disk in drive A: or B: (FM track 0 with 18 x 128 bytes, 39 MFM tracks with 17 x 256 bytes, `E5h`, interleave 1,10,2,11,... as MFORMAT of CP/M 2.27) and ends with a warm boot | |

The system is a 59K CP/M (`MSIZE` 59): CCP `D000h`, BDOS entry `D806h`, BIOS `E600h`.

## What You Supply

| File | Content | SHA1 of the files this was built with |
| --- | --- | --- |
| `SVI328BIOS224.ASM` | Spectravideo CP/M BIOS source, revision 2.24 (header "BIOS for Spectravideo CP/M - 11/29/83") | `d513bbb7caad3a6149387f8b69f5c372d2656b22` |
| `CPM227DS.DSK` | Spectravideo CP/M 2.27 system disk for the SVI-608M (double-sided image, 346112 bytes); only its boot sector is used | `f85b50ab5881527b718519e9c4e7f0281d3321bb` |

With exactly these two files, the build writes a disk with the SHA1 `ea8521f8be0e813e8c60970f0febc9f343d54b64`.

## Build

```powershell
python build.py --bios <path>\SVI328BIOS224.ASM --boot-disk <path>\CPM227DS.DSK --output CPM224SS.DSK
```

Without `--bios` and `--boot-disk` the script looks for both files in `src`; without `--output` it writes
`CPM224SS.DSK` next to itself. It needs Python 3, [zmac](http://48k.ca/zmac.html) and
[pasmo](https://pasmo.speccy.org/) (on the PATH, or given with `--zmac` and `--pasmo`). It:

1. Clears bit 7 of every source byte (WordStar marks single characters with it).
2. Splits the `!`-separated statements of the Digital Research sources; outside quotes `!` also ends a comment, so `cpi intlen ;done?! rnc` contains an `RNC`.
3. Assembles CCP and BDOS in zmac's 8080 mode.
4. Assembles the BIOS in zmac's normal mode, which accepts 8080 and Z80.LIB mnemonics together. Four 8080 mnemonics name a different Z80 instruction there and are written in their Z80 form first: `CPI n` to `CP n`, `RLC` to `RLCA`, `RRC` to `RRCA`, `JP addr` (8080: jump if plus) to `JP P,addr`. Labels start in column 1.
5. Puts the BIOS's two CCP patches into the CCP: the jumps at `CCP+4B0h` and `CCP+4E5h` replace `push d; ani 11b` and `push psw; cpi 3` of the directory listing (2 names per line on the 40-column screen, 4 on 80 columns). The script checks that the CCP holds these instructions there.
6. Changes the boot sector to load 17 FM sectors and 4 MFM sectors to `E600h` and to jump there.
7. Assembles FORMAT.ASM with pasmo.
8. Writes the disk image and the programs with their directory entries.

The intermediate files (assembler sources, listings, HEX files, `FORMAT.COM`) go to `build`.

## Disk Layout

| Track | Sectors | Content |
| --- | --- | --- |
| 0 | 1 (FM, 128 bytes) | Boot sector; the BASIC ROM loads it to `C100h` and jumps there. It selects RAM bank 21, loads the BIOS and jumps to `E600h`. |
| 0 | 2-18 (FM) | BIOS `E600h`-`EE7Fh` |
| 1 | 1-4 (MFM, 256 bytes) | BIOS `EE80h`-`F27Fh` |
| 1-2 | track 1 sectors 13-17, track 2 sectors 1-17 | CCP and BDOS `D000h`-`E5FFh`, read by the BIOS at cold and warm boot |
| 3-39 | all | Directory (64 entries, 2 KiB) and 155 KiB of data in 1 KiB blocks |

The sectors of each track follow in the order 1, 2, 3, ... as on the original disks. Consecutive sectors therefore each cost about one revolution; copying a 6 KiB file with PIP takes about 25 seconds. The keyboard is not read while the BIOS accesses the disk.

## Use

Insert `CPM224SS.DSK` in drive A of the SVI-605B and reset. The BIOS reports `Spectravideo CP/M-80 Revision 2.24`, the CCP shows `A>`. `FORMAT` (also on key F9) formats a disk in A: or B:; an empty drive gets a blank disk from the SVI-605B. The original Spectravideo FORMAT.COM is not available; MFORMAT of CP/M 2.27 calls fixed addresses of the 2.27 BIOS and does not run here.

The SVI-328 emulator's tests boot this disk to `A>`, check that `DIR` lists `STAT.COM` and format the empty drive B with `FORMAT` (blank disk, `E5h` throughout).

## License

`build.py`, `src/FORMAT.ASM` and this README are under the license of this repository. CP/M 2.2 (`src/cpm22`, `src/files`) is used under the grant of DRDOS, Inc. of 7 July 2022 ("a right to use, distribute, modify, enhance, and otherwise make available in a nonexclusive manner CP/M and its derivatives"), published on the Unofficial CP/M Web Site. The Spectravideo BIOS and boot sector are not part of this folder, nor is a built disk image, since it contains both.
