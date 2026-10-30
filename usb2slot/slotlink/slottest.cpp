//***************************************************************************************************
//* HEADER - slottest.cpp - test program for the binary protocol of the USB slot bridge
//***************************************************************************************************
//* Usage:   slottest <COMx> <command> [parameters]
//*
//*   hello                       protocol version and block size
//*   state                       bridge state (RUNNING/OFF), /INT level
//*   read <slot> <adr> [m1]      one read cycle (m1 = opcode fetch), numbers in hex
//*   write <slot> <adr> <value>  one write cycle
//*   in <port>  |  out <port> <value>   read / write I/O
//*   outw <port> <value>         write I/O with a response, shows the /WAIT wait clocks
//*   scan                        detects what is plugged into bridge slots 1 and 2 (for the start script)
//*   dump <slot> <adr> <length> [file]   read a memory range block by block, show it in hex
//*   reset                       /RESET pulse
//*   bench                       timing: single cycles, blocks, bundled write cycles
//*   selftest                    automatic protocol test with a result per check
//*   loop <seconds> [block|read|mixed]   generate traffic (for logic analyzer measurements)
//*   watch <seconds>             show /INT messages
//*
//* Afterwards the bridge is back in text mode without monitoring (option --keep leaves it in
//* binary mode; without a heartbeat it goes to state OFF after the timeout).
//***************************************************************************************************
//* File:  slottest.cpp
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner
//***************************************************************************************************
//* VERSION: 30/10/26
//***************************************************************************************************

#define _CRT_SECURE_NO_WARNINGS
#include "slotlink.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
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
    printf("  %s  %s\n", ok ? "OK    " : "FAIL  ", text);
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
// Self test
//===================================================================================================

