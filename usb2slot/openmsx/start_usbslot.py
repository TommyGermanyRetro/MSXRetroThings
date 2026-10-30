"""
start_usbslot.py - erkennt ueber die Bridge, was in Bridge-Slot 1 und 2 steckt, legt dazu passende Erweiterungen
an und startet den selbst gebauten openMSX damit

Erkennung mit `slottest <port> scan` (SlotLink): Slot-Expander, Inhalt je Unterslot (ROM / Daten / leer),
Switched-I/O-Kennung C0h an Port 40h (RCX/XIO) und die Mapper-Ports FCh-FFh. Daraus entstehen im
Benutzerverzeichnis von openMSX:

  USBSlotAuto1.xml, USBSlotAuto2.xml  je belegter Bridge-Slot
      - SD Mapper/Megaram 512K (Expander, ROM in Unterslot 1, Daten in Unterslot 3 und in ROM-Bank 7 der
        Nextor-Treiber "SDMapper Driver"): Nextor-Bank und SPI-Schalter 6001h in Unterslot 1 mit Sektorblock
        (Bridge-Protokoll 3), Unterslot 3 als Memory Mapper (Ports FCh-FFh antworten mit E0h..) oder MegaRAM
      - anderer Expander: <expander>true</expander> ohne weitere Regeln
      - ohne Expander: einfache Cartridge; MegaROM (slottest meldet ROMMAPPER konami/konamiscc/ascii8/ascii16)
        mit <rommapper type="..."/>
      - leerer Slot: keine Erweiterung
  USBSlotAutoIO.xml  I/O-Ports 40h-47h, wenn an Port 40h die Kennung C0h antwortet

Gemischter Betrieb: Weitere openMSX-Parameter (z. B. -carta spiel.rom, -cartb, -exta, -ext <erweiterung>)
werden VOR den Bridge-Erweiterungen uebergeben. openMSX vergibt dann zuerst die fest gewuenschten Slots, die
Bridge-Erweiterungen (slot="any") bekommen die uebrigen freien Slots. Mit --bridge 1, 2 oder 12 werden nur
diese Bridge-Slots eingebunden, damit ein Slot der Maschine fuer emulierte Cartridges frei bleibt.

Uhr der RC2014-Karte A: Mit --clock [adresse] stellt `slottest <port> clock <adresse>` vor dem Start die Uhr
PCF8583 auf die PC-Ortszeit (PCF8584 an der IO-Adresse, dezimal wie CALL I2C INIT, Vorgabe 0). Nur bei MSX1-
Maschinen (<type>MSX</type> in der Maschinenbeschreibung) und nur, wenn alle Pruefungen von slottest bestehen.

Aufruf:   python start_usbslot.py [maschine] [--port COMx] [--bridge 1|2|12] [--clock [adresse]]
                                  [weitere openMSX-Parameter]
Vorgabe:  Maschine C-BIOS_MSX2+_JP, alle belegten Bridge-Slots, Port COM16 (oder Umgebungsvariable USBSLOT_PORT),
          Uhr nicht stellen

openMSX: bin\\openmsx.exe neben diesem Skript (mit eigenem share-Ordner), sonst der selbst gebaute openMSX unter
D:\\openMSX_build mit den Systemdaten aus C:\\Program Files\\openMSX\\share. Die Umgebungsvariablen
USBSLOT_OPENMSX und USBSLOT_SHARE ueberschreiben beides.
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
CLOCK = None                                                    # IO-Adresse fuer --clock, None = Uhr nicht stellen
OHNE_FENSTER = getattr(subprocess, "CREATE_NO_WINDOW", 0)       # slottest ohne eigenes Konsolenfenster

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
    """Liefert (openmsx.exe, Systemdaten-Ordner oder None, wenn share neben der exe liegt)."""
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
    """Fuehrt slottest scan aus und liefert (slots, swio, mapperports)."""
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
        raise RuntimeError("keine Antwort von slottest scan:\n" + ausgabe)
    return slots, swio, mapperports


def geraet(nr, regeln, io=""):
    """Ein USBSlot-Geraet fuer Bridge-Slot nr in einem eigenen primaeren Slot."""
    return ("    <primary slot=\"any\">\n      <secondary slot=\"any\">\n"
            "        <USBSlot id=\"USB Slot {nr}\">\n"
            "          <mem base=\"0x0000\" size=\"0x10000\"/>\n{io}"
            "          <port>{port}</port>\n          <slot>{nr}</slot>\n{regeln}"
            "        </USBSlot>\n      </secondary>\n    </primary>\n").format(nr=nr, io=io, port=PORT, regeln=regeln)


def erweiterung_slot(nr, info, mapper_aktiv):
    """Liefert (beschreibung, xml) fuer einen Bridge-Slot oder None bei leerem Slot."""
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
    return "Slot-Expander ({})".format(belegt), geraet(nr, "          <expander>true</expander>\n")


def schreiben(name, titel, beschreibung, inhalt):
    with open(os.path.join(USER_EXT, name + ".xml"), "w", encoding="utf-8", newline="\n") as f:
        f.write(KOPF.format(name=titel, beschreibung=beschreibung) + inhalt + FUSS)


def argumente(args):
    """Liefert (maschine, auswahl, weitere): die Maschine ist das erste Argument, wenn es nicht mit '-' beginnt,
    --bridge und --port duerfen an beliebiger Stelle stehen."""
    global PORT, CLOCK
    args = list(args)
    if "--clock" in args:
        i = args.index("--clock")
        CLOCK = 0
        if i + 1 < len(args) and args[i + 1].isdigit():
            CLOCK = int(args[i + 1])
            del args[i + 1]
        if CLOCK > 254:
            raise SystemExit("--clock erwartet eine IO-Adresse von 0 bis 254")
        del args[i]
    if "--port" in args:
        i = args.index("--port")
        if i + 1 >= len(args):
            raise SystemExit("--port erwartet einen COM-Port, z. B. COM5")
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
            raise SystemExit("--bridge erwartet 1, 2 oder 12")
        auswahl = tuple(sorted({int(z) for z in wert}))
        del args[i:i + 2]
    return maschine, auswahl, args


def mapper_antwortet(mapperports):
    """True, wenn die Ports FCh-FFh als Memory Mapper antworten (nach dem Reset E0h..E3h). Ein Port, der nicht
    antwortet, liefert FFh (Datenbus vorgeladen) und zaehlt nicht als Mapper."""
    return all(w != 0xFF and (w & 0xE0) == 0xE0 for w in mapperports)


def ist_msx1(maschine, share):
    """True, wenn die Maschinenbeschreibung (Benutzer- oder Systemdaten) <type>MSX</type> angibt."""
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
    """Stellt die Uhr der Karte A ueber slottest clock; slottest schreibt nur, wenn alle Pruefungen bestehen."""
    ausgabe = subprocess.run([SLOTTEST, PORT, "clock", str(CLOCK)], capture_output=True, text=True, check=False,
                             creationflags=OHNE_FENSTER).stdout.strip()
    print("Uhr Karte A (Adresse {}): {}".format(CLOCK, ausgabe or "keine Antwort von slottest"))


def main():
    maschine, auswahl, weitere = argumente(sys.argv[1:])

    slots, swio, mapperports = scan()
    if CLOCK is not None:
        if ist_msx1(maschine, openmsx_pfade()[1] or os.path.join(os.path.dirname(openmsx_pfade()[0]), "share")):
            uhr_stellen()
        else:
            print("Uhr Karte A: nicht gestellt, {} ist keine MSX1-Maschine".format(maschine))
    mapper_aktiv = mapper_antwortet(mapperports)
    os.makedirs(USER_EXT, exist_ok=True)

    erweiterungen = []
    for nr in (1, 2):
        ergebnis = erweiterung_slot(nr, slots[nr], mapper_aktiv)
        if ergebnis is None:
            print("Bridge-Slot {}: leer".format(nr))
            continue
        if nr not in auswahl:
            print("Bridge-Slot {}: {} - nicht eingebunden (--bridge)".format(nr, ergebnis[0]))
            continue
        beschreibung, xml = ergebnis
        print("Bridge-Slot {}: {}".format(nr, beschreibung))
        schreiben("USBSlotAuto{}".format(nr), "USB Slot {} (auto)".format(nr),
                  "Slot {} of the USB slot bridge, detected at start: {}".format(nr, beschreibung), xml)
        erweiterungen.append("USBSlotAuto{}".format(nr))
    if swio:
        print("Switched I/O 40h-47h: vorhanden")
        io = ("    <USBSlot id=\"USB Slot I/O\">\n      <io base=\"0x40\" num=\"8\" type=\"IO\"/>\n"
              "      <port>{}</port>\n      <slot>1</slot>\n    </USBSlot>\n").format(PORT)
        schreiben("USBSlotAutoIO", "USB Slot I/O 40h-47h (auto)",
                  "I/O ports 40h-47h (switched I/O) of the cartridges on the USB slot bridge, detected at start", io)
        erweiterungen.append("USBSlotAutoIO")

    # Eigene Parameter zuerst: fest gewuenschte Slots (-carta, -exta ...) werden vor den Bridge-Erweiterungen
    # (slot="any") vergeben
    openmsx, share = openmsx_pfade()
    befehl = [openmsx, "-machine", maschine] + weitere
    for e in erweiterungen:
        befehl += ["-ext", e]
    print("Start:", " ".join(befehl[1:]))
    if share:
        os.environ["OPENMSX_SYSTEM_DATA"] = share
    return subprocess.call(befehl)


def ohne_konsole():
    """True beim Start mit pythonw.exe (Desktop-Verknuepfung): keine Konsole, Ausgaben gehen ins Leere."""
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
