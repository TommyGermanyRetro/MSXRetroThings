//***************************************************************************************************
//* HEADER - slotlink.h - PC side of the binary protocol to the USB slot bridge (Windows)
//***************************************************************************************************
//* Opens the virtual COM port of the bridge (BridgeTeensy) and speaks the binary protocol from
//* ANALYSE.md, section 4.3. Write cycles without a response (MWR, IOWR) are collected and sent in
//* one USB packet together with the next cycle that has a response. /INT messages from the bridge
//* can arrive between any two responses and are put into a queue.
//*
//* Win32 API only, no additional libraries.
//***************************************************************************************************
//* File:  slotlink.h
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner
//***************************************************************************************************
//* VERSION: 28/10/26
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
    OpMrdng = 0x0E,                                     // protocol 3 and up
    EvInt   = 0xC0
};

enum Status : uint8_t
{
    StOk    = 0,
    StOff   = 1,                                        // bridge in state OFF
    StClock = 2,                                        // no CLOCK at the bridge
    StArg   = 3                                         // invalid parameter
};

struct ReadResult
{
    uint8_t status = StOk;
    uint8_t data   = 0xFF;
    uint8_t waits  = 0;                                 // clocks inserted by /WAIT
};

struct SyncResult
{
    bool    running  = false;                           // true = RUNNING, false = OFF
    uint8_t intLevel = 1;                               // 0 = /INT LOW (active)
    uint8_t errors   = 0;                               // write cycles discarded since the last sync
};

class Link
{
public:
    ~Link() { close(); }

    // Open the COM port, e.g. "COM10"
    bool open(const std::string &port)
    {
        close();
        std::string path = "\\\\.\\" + port;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                              nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
        {
            error_ = "cannot open COM port (error " + std::to_string(GetLastError()) + ")";
            return false;
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(dcb);
        GetCommState(handle_, &dcb);
        dcb.BaudRate     = 115200;                      // irrelevant for USB CDC
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

    // Response timeout in ms
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

    // Switch from text commands to the binary protocol ("b", response "BIN") and check the version
    bool enterBinary()
    {
        // Byte 00 takes a bridge out of binary mode (left over from an earlier run); in text mode it is
        // an unknown character and produces at most an error line
        const char cmd[] = "\0\n\nb\n";
        if (!writeAll(reinterpret_cast<const uint8_t *>(cmd), sizeof(cmd) - 1))
        {
            return false;
        }

        // Read text lines until "BIN" arrives; other lines (error notes, leftovers) are discarded
        std::string line;
        for (int i = 0; i < 4096; i++)
        {
            uint8_t b;
            if (!readExact(&b, 1, false))
            {
                error_ = "no response to 'b' (Teensy not in text mode or not connected)";
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
        error_ = "'BIN' not found";
        return false;
    }

    // Back to the text commands
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

    // Read memory; m1 = opcode fetch with refresh
    bool memRead(uint8_t slot, uint16_t addr, bool m1, ReadResult &out)
    {
        tx_.insert(tx_.end(), { OpMrd, slot, lo(addr), hi(addr), static_cast<uint8_t>(m1 ? 1 : 0) });
        return readCycle(OpMrd, out);
    }

    // Write memory without a response (sent with the next cycle or with flush())
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

    // Write I/O with a response: returns the clocks inserted by /WAIT
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

    // n consecutive read cycles starting at addr (n = 1 to maxBlock); with gap > 0 (protocol 3 and
    // up) there are gap clocks between the starts of the cycles
    bool memReadBlock(uint8_t slot, uint16_t addr, uint16_t n, std::vector<uint8_t> &out, uint16_t &waits,
                      uint8_t &status, uint8_t gap = 0)
    {
        const uint8_t op = gap ? OpMrdng : OpMrdn;
        tx_.insert(tx_.end(), { op, slot, lo(addr), hi(addr), lo(n), hi(n) });
        if (gap)
        {
            tx_.push_back(gap);
        }
        if (!flush())
        {
            return false;
        }

        uint8_t head[2];
        if (!readReply(op, head, 2))
        {
            return false;
        }
        status = head[1];
        if (status == StArg)
        {
            out.clear();
            waits = 0;
            return true;                                // argument error: the response consists of the header only
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

    // /RESET pulse of the bridge (10 ms), response after the pulse
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

    // Heartbeat; without a heartbeat the bridge goes to state OFF after the timeout
    bool run()
    {
        tx_.push_back(OpRun);
        return flush();
    }

    // Waits until all previous cycles have been executed and returns the state
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

    // Heartbeat timeout in ms (0 = no monitoring)
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

    // Test line of the bridge (jumpers to /WAIT and /INT)
    bool testLine(bool level)
    {
        uint8_t r[2];
        tx_.insert(tx_.end(), { OpTest, static_cast<uint8_t>(level ? 1 : 0) });
        return flush() && readReply(OpTest, r, 2);
    }

    // Wait for the timeout and only collect /INT messages (for tests)
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

    // Collect pending /INT messages without waiting (call outside a request)
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

    // Next /INT message from the queue, false = none
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

    // Send the collected frames
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
            error_ = "write to the bridge failed";
            return false;
        }
        return true;
    }

    // Read exactly n bytes; with skipEvents, /INT messages (0xC0, level) in between are caught
    bool readExact(uint8_t *p, size_t n, bool skipEvents)
    {
        size_t got = 0;
        while (got < n)
        {
            DWORD r = 0;
            if (!ReadFile(handle_, p + got, static_cast<DWORD>(n - got), &r, nullptr) || r == 0)
            {
                error_ = "timeout while reading from the bridge";
                return false;
            }
            got += r;
        }
        (void)skipEvents;
        return true;
    }

    // Read the header of a response; /INT messages before it are put into the queue
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
                error_ = "unexpected response ID " + std::to_string(b);
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
