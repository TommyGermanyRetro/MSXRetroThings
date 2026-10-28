#include "USBSLOT/UsbSlot.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "slotlink.h"

#include <utility>

namespace usbslot {

namespace {

// Protocol 3 adds spaced block reads (MRDNG); with protocol 2 they are refused.
constexpr std::uint8_t OldestProtocol = 2;
constexpr std::uint8_t SpacedProtocol = 3;
constexpr auto HeartbeatInterval = std::chrono::milliseconds(20);
// Half the time limit of the board (200 ms): a longer gap may have switched the cartridges off.
constexpr auto OffCheckGap = std::chrono::milliseconds(100);

class SerialTransport final : public Transport {
public:
    bool open(const std::string& port, std::string& error)
    {
        if (!link.open(port)) {
            error = "cannot open " + port + ": " + link.error();
            return false;
        }
        if (!link.enterBinary()) {
            error = "no answer from the board on " + port + ": " + link.error();
            return false;
        }
        std::uint8_t version = 0;
        std::uint16_t maxBlock = 0;
        if (!link.hello(version, maxBlock) || version < OldestProtocol || version > SpacedProtocol) {
            error = "unsupported protocol version " + std::to_string(version) + " on " + port;
            return false;
        }
        protocol = version;
        return link.run();
    }

    bool memRead(std::uint8_t slot, std::uint16_t address, bool m1, std::uint8_t& data, std::uint32_t& waits) override
    {
        slotlink::ReadResult result;
        if (!link.memRead(slot, address, m1, result)) return false;
        data = result.data;
        waits = result.waits;
        return true;
    }

    bool memWrite(std::uint8_t slot, std::uint16_t address, std::uint8_t value) override
    {
        link.memWrite(slot, address, value);
        return true;
    }

    bool memReadBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count,
                      std::vector<std::uint8_t>& data) override
    {
        std::uint16_t waits = 0;
        std::uint8_t status = 0;
        if (!link.memReadBlock(slot, address, count, data, waits, status)) return false;
        if (status != slotlink::StOk) data.clear();
        return true;
    }

    bool memReadBlockSpaced(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::uint8_t gap,
                            std::vector<std::uint8_t>& data) override
    {
        data.clear();
        if (protocol < SpacedProtocol) return true;
        std::uint16_t waits = 0;
        std::uint8_t status = 0;
        if (!link.memReadBlock(slot, address, count, data, waits, status, gap)) return false;
        if (status != slotlink::StOk) data.clear();
        return true;
    }

    bool ioRead(std::uint8_t port, std::uint8_t& data, std::uint32_t& waits) override
    {
        slotlink::ReadResult result;
        if (!link.ioRead(port, result)) return false;
        data = result.data;
        waits = result.waits;
        return true;
    }

    bool ioWrite(std::uint8_t port, std::uint8_t value, std::uint32_t& waits) override
    {
        std::uint8_t status = 0;
        return link.ioWriteWait(port, value, status, waits);
    }

    bool reset() override
    {
        std::uint8_t status = 0;
        return link.reset(status);
    }

    bool run() override { return link.run(); }

    bool sync(bool& running, std::uint8_t& intLevel) override
    {
        slotlink::SyncResult result;
        if (!link.sync(result)) return false;
        running = result.running;
        intLevel = result.intLevel;
        return true;
    }

    // Link::poll() only reads what is waiting and has no failure of its own.
    bool poll() override
    {
        link.poll();
        return true;
    }

    bool popInterrupt(std::uint8_t& level) override { return link.popInt(level); }

    std::string error() const override { return link.error(); }

private:
    slotlink::Link link;
    std::uint8_t protocol = OldestProtocol;
};

} // namespace

std::unique_ptr<Transport> openSerial(const std::string& port, std::string& error)
{
    auto transport = std::make_unique<SerialTransport>();
    if (!transport->open(port, error)) return nullptr;
    return transport;
}

Bridge::Bridge(std::unique_ptr<Transport> transport)
    : transport(std::move(transport)),
      lastRun(std::chrono::steady_clock::now())
{
    if (!this->transport) {
        failed = true;
        error = "no connection";
    }
}

bool Bridge::connected() const noexcept
{
    return !failed;
}

const std::string& Bridge::lastError() const noexcept
{
    return error;
}

