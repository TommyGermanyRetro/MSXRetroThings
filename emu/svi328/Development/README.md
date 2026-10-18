# SVI-328 Emulator – Developer Guide

Emulator of the Spectravideo SVI-328 (PAL or NTSC, 64 KiB RAM, BASIC 1.11) in C++17 as a Visual Studio solution. The machine is built
from component libraries: the chips in `chips`, the expansion interface, expanders and cards in
`svi3x8` (both shared with the SVI-318 emulator), plus a Windows frontend and automated tests.
Use, technical reference and sources are in the user manual [MANUAL.md](../Distribution/MANUAL.md); this guide
covers the project layout, building, testing and the development tools. The programming interfaces are described
in the READMEs of the components.

## Layout

| Path | Contents |
| --- | --- |
| [SVI328.sln](SVI328.sln) | Solution with all libraries, the frontend and the tests (x64, Debug/Release) |
| `..\Build\svi328.exe` | Frontend, copied to `..\Build` after every frontend build; `-state <file.sst>` starts with a snapshot |
| [SVI328/tools/svi328_state.cpp](SVI328/tools/svi328_state.cpp) | `svi328_state.exe`: the machine without a window at full speed; makes snapshots and checks input by script (see below) |
| [Common.props](Common.props) | Settings of the solution: output to `..\Build`, include path of the machine; builds on the settings of `svi3x8` and `chips` (MSVC v143, C++17/C11, `/W4 /permissive- /utf-8`, static runtime library) |
| [create_installation.ps1](create_installation.ps1) | Builds Release\|x64 and the setup `..\Distribution\SVI328-Setup.exe` (program, `svi111.rom`, `svi806.rom`, `svi609.rom`, manual); with `-NoRoms` the setup without ROMs `..\Distribution\SVI328-Setup-NoRoms.exe`, which creates the empty ROM folder |
| [run_tests.ps1](run_tests.ps1) | Runs all test programs and reports the result of each |
| [create_callgraph.ps1](create_callgraph.ps1) | Builds the function tree with Doxygen and Graphviz into `CallGraph\html` and the entry page `Doxygen_index.html` |
| [CallGraph/Doxyfile](CallGraph/Doxyfile) | Doxygen settings of the function tree: machine, frontend and components without tests, tools and third-party code |
| `Doxygen_index.html` | Function tree, written by `create_callgraph.ps1` (opens `CallGraph/html/index.html`): call and caller graph of every function (SVG, clickable), source with cross-references |
| [installer/](installer/Installer.cs) | Source of the Windows setup and uninstall program |
| [SVI328/](SVI328/README.md) | Machine (memory banks, ports, keyboard, joysticks, cassette, modules), Windows frontend, BASIC ROM, machine tests |
| [CardTests/](CardTests) | Tests of the expanders and cards that boot the SVI-328 (one test project per component; the libraries are in `svi3x8`) |
| [svi3x8](../../svi3x8/README.md) | Shared with the SVI-318: expansion interface `SVI3X8`, expanders, cards and card ROMs |
| [chips](../../chips/README.md) | Shared chips: Z80, 8255, AY-3-8910, TMS99x8, FD1793, INS8250, MC6845, MC6821, Z8530, WD1002-SHD, printer, video recorder |
| `..\Build\` | Build output: `Build\x64\<configuration>` for programs and libraries, `Build\obj` for intermediate files |

## Building and Testing

1. Requirement: Visual Studio 2022 with **Desktop development with C++** (MSVC v143, Windows SDK).
2. Open [SVI328.sln](SVI328.sln) in Visual Studio and build it, or in a Developer PowerShell:

   ```powershell
   msbuild SVI328.sln /p:Configuration=Release /p:Platform=x64
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

