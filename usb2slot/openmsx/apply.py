"""
apply.py - spielt das Geraet USBSlot in einen openMSX-21.0-Quellbaum ein

Kopiert USBSlot.hh/.cc und die Bibliothek slotlink.h nach <baum>/src/memory, tragt das Geraet in
DeviceFactory.cc und in das MSVC-Projekt ein und installiert die Erweiterungen USBSlot1.xml und
USBSlot2.xml in das Benutzerverzeichnis von openMSX. Mehrfaches Ausfuehren aendert nichts.

Aufruf:   python apply.py [baum]      Vorgabe: D:\\openMSX_build\\openMSX-21.0
"""

import os
import shutil
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
BAUM = sys.argv[1] if len(sys.argv) > 1 else r"D:\openMSX_build\openMSX-21.0"
USER_EXT = os.path.join(os.path.expanduser("~"), "Documents", "openMSX", "share", "extensions")


def lesen(pfad):
    with open(pfad, encoding="utf-8", newline="") as f:
        return f.read()


def schreiben(pfad, text):
    with open(pfad, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def einfuegen(pfad, marke, neu, vor=True, schon=None):
    """Fuegt 'neu' vor oder nach der ersten Zeile mit 'marke' ein, falls 'neu' noch fehlt."""
    text = lesen(pfad)
    if (schon or neu.strip()) in text:
        return False
    nl = "\r\n" if "\r\n" in text else "\n"
    zeilen = text.split(nl)
    for i, z in enumerate(zeilen):
        if marke in z:
            zeilen.insert(i if vor else i + 1, neu.replace("\n", nl))
            schreiben(pfad, nl.join(zeilen))
            return True
    raise RuntimeError("Marke nicht gefunden: %s in %s" % (marke, pfad))


def main():
    memory = os.path.join(BAUM, "src", "memory")
    for name, quelle in (("USBSlot.hh", HIER), ("USBSlot.cc", HIER),
                         ("slotlink.h", os.path.join(HIER, "..", "SlotLink"))):
        shutil.copyfile(os.path.join(quelle, name), os.path.join(memory, name))
        print("kopiert:", name)

    fabrik = os.path.join(BAUM, "src", "DeviceFactory.cc")
    if einfuegen(fabrik, '#include "MSXRam.hh"', '#include "USBSlot.hh"'):
        print("DeviceFactory.cc: Include eingetragen")
    if einfuegen(fabrik, '} else if (type == "RAM") {', schon='type == "USBSlot"', neu=
                 '\t} else if (type == "USBSlot") {\n\t\tresult = std::make_unique<USBSlot>(conf);'):
        print("DeviceFactory.cc: Geraet eingetragen")

    proj = os.path.join(BAUM, "build", "msvc", "openmsx.vcxproj")
    if einfuegen(proj, "memory\\MSXRam.cc", '    <ClCompile Include="..\\..\\src\\memory\\USBSlot.cc" />'):
        print("openmsx.vcxproj: USBSlot.cc eingetragen")
    if einfuegen(proj, "memory\\MSXRam.hh", '    <ClInclude Include="..\\..\\src\\memory\\USBSlot.hh" />'):
        print("openmsx.vcxproj: USBSlot.hh eingetragen")
    if einfuegen(proj, "memory\\MSXRam.hh", '    <ClInclude Include="..\\..\\src\\memory\\slotlink.h" />'):
        print("openmsx.vcxproj: slotlink.h eingetragen")

    os.makedirs(USER_EXT, exist_ok=True)
    for name in sorted(n for n in os.listdir(os.path.join(HIER, "extensions")) if n.endswith(".xml")):
        shutil.copyfile(os.path.join(HIER, "extensions", name), os.path.join(USER_EXT, name))
        print("installiert:", os.path.join(USER_EXT, name))


if __name__ == "__main__":
    main()