static void selftest(Link &link)
{
    uint8_t version = 0;
    uint16_t maxBlock = 0;
    SyncResult sy;
    ReadResult rr;
    uint8_t st = 0;

    printf("Self test binary protocol\n");

    check(link.hello(version, maxBlock), "HELLO answered");
    checkf(version == 3 && maxBlock == 1024, "version %u, largest block %u (expected 3, 1024)", version,
           maxBlock);

    check(link.sync(sy), "SYNC answered");
    checkf(sy.running, "bridge in state RUNNING after switching to the binary protocol");
    checkf(sy.intLevel == 1, "/INT released (level %u)", sy.intLevel);

    // Heartbeat and timeout: RUN every 20 ms for 300 ms, reading in between
    check(link.config(100, st) && st == StOk, "CFG: timeout 100 ms");
    bool alive = true;
    for (int i = 0; i < 15; i++)
    {
        link.run();
        alive = alive && link.memRead(1, 0x4000, false, rr) && rr.status == StOk;
        Sleep(20);
    }
    check(alive, "read for 300 ms with a heartbeat every 20 ms without error");
    check(link.sync(sy) && sy.running, "bridge keeps running (heartbeat is enough)");

    // Single cycles
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk && rr.waits == 0, "MRD Slot 1, 4000h");
    check(link.memRead(2, 0x8000, false, rr) && rr.status == StOk && rr.waits == 0, "MRD Slot 2, 8000h");
    check(link.memRead(1, 0x4000, true, rr) && rr.status == StOk && rr.waits == 0, "MRD with M1 (opcode fetch)");
    check(link.ioRead(0x98, rr) && rr.status == StOk && rr.waits == 0, "IORD Port 98h");
    check(link.memRead(3, 0x4000, false, rr) && rr.status == StArg, "MRD with invalid slot 3 -> argument error");

    // Bundled write cycles without a response, then SYNC
    double t0 = nowUs();
    for (int i = 0; i < 1000; i++)
    {
        link.memWrite(1, static_cast<uint16_t>(0x4000 + i), static_cast<uint8_t>(i));
    }
    check(link.sync(sy), "1000 MWR bundled, then SYNC");
    double t1 = nowUs();
    checkf(sy.errors == 0, "no discarded write cycles (%u)", sy.errors);
    printf("        1000 MWR + SYNC in %.0f us = %.2f us per cycle\n", t1 - t0, (t1 - t0) / 1000.0);
    link.ioWrite(0x98, 0x55);
    check(link.sync(sy) && sy.errors == 0, "IOWR Port 98h");

    // Block read
    std::vector<uint8_t> blk;
    uint16_t waits = 0;
    check(link.memReadBlock(1, 0x4000, 256, blk, waits, st) && st == StOk && blk.size() == 256, "MRDN 256 Bytes");
    checkf(waits == 0, "MRDN without wait clocks (%u)", waits);
    check(link.memReadBlock(1, 0x4000, 1024, blk, waits, st) && st == StOk && blk.size() == 1024, "MRDN 1024 Bytes");
    check(link.memReadBlock(1, 0x4000, 1025, blk, waits, st) && st == StArg, "MRDN 1025 -> argument error");
    check(link.memReadBlock(1, 0x4000, 0, blk, waits, st) && st == StArg, "MRDN 0 -> argument error");
    check(link.memReadBlock(3, 0x4000, 16, blk, waits, st) && st == StArg, "MRDN slot 3 -> argument error");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk, "data stream still in sync after errors");

    // /RESET pulse
    t0 = nowUs();
    check(link.reset(st) && st == StOk, "RESET answered");
    t1 = nowUs();
    checkf(t1 - t0 > 9000 && t1 - t0 < 20000, "response only after the pulse (%.1f ms, expected 10 ms)", (t1 - t0) / 1000.0);

    // /INT via the test line (jumper pin 29 -> pin 5)
    uint8_t level = 9;
    link.testLine(false);
    link.pollEvents(50);
    check(link.popInt(level) && level == 0, "report /INT LOW after TEST LOW");
    link.testLine(true);
    link.pollEvents(50);
    check(link.popInt(level) && level == 1, "report /INT HIGH after TEST HIGH");

    // Wait clocks: TEST also slows down /WAIT; without the wait-clock test mode the cycle is unaffected
    check(link.memRead(1, 0x4000, false, rr) && rr.waits == 0, "MRD without /WAIT slowdown: 0 wait clocks");

    // State OFF: 300 ms without a heartbeat at a timeout of 100 ms
    Sleep(300);
    check(link.sync(sy) && !sy.running, "without heartbeat: bridge in state OFF");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOff && rr.data == 0xFF, "MRD in OFF: status OFF, data FFh");
    link.memWrite(1, 0x4000, 0x11);
    check(link.sync(sy) && sy.errors == 1, "MWR in OFF is discarded and counted");
    check(link.memReadBlock(1, 0x4000, 8, blk, waits, st) && st == StOff && blk.size() == 8 && blk[0] == 0xFF,
          "MRDN in OFF: status OFF, data FFh");
    check(link.reset(st) && st == StOff, "RESET in OFF: status OFF");
    link.run();
    check(link.sync(sy) && sy.running, "RUN wakes the bridge from OFF");
    check(link.memRead(1, 0x4000, false, rr) && rr.status == StOk, "MRD after waking");

    check(link.config(0, st) && st == StOk, "CFG: monitoring off");
    check(link.config(60001, st) && st == StArg, "CFG: 60001 ms -> argument error");

    printf("\n%d checks, %d failed\n", checks, failures);
}

//===================================================================================================
// Timing
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
    printf("MRD single (5000): Min %.0f us, Median %.0f us, 99 %% %.0f us, Max %.0f us\n", v.front(),
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
        printf("MRDN %4u bytes (200): Median %.0f us = %.2f us per byte = %.0f kByte/s\n", n, v[v.size() / 2],
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
        printf("%5d MWR bundled + SYNC: %.0f us = %.2f us per cycle\n", n, t1 - t0, (t1 - t0) / n);
        link.run();
    }
    link.config(0, st);
}

