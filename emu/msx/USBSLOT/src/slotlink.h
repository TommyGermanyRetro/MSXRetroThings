//***************************************************************************************************
//* HEADER - slotlink.h - PC-Seite des Binaerprotokolls zur USB-Slot-Bridge (Windows)
//***************************************************************************************************
//* Oeffnet die virtuelle COM-Schnittstelle der Bridge (BridgeTeensy) und spricht das Binaer-
//* protokoll aus ANALYSE.md, Abschnitt 4.3. Schreibzyklen ohne Antwort (MWR, IOWR) werden
//* gesammelt und mit dem naechsten Zyklus mit Antwort in einem USB-Paket gesendet. /INT-Meldungen
//* der Bridge koennen zwischen jeder Antwort eintreffen und werden in eine Warteschlange gelegt.
//*
//* Nur Win32-API, keine Zusatzbibliotheken.
//***************************************************************************************************
//* Datei: slotlink.h
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner
//***************************************************************************************************
//* VERSION: 21/10/26
//***************************************************************************************************

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace slotlink
{

enum Op : uint8_t
{
    OpText  = 0x00,
    OpHello = 0x01,
    OpMrd   = 0x02,
    OpMwr   = 0x03,
    OpIord  = 0x04,
    OpIowr  = 0x05,
    OpMrdn  = 0x06,
    OpReset = 0x07,
    OpRun   = 0x08,
    OpSync  = 0x09,
    OpCfg   = 0x0B,
    OpTest  = 0x0C,
    OpIowrw = 0x0D,
    EvInt   = 0xC0
};

enum Status : uint8_t
{
    StOk    = 0,
    StOff   = 1,                                        // Bridge im Zustand AUS
    StClock = 2,                                        // kein CLOCK an der Bridge
    StArg   = 3                                         // ungueltiger Parameter
};

struct ReadResult
{
    uint8_t status = StOk;
    uint8_t data   = 0xFF;
    uint8_t waits  = 0;                                 // durch /WAIT eingefuegte Takte
};

struct SyncResult
{
    bool    running  = false;                           // true = LAEUFT, false = AUS
    uint8_t intLevel = 1;                               // 0 = /INT LOW (aktiv)
    uint8_t errors   = 0;                               // seit dem letzten Sync verworfene Schreibzyklen
};

class Link
{
public:
    ~Link() { close(); }

    // COM-Port oeffnen, z. B. "COM10"
    bool open(const std::string &port)
    {
        close();
        std::string path = "\\\\.\\" + port;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                              nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            error_ = "COM-Port laesst sich nicht oeffnen (Fehler " + std::to_string(GetLastError()) + ")";
            return false;
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(dcb);
        GetCommState(handle_, &dcb);
        dcb.BaudRate     = 115200;                      // bei USB-CDC ohne Bedeutung
        dcb.ByteSize     = 8;
        dcb.Parity       = NOPARITY;
        dcb.StopBits     = ONESTOPBIT;
        dcb.fBinary      = TRUE;
        dcb.fDtrControl  = DTR_CONTROL_ENABLE;
        dcb.fRtsControl  = RTS_CONTROL_ENABLE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fOutX        = FALSE;
        dcb.fInX         = FALSE;
        SetCommState(handle_, &dcb);

        COMMTIMEOUTS to = {};
        to.ReadIntervalTimeout        = 0;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant   = timeoutMs_;
        to.WriteTotalTimeoutConstant  = 1000;
        SetCommTimeouts(handle_, &to);

        SetupComm(handle_, 1 << 16, 1 << 16);
        PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
        return true;
    }

    void close()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
        tx_.clear();
    }

    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }
    const std::string &error() const { return error_; }

    // Zeitgrenze fuer eine Antwort in ms
    void setTimeout(DWORD ms)
    {
        timeoutMs_ = ms;
        if (isOpen())
        {
            COMMTIMEOUTS to = {};
            to.ReadTotalTimeoutConstant  = ms;
            to.WriteTotalTimeoutConstant = 1000;
            SetCommTimeouts(handle_, &to);
        }
    }

    // Von Textkommandos ins Binaerprotokoll wechseln ("b", Antwort "BIN") und die Version pruefen
    bool enterBinary()
    {
        // Byte 00 holt eine Bridge aus dem Binaermodus (von einem frueheren Lauf); im Textmodus
        // ist es ein unbekanntes Zeichen und erzeugt hoechstens eine Fehlerzeile
        const char cmd[] = "\0\n\nb\n";
        if (!writeAll(reinterpret_cast<const uint8_t *>(cmd), sizeof(cmd) - 1))
        {
            return false;
        }

        // Textzeilen lesen, bis "BIN" kommt; andere Zeilen (Fehlerhinweise, Reste) werden verworfen
        std::string line;
        for (int i = 0; i < 4096; i++)
        {
            uint8_t b;
            if (!readExact(&b, 1, false))
            {
                error_ = "keine Antwort auf 'b' (Teensy nicht im Textmodus oder nicht angeschlossen)";
                return false;
            }
            if (b == '\n')
            {
                if (line.find("BIN") != std::string::npos)
                {
                    return true;
                }
                line.clear();
            }
            else if (b != '\r')
            {
                line.push_back(static_cast<char>(b));
            }
        }
        error_ = "'BIN' nicht gefunden";
        return false;
    }

    // Zurueck zu den Textkommandos
    bool leaveBinary()
    {
        tx_.push_back(OpText);
        return flush();
    }

    bool hello(uint8_t &version, uint16_t &maxBlock)
    {
        uint8_t r[5];
        tx_.push_back(OpHello);
        if (!flush() || !readReply(OpHello, r, 5))
        {
            return false;
        }
        version  = r[2];
        maxBlock = static_cast<uint16_t>(r[3] | (r[4] << 8));
        return r[1] == StOk;
    }

    // Speicher lesen; m1 = Opcode-Fetch mit Refresh
    bool memRead(uint8_t slot, uint16_t addr, bool m1, ReadResult &out)
    {
        tx_.insert(tx_.end(), { OpMrd, slot, lo(addr), hi(addr), static_cast<uint8_t>(m1 ? 1 : 0) });
        return readCycle(OpMrd, out);
    }

    // Speicher schreiben ohne Antwort (wird mit dem naechsten Zyklus gesendet oder mit flush())
    void memWrite(uint8_t slot, uint16_t addr, uint8_t value)
    {
        tx_.insert(tx_.end(), { OpMwr, slot, lo(addr), hi(addr), value });
    }

    bool ioRead(uint8_t port, ReadResult &out)
    {
        tx_.insert(tx_.end(), { OpIord, port });
        return readCycle(OpIord, out);
    }

    void ioWrite(uint8_t port, uint8_t value)
    {
        tx_.insert(tx_.end(), { OpIowr, port, value });
    }

    // I/O schreiben mit Antwort: liefert die durch /WAIT eingefuegten Takte
    bool ioWriteWait(uint8_t port, uint8_t value, uint8_t &status, uint32_t &waits)
    {
        uint8_t r[6];
        tx_.insert(tx_.end(), { OpIowrw, port, value });
        if (!flush() || !readReply(OpIowrw, r, 6))
        {
            return false;
        }
        status = r[1];
        waits  = static_cast<uint32_t>(r[2]) | (static_cast<uint32_t>(r[3]) << 8) |
                 (static_cast<uint32_t>(r[4]) << 16) | (static_cast<uint32_t>(r[5]) << 24);
        return true;
    }

    // n aufeinanderfolgende Lesezyklen ab addr (n = 1 bis maxBlock)
    bool memReadBlock(uint8_t slot, uint16_t addr, uint16_t n, std::vector<uint8_t> &out, uint16_t &waits,
                      uint8_t &status)
    {
        tx_.insert(tx_.end(), { OpMrdn, slot, lo(addr), hi(addr), lo(n), hi(n) });
        if (!flush())
        {
            return false;
        }

        uint8_t head[2];
        if (!readReply(OpMrdn, head, 2))
        {
            return false;
        }
        status = head[1];
        if (status == StArg)
        {
            out.clear();
            waits = 0;
            return true;                                // Argumentfehler: die Antwort besteht nur aus dem Kopf
        }

        out.assign(n, 0);
        if (!readExact(out.data(), n, true))
        {
            return false;
        }
        uint8_t w[2];
        if (!readExact(w, 2, true))
        {
            return false;
        }
        waits = static_cast<uint16_t>(w[0] | (w[1] << 8));
        return true;
    }

    // /RESET-Impuls der Bridge (10 ms), Antwort nach dem Impuls
    bool reset(uint8_t &status)
    {
        uint8_t r[2];
        tx_.push_back(OpReset);
        if (!flush() || !readReply(OpReset, r, 2))
        {
            return false;
        }
        status = r[1];
        return true;
    }

    // Herzschlag; ohne Herzschlag geht die Bridge nach der Zeitgrenze in den Zustand AUS
    bool run()
    {
        tx_.push_back(OpRun);
        return flush();
    }

    // Wartet, bis alle vorherigen Zyklen ausgefuehrt sind, und liefert den Zustand
    bool sync(SyncResult &out)
    {
        uint8_t r[5];
        tx_.push_back(OpSync);
        if (!flush() || !readReply(OpSync, r, 5))
        {
            return false;
        }
        out.running  = r[2] != 0;
        out.intLevel = r[3];
        out.errors   = r[4];
        return r[1] == StOk;
    }

    // Zeitgrenze fuer den Herzschlag in ms (0 = keine Ueberwachung)
    bool config(uint16_t ms, uint8_t &status)
    {
        uint8_t r[2];
        tx_.insert(tx_.end(), { OpCfg, lo(ms), hi(ms) });
        if (!flush() || !readReply(OpCfg, r, 2))
        {
            return false;
        }
        status = r[1];
        return true;
    }

    // Testleitung der Bridge (Bruecken nach /WAIT und /INT)
    bool testLine(bool level)
    {
        uint8_t r[2];
        tx_.insert(tx_.end(), { OpTest, static_cast<uint8_t>(level ? 1 : 0) });
        return flush() && readReply(OpTest, r, 2);
    }

    // Zeitgrenze abwarten und nur /INT-Meldungen einsammeln (fuer Tests)
    void pollEvents(DWORD ms)
    {
        DWORD start = GetTickCount();
        setTimeout(20);
        while (GetTickCount() - start < ms)
        {
            uint8_t b;
            DWORD n = 0;
            if (!ReadFile(handle_, &b, 1, &n, nullptr) || n == 0)
            {
                continue;
            }
            if (b == EvInt)
            {
                uint8_t level;
                if (readExact(&level, 1, false))
                {
                    ints_.push_back(level);
                }
            }
        }
        setTimeout(500);
    }

    // Vorhandene /INT-Meldungen ohne Warten einsammeln (ausserhalb einer Anfrage aufrufen)
    void poll()
    {
        if (!isOpen())
        {
            return;
        }
        COMSTAT stat = {};
        DWORD errors = 0;
        if (!ClearCommError(handle_, &errors, &stat))
        {
            return;
        }
        while (stat.cbInQue >= 2)
        {
            uint8_t b[2];
            DWORD n = 0;
            if (!ReadFile(handle_, b, 2, &n, nullptr) || n != 2)
            {
                return;
            }
            if (b[0] == EvInt)
            {
                ints_.push_back(b[1]);
            }
            stat.cbInQue -= 2;
        }
    }

    // Naechste /INT-Meldung aus der Warteschlange, false = keine
    bool popInt(uint8_t &level)
    {
        if (ints_.empty())
        {
            return false;
        }
        level = ints_.front();
        ints_.pop_front();
        return true;
    }

    // Gesammelte Rahmen senden
    bool flush()
    {
        if (tx_.empty())
        {
            return true;
        }
        bool ok = writeAll(tx_.data(), tx_.size());
        tx_.clear();
        return ok;
    }

