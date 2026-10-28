//***************************************************************************************************
//* HEADER - slottest.cpp - Testprogramm fuer das Binaerprotokoll der USB-Slot-Bridge
//***************************************************************************************************
//* Aufruf:  slottest <COMx> <kommando> [parameter]
//*
//*   hello                       Protokollversion und Blockgroesse
//*   state                       Zustand der Bridge (LAEUFT/AUS), /INT-Pegel
//*   read <slot> <adr> [m1]      ein Lesezyklus (m1 = Opcode-Fetch), Zahlen hexadezimal
//*   write <slot> <adr> <wert>   ein Schreibzyklus
//*   in <port>  |  out <port> <wert>   I/O lesen / schreiben
//*   outw <port> <wert>          I/O schreiben mit Antwort, zeigt die /WAIT-Wartetakte
//*   scan                        erkennt, was in Bridge-Slot 1 und 2 steckt (fuer das Startskript)
//*   dump <slot> <adr> <laenge> [datei]   Speicherbereich blockweise lesen, hexadezimal anzeigen
//*   reset                       /RESET-Impuls
//*   bench                       Zeitmessung: einzelne Zyklen, Bloecke, gebuendelte Schreibzyklen
//*   selftest                    automatischer Protokolltest mit Ergebnis je Pruefung
//*   loop <sekunden> [block|read|mixed]   Verkehr erzeugen (fuer Messungen am Logikanalysator)
//*   watch <sekunden>            /INT-Meldungen anzeigen
//*
//* Die Bridge steht danach wieder im Textmodus ohne Ueberwachung (Option --keep laesst sie im
//* Binaermodus; ohne Herzschlag geht sie nach der Zeitgrenze in den Zustand AUS).
//***************************************************************************************************
//* Datei: slottest.cpp
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner
//***************************************************************************************************
//* VERSION: 28/10/26
//***************************************************************************************************

#define _CRT_SECURE_NO_WARNINGS
#include "slotlink.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace slotlink;

static LARGE_INTEGER freq;

static double nowUs()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return 1e6 * static_cast<double>(t.QuadPart) / static_cast<double>(freq.QuadPart);
}

static unsigned hex(const char *s) { return static_cast<unsigned>(strtoul(s, nullptr, 16)); }

static int failures = 0;
static int checks   = 0;

static void check(bool ok, const char *text)
{
    checks++;
    if (!ok)
    {
        failures++;
    }
    printf("  %s  %s\n", ok ? "OK    " : "FEHLER", text);
}

static void checkf(bool ok, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    check(ok, buf);
}

//===================================================================================================
// Selbsttest
//===================================================================================================