//===================================================================================================
// Main program
//===================================================================================================

// Detection for the start script: what is plugged into bridge slots 1 and 2. Only FFFFh is written
// (expander check, afterwards 00h or the old value) and port 40h (switched I/O ID C0h, afterwards 00h).
// Output line by line:
//   SLOT <s> EXPANDER <0|1>
//   SUB <s> <subslot> <ROM|DATA|EMPTY>      ROM = "AB" at 4000h/8000h, EMPTY = only FFh, DATA = otherwise
//   SWIO <0|1>                               1 = the ID C0h answers at port 40h (RCX/XIO)
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
            // Nextor driver of an SD Mapper: header in ROM bank 7 of subslot 1 ("NEXTOR_DRIVER" at 4100h,
            // driver name at 4110h); afterwards back to bank 0 and expander register 00h
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
            const char *kind = classify(slot);
            printf("SUB %u 0 %s\n", slot, kind);
            // MegaROM: guess the mapper like openMSX - count LD (nnnn),A (32h nn nn) to the bank registers of the
            // types in the 32 KB at 4000h-BFFFh after /RESET; at least 3 hits, otherwise a plain ROM
            if (strcmp(kind, "ROM") == 0)
            {
                std::vector<uint8_t> rom;
                bool ok = true;
                for (uint32_t base = 0x4000; base < 0xC000 && ok; base += 0x400)
                {
                    std::vector<uint8_t> blk;
                    uint16_t waits = 0;
                    uint8_t status = 0;
                    ok = link.memReadBlock(slot, static_cast<uint16_t>(base), 0x400, blk, waits, status) &&
                         status == StOk && blk.size() == 0x400;
                    rom.insert(rom.end(), blk.begin(), blk.end());
                }
                // 0 konami, 1 konamiscc, 2 ascii8, 3 ascii16
                unsigned hits[4] = {};
                for (size_t i = 0; ok && i + 2 < rom.size(); i++)
                {
                    if (rom[i] != 0x32)
                    {
                        continue;
                    }
                    switch (rom[i + 1] | (rom[i + 2] << 8))
                    {
                    case 0x5000: case 0x9000: case 0xB000: hits[1]++; break;
                    case 0x4000: case 0x8000: case 0xA000: hits[0]++; break;
                    case 0x6800: case 0x7800: hits[2]++; break;
                    case 0x6000: hits[0]++; hits[2]++; hits[3]++; break;
                    case 0x7000: hits[1]++; hits[2]++; hits[3]++; break;
                    case 0x77FF: hits[3]++; break;
                    default: break;
                    }
                }
                if (hits[2] > 0)
                {
                    hits[2]--;
                }
                static const char *const names[4] = { "konami", "konamiscc", "ascii8", "ascii16" };
                int best = -1;
                for (int t = 0; t < 4; t++)
                {
                    if (hits[t] > 0 && (best < 0 || hits[t] >= hits[best]))
                    {
                        best = t;
                    }
                }
                if (best >= 0 && hits[best] >= 3)
                {
                    printf("ROMMAPPER %u %s\n", slot, names[best]);
                }
            }
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

// Set the PCF8583 clock of RC2014 card A (PCF8584 at IO address adr, decimal like CALL I2C INIT) to the
// PC local time, using the registers of CALL SET TIME and CALL SET DATE of the RCX ROM (registers 0-6,
// year in RAM 10h). Written only if switched I/O answers, the RCX ROM ("RCX_INTERFACE" at 4010h) is in a
// bridge slot or subslot, the PCF8584 is idle after initialization (PIN = 1, BB = 1) and the PCF8583
// acknowledges its address A0h. Output: CLOCK <SET|NOSWIO|NORCX|NOCONTROLLER|NOCLOCK> [time]

