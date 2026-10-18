# Emulators

Four Windows emulators of the machines behind the MSXRetroThings hardware, written in C++17. They step the
Z80 T-state by T-state and run every chip on the same master clock, so that cards, ROM code and software
can be tried at the timing of the real machine.

| Folder | Content |
| --- | --- |
| [svi318/](svi318/README.md) | Spectravideo SVI-318 (16 KiB RAM, BASIC 1.11) with expanders SVI-601, SVI-602, SVI-605B (also with hard disk), SVI-608M and their cards |
| [svi328/](svi328/README.md) | Spectravideo SVI-328 (64 KiB RAM, BASIC 1.11), the same expanders plus the SVI-609 Master Station |
| [svi728/](svi728/README.md) | Spectravideo SVI-728 (MSX1) with cassette, modules and printer |
| [vg8235/](vg8235/README.md) | Philips VG 8235/00 (MSX2) with V9938, disk drive, real-time clock, cassette, modules and printer |
| [svi3x8/](svi3x8/README.md) | Shared by SVI-318 and SVI-328: expansion interface, expanders and cards |
| [chips/](chips/README.md) | Shared by all four: one library per chip (Z80, 8255, AY-3-8910, TMS9918A/9929A, V9938, FD1793, WD2793, ...) and the video recorder |
| [msx/](msx/README.md) | Shared by SVI-728 and VG-8235: the MSX cartridge slot with its mappers |

Each emulator folder holds `Development` (Visual Studio solution, machine, frontend, tests, installer
source; see its developer guide) and `Distribution` (user manual `MANUAL.md` and `INSTALLATION.txt`).

## Requirements

- Windows 10 or 11, 64-bit; the system requirements of each emulator are in its manual.
- To build: Visual Studio 2022 with **Desktop development with C++** (MSVC v143, Windows SDK).

## Building and Testing

Open `<emulator>/Development/<NAME>.sln` in Visual Studio, or in a PowerShell in that folder:

```powershell
.\run_tests.ps1 -Build -Configuration Release
```

The solution builds the shared libraries as well; the output goes to `<emulator>/Build`. `create_installation.ps1 -NoRoms`
builds a setup without ROM images.

## ROM Images

The original ROM images are not part of this repository. Each `rom` folder has a README with the file
names, sizes and SHA1 checksums the emulator expects, and each manual lists where files with the same
content are known. Without ROM images the emulators build, the tests that need a ROM report `SKIPPED`, and
the programs tell which file is missing.

## License

Software under the MIT license of this repository; the Z80 core in `chips/Z80CPU/third_party/chips` keeps
its zlib license (see [LICENSE.txt](../LICENSE.txt)).