static void selftest(Link &link)
{
    uint8_t version = 0;
    uint16_t maxBlock = 0;
    SyncResult sy;
    ReadResult rr;
    uint8_t st = 0;

    printf("Selbsttest Binaerprotokoll\n");

    check(link.hello(version, maxBlock), "HELLO beantwortet");
    checkf(version == 3 && maxBlock == 1024, "Version %u, groesster Block %u (Soll 3, 1024)", version,
           maxBlock);

    check(link.sync(sy), "SYNC beantwortet");
    checkf(sy.running, "Bridge im Zustand LAEUFT nach dem Wechsel ins Binaerprotokoll");
    checkf(sy.intLevel == 1, "/INT frei (Pegel %u)", sy.intLevel);

    // Herzschlag und Zeitgrenze: 300 ms lang alle 20 ms RUN, dazwischen lesen
    check(link.config(100, st) && st == StOk, "CFG: Zeitgrenze 100 ms");
    bool alive = true;
    for (int i = 0; i < 15; i++)
    {
        link.run();
        alive = alive && link.memRead(1, 0x4000, false, rr) && rr.status == StOk;
        Sleep(20);
    }
    check(alive, "300 ms lang lesen mit Herzschlag alle 20 ms ohne Fehler");
    check(link.sync(sy) && sy.running, "Bridge laeuft weiter (Herzschlag genuegt)");

    // Einzelzyklen
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk && rr.waits == 0, "MRD Slot 1, 4000h");
    check(link.memRead(2, 0x8000, false, rr) && rr.status == StOk && rr.waits == 0, "MRD Slot 2, 8000h");
    check(link.memRead(1, 0x4000, true, rr) && rr.status == StOk && rr.waits == 0, "MRD mit M1 (Opcode-Fetch)");
    check(link.ioRead(0x98, rr) && rr.status == StOk && rr.waits == 0, "IORD Port 98h");
    check(link.memRead(3, 0x4000, false, rr) && rr.status == StArg, "MRD mit ungueltigem Slot 3 -> Argumentfehler");

    // Gebuendelte Schreibzyklen ohne Antwort, danach SYNC
    double t0 = nowUs();
    for (int i = 0; i < 1000; i++)
    {
        link.memWrite(1, static_cast<uint16_t>(0x4000 + i), static_cast<uint8_t>(i));
    }
    check(link.sync(sy), "1000 MWR gebuendelt, danach SYNC");
    double t1 = nowUs();
    checkf(sy.errors == 0, "keine verworfenen Schreibzyklen (%u)", sy.errors);
    printf("        1000 MWR + SYNC in %.0f us = %.2f us je Zyklus\n", t1 - t0, (t1 - t0) / 1000.0);
    link.ioWrite(0x98, 0x55);
    check(link.sync(sy) && sy.errors == 0, "IOWR Port 98h");

    // Blocklesen
    std::vector<uint8_t> blk;
    uint16_t waits = 0;
    check(link.memReadBlock(1, 0x4000, 256, blk, waits, st) && st == StOk && blk.size() == 256, "MRDN 256 Bytes");
    checkf(waits == 0, "MRDN ohne Wartetakte (%u)", waits);
    check(link.memReadBlock(1, 0x4000, 1024, blk, waits, st) && st == StOk && blk.size() == 1024, "MRDN 1024 Bytes");
    check(link.memReadBlock(1, 0x4000, 1025, blk, waits, st) && st == StArg, "MRDN 1025 -> Argumentfehler");
    check(link.memReadBlock(1, 0x4000, 0, blk, waits, st) && st == StArg, "MRDN 0 -> Argumentfehler");
    check(link.memReadBlock(3, 0x4000, 16, blk, waits, st) && st == StArg, "MRDN Slot 3 -> Argumentfehler");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk, "Datenstrom nach Fehlern weiter synchron");

    // /RESET-Impuls
    t0 = nowUs();
    check(link.reset(st) && st == StOk, "RESET beantwortet");
    t1 = nowUs();
    checkf(t1 - t0 > 9000 && t1 - t0 < 20000, "Antwort erst nach dem Impuls (%.1f ms, Soll 10 ms)", (t1 - t0) / 1000.0);

    // /INT ueber die Testleitung (Bruecke Pin 29 -> Pin 5)
    uint8_t level = 9;
    link.testLine(false);
    link.pollEvents(50);
    check(link.popInt(level) && level == 0, "Meldung /INT LOW nach TEST LOW");
    link.testLine(true);
    link.pollEvents(50);
    check(link.popInt(level) && level == 1, "Meldung /INT HIGH nach TEST HIGH");

    // Wartetakte: TEST bremst auch /WAIT; ohne Wartetakt-Testmodus bleibt der Zyklus unbeeinflusst
    check(link.memRead(1, 0x4000, false, rr) && rr.waits == 0, "MRD ohne /WAIT-Bremse: 0 Wartetakte");

    // Zustand AUS: 300 ms ohne Herzschlag bei Zeitgrenze 100 ms
    Sleep(300);
    check(link.sync(sy) && !sy.running, "ohne Herzschlag: Bridge im Zustand AUS");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOff && rr.data == 0xFF, "MRD in AUS: Status AUS, Daten FFh");
    link.memWrite(1, 0x4000, 0x11);
    check(link.sync(sy) && sy.errors == 1, "MWR in AUS wird verworfen und gezaehlt");
    check(link.memReadBlock(1, 0x4000, 8, blk, waits, st) && st == StOff && blk.size() == 8 && blk[0] == 0xFF,
          "MRDN in AUS: Status AUS, Daten FFh");
    check(link.reset(st) && st == StOff, "RESET in AUS: Status AUS");
    link.run();
    check(link.sync(sy) && sy.running, "RUN weckt die Bridge aus AUS");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk, "MRD nach dem Wecken");

    check(link.config(0, st) && st == StOk, "CFG: Ueberwachung aus");
    check(link.config(60001, st) && st == StArg, "CFG: 60001 ms -> Argumentfehler");

    printf("\n%d Pruefungen, %d Fehler\n", checks, failures);
}

