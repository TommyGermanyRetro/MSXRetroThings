"""
start_usbslot.py - detects via the bridge what is plugged into bridge slots 1 and 2, creates matching extensions
and starts the self-built openMSX with them

Detection with `slottest <port> scan` (SlotLink): slot expander, content per subslot (ROM / data / empty),
switched I/O ID C0h at port 40h (RCX/XIO) and the mapper ports FCh-FFh. From this the following files are
created in the openMSX user directory:

  USBSlotAuto1.xml, USBSlotAuto2.xml  one per occupied bridge slot
      - SD Mapper/Megaram 512K (expander, ROM in subslot 1, data in subslot 3 and the Nextor driver
        "SDMapper Driver" in ROM bank 7): Nextor bank and SPI switch 6001h in subslot 1 with sector block
        (bridge protocol 3), subslot 3 as Memory Mapper (ports FCh-FFh answer with E0h..) or MegaRAM
      - other expander: <expander>true</expander> without further rules
      - no expander: plain cartridge; MegaROM (slottest reports ROMMAPPER konami/konamiscc/ascii8/ascii16)
        with <rommapper type="..."/>
      - empty slot: no extension
  USBSlotAutoIO.xml  I/O ports 40h-47h, if the ID C0h answers at port 40h

Mixed operation: further openMSX parameters (e.g. -carta game.rom, -cartb, -exta, -ext <extension>) are
passed BEFORE the bridge extensions. openMSX then assigns the explicitly requested slots first, the bridge
extensions (slot="any") get the remaining free slots. With --bridge 1, 2 or 12 only these bridge slots are
included, so that a slot of the machine stays free for emulated cartridges.

Clock of RC2014 card A: with --clock [address], `slottest <port> clock <address>` sets the PCF8583 clock to
the PC local time before the start (PCF8584 at the IO address, decimal like CALL I2C INIT, default 0). Only
for MSX1 machines (<type>MSX</type> in the machine description) and only if all checks of slottest pass.

Usage:    python start_usbslot.py [machine] [--port COMx] [--bridge 1|2|12] [--clock [address]]
                                  [further openMSX parameters]
Default:  machine C-BIOS_MSX2+_JP, all occupied bridge slots, port COM16 (or environment variable
          USBSLOT_PORT), clock not set

openMSX: bin\\openmsx.exe next to this script (with its own share folder), otherwise the self-built openMSX
under D:\\openMSX_build with the system data from C:\\Program Files\\openMSX\\share. The environment
variables USBSLOT_OPENMSX and USBSLOT_SHARE override both.
"""

import os
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
SLOTTEST = os.path.join(HIER, "..", "SlotLink", "slottest.exe")
BUILD_OPENMSX = r"D:\openMSX_build\openMSX-21.0\derived\x64-VC-Release\install\openmsx.exe"
BUILD_SHARE = r"C:\Program Files\openMSX\share"
USER_EXT = os.path.join(os.path.expanduser("~"), "Documents", "openMSX", "share", "extensions")
PORT = os.environ.get("USBSLOT_PORT", "COM16")
CLOCK = None                                                    # IO address for --clock, None = do not set the clock
OHNE_FENSTER = getattr(subprocess, "CREATE_NO_WINDOW", 0)       # slottest without its own console window

KOPF = """<?xml version="1.0" ?>
<!DOCTYPE msxconfig SYSTEM 'msxconfig2.dtd'>
<msxconfig>
  <info>
    <name>{name}</name>
    <manufacturer>TommyGermanyRetro</manufacturer>
    <code/>
    <release_year>2026</release_year>
    <description>{beschreibung}</description>
    <type>USB slot bridge</type>
  </info>
  <devices>
"""
FUSS = """  </devices>
</msxconfig>
"""


def openmsx_pfade():
    """Returns (openmsx.exe, system data folder or None if share lies next to the exe)."""
    exe = os.environ.get("USBSLOT_OPENMSX")
    if not exe:
        exe = os.path.join(HIER, "bin", "openmsx.exe")
        if not os.path.isfile(exe):
            exe = BUILD_OPENMSX
    share = os.environ.get("USBSLOT_SHARE")
    if not share and not os.path.isdir(os.path.join(os.path.dirname(exe), "share")):
        share = BUILD_SHARE
    return exe, share