void Bridge::fail()
{
    failed = true;
    error = transport->error();
}

void Bridge::takeInterrupts()
{
    std::uint8_t level = 1;
    while (transport->popInterrupt(level)) intLow = level == 0;
}

void Bridge::sendRun(std::chrono::steady_clock::time_point now)
{
    if (now - lastRun >= OffCheckGap) {
        bool running = true;
        std::uint8_t intLevel = 1;
        if (!transport->sync(running, intLevel)) {
            fail();
            return;
        }
        // The level of the answer is newer than the reports queued before it
        std::uint8_t older = 1;
        while (transport->popInterrupt(older)) {
        }
        intLow = intLevel == 0;
        if (!running) switchedOff = true;
    }
    lastRun = now;
    if (!transport->run()) fail();
}

void Bridge::keepAlive()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - lastRun < HeartbeatInterval) return;
    sendRun(now);
}

std::uint8_t Bridge::readMemory(std::uint8_t slot, std::uint16_t address, bool m1, std::uint32_t& waits)
{
    waits = 0;
    std::uint8_t data = 0xFF;
    if (failed) return data;
    keepAlive();
    if (!failed && !transport->memRead(slot, address, m1, data, waits)) {
        fail();
        waits = 0;
        return 0xFF;
    }
    takeInterrupts();
    return data;
}

void Bridge::writeMemory(std::uint8_t slot, std::uint16_t address, std::uint8_t value)
{
    if (failed) return;
    keepAlive();
    if (!failed && !transport->memWrite(slot, address, value)) fail();
}

bool Bridge::readBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::vector<std::uint8_t>& data)
{
    data.clear();
    if (failed) return false;
    keepAlive();
    if (!failed && !transport->memReadBlock(slot, address, count, data)) fail();
    if (!failed) takeInterrupts();
    if (failed || data.size() != count) {
        data.clear();
        return false;
    }
    return true;
}

bool Bridge::readBlockSpaced(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::uint8_t gap,
                             std::vector<std::uint8_t>& data)
{
    data.clear();
    if (failed) return false;
    keepAlive();
    if (!failed && !transport->memReadBlockSpaced(slot, address, count, gap, data)) fail();
    if (!failed) takeInterrupts();
    if (failed || data.size() != count) {
        data.clear();
        return false;
    }
    return true;
}

std::uint8_t Bridge::readIo(std::uint8_t port, std::uint32_t& waits)
{
    waits = 0;
    std::uint8_t data = 0xFF;
    if (failed) return data;
    keepAlive();
    if (!failed && !transport->ioRead(port, data, waits)) {
        fail();
        waits = 0;
        return 0xFF;
    }
    takeInterrupts();
    return data;
}

void Bridge::writeIo(std::uint8_t port, std::uint8_t value, std::uint32_t& waits)
{
    waits = 0;
    if (failed) return;
    keepAlive();
    if (!failed && !transport->ioWrite(port, value, waits)) {
        fail();
        waits = 0;
    }
    if (!failed) takeInterrupts();
}

void Bridge::reset()
{
    if (failed) return;
    lastRun = std::chrono::steady_clock::now();
    switchedOff = false;
    if (!transport->run() || !transport->reset()) {
        fail();
        return;
    }
    // The board reports only changes of /INT; take the level after the reset
    bool running = true;
    std::uint8_t intLevel = 1;
    if (!transport->sync(running, intLevel)) {
        fail();
        return;
    }
    std::uint8_t older = 1;
    while (transport->popInterrupt(older)) {
    }
    intLow = intLevel == 0;
}

void Bridge::heartbeat()
{
    if (failed) return;
    sendRun(std::chrono::steady_clock::now());
    if (!failed && !transport->poll()) fail();
    if (!failed) takeInterrupts();
}

bool Bridge::interruptRequest() const noexcept
{
    return !failed && intLow;
}

bool Bridge::takeSwitchedOff() noexcept
{
    const bool result = switchedOff;
    switchedOff = false;
    return result;
}

bool CycleTracker::begins(bool active, std::uint16_t address) noexcept
{
    const bool first = active && !(lastActive && lastAddress == address);
    lastActive = active;
    lastAddress = address;
    return first;
}

} // namespace usbslot