//===================================================================================================
// Zeitmessung
//===================================================================================================

static void bench(Link &link)
{
    ReadResult rr;
    SyncResult sy;
    uint8_t st;
    std::vector<uint8_t> blk;
    uint16_t waits;
    std::vector<double> v;

    link.config(1000, st);
    link.run();

    for (int i = 0; i < 200; i++)
    {
        link.memRead(1, 0x4000, false, rr);
    }
    for (int i = 0; i < 5000; i++)
    {
        double t0 = nowUs();
        link.memRead(1, static_cast<uint16_t>(0x4000 + (i & 0xFF)), false, rr);
        v.push_back(nowUs() - t0);
        if ((i & 255) == 0)
        {
            link.run();
        }
    }
    std::sort(v.begin(), v.end());
    printf("MRD einzeln (5000): Min %.0f us, Median %.0f us, 99 %% %.0f us, Max %.0f us\n", v.front(),
           v[v.size() / 2], v[v.size() * 99 / 100], v.back());

    for (int nn : { 16, 64, 256, 1024 })
    {
        const uint16_t n = static_cast<uint16_t>(nn);
        v.clear();
        for (int i = 0; i < 200; i++)
        {
            uint8_t s;
            double t0 = nowUs();
            link.memReadBlock(1, 0x4000, n, blk, waits, s);
            v.push_back(nowUs() - t0);
            if ((i & 15) == 0)
            {
                link.run();
            }
        }
        std::sort(v.begin(), v.end());
        printf("MRDN %4u Bytes (200): Median %.0f us = %.2f us je Byte = %.0f kByte/s\n", n, v[v.size() / 2],
               v[v.size() / 2] / n, n * 1000.0 / v[v.size() / 2]);
    }

    for (int n : { 100, 1000, 5000 })
    {
        double t0 = nowUs();
        for (int i = 0; i < n; i++)
        {
            link.memWrite(1, static_cast<uint16_t>(0x4000 + (i & 0xFF)), static_cast<uint8_t>(i));
        }
        link.sync(sy);
        double t1 = nowUs();
        printf("%5d MWR gebuendelt + SYNC: %.0f us = %.2f us je Zyklus\n", n, t1 - t0, (t1 - t0) / n);
        link.run();
    }
    link.config(0, st);
}

//===================================================================================================
// Hauptprogramm
//===================================================================================================

// Erkennung fuer das Startskript: was steckt in Bridge-Slot 1 und 2. Geschrieben werden nur FFFFh
// (Expander-Pruefung, danach 00h bzw. alter Wert) und Port 40h (Switched-I/O-Kennung C0h, danach 00h).
// Ausgabe zeilenweise:
//   SLOT <s> EXPANDER <0|1>
//   SUB <s> <unterslot> <ROM|DATA|EMPTY>    ROM = "AB" bei 4000h/8000h, EMPTY = nur FFh, DATA = sonst
//   SWIO <0|1>                               1 = an Port 40h antwortet die Kennung C0h (RCX/XIO)
//   MAPPERPORTS <FC> <FD> <FE> <FF>