4. Start the emulator. Without an argument it loads `svi111.rom` as described under [ROM images](#rom-images-in-the-project-tree),
   otherwise the given ROM file. Keys and joysticks are described in the [SVI328 README](SVI328/README.md).

   ```powershell
   ..\Build\svi328.exe
   ```

## ROM Images in the Project Tree

The frontend looks for each ROM image, by file name and size, first in the `rom` folder of its component, then in the development media folder `2100 - MSX - MEDIEN\SVIROM`, then next to the program; your own images in the component folders win. The user manual describes the installed case.

| File | Purpose | Folder |
| --- | --- | --- |
| `svi111.rom` | BASIC 1.11 (32 KiB), required | `Development\SVI328\rom` |
| `svi806.rom` | Character ROM of the SVI-806 80-column card | `svi3x8\SVI806\rom` |
| `svi609.rom` | EPROM of the SVI-608M boot card and of the SVI-609 LAN card | `svi3x8\LANIF01\rom` |

## Making Snapshots without the Window

`Build\x64\Release\svi328_state.exe` runs the machine without a window as fast as the PC allows (about four times real time) and works through its arguments in order. It makes snapshots that need a tape load, and it checks input on a snapshot without the frontend:

```powershell
# CHESS: boot, BLOAD from tape (6 minutes emulated, about 90 s), wait for the title, save
.\Build\x64\Release\svi328_state.exe Development\SVI328\rom\svi111.rom --until Ok 1000 `
    --cas "..\2100 - MSX - MEDIEN\SVICAS\GAMES\Chess (1986)(JTM Software).cas" `
    --type 'bload\"cas:\",r\n' --tape --run 500 --save "..\2100 - MSX - MEDIEN\SVISTATE\Chess.sst"
# Load it, choose level 1, white, black background, move joystick 1 down, write a picture
.\Build\x64\Release\svi328_state.exe Development\SVI328\rom\svi111.rom --load "..\2100 - MSX - MEDIEN\SVISTATE\Chess.sst" `
    --type 1 --run 50 --type 2 --run 50 --type 1 --run 300 --joy 1 D 4 --bmp board.bmp
```

Called without actions, it lists them: `--load`, `--cas`, `--run`, `--until`, `--type`, `--tape`, `--key`, `--joy`, `--set-joy`, `--bmp`, `--text`, `--psg`, `--dump`, `--profile` and `--save`.

The examples run in the project folder (the parent of `Development`).

## Development Mode

Started from `..\Build`, the emulator finds `2100 - MSX - MEDIEN` above the project and uses its folders (SVICAS, SVIDSK, SVIHDD, SVIMOD, SVIPRINT, SVIROM, SVISTATE, SVIVIDEO)
instead of `Documents\SVI-328`; the manual (menu **?**) comes from `..\Distribution`.

## Components

The build writes programs and libraries to `Build\x64\<configuration>` and copies `svi328.exe` to `Build`, where it is started during development; `Development\create_installation.ps1` makes the setup `Distribution\SVI328-Setup.exe`. `run_tests.ps1` runs the test programs of all components and of the machine; the tests that boot Disk BASIC and CP/M use the disk images from the media folder when they are present. The chips are shared with the SVI-318 emulator in the folder `chips`, the expansion interface, expanders and cards (with their ROMs) in `svi3x8`, next to the project folder; the solution builds them into its own `Build` folder, and the tests of expanders and cards, which boot the machine, are in `Development\CardTests`.

| Component | Content |
| --- | --- |
| [SVI328](SVI328/README.md) | machine, frontend, BASIC ROM |
| [SVI3X8](../../svi3x8/SVI3X8/README.md) | expansion connector interface shared by the SVI-318 and SVI-328, its expanders and cards |
| [Z80CPU](../../chips/Z80CPU/README.md) | Z80 CPU |
| [PPI8255](../../chips/PPI8255/README.md) | 8255 PPI: keyboard, cassette control, joystick triggers |
| [AY8910](../../chips/AY8910/README.md) | AY-3-8910: sound, joysticks, memory banks |
| [TMS99X8](../../chips/TMS99X8/README.md), [TMS9918A](../../chips/TMS9918A/README.md), [TMS9929A](../../chips/TMS9929A/README.md) | video display processor family and its NTSC and PAL chips |
| [SVI601](../../svi3x8/SVI601/README.md) | SVI-601 Super Expander |
| [SVI602](../../svi3x8/SVI602/README.md) | SVI-602 Mini Expander |
| [SVI605B](../../svi3x8/SVI605B/README.md) | SVI-605B expander: main board with card slots, disk unit and printer port |
| [SVI605BHD](../../svi3x8/SVI605BHD/README.md) | SVI-605B with hard disk: main board with one drive and built-in SASI host adapter |
| [SVI608M](../../svi3x8/SVI608M/README.md) | SVI-608M expander: main board with five slots, one drive and boot source detection |
| [SVI609](../../svi3x8/SVI609/README.md) | Network Expander of the SVI-609 Master Station with its factory cards |
| [SVI801](../../svi3x8/SVI801/README.md) | SVI-801 floppy disk controller with two SVI-902 drives; disk unit and disk images of the expanders |
| [SVI802](../../svi3x8/SVI802/README.md) | SVI-802 Centronics interface; printer port of the expanders |
| [SVI803](../../svi3x8/SVI803/README.md) | SVI-803 16 KiB RAM |
| [SVI805](../../svi3x8/SVI805/README.md) | SVI-805 RS-232 interface on a Windows COM port |
| [SVI806](../../svi3x8/SVI806/README.md) | SVI-806 80-column card |
| [SVI807](../../svi3x8/SVI807/README.md) | SVI-807 64 KiB RAM |
| [LANHD01](../../svi3x8/LANHD01/README.md) | LAN-HD-01 SASI host adapter of the SVI-608M and SVI-609 |
| [LANIF01](../../svi3x8/LANIF01/README.md) | LAN-IF-01 boot card of the SVI-608M and LAN card of the SVI-609 with printer port |
| [FD1793](../../chips/FD1793/README.md) | floppy disk controller |
| [WD1002SHD](../../chips/WD1002SHD/README.md) | hard disk controller |
| [INS8250](../../chips/INS8250/README.md) | UART of the RS-232 card |
| [MC6845](../../chips/MC6845/README.md) | CRT controller of the 80-column card |
| [MC6821](../../chips/MC6821/README.md) | PIA of the printer port on the SVI-609 LAN card |
| [Z8530](../../chips/Z8530/README.md) | serial communications controller of the SVI-609 LAN card (not yet connected) |
| [PRINTER](../../chips/PRINTER/README.md) | printer on the Centronics port |
| [VIDEOREC](../../chips/VIDEOREC/README.md) | MP4 video recording of picture and sound (System > Record video) |

## Building the Setup

In a PowerShell in the folder `Development`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1 -NoRoms
```

The script builds Release|x64 and creates `..\Distribution\SVI328-Setup.exe` with the ROM images, or with
`-NoRoms` `..\Distribution\SVI328-Setup-NoRoms.exe` without them, for publishing. The setup asks for the installation
folder, installs the program and the manual, creates shortcuts and the media folders in `Documents\SVI-328` (including
the empty `ROM` folder) and registers the uninstaller in the Windows app settings. Every setup, with or without
ROMs, carries the blank disks from `installer\disks` (no third-party code) and puts them write-protected into
`DSK\Original`; a disk already there stays as it is.