static int clockSet(Link &link, uint8_t adr)
{
    ReadResult rr;
    auto rd = [&](uint8_t slot, uint16_t a) -> int
    {
        if (!link.memRead(slot, a, false, rr) || rr.status != StOk)
        {
            return -1;
        }
        return rr.data;
    };
    auto rcx = [&](uint8_t slot) -> bool
    {
        static const char id[] = "RCX_INTERFACE";
        std::vector<uint8_t> blk;
        uint16_t waits = 0;
        uint8_t status = 0;
        return link.memReadBlock(slot, 0x4010, sizeof id, blk, waits, status) && status == StOk &&
               std::equal(id, id + sizeof id, blk.begin());
    };

    bool rcxRom = false;
    for (uint8_t slot = 1; slot <= 2 && !rcxRom; slot++)
    {
        int old = rd(slot, 0xFFFF);
        link.memWrite(slot, 0xFFFF, 0x5A);
        if (rd(slot, 0xFFFF) == 0xA5)
        {
            for (unsigned sub = 0; sub < 4 && !rcxRom; sub++)
            {
                link.memWrite(slot, 0xFFFF, static_cast<uint8_t>(sub * 0x55));
                rcxRom = rcx(slot);
            }
            link.memWrite(slot, 0xFFFF, 0x00);
        }
        else
        {
            if (old >= 0)
            {
                link.memWrite(slot, 0xFFFF, static_cast<uint8_t>(old));
            }
            rcxRom = rcx(slot);
        }
    }

    link.ioWrite(0x40, 0xC0);
    bool swio = link.ioRead(0x40, rr) && rr.data == 0x3F;
    if (!swio || !rcxRom)
    {
        link.ioWrite(0x40, 0x00);
        printf("CLOCK %s\n", !swio ? "NOSWIO" : "NORCX");
        return 3;
    }

    // PCF8584 behind the switched I/O: 46h register address, 47h data; S0 at A0 = 0, S1 at A0 = 1
    const uint8_t s0 = adr;
    const uint8_t s1 = static_cast<uint8_t>(adr + 1);
    auto wr = [&](uint8_t reg, uint8_t value)
    {
        link.ioWrite(0x46, reg);
        link.ioWrite(0x47, value);
    };
    auto status = [&]() -> int
    {
        link.ioWrite(0x46, s1);
        return link.ioRead(0x47, rr) ? rr.data : 0xFF;
    };
    // Up to 255 read accesses to S1, like I2CBCTRL and I2CPCTRL of the RCX ROM
    auto waitStatus = [&](uint8_t mask, uint8_t wanted, int &value) -> bool
    {
        for (int n = 0; n < 255; n++)
        {
            value = status();
            if ((value & mask) == wanted)
            {
                return true;
            }
        }
        return false;
    };
    // Write an I2C block like CORE8: START with chip address, bytes, STOP; false without acknowledge
    auto block = [&](std::initializer_list<uint8_t> bytes) -> bool
    {
        int value = 0;
        if (!waitStatus(0x01, 0x01, value))
        {
            return false;
        }
        wr(s0, 0xA0);
        wr(s1, 0xC5);
        bool ack = true;
        auto it = bytes.begin();
        for (;;)
        {
            if (!waitStatus(0x80, 0x00, value) || (value & 0x08) != 0)
            {
                ack = false;
                break;
            }
            if (it == bytes.end())
            {
                break;
            }
            wr(s0, *it++);
        }
        wr(s1, 0xC3);
        return ack;
    };
    auto bcd = [](int v) -> uint8_t { return static_cast<uint8_t>((((v / 10) % 10) << 4) | (v % 10)); };

    // Initialization like CORE6 (CALL I2C INIT): own address 55h, clock 8 MHz / SCL 90 kHz, bus idle
    wr(s1, 0x80);
    wr(s0, 0x55);
    wr(s1, 0xA0);
    wr(s0, 0x18);
    wr(s1, 0xC1);
    SYSTEMTIME t{};
    GetLocalTime(&t);
    const char *result = "SET";
    if ((status() & 0x81) != 0x81)
    {
        result = "NOCONTROLLER";
    }
    else if (!block({0x00, 0x00, 0x00, bcd(t.wSecond), bcd(t.wMinute), bcd(t.wHour)}))
    {
        result = "NOCLOCK";
    }
    else if (!block({0x05, bcd(t.wDay), bcd(t.wMonth)}) || !block({0x10, bcd(t.wYear % 100)}))
    {
        result = "NOCLOCK";
    }
    link.ioWrite(0x40, 0x00);
    SyncResult sy;
    link.sync(sy);
    printf("CLOCK %s %04u-%02u-%02u %02u:%02u:%02u\n", result, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return strcmp(result, "SET") == 0 ? 0 : 3;
}

static void usage()
{
    printf("Usage: slottest <COMx> <command> [parameters] [--keep]\n");
    printf("  hello | state | read <slot> <adr> [m1] | write <slot> <adr> <value> | in <port> | out <port> <value> | outw <port> <value> | scan | clock <adr decimal>\n");
    printf("  dump <slot> <adr> <length> [file] | reset | bench | selftest | loop <sec> [block|read|mixed] | watch <sec>\n");
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
        printf("Error: %s\n", link.error().c_str());
        return 1;
    }

    const std::string &cmd = a[1];
    ReadResult rr;
    SyncResult sy;
    uint8_t st = 0;
    int rc = 0;

    // Monitoring with heartbeat: without RUN the bridge goes to OFF after the timeout;
    // the state query must not wake the bridge
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
            printf("protocol version %u, largest block %u\n", v, m);
        }
        else
        {
            printf("Error: %s\n", link.error().c_str());
            rc = 1;
        }
    }
    else if (cmd == "state")
    {
        if (link.sync(sy))
        {
            printf("%s, /INT %s, discarded write cycles %u\n", sy.running ? "RUNNING" : "OFF",
                   sy.intLevel ? "released" : "active (LOW)", sy.errors);
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
            printf("status %u, data %02X, wait clocks %u\n", rr.status, rr.data, rr.waits);
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
        printf("written, discarded write cycles %u\n", sy.errors);
    }
    else if (cmd == "in" && a.size() >= 3)
    {
        if (link.ioRead(static_cast<uint8_t>(hex(a[2].c_str())), rr))
        {
            printf("status %u, data %02X, wait clocks %u\n", rr.status, rr.data, rr.waits);
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
        printf("written, discarded write cycles %u\n", sy.errors);
    }
    else if (cmd == "scan")
    {
        scan(link);
    }
    else if (cmd == "clock" && a.size() >= 3)
    {
        rc = clockSet(link, static_cast<uint8_t>(atoi(a[2].c_str())));
    }
    else if (cmd == "outw" && a.size() >= 4)
    {
        uint8_t status = 0;
        uint32_t waits = 0;
        if (link.ioWriteWait(static_cast<uint8_t>(hex(a[2].c_str())), static_cast<uint8_t>(hex(a[3].c_str())),
                             status, waits))
        {
            printf("status %u, wait clocks %lu\n", status, static_cast<unsigned long>(waits));
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
                printf("read error (status %u)\n", st);
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
                printf("%zu bytes written to %s\n", all.size(), a[5].c_str());
            }
        }
    }
    else if (cmd == "reset")
    {
        double t0 = nowUs();
        if (link.reset(st))
        {
            printf("RESET pulse, status %u, response after %.1f ms\n", st, (nowUs() - t0) / 1000.0);
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
        printf("%lu cycles in %.1f s (%.0f cycles/s)\n", cycles, (nowUs() - start) / 1e6, cycles / ((nowUs() - start) / 1e6));
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
                printf("%.1f s: /INT %s\n", (nowUs() - start) / 1e6, level ? "released (HIGH)" : "active (LOW)");
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