static void scan(Link &link)
{
    ReadResult rr;
    uint8_t st = 0;

    auto rd = [&](uint8_t slot, uint16_t adr) -> int
    {
        if (!link.memRead(slot, adr, false, rr) || rr.status != StOk)
        {
            return -1;
        }
        return rr.data;
    };

    auto classify = [&](uint8_t slot) -> const char *
    {
        bool rom = false;
        bool data = false;
        static const uint16_t bases[] = { 0x4000, 0x8000, 0xC000 };
        for (uint16_t base : bases)
        {
            if (base != 0xC000 && rd(slot, base) == 0x41 && rd(slot, static_cast<uint16_t>(base + 1)) == 0x42)
            {
                rom = true;
            }
            std::vector<uint8_t> blk;
            uint16_t waits = 0;
            uint8_t status = 0;
            if (link.memReadBlock(slot, base, 64, blk, waits, status) && status == StOk)
            {
                for (uint8_t v : blk)
                {
                    if (v != 0xFF)
                    {
                        data = true;
                    }
                }
            }
        }
        return rom ? "ROM" : (data ? "DATA" : "EMPTY");
    };

    link.reset(st);
    for (uint8_t slot = 1; slot <= 2; slot++)
    {
        link.run();
        int old = rd(slot, 0xFFFF);
        link.memWrite(slot, 0xFFFF, 0x5A);
        bool expander = (rd(slot, 0xFFFF) == 0xA5);
        printf("SLOT %u EXPANDER %u\n", slot, expander ? 1 : 0);
        if (expander)
        {
            const char *kind[4] = {};
            for (unsigned sub = 0; sub < 4; sub++)
            {
                link.memWrite(slot, 0xFFFF, static_cast<uint8_t>(sub * 0x55));
                kind[sub] = classify(slot);
                printf("SUB %u %u %s\n", slot, sub, kind[sub]);
            }
            link.memWrite(slot, 0xFFFF, 0x00);
            // Nextor-Treiber eines SD Mappers: Kopf in ROM-Bank 7 von Unterslot 1 ("NEXTOR_DRIVER" bei 4100h,
            // Treibername bei 4110h); danach wieder Bank 0 und Expander-Register 00h
            if (strcmp(kind[1], "ROM") == 0 && strcmp(kind[3], "DATA") == 0)
            {
                link.memWrite(slot, 0xFFFF, 0x04);
                link.memWrite(slot, 0x6000, 0x07);
                std::vector<uint8_t> head;
                uint16_t waits = 0;
                uint8_t status = 0;
                bool ok = link.memReadBlock(slot, 0x4100, 0x30, head, waits, status) && status == StOk &&
                          head.size() == 0x30 && memcmp(head.data(), "NEXTOR_DRIVER", 14) == 0;
                link.memWrite(slot, 0x6000, 0x00);
                link.memWrite(slot, 0xFFFF, 0x00);
                std::string name;
                if (ok)
                {
                    name.assign(reinterpret_cast<const char *>(head.data() + 0x10), 32);
                    while (!name.empty() && name.back() == ' ')
                    {
                        name.pop_back();
                    }
                }
                printf("DRIVER %u %s\n", slot, name.empty() ? "-" : name.c_str());
            }
        }
        else
        {
            if (old >= 0)
            {
                link.memWrite(slot, 0xFFFF, static_cast<uint8_t>(old));
            }
            printf("SUB %u 0 %s\n", slot, classify(slot));
        }
    }

    link.ioWrite(0x40, 0xC0);
    bool swio = link.ioRead(0x40, rr) && rr.data == 0x3F;
    link.ioWrite(0x40, 0x00);
    printf("SWIO %u\n", swio ? 1 : 0);

    printf("MAPPERPORTS");
    for (uint8_t port = 0xFC; port != 0x00; port++)
    {
        printf(" %02X", link.ioRead(port, rr) ? rr.data : 0xFF);
    }
    printf("\n");
    SyncResult sy;
    link.sync(sy);
}

