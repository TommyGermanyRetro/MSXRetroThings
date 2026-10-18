# SVI-728 Emulator – Developer Guide

Emulator of the Spectravideo SVI-728 (MSX1, PAL or NTSC) in C++17 as a Visual Studio solution. The machine is
built from component libraries: the chips in `chips` (shared with all emulators) and the cartridge
slot in `msx` (shared with the VG-8235), plus a Windows frontend with keyboard, sound and cassette and
automated tests. Use, technical reference and sources are in the user manual [MANUAL.md](../Distribution/MANUAL.md);
this guide covers the project layout, building, testing and the development tools.

## Layout

| Path | Contents |
| --- | --- |
| [SVI728.sln](SVI728.sln) | Solution with all libraries, the frontend and the tests (x64, Debug/Release) |
| [Common.props](Common.props) | Settings of the solution: output to `..\Build`, include path of the machine; builds on the settings of `msx` and `chips` |
| [create_installation.ps1](create_installation.ps1) | Builds Release\|x64 and the setup `..\Distribution\SVI728-Setup.exe` (program, BIOS ROM, manual); with `-NoRoms` the setup without ROMs `..\Distribution\SVI728-Setup-NoRoms.exe`, which creates the empty ROM folder |
| [run_tests.ps1](run_tests.ps1) | Runs all test programs and reports the result of each |
| [create_callgraph.ps1](create_callgraph.ps1) | Builds the function tree with Doxygen and Graphviz into `CallGraph\html` and the entry page `Doxygen_index.html` |
| [CallGraph/Doxyfile](CallGraph/Doxyfile) | Doxygen settings of the function tree: machine, frontend and components without tests, tools and third-party code |
| `Doxygen_index.html` | Function tree, written by `create_callgraph.ps1` (opens `CallGraph/html/index.html`): call and caller graph of every function (SVG, clickable), source with cross-references |
| [installer/](installer/Installer.cs) | Source of the Windows setup and uninstall program |
| `..\Build\svi728.exe` | Frontend, copied to `..\Build` after every frontend build |
| [SVI728/](SVI728/README.md) | Machine (memory, slots, I/O, cassette), Windows frontend, BIOS ROM, machine and cassette tests |
| [SVI728/docs/CAS.md](SVI728/docs/CAS.md) | Cassette image format and tape signal |
| [chips](../../chips/README.md) | Shared chips of all emulators (here: Z80CPU, PPI8255, AY8910, TMS99X8, TMS9918A, TMS9929A, PRINTER, VIDEOREC) |
| [msx](../../msx/README.md) | Shared with the VG-8235: cartridge slot `CARTRIDGE` (ROM modules with mapper detection) |
| `..\Build\` | Build output: `Build\x64\<configuration>` for programs and libraries, `Build\obj` for intermediate files |

## Building and Testing

1. Requirement: Visual Studio 2022 with **Desktop development with C++** (MSVC v143, Windows SDK).
2. Open [SVI728.sln](SVI728.sln) in Visual Studio and build it (start project `svi728_frontend`), or in a Developer
   PowerShell in the solution folder:

   ```powershell
   msbuild SVI728.sln /p:Configuration=Release /p:Platform=x64
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

4. Start the emulator. Without an argument it loads the BIOS ROM as described under
   [ROM images](#rom-images-in-the-project-tree), otherwise the given ROM file:

   ```powershell
   ..\Build\svi728.exe
   ..\Build\svi728.exe .\SVI728\rom\SVI-MSX-BIOS+Basic.bin
   ```

   Operation (modules, cassette, printer, snapshots) is described in the user manual and the
   [SVI728 README](SVI728/README.md).

## ROM Images in the Project Tree

The frontend looks for `SVI-MSX-BIOS+Basic.bin`, by file name and size, first in `Development\SVI728\rom`, then in the development media folder `2100 - MSX - MEDIEN\ROM`, then next to the program; your own image in the component folder wins.

## Development Mode

Started from `..\Build`, the emulator finds `2100 - MSX - MEDIEN` above the project and uses its folders (CAS, CARD, ROM, STATE, PRINT, VIDEO)
instead of `Documents\SVI-728`; the manual (menu **?**) comes from `..\Distribution`.

## Components

The chips are shared with the other emulators in the folder `chips` and the cartridge slot with the VG-8235 in `msx`, both next to the project folder; the solution builds them into its own `Build` folder. The build writes programs and libraries to `Build\x64\<configuration>` and copies `svi728.exe` to `Build`, where it is started during development; `Development\create_installation.ps1` makes the setup `Distribution\SVI728-Setup.exe`. `run_tests.ps1` runs the test programs of all components and of the machine; two of them boot the BIOS and load and save programs through the emulated tape.

| Component | Content |
| --- | --- |
| [SVI728](SVI728/README.md) | machine, frontend, BIOS ROM, cassette |
| [Z80CPU](../../chips/Z80CPU/README.md) | Z80 CPU |
| [PPI8255](../../chips/PPI8255/README.md) | 8255 PPI: slot selection, keyboard, cassette control, keyboard click |
| [AY8910](../../chips/AY8910/README.md) | AY-3-8910: sound, joysticks, cassette input |
| [TMS99X8](../../chips/TMS99X8/README.md), [TMS9918A](../../chips/TMS9918A/README.md), [TMS9929A](../../chips/TMS9929A/README.md) | video display processor family and its NTSC and PAL chips |
| [CARTRIDGE](../../msx/CARTRIDGE/README.md) | ROM modules and MegaROM mappers |
| [PRINTER](../../chips/PRINTER/README.md) | printer on the MSX printer port |
| [VIDEOREC](../../chips/VIDEOREC/README.md) | MP4 video recording of picture and sound (System > Record video) |

## Building the Setup

In a PowerShell in the folder `Development`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\create_installation.ps1 -NoRoms
```

The script builds Release|x64 and creates `..\Distribution\SVI728-Setup.exe` with the ROM images, or with
`-NoRoms` `..\Distribution\SVI728-Setup-NoRoms.exe` without them, for publishing. The setup asks for the installation
folder, installs the program and the manual, creates shortcuts and the media folders in `Documents\SVI-728` (including
the empty `ROM` folder) and registers the uninstaller in the Windows app settings.