def scan():
    """Runs slottest scan and returns (slots, swio, mapperports)."""
    ausgabe = subprocess.run([SLOTTEST, PORT, "scan"], capture_output=True, text=True, check=False,
                             creationflags=OHNE_FENSTER).stdout
    slots = {1: {"expander": False, "sub": {}, "treiber": "", "rommapper": ""},
             2: {"expander": False, "sub": {}, "treiber": "", "rommapper": ""}}
    swio = False
    mapperports = []
    for zeile in ausgabe.splitlines():
        teile = zeile.split()
        if not teile:
            continue
        if teile[0] == "SLOT":
            slots[int(teile[1])]["expander"] = teile[3] == "1"
        elif teile[0] == "SUB":
            slots[int(teile[1])]["sub"][int(teile[2])] = teile[3]
        elif teile[0] == "ROMMAPPER":
            slots[int(teile[1])]["rommapper"] = teile[2]
        elif teile[0] == "DRIVER":
            slots[int(teile[1])]["treiber"] = " ".join(teile[2:])
        elif teile[0] == "SWIO":
            swio = teile[1] == "1"
        elif teile[0] == "MAPPERPORTS":
            mapperports = [int(w, 16) for w in teile[1:]]
    if not mapperports:
        raise RuntimeError("no response from slottest scan:\n" + ausgabe)
    return slots, swio, mapperports


def geraet(nr, regeln, io=""):
    """A USBSlot device for bridge slot nr in its own primary slot."""
    return ("    <primary slot=\"any\">\n      <secondary slot=\"any\">\n"
            "        <USBSlot id=\"USB Slot {nr}\">\n"
            "          <mem base=\"0x0000\" size=\"0x10000\"/>\n{io}"
            "          <port>{port}</port>\n          <slot>{nr}</slot>\n{regeln}"
            "        </USBSlot>\n      </secondary>\n    </primary>\n").format(nr=nr, io=io, port=PORT, regeln=regeln)


def erweiterung_slot(nr, info, mapper_aktiv):
    """Returns (beschreibung, xml) for a bridge slot, or None for an empty slot."""
    sub = info["sub"]
    if not info["expander"]:
        if sub.get(0) == "EMPTY":
            return None
        if info["rommapper"]:
            regeln = "          <rommapper type=\"{}\"/>\n".format(info["rommapper"])
            return "Cartridge ({})".format(info["rommapper"]), geraet(nr, regeln)
        return "Cartridge", geraet(nr, "")
    if sub.get(1) == "ROM" and sub.get(3) == "DATA" and info["treiber"] == "SDMapper Driver":
        nextor = ("          <subslot id=\"1\">\n"
                  "            <bank page=\"1\" address=\"0x6000\" mask=\"0x07\"/>\n"
                  "            <nocacheswitch address=\"0x6001\" base=\"0x4000\" size=\"0x0801\"/>\n"
                  "            <spiblock start=\"0x4001\" end=\"0x41FF\" gap=\"24\"/>\n"
                  "          </subslot>\n")
        if mapper_aktiv:
            regeln = ("          <expander>true</expander>\n" + nextor +
                      "          <subslot id=\"3\">\n            <mapper port=\"0xFC\" mask=\"0x1F\"/>\n"
                      "          </subslot>\n")
            return "SD Mapper (Memory Mapper)", geraet(nr, regeln, "          <io base=\"0xFC\" num=\"4\" type=\"IO\"/>\n")
        regeln = ("          <expander>true</expander>\n" + nextor +
                  "          <subslot id=\"3\">\n            <megaram port=\"0x8E\"/>\n          </subslot>\n")
        return "SD Mapper (MegaRAM)", geraet(nr, regeln, "          <io base=\"0x8E\" num=\"2\" type=\"IO\"/>\n")
    belegt = ", ".join("{}: {}".format(n, sub[n]) for n in sorted(sub))
    return "Slot expander ({})".format(belegt), geraet(nr, "          <expander>true</expander>\n")


def schreiben(name, titel, beschreibung, inhalt):
    with open(os.path.join(USER_EXT, name + ".xml"), "w", encoding="utf-8", newline="\n") as f:
        f.write(KOPF.format(name=titel, beschreibung=beschreibung) + inhalt + FUSS)


def argumente(args):
    """Returns (maschine, auswahl, weitere): the machine is the first argument if it does not start with '-',
    --bridge and --port may appear anywhere."""
    global PORT, CLOCK
    args = list(args)
    if "--clock" in args:
        i = args.index("--clock")
        CLOCK = 0
        if i + 1 < len(args) and args[i + 1].isdigit():
            CLOCK = int(args[i + 1])
            del args[i + 1]
        if CLOCK > 254:
            raise SystemExit("--clock expects an IO address from 0 to 254")
        del args[i]
    if "--port" in args:
        i = args.index("--port")
        if i + 1 >= len(args):
            raise SystemExit("--port expects a COM port, e.g. COM5")
        PORT = args[i + 1]
        del args[i:i + 2]
    maschine = "C-BIOS_MSX2+_JP"
    if args and not args[0].startswith("-"):
        maschine = args.pop(0)
    auswahl = (1, 2)
    if "--bridge" in args:
        i = args.index("--bridge")
        wert = args[i + 1] if i + 1 < len(args) else ""
        if not wert or any(z not in "12" for z in wert):
            raise SystemExit("--bridge expects 1, 2 or 12")
        auswahl = tuple(sorted({int(z) for z in wert}))
        del args[i:i + 2]
    return maschine, auswahl, args