static void usage()
{
    printf("Aufruf: slottest <COMx> <kommando> [parameter] [--keep]\n");
    printf("  hello | state | read <slot> <adr> [m1] | write <slot> <adr> <wert> | in <port> | out <port> <wert> | outw <port> <wert> | scan\n");
    printf("  dump <slot> <adr> <laenge> [datei] | reset | bench | selftest | loop <sek> [block|read|mixed] | watch <sek>\n");
}

int main(int argc, char **argv)
{
    QueryPerformanceFrequency(&freq);

    bool keep = false;
    std::vector<std::string> a;
    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == "--keep")
        {
            keep = true;
        }
        else
        {
            a.push_back(argv[i]);
        }
    }
    if (a.size() < 2)
    {
        usage();
        return 2;
    }

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    Link link;
    if (!link.open(a[0]) || !link.enterBinary())
    {
        printf("Fehler: %s\n", link.error().c_str());
        return 1;
    }

    const std::string &cmd = a[1];
    ReadResult rr;
    SyncResult sy;
    uint8_t st = 0;
    int rc = 0;

    // Ueberwachung mit Herzschlag: ohne RUN geht die Bridge nach der Zeitgrenze nach AUS;
    // die Zustandsabfrage darf die Bridge nicht wecken
    if (cmd != "state")
    {
        link.run();
    }

    if (cmd == "hello")
    {
        uint8_t v;
        uint16_t m;
        if (link.hello(v, m))
        {
            printf("Protokollversion %u, groesster Block %u\n", v, m);
        }
        else
        {
            printf("Fehler: %s\n", link.error().c_str());
            rc = 1;
        }
    }
    else if (cmd == "state")
    {
        if (link.sync(sy))
        {
            printf("%s, /INT %s, verworfene Schreibzyklen %u\n", sy.running ? "LAEUFT" : "AUS",
                   sy.intLevel ? "frei" : "aktiv (LOW)", sy.errors);
        }
        else
        {
            rc = 1;
        }
    }
    else if (cmd == "read" && a.size() >= 4)
    {
        bool m1 = a.size() > 4 && a[4] == "m1";
        if (link.memRead(static_cast<uint8_t>(atoi(a[2].c_str())),
                         static_cast<uint16_t>(hex(a[3].c_str())), m1, rr))
        {
            printf("Status %u, Daten %02X, Wartetakte %u\n", rr.status, rr.data, rr.waits);
        }
        else
        {
            rc = 1;
        }
    }
    else if (cmd == "write" && a.size() >= 5)
    {
        link.memWrite(static_cast<uint8_t>(atoi(a[2].c_str())), static_cast<uint16_t>(hex(a[3].c_str())),
                      static_cast<uint8_t>(hex(a[4].c_str())));
        link.sync(sy);
        printf("Geschrieben, verworfene Schreibzyklen %u\n", sy.errors);
    }
    else if (cmd == "in" && a.size() >= 3)
    {
        if (link.ioRead(static_cast<uint8_t>(hex(a[2].c_str())), rr))
        {
            printf("Status %u, Daten %02X, Wartetakte %u\n", rr.status, rr.data, rr.waits);
        }
        else
        {
            rc = 1;
        }
    }
    else if (cmd == "out" && a.size() >= 4)
    {
        link.ioWrite(static_cast<uint8_t>(hex(a[2].c_str())), static_cast<uint8_t>(hex(a[3].c_str())));
        link.sync(sy);
        printf("Geschrieben, verworfene Schreibzyklen %u\n", sy.errors);
    }
    else if (cmd == "scan")
    {
        scan(link);
    }
    else if (cmd == "outw" && a.size() >= 4)
    {
        uint8_t status = 0;
        uint32_t waits = 0;
        if (link.ioWriteWait(static_cast<uint8_t>(hex(a[2].c_str())), static_cast<uint8_t>(hex(a[3].c_str())),
                             status, waits))
        {
            printf("Status %u, Wartetakte %lu\n", status, static_cast<unsigned long>(waits));
        }
        else
        {
            rc = 1;
        }
    }
    else if (cmd == "dump" && a.size() >= 5)
    {
        uint8_t slot = static_cast<uint8_t>(atoi(a[2].c_str()));
        unsigned addr = hex(a[3].c_str());
        unsigned len = hex(a[4].c_str());
        std::vector<uint8_t> all;
        while (all.size() < len)
        {
            std::vector<uint8_t> blk;
            uint16_t waits;
            uint16_t n = static_cast<uint16_t>(std::min<size_t>(1024, len - all.size()));
            link.run();
            if (!link.memReadBlock(slot, static_cast<uint16_t>(addr + all.size()), n, blk, waits, st) || st != StOk)
            {
                printf("Fehler beim Lesen (Status %u)\n", st);
                rc = 1;
                break;
            }
            all.insert(all.end(), blk.begin(), blk.end());
        }
        for (size_t i = 0; i < all.size(); i++)
        {
            if (i % 16 == 0)
            {
                printf("%s%04X:", i ? "\n" : "", static_cast<unsigned>(addr + i) & 0xFFFF);
            }
            printf(" %02X", all[i]);
        }
        printf("\n");
        if (a.size() > 5)
        {
            FILE *f = fopen(a[5].c_str(), "wb");
            if (f)
            {
                fwrite(all.data(), 1, all.size(), f);
                fclose(f);
                printf("%zu Bytes in %s geschrieben\n", all.size(), a[5].c_str());
            }
        }
    }
    else if (cmd == "reset")
    {
        double t0 = nowUs();
        if (link.reset(st))
        {
            printf("RESET-Impuls, Status %u, Antwort nach %.1f ms\n", st, (nowUs() - t0) / 1000.0);
        }
        else
        {
            rc = 1;
        }
    }
    else if (cmd == "bench")
    {
        bench(link);
    }
    else if (cmd == "selftest")
    {
        selftest(link);
        rc = failures ? 1 : 0;
    }
    else if (cmd == "loop" && a.size() >= 3)
    {
        double seconds = atof(a[2].c_str());
        std::string mode = a.size() > 3 ? a[3] : "block";
        std::vector<uint8_t> blk;
        uint16_t waits;
        double start = nowUs();
        unsigned long cycles = 0;
        link.config(200, st);
        while ((nowUs() - start) < seconds * 1e6)
        {
            link.run();
            if (mode == "block" || mode == "mixed")
            {
                link.memReadBlock(1, 0x4000, 256, blk, waits, st);
                cycles += 256;
            }
            if (mode == "read" || mode == "mixed")
            {
                for (int i = 0; i < 16; i++)
                {
                    link.memRead(1, static_cast<uint16_t>(0x8000 + i), false, rr);
                    cycles++;
                }
                link.memWrite(2, 0x9000, 0xA5);
                cycles++;
            }
        }
        link.sync(sy);
        printf("%lu Zyklen in %.1f s (%.0f Zyklen/s)\n", cycles, (nowUs() - start) / 1e6, cycles / ((nowUs() - start) / 1e6));
    }
    else if (cmd == "watch" && a.size() >= 3)
    {
        double seconds = atof(a[2].c_str());
        double start = nowUs();
        link.config(500, st);
        while ((nowUs() - start) < seconds * 1e6)
        {
            link.run();
            link.pollEvents(100);
            uint8_t level;
            while (link.popInt(level))
            {
                printf("%.1f s: /INT %s\n", (nowUs() - start) / 1e6, level ? "frei (HIGH)" : "aktiv (LOW)");
            }
        }
    }
    else
    {
        usage();
        rc = 2;
    }

    if (!keep)
    {
        link.config(0, st);
        link.leaveBinary();
    }
    return rc;
}
