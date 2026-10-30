# mbc89 - Maerklin Connect6021 CAN accessory module

A full MSX-DOS 2 port of `mbc89`, one of the MBCAN family of Maerklin CS2/CS3 CAN-bus accessory
modules, built on top of the [XIO cartridge](../xio/) (switched IO/IM2/PIC)
and the [RCX ROM cartridge](../rcx/)'s `RC2014 card E` (SJA1000 CAN controller). Instead of the
original's ATmega firmware, this version runs as a normal `.COM` program on an MSX with an RC2014
backplane, using [MSXgl](https://github.com/aoineko-fr/MSXgl)/SDCC for the C port.

Runs on MSX1, MSX2, and MSX2+ alike - all graphics use only the TMS9918A-standard SCREEN0/SCREEN2
modes, which every later VDP (V9938/V9958) stays backward-compatible with. The only real
requirement is an MSX-DOS 2 kernel (not a particular machine generation) - see "Compatibility"
below.

The module registers itself on the CS2/CS3 CAN bus exactly like a real Connect6021, answers the
config-channel walk (SW version, central-station-type mapping, keyboard base address, I2C clock),
and simulates up to 16 Keyboard-6040 keyboards, one CU6021 control unit, up to 8 Control80f
control units, and up to 4 Memory-6043 route memories - all as one switchable on-screen panel, driven by the MSX keyboard and the real CAN
bus at the same time (a button press sends a real CAN message; an incoming CAN message updates the
panel's LEDs/knob live).

## Impressions

All screenshots and the video come from the built-in video recording of the VG-8235 emulator
([`../emu/`](../emu/)), running mbc89 with the real XIO/RCX cartridges and `RC2014 card E` on the
[USB2SLOT board](../usb2slot/), connected to a CS3.

Splash screen

![mbc89 splash](images/splash.png)

Boot info screen - startup configuration (environment variables), persisted configuration, and
discovery/init results for the XIO/RCX/CAN chain

![mbc89 boot info](images/init.png)

CU6021 panel - locomotive control (speed knob, direction toggle, functions F0-F4), one instance -
original Maerklin device (left) vs. mbc89 emulation (right)

![mbc89 CU6021](images/6021_combo.jpg)

Keyboard-6040 panel - accessory control (a grid of red/green switch/signal button pairs for
turnouts and signals), up to 16 independent instances - original Maerklin device (left) vs.
mbc89 emulation (right)

![mbc89 Keyboard-6040](images/6040_combo.jpg)

Control80f panel - a second locomotive controller alongside CU6021, same speed/direction/function
control, up to 8 independent instances - original Maerklin device (left) vs. mbc89 emulation
(right)

![mbc89 Control80f](images/c80f_combo.jpg)

Memory-6043 panel - route memory (record keyboard presses as a route, play it back with one
button), up to 4 independent instances with 24 routes each - original Maerklin device (left) vs.
mbc89 emulation (right)

![mbc89 Memory-6043](images/6043_combo.jpg)

A full video from the Nextor prompt through the splash and boot info screens to all four panel
types is included: [`images/startup.mp4`](images/startup.mp4).

## Panels

All four panel types share one carousel (two triangle sprites, `SPACE` switches between instances)
and the same bidirectional CAN wiring:

- **Keyboard-6040** (up to 16 instances, one CS2 address block of 16 per instance): a grid of
  switch/signal pairs, cursor-navigated with the MSX keyboard, sends `CAN_ID_ZUBEHOER_SCHALTEN`
  (0x16) on press and updates its LEDs live from incoming messages for the same address range.
- **CU6021** (1 instance): loco speed/direction/F0-F4 control, `CAN_ID_LOK_*` (0x08/0x0A/0x0C).
- **Control80f** (up to 8 instances): same loco control as CU6021, addressed per instance.
- **Memory-6043** (up to 4 instances, 24 routes A1-C8 each): records Keyboard-6040 presses as a
  route (`in`, route button, keyboard presses, `end`; `clr` during recording removes the last
  command, `in`+`clr`+route button deletes the whole route) and plays a route back as a burst of
  `CAN_ID_ZUBEHOER_SCHALTEN` (0x16) messages. Routes are stored in `MBC89R.CFG` (one 128-byte
  record per route) and survive a restart. Interlocking and S88-triggered automatic routes are not
  implemented yet.

Startup configuration (`IO`/`INT`/`KEYB`/`C80F`/`MEMB`) is read from environment variables via
a raw BDOS `GENV` call (function 6Bh) - a MSX-DOS 2 extension with no MSX-DOS 1 equivalent, so this
is the module's actual compatibility floor (see "Compatibility" below), not the machine generation.
Config values changed via ParaCenter or a real CS2/CS3 central station (SW version,
central-station-type mapping, keyboard base address, I2C clock, module name, GUiD/PC database
number) are persisted to `MBC89.CFG` on the current drive via the FCB-based file functions (MSX-DOS 1
compatible, and just as usable under MSX-DOS 2), and reloaded on every startup - see
`mbc89_store.c`.

## Compatibility

- **Machine generation**: MSX1, MSX2, or MSX2+ - graphics use only SCREEN0 (text) and SCREEN2
  (pattern/sprite), both plain TMS9918A modes that V9938 (MSX2) and V9958 (MSX2+) remain fully
  backward-compatible with. No MSX2-exclusive VDP mode is used anywhere.
- **DOS kernel**: requires MSX-DOS 2 (built into most MSX2/MSX2+ machines, or loadable from a
  MSX-DOS 2 disk/CF card on MSX1) - the startup environment variables above need the DOS2-only
  `GENV` call. Without it, the module aborts to the DOS prompt with a clear error message rather
  than crashing or misbehaving, but it will not run.

## Source layout

- `mbc89.c` - main program: environment variables, splash/boot-info screens, XIO/RCX/CAN discovery
  and init, hands off to the GUI.
- `mbc89_base.c`/`.h` - CS2/CS3 registration, PC/CSx config-channel protocol, `PC_ARRAY_DATA`
  systemarray bridge.
- `mbc89_com.c`/`.h` - simulated Connect6021 firmware (SW/mapping/basis/I2C config channels, CONNECT6021
  handshake, loco database, Stopp/Go, loco speed/direction/function handling).
- `mbc89_can.c`/`.h` - conversion between the logical 13-byte Maerklin CAN message and the raw PeliCAN
  SJA1000 TX/RX register format `rcx_io.h` expects.
- `mbc89_gui.c`/`.h` - SCREEN2 panel rendering and interaction (Keyboard-6040/CU6021/Control80f/
  Memory-6043 carousel).
- `mbc89_sim.c`/`.h` - simulated plug/voltage/temperature sensor values (`P`/arrow keys).
- `mbc89_store.c`/`.h` - `MBC89.CFG` file-based persistence (FCB, MSX-DOS 1 compatible).
- `mbc89_memory.c`/`.h` - Memory-6043 route recording/playback, `MBC89R.CFG` (FCB random block I/O).
- `content/` - embedded panel graphics/sprite data (RLE-packed screen data, generated content
  headers).

## Rebuilding

Clone [MSXgl](https://github.com/aoineko-fr/MSXgl), then copy [`../lib/xio_io/`](../lib/xio_io/)'s
contents into `MSXgl/lib/xio_io/`, [`../lib/rcx_io/`](../lib/rcx_io/)'s contents into
`MSXgl/lib/rcx_io/`, [`../lib/dostools/`](../lib/dostools/)'s contents into `MSXgl/lib/dostools/`,
and this folder's contents into `MSXgl/projects/Mbc89/` - all paths must stay space-free, a
requirement of the MSXgl build tool itself. `project_config.js` already references the libraries it
needs with the matching relative paths:

```js
CompileOpt = "-I../../lib/xio_io -I../../lib/rcx_io -I../../lib/dostools";
AddLibs = [ "../../lib/xio_io/xio_io.lib", "../../lib/rcx_io/rcx_io.lib", "../../lib/dostools/dostools.lib" ];
```

```sh
cd MSXgl/projects/Mbc89 && node ../../engine/script/js/build.js
```

A prebuilt [`Mbc89.com`](Mbc89.com) is included so the module can be used without rebuilding.

## Hardware

Runs on any MSX1/MSX2/MSX2+ with `RC2014 card E` (SJA1000 CAN controller, see [`../rcx/rc2014_card_e/`](../rcx/rc2014_card_e/))
installed behind an [XIO cartridge](../xio/)+RC2014 adapter, connected to a Maerklin CS2/CS3 CAN
bus, and MSX-DOS 2 as the active kernel (see "Compatibility" above).