def mapper_antwortet(mapperports):
    """True if the ports FCh-FFh answer as a Memory Mapper (E0h..E3h after reset). A port that does not
    answer returns FFh (data bus precharged) and does not count as a mapper."""
    return all(w != 0xFF and (w & 0xE0) == 0xE0 for w in mapperports)


def ist_msx1(maschine, share):
    """True if the machine description (user or system data) specifies <type>MSX</type>."""
    ordner = [os.path.join(os.path.dirname(USER_EXT), "machines")]
    if share:
        ordner.append(os.path.join(share, "machines"))
    for o in ordner:
        datei = os.path.join(o, maschine + ".xml")
        if os.path.isfile(datei):
            with open(datei, encoding="utf-8", errors="replace") as f:
                text = f.read()
            anfang = text.find("<type>")
            return anfang >= 0 and text[anfang + 6:text.find("</type>", anfang)].strip() == "MSX"
    return False


def uhr_stellen():
    """Sets the clock of card A via slottest clock; slottest only writes if all checks pass."""
    ausgabe = subprocess.run([SLOTTEST, PORT, "clock", str(CLOCK)], capture_output=True, text=True, check=False,
                             creationflags=OHNE_FENSTER).stdout.strip()
    print("Clock card A (address {}): {}".format(CLOCK, ausgabe or "no response from slottest"))


def main():
    maschine, auswahl, weitere = argumente(sys.argv[1:])

    slots, swio, mapperports = scan()
    if CLOCK is not None:
        if ist_msx1(maschine, openmsx_pfade()[1] or os.path.join(os.path.dirname(openmsx_pfade()[0]), "share")):
            uhr_stellen()
        else:
            print("Clock card A: not set, {} is not an MSX1 machine".format(maschine))
    mapper_aktiv = mapper_antwortet(mapperports)
    os.makedirs(USER_EXT, exist_ok=True)

    erweiterungen = []
    for nr in (1, 2):
        ergebnis = erweiterung_slot(nr, slots[nr], mapper_aktiv)
        if ergebnis is None:
            print("Bridge slot {}: empty".format(nr))
            continue
        if nr not in auswahl:
            print("Bridge slot {}: {} - not included (--bridge)".format(nr, ergebnis[0]))
            continue
        beschreibung, xml = ergebnis
        print("Bridge slot {}: {}".format(nr, beschreibung))
        schreiben("USBSlotAuto{}".format(nr), "USB Slot {} (auto)".format(nr),
                  "Slot {} of the USB slot bridge, detected at start: {}".format(nr, beschreibung), xml)
        erweiterungen.append("USBSlotAuto{}".format(nr))
    if swio:
        print("Switched I/O 40h-47h: present")
        io = ("    <USBSlot id=\"USB Slot I/O\">\n      <io base=\"0x40\" num=\"8\" type=\"IO\"/>\n"
              "      <port>{}</port>\n      <slot>1</slot>\n    </USBSlot>\n").format(PORT)
        schreiben("USBSlotAutoIO", "USB Slot I/O 40h-47h (auto)",
                  "I/O ports 40h-47h (switched I/O) of the cartridges on the USB slot bridge, detected at start", io)
        erweiterungen.append("USBSlotAutoIO")

    # Own parameters first: explicitly requested slots (-carta, -exta ...) are assigned before the bridge
    # extensions (slot="any")
    openmsx, share = openmsx_pfade()
    befehl = [openmsx, "-machine", maschine] + weitere
    for e in erweiterungen:
        befehl += ["-ext", e]
    print("Start:", " ".join(befehl[1:]))
    if share:
        os.environ["OPENMSX_SYSTEM_DATA"] = share
    return subprocess.call(befehl)


def ohne_konsole():
    """True when started with pythonw.exe (desktop shortcut): no console, output goes nowhere."""
    return sys.stdout is None or sys.executable.lower().endswith("pythonw.exe")


if __name__ == "__main__":
    if not ohne_konsole():
        sys.exit(main())
    sys.stdout = sys.stderr = open(os.devnull, "w")
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as fehler:
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, str(fehler), "openMSX USB2SLOT", 0x10)
        sys.exit(1)