private:
    static uint8_t lo(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }
    static uint8_t hi(uint16_t v) { return static_cast<uint8_t>(v >> 8); }

    bool writeAll(const uint8_t *p, size_t n)
    {
        DWORD written = 0;
        if (!WriteFile(handle_, p, static_cast<DWORD>(n), &written, nullptr) || written != n)
        {
            error_ = "Schreiben zur Bridge fehlgeschlagen";
            return false;
        }
        return true;
    }

    // Genau n Bytes lesen; mit skipEvents werden /INT-Meldungen (0xC0, Pegel) dazwischen abgefangen
    bool readExact(uint8_t *p, size_t n, bool skipEvents)
    {
        size_t got = 0;
        while (got < n)
        {
            DWORD r = 0;
            if (!ReadFile(handle_, p + got, static_cast<DWORD>(n - got), &r, nullptr) || r == 0)
            {
                error_ = "Zeitueberschreitung beim Lesen von der Bridge";
                return false;
            }
            got += r;
        }
        (void)skipEvents;
        return true;
    }

    // Kopf einer Antwort lesen; /INT-Meldungen davor werden in die Warteschlange gelegt
    bool readReply(uint8_t op, uint8_t *buf, size_t total)
    {
        for (;;)
        {
            uint8_t b;
            if (!readExact(&b, 1, false))
            {
                return false;
            }
            if (b == EvInt)
            {
                uint8_t level;
                if (!readExact(&level, 1, false))
                {
                    return false;
                }
                ints_.push_back(level);
                continue;
            }
            if (b != (op | 0x80))
            {
                error_ = "unerwartete Antwortkennung " + std::to_string(b);
                return false;
            }
            buf[0] = b;
            return total == 1 || readExact(buf + 1, total - 1, false);
        }
    }

    bool readCycle(uint8_t op, ReadResult &out)
    {
        uint8_t r[4];
        if (!flush() || !readReply(op, r, 4))
        {
            return false;
        }
        out.status = r[1];
        out.data   = r[2];
        out.waits  = r[3];
        return true;
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    DWORD timeoutMs_ = 500;
    std::string error_;
    std::vector<uint8_t> tx_;
    std::deque<uint8_t> ints_;
};

} // namespace slotlink
