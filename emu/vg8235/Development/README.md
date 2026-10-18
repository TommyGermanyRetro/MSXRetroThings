# VG-8235 Emulator – Developer Guide

Emulator of the Philips VG 8235/00 (MSX2, PAL) in C++17 as a Visual Studio solution. The machine is built from
component libraries: the chips in `chips` (shared with all emulators, among them the Yamaha V9938,
the RP5C01 clock and the WD2793 disk controller) and the cartridge slot in `msx` (shared with the
SVI-728), plus a Windows frontend and automated tests. Use, technical reference and sources are in the user manual
[MANUAL.md](../Distribution/MANUAL.md); this guide covers the project layout, building, testing and the development
tools.

## Layout

| Path | Contents |
| --- | --- |
| [VG8235.sln](VG8235.sln) | Solution with all libraries, the frontend and the tests (x64, Debug/Release) |
| [Common.props](Common.props) | Settings of the solution: output to `..\Build`, include path of the machine; builds on the settings of `msx` and `chips` |
| [create_installation.ps1](create_installation.ps1) | Builds Release\|x64 and the setup `..\Distribution\VG8235-Setup.exe` (program, system ROMs, manual); with `-NoRoms` the setup without ROMs `..\Distribution\VG8235-Setup-NoRoms.exe`, which creates the empty ROM folder |
| [run_tests.ps1](run_tests.ps1) | Runs all test programs and reports the result of each |
| [create_callgraph.ps1](create_callgraph.ps1) | Builds the function tree with Doxygen and Graphviz into `CallGraph\html` and the entry page `Doxygen_index.html` |
| [CallGraph/Doxyfile](CallGraph/Doxyfile) | Doxygen settings of the function tree: machine, frontend and components without tests, tools and third-party code |
| `Doxygen_index.html` | Function tree, written by `create_callgraph.ps1` (opens `CallGraph/html/index.html`): call and caller graph of every function (SVG, clickable), source with cross-references |
| [installer/](installer/Installer.cs) | Source of the Windows setup and uninstall program |
| `..\Build\vg8235.exe` | Emulator, copied to `..\Build` after every build; next to it `vg8235.cmos` (CMOS of the clock) and `vg8235.ini` (settings, last disk and modules) |
| `VG8235/` | Machine: slot 0 BIOS/BASIC, slots 1/2 modules, slot 3 with secondary slots (3-0 sub-ROM, 3-2 128 KiB memory mapper, 3-3 disk ROM with the WD2793 at the Philips interface `7FF8h`-`7FFFh`, mirrored at `BFF8h`), port mirrors of the S-3527, M1 wait state, RP5C01 at `B4h`/`B5h` with its time base from the CPU clock, printer port `90h`-`97h`, cassette, keyboard; with `vg8235_frontend` also the Windows frontend |
| `VG8235/rom/` | System ROMs: BIOS/BASIC 2.0, sub-ROM and disk ROM |
| [chips](../../chips/README.md) | Shared chips of all emulators (here: Z80CPU, PPI8255, AY8910, PRINTER, V9938, RP5C01, WD2793, VIDEOREC) |
| [msx](../../msx/README.md) | Shared with the SVI-728: cartridge slot `CARTRIDGE` (ROM modules with mapper detection) |
| `..\Build\` | Build output: `Build\x64\<configuration>`, intermediate files in `Build\obj` |

## Building and Testing

1. Requirement: Visual Studio 2022 with **Desktop development with C++** (MSVC v143, Windows SDK).
2. Open [VG8235.sln](VG8235.sln) in Visual Studio and build it, or in a Developer PowerShell in the solution folder:

   ```powershell
   msbuild VG8235.sln /p:Configuration=Release /p:Platform=x64
   ```

3. Run the tests (`-Build` builds the solution first):

   ```powershell
   .\run_tests.ps1 -Configuration Release
   ```

   Rebuild the function tree (Doxygen and Graphviz, once: `winget install DimitriVanHeesch.Doxygen`
   and `winget install Graphviz.Graphviz`), then open `Doxygen_index.html` in a browser:

   ```powershell
   .\create_callgraph.ps1
   ```

4. Start the emulator; the command line can name a disk image for drive A and a snapshot:

   ```powershell
   ..\Build\vg8235.exe
   ..\Build\vg8235.exe "C:\path\disk.dsk"
   ..\Build\vg8235.exe -state "..\..\2100 - MSX - MEDIEN\STATE\Game.sst"
   ```

   Operation is described in the user manual.

## ROM Images in the Project Tree

The frontend looks for each system ROM, by file name and size, first in `Development\VG8235\rom`, then in the development media folder `2100 - MSX - MEDIEN\ROM`, then next to the program; your own images in the component folder win.

## Development Mode

Started from `..\Build`, the emulator finds `2100 - MSX - MEDIEN` above the project and uses its folders (CAS, DISK, CARD, ROM, STATE, PRINT, VIDEO)
instead of `Documents\VG-8235`; the manual (menu **?**) comes from `..\Distribution`.

## Components

The chips are shared with the other emulators in the folder `chips` and the cartridge slot with the SVI-728 in `msx`, both next to the project folder; the solution builds them into its own `Build` folder. The build writes programs and libraries to `Build\x64\<configuration>` and copies `vg8235.exe` to `Build`, where it is started during development; `Development\create_installation.ps1` makes the setup `Distribution\VG8235-Setup.exe`. `run_tests.ps1` runs the test programs of all components and of the machine; the machine test boots the system ROMs to BASIC and Disk BASIC.

| Component | Content |
| --- | --- |
| [Z80CPU](../../chips/Z80CPU/README.md) | Z80 CPU |
| [PPI8255](../../chips/PPI8255/README.md) | 8255 PPI: slot selection, keyboard, cassette control, keyboard click |
| [AY8910](../../chips/AY8910/README.md) | PSG as YM2149: sound, joysticks, cassette input |
| [V9938](../../chips/V9938/README.md) | Yamaha V9938 video display processor |
| [RP5C01](../../chips/RP5C01/README.md) | Ricoh RP5C01 clock with CMOS memory |
| [WD2793](../../chips/WD2793/README.md) | WD2793 disk controller and 3.5" drive |
| [CARTRIDGE](../../msx/CARTRIDGE/README.md) | ROM modules and MegaROM mappers |
| [PRINTER](../../chips/PRINTER/README.md) | printer on the MSX printer port |
| [VIDEOREC](../../chips/VIDEOREC/README.md) | MP4 video recording of picture and sound (System > Record video) |

## Building the Setup

In a PowerShell in the folder `Development`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1 -NoRoms
```

The script builds Release|x64 and creates `..\Distribution\VG8235-Setup.exe` with the ROM images, or with
`-NoRoms` `..\Distribution\VG8235-Setup-NoRoms.exe` without them, for publishing. The setup asks for the installation
folder, installs the program and the manual, creates shortcuts and the media folders in `Documents\VG-8235` (including
the empty `ROM` folder) and registers the uninstaller in the Windows app settings. Every setup, with or without
ROMs, carries the blank disks from `installer\disks` (no third-party code) and puts them write-protected into
`DISK\Original`; a disk already there stays as it is.
