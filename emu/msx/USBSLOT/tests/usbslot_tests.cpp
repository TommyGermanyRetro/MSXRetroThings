#include "USBSLOT/BoardSlot.hpp"
#include "USBSLOT/UsbSlot.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// Records every call; memory reads return the low byte of the address, I/O reads the port plus one.
struct Log {
    std::vector<std::string> calls;
    bool broken = false;
    std::uint32_t waits = 0;
    bool running = true;
    std::uint8_t intLevel = 1;
    std::deque<std::uint8_t> interrupts;
};

class FakeTransport final : public usbslot::Transport {
public:
    explicit FakeTransport(Log& log) : log(log) {}

    bool memRead(std::uint8_t slot, std::uint16_t address, bool m1, std::uint8_t& data, std::uint32_t& waits) override
    {
        log.calls.push_back("MRD " + std::to_string(slot) + " " + std::to_string(address) + (m1 ? " M1" : ""));
        if (log.broken) return false;
        data = static_cast<std::uint8_t>(address);
        waits = log.waits;
        return true;
    }

    bool memWrite(std::uint8_t slot, std::uint16_t address, std::uint8_t value) override
    {
        log.calls.push_back("MWR " + std::to_string(slot) + " " + std::to_string(address) + " " + std::to_string(value));
        return !log.broken;
    }

    bool memReadBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::vector<std::uint8_t>& data) override
    {
        log.calls.push_back("MRDN " + std::to_string(slot) + " " + std::to_string(address) + " " + std::to_string(count));
        if (log.broken) return false;
        data.assign(count, 0);
        return true;
    }

    bool ioRead(std::uint8_t port, std::uint8_t& data, std::uint32_t& waits) override
    {
        log.calls.push_back("IORD " + std::to_string(port));
        if (log.broken) return false;
        data = static_cast<std::uint8_t>(port + 1);
        waits = log.waits;
        return true;
    }

    bool ioWrite(std::uint8_t port, std::uint8_t value, std::uint32_t& waits) override
    {
        log.calls.push_back("IOWR " + std::to_string(port) + " " + std::to_string(value));
        waits = log.waits;
        return !log.broken;
    }

    bool reset() override
    {
        log.calls.push_back("RESET");
        return !log.broken;
    }

    bool run() override
    {
        log.calls.push_back("RUN");
        return !log.broken;
    }

    bool sync(bool& running, std::uint8_t& intLevel) override
    {
        log.calls.push_back("SYNC");
        running = log.running;
        intLevel = log.intLevel;
        return !log.broken;
    }

    bool poll() override { return !log.broken; }

    bool popInterrupt(std::uint8_t& level) override
    {
        if (log.interrupts.empty()) return false;
        level = log.interrupts.front();
        log.interrupts.pop_front();
        return true;
    }

    std::string error() const override { return "fake failure"; }

private:
    Log& log;
};

void testForwarding()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    require(bridge.connected(), "a bridge with a transport is connected");

    std::uint32_t waits = 99;
    log.waits = 3;
    require(bridge.readMemory(2, 0x4012, true, waits) == 0x12, "memory read returns the cartridge data");
    require(waits == 3, "memory read reports the wait clocks");
    bridge.writeMemory(1, 0x6000, 5);
    require(bridge.readIo(0x40, waits) == 0x41 && waits == 3, "I/O read returns data and wait clocks");
    bridge.writeIo(0x46, 0x21, waits);
    require(waits == 3, "I/O write reports the wait clocks");

    const std::vector<std::string> expected = {"MRD 2 16402 M1", "MWR 1 24576 5", "IORD 64", "IOWR 70 33"};
    require(log.calls == expected, "every access goes to the board once, with slot, address and M1");
}

void testResetWakesTheBoard()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    bridge.reset();
    bridge.heartbeat();
    const std::vector<std::string> expected = {"RUN", "RESET", "SYNC", "RUN"};
    require(log.calls == expected, "reset sends a heartbeat before the /RESET pulse and asks for /INT after it");
}

void testInterrupts()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    require(!bridge.interruptRequest(), "/INT starts inactive");
    std::uint32_t waits = 0;
    log.interrupts = {0};
    (void)bridge.readMemory(1, 0x4000, false, waits);
    require(bridge.interruptRequest(), "an /INT report that came with an answer activates the line");
    log.interrupts = {1};
    bridge.heartbeat();
    require(!bridge.interruptRequest(), "a report taken by the heartbeat releases it");
    log.interrupts = {1};
    log.intLevel = 0;
    bridge.reset();
    require(bridge.interruptRequest() && log.interrupts.empty(),
            "after a reset the level of the SYNC answer counts, older reports are dropped");
}

void testSwitchedOffDuringPause()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    bridge.heartbeat();
    require(!bridge.takeSwitchedOff(), "nothing to report while heartbeats come");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    log.running = false;
    log.calls.clear();
    bridge.heartbeat();
    const std::vector<std::string> expected = {"SYNC", "RUN"};
    require(log.calls == expected, "after a long gap the heartbeat asks the board first");
    require(bridge.takeSwitchedOff(), "the board switched the cartridges off while the emulation stood still");
    require(!bridge.takeSwitchedOff(), "the report comes once");
}

void testHeartbeatAlongWithCycles()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    std::uint32_t waits = 0;
    (void)bridge.readMemory(1, 0x4000, false, waits);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    (void)bridge.readMemory(1, 0x4001, false, waits);
    (void)bridge.readMemory(1, 0x4002, false, waits);
    const std::vector<std::string> expected = {"MRD 1 16384", "RUN", "MRD 1 16385", "MRD 1 16386"};
    require(log.calls == expected, "a cycle more than 20 ms after the last heartbeat carries one along");
}

void testLostConnection()
{
    Log log;
    usbslot::Bridge bridge(std::make_unique<FakeTransport>(log));
    log.broken = true;
    std::uint32_t waits = 7;
    log.waits = 7;
    require(bridge.readMemory(1, 0x4000, false, waits) == 0xFF && waits == 0, "a failed read returns FFh");
    require(!bridge.connected(), "a failed transfer marks the connection as lost");
    require(bridge.lastError() == "fake failure", "the error text of the transport is kept");

    const auto callsBefore = log.calls.size();
    bridge.writeMemory(1, 0x4000, 1);
    require(bridge.readIo(0x40, waits) == 0xFF, "I/O reads return FFh after the loss");
    bridge.writeIo(0x40, 1, waits);
    bridge.reset();
    bridge.heartbeat();
    require(log.calls.size() == callsBefore, "nothing is sent after the connection was lost");

    usbslot::Bridge none(nullptr);
    require(!none.connected() && none.readMemory(1, 0x4000, false, waits) == 0xFF,
            "a bridge without transport reads FFh");
}

void testCycleTracker()
{
    usbslot::CycleTracker tracker;
    require(tracker.begins(true, 0x4000), "a new request starts a cycle");
    require(!tracker.begins(true, 0x4000), "the same request in the next T-state (wait) is the same cycle");
    require(!tracker.begins(false, 0x4000), "no request, no cycle");
    require(tracker.begins(true, 0x4000), "the same address after a gap is a new cycle");
    require(tracker.begins(true, 0x4001), "another address is a new cycle");
}

// Simulated board for BoardSlot and detectBoard. Slot 1: 32 KiB ROM at 4000h. Slot 2: SD Mapper/Megaram 512K with
// its own slot expander - subslot 1 Nextor ROM, 8 banks of 16 KiB in page 1 switched at 6000h (bank 7 holds the
// driver header "NEXTOR_DRIVER"/"SDMapper Driver" at 4100h), bit 0 of a write to 6001h switches 4000h-47FFh and
// 4800h to the SPI port (every read there returns the next value of a counter that starts at 5Ah); subslot 3
// 512 KiB RAM as Memory Mapper (ports FCh-FFh) or MegaRAM (ports 8Eh/8Fh, 8 KiB banks).

class SimulatedBoard final : public usbslot::Transport {
public:
    bool megaramMode = false;
    bool spacedReads = true;      // protocol 3 (MRDNG); false: refused like protocol 2
    int singleReads = 0;
    int blockReads = 0;
    int spacedBlocks = 0;
    std::uint16_t lastSpacedAddress = 0;
    std::uint16_t lastSpacedCount = 0;
    std::uint8_t lastSpacedGap = 0;

    SimulatedBoard()
    {
        for (std::size_t i = 0; i < rom.size(); ++i) rom[i] = static_cast<std::uint8_t>(i ^ (i >> 8));
        rom[0] = 'A';
        rom[1] = 'B';
        for (std::size_t i = 0; i < nextor.size(); ++i) nextor[i] = static_cast<std::uint8_t>(0x80 | (i >> 14));
        nextor[0] = 'A';
        nextor[1] = 'B';
        setDriverHeader(true);
        // Driver code in bank 7 as in the SDMapper driver: LD HL,4000h and 512 LDI at 5A73h (sector), 16 LDI at
        // 4BEAh followed by two LD A,(HL) (16-byte card register and CRC)
        for (std::size_t i = 0; i < 512; ++i) {
            nextor[7 * 0x4000 + 0x1A73 + 2 * i] = 0xED;
            nextor[7 * 0x4000 + 0x1A74 + 2 * i] = 0xA0;
        }
        nextor[7 * 0x4000 + 0x1A73 + 1024] = 0x3A;
        for (std::size_t i = 0; i < 16; ++i) {
            nextor[7 * 0x4000 + 0x0BEA + 2 * i] = 0xED;
            nextor[7 * 0x4000 + 0x0BEB + 2 * i] = 0xA0;
        }
        nextor[7 * 0x4000 + 0x0C0A] = 0x7E;
        nextor[7 * 0x4000 + 0x0C0B] = 0x7E;
        for (std::size_t i = 0; i < ram.size(); ++i) ram[i] = static_cast<std::uint8_t>((i >> 13) + 1);
    }

    void setDriverHeader(bool present)
    {
        static constexpr char Header[] = "NEXTOR_DRIVER\0\3\0SDMapper Driver";
        const std::size_t at = 7 * 0x4000 + 0x100;
        for (std::size_t i = 0; i < sizeof Header - 1; ++i) {
            nextor[at + i] = present ? static_cast<std::uint8_t>(Header[i]) : static_cast<std::uint8_t>(0x87);
        }
    }

    bool memReadBlockSpaced(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::uint8_t gap,
                            std::vector<std::uint8_t>& data) override
    {
        data.clear();
        if (!spacedReads) return true;
        ++spacedBlocks;
        lastSpacedAddress = address;
        lastSpacedCount = count;
        lastSpacedGap = gap;
        for (std::uint16_t i = 0; i < count; ++i) data.push_back(load(slot, static_cast<std::uint16_t>(address + i)));
        return true;
    }

    bool memRead(std::uint8_t slot, std::uint16_t address, bool, std::uint8_t& data, std::uint32_t& waits) override
    {
        ++singleReads;
        data = load(slot, address);
        waits = 0;
        return true;
    }

    bool memReadBlock(std::uint8_t slot, std::uint16_t address, std::uint16_t count, std::vector<std::uint8_t>& data) override
    {
        ++blockReads;
        data.resize(count);
        for (std::uint16_t i = 0; i < count; ++i) data[i] = load(slot, static_cast<std::uint16_t>(address + i));
        return true;
    }

    bool memWrite(std::uint8_t slot, std::uint16_t address, std::uint8_t value) override
    {
        if (slot != 2) return true;
        if (address == 0xFFFF) {
            expander = value;
            return true;
        }
        const unsigned sub = (expander >> (2 * (address >> 14))) & 3;
        if (sub == 1 && address == 0x6000) nextorBank = value & 7;
        if (sub == 1 && address == 0x6001) spiOn = (value & 1) != 0;
        if (sub == 3) {
            if (megaramMode && !megaramWrite) {
                megaramBank[(address >> 13) & 3] = value & 0x3F;
            } else {
                ram[ramOffset(address)] = value;
            }
        }
        return true;
    }

    bool ioRead(std::uint8_t port, std::uint8_t& data, std::uint32_t& waits) override
    {
        waits = 0;
        data = 0xFF;
        if (!megaramMode && port >= 0xFC) data = static_cast<std::uint8_t>(0xE0 | segment[port & 3]);
        if (megaramMode && (port & 0xFE) == 0x8E) megaramWrite = true;
        return true;
    }

    bool ioWrite(std::uint8_t port, std::uint8_t value, std::uint32_t& waits) override
    {
        waits = 0;
        if (!megaramMode && port >= 0xFC) segment[port & 3] = value & 0x1F;
        if (megaramMode && (port & 0xFE) == 0x8E) megaramWrite = false;
        return true;
    }

    bool reset() override
    {
        expander = 0;
        nextorBank = 0;
        spiOn = false;
        segment = {0, 1, 2, 3};
        megaramBank = {0, 1, 2, 3};
        megaramWrite = false;
        return true;
    }

    bool run() override { return true; }
    bool sync(bool& running, std::uint8_t& intLevel) override
    {
        running = true;
        intLevel = 1;
        return true;
    }
    bool poll() override { return true; }
    bool popInterrupt(std::uint8_t&) override { return false; }
    std::string error() const override { return {}; }

    [[nodiscard]] std::uint8_t ramAt(std::size_t offset) const { return ram[offset]; }
    [[nodiscard]] std::uint8_t spiValueNow() const { return spiValue; }

private:
    std::size_t ramOffset(std::uint16_t address) const
    {
        if (megaramMode) return (static_cast<std::size_t>(megaramBank[(address >> 13) & 3]) << 13) | (address & 0x1FFF);
        return (static_cast<std::size_t>(segment[address >> 14]) << 14) | (address & 0x3FFF);
    }

    std::uint8_t load(std::uint8_t slot, std::uint16_t address) const
    {
        if (slot == 1) return (address >= 0x4000 && address < 0xC000) ? rom[address - 0x4000] : 0xFF;
        if (address == 0xFFFF) return static_cast<std::uint8_t>(~expander);
        const unsigned sub = (expander >> (2 * (address >> 14))) & 3;
        if (sub == 1 && spiOn && ((address >= 0x4000 && address < 0x4800) || address == 0x4800)) return spiValue++;
        if (sub == 1 && address >= 0x4000 && address < 0x8000) {
            return nextor[(static_cast<std::size_t>(nextorBank) << 14) | (address & 0x3FFF)];
        }
        if (sub == 3) return ram[ramOffset(address)];
        return 0xFF;
    }

    std::array<std::uint8_t, 0x8000> rom{};
    std::array<std::uint8_t, 8 * 0x4000> nextor{};
    std::array<std::uint8_t, 512 * 1024> ram{};
    std::uint8_t expander = 0;
    std::uint8_t nextorBank = 0;
    bool spiOn = false;
    mutable std::uint8_t spiValue = 0x5A;
    std::array<std::uint8_t, 4> segment{0, 1, 2, 3};
    std::array<std::uint8_t, 4> megaramBank{0, 1, 2, 3};
    bool megaramWrite = false;
};

void testDetection()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    auto detection = usbslot::detectBoard(bridge);
    require(detection.occupied[0] && detection.occupied[1], "both slots are found occupied");
    require(!detection.slots[0].expander && detection.slots[0].description == "Cartridge",
            "slot 1 holds a plain cartridge");
    const auto& sdMapper = detection.slots[1];
    require(sdMapper.expander && sdMapper.description == "SD Mapper (Memory Mapper)", "slot 2 is the SD Mapper as Memory Mapper");
    require(sdMapper.rules[1].bankPage == 1 && sdMapper.rules[1].bankAddress == 0x6000 && sdMapper.rules[1].bankMask == 7 &&
            sdMapper.rules[1].uncached.none() && sdMapper.rules[1].switchAddress == 0x6001 &&
            sdMapper.rules[1].switchedUncached.test(0x40) && sdMapper.rules[1].switchedUncached.test(0x48) &&
            !sdMapper.rules[1].switchedUncached.test(0x49),
            "the Nextor subslot gets its bank register and the SPI port 4000h-4800h switched at 6001h");
    require(sdMapper.rules[3].mapper && sdMapper.rules[3].mapperMask == 0x1F, "subslot 3 is Memory Mapper RAM");
    require(sdMapper.ioPorts == std::vector<std::uint8_t>({0xFC, 0xFD, 0xFE, 0xFF}), "the mapper ports go to the board");
    require(!detection.switchedIo, "no switched I/O without an answer at port 40h");
    require(sdMapper.rules[1].spiBlockStart == 0x4001 && sdMapper.rules[1].spiBlockEnd == 0x41FF &&
            sdMapper.rules[1].spiBlockGap == 24, "the SD Mapper gets the sector block read 4001h-41FFh, 24 clocks apart");

    board->setDriverHeader(false);
    detection = usbslot::detectBoard(bridge);
    require(detection.slots[1].description.rfind("Slot expander (", 0) == 0 && detection.slots[1].rules[1].switchAddress < 0 &&
                detection.slots[1].rules[1].spiBlockStart < 0 && detection.slots[1].rules[1].bankPage < 0,
            "without the SDMapper driver header the same cartridge is only a plain slot expander");
    board->setDriverHeader(true);

    board->megaramMode = true;
    detection = usbslot::detectBoard(bridge);
    require(detection.slots[1].description == "SD Mapper (MegaRAM)" && detection.slots[1].rules[3].megaram &&
            detection.slots[1].ioPorts == std::vector<std::uint8_t>({0x8E, 0x8F}),
            "without mapper answers subslot 3 is MegaRAM with ports 8Eh/8Fh");
}

void testRomCache()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    usbslot::SlotConfig config;
    config.rules[0].noCache(0x8000, 0x100);
    usbslot::BoardSlot slot(bridge, 1, config);
    std::uint32_t waits = 0;

    require(slot.read(0x4000, true, waits) == 'A' && board->blockReads == 1 && board->singleReads == 0,
            "the first read fetches its 256-byte line");
    for (std::uint16_t address = 0x4001; address < 0x4100; ++address) (void)slot.read(address, false, waits);
    require(board->blockReads == 1 && board->singleReads == 0, "the rest of the line comes from the cache");
    require(slot.peek(0x4002) == 0x02 && slot.peek(0x4100) == 0xFF, "peek shows cached bytes, FFh otherwise");

    (void)slot.read(0x8000, true, waits);
    (void)slot.read(0x8000, true, waits);
    require(board->singleReads == 2 && board->blockReads == 1, "uncached lines are read on the board every time");

    slot.write(0x4010, 0x55);
    require(slot.peek(0x4010) == 0xFF, "a write to ROM marks the byte as stale");
    require(slot.read(0x4010, false, waits) == 0x10 && board->singleReads == 3, "a stale byte is read on the board once");
    (void)slot.read(0x4010, false, waits);
    require(board->singleReads == 3, "afterwards it is cached again");

    slot.reset();
    (void)slot.read(0x4000, false, waits);
    require(board->blockReads == 2, "reset empties the cache");
}

void testExpanderBankAndMapper()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    const auto detection = usbslot::detectBoard(bridge);
    usbslot::BoardSlot slot(bridge, 2, detection.slots[1]);
    slot.reset();
    std::uint32_t waits = 0;

    slot.write(0xFFFF, 0xC4);     // page 1 = subslot 1 (Nextor), page 3 = subslot 3 (RAM)
    const int before = board->singleReads + board->blockReads;
    require(slot.read(0xFFFF, false, waits) == 0x3B && board->singleReads + board->blockReads == before,
            "FFFFh reads the complement of the expander register without a board cycle");
    require(slot.read(0x4000, false, waits) == 'A', "subslot 1 shows Nextor bank 0");
    require(slot.read(0x5000, false, waits) == 0x80, "bank 0 data");
    slot.write(0x6000, 2);
    require(slot.read(0x5000, false, waits) == 0x82, "a write to 6000h selects Nextor bank 2");
    const int blocks = board->blockReads;
    slot.write(0x6000, 0);
    require(slot.read(0x5000, false, waits) == 0x80 && board->blockReads == blocks, "bank 0 is still cached");

    require(slot.read(0xC000, false, waits) == 7, "page 3 shows mapper segment 3 (each RAM byte holds its 8 KiB block + 1)");
    slot.ioWritten(0xFF, 5);
    std::uint32_t ioWaits = 0;
    bridge.writeIo(0xFF, 5, ioWaits);
    require(slot.read(0xC000, false, waits) == 11, "OUT FFh selects segment 5 for page 3");
    (void)slot.read(0xC123, false, waits);
    slot.write(0xC123, 0x77);
    const int reads = board->singleReads + board->blockReads;
    require(slot.read(0xC123, false, waits) == 0x77 && board->singleReads + board->blockReads == reads,
            "mapper RAM takes written values into the cache");
    require(board->ramAt(5 * 0x4000 + 0x123) == 0x77, "the write reached the cartridge");
    require(slot.claimsPort(0xFC) && !slot.claimsPort(0x98), "the slot claims the mapper ports only");
}

void testSdMapperSpiSwitch()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    const auto detection = usbslot::detectBoard(bridge);
    usbslot::BoardSlot slot(bridge, 2, detection.slots[1]);
    slot.reset();
    std::uint32_t waits = 0;

    slot.write(0xFFFF, 0x04);     // page 1 = subslot 1 (Nextor)
    require(slot.read(0x4000, true, waits) == 'A' && slot.read(0x4300, true, waits) == 0x80,
            "with the SPI port off 4000h-47FFh is the Nextor ROM");
    int reads = board->singleReads + board->blockReads;
    for (std::uint16_t address = 0x4000; address < 0x4010; ++address) (void)slot.read(address, true, waits);
    require(board->singleReads + board->blockReads == reads, "and it is cached");

    slot.write(0x6001, 1);
    const int singles = board->singleReads;
    require(slot.read(0x4000, false, waits) == 0x5A && slot.read(0x4000, false, waits) == 0x5B &&
            slot.read(0x4800, false, waits) == 0x5C && board->singleReads == singles + 3,
            "with the SPI port on every read of 4000h-47FFh and 4800h goes to the board");
    reads = board->singleReads + board->blockReads;
    require(slot.read(0x4801, false, waits) == 0x80 && slot.read(0x5000, false, waits) == 0x80,
            "the rest of page 1 is still the ROM");
    slot.write(0x4000, 0x12);     // SPI data, not a ROM write

    slot.write(0x6001, 0);
    reads = board->singleReads + board->blockReads;
    require(slot.read(0x4000, false, waits) == 'A' && board->singleReads + board->blockReads == reads,
            "switched off again the cached ROM is used without a board cycle");

    slot.write(0x6001, 1);
    slot.reset();
    reads = board->singleReads + board->blockReads;
    (void)slot.read(0x4000, false, waits);
    require(board->blockReads > 0 && board->singleReads + board->blockReads == reads + 1,
            "reset switches the SPI port off for the cache");
}

void testSdMapperSectorBlock()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    usbslot::Bridge bridge(std::move(transport));
    const auto detection = usbslot::detectBoard(bridge);
    usbslot::BoardSlot slot(bridge, 2, detection.slots[1]);
    slot.reset();
    std::uint32_t waits = 0;
    slot.write(0xFFFF, 0x04);     // page 1 = subslot 1 (Nextor)
    slot.write(0x6000, 7);        // driver bank
    slot.write(0x6001, 1);
    // LDI number i of the chain at chain: both opcode fetches, then the read of the SPI window
    auto ldi = [&](std::uint16_t chain, unsigned i, std::uint16_t source) {
        (void)slot.read(static_cast<std::uint16_t>(chain + 2 * i), true, waits);
        (void)slot.read(static_cast<std::uint16_t>(chain + 2 * i + 1), true, waits);
        return slot.read(source, false, waits);
    };

    // Sector: LD HL,4000h and 512 LDI at 5A73h - 4000h single, the LDI reading 4001h fetches 4001h-41FFh
    std::uint8_t expected = board->spiValueNow();
    const int singles = board->singleReads;
    bool inOrder = true;
    for (unsigned i = 0; i < 512; ++i) {
        inOrder = ldi(0x5A73, i, static_cast<std::uint16_t>(0x4000 + i)) == expected++ && inOrder;
        if (i == 1) {
            require(board->spacedBlocks == 1 && board->lastSpacedAddress == 0x4001 && board->lastSpacedCount == 511 &&
                        board->lastSpacedGap == 24,
                    "the LDI reading 4001h fetches the 511 bytes its chain still reads, 24 clocks per byte");
        }
    }
    require(inOrder, "the CPU gets all 512 bytes of the sector in order");
    require(board->singleReads == singles + 1 && board->spacedBlocks == 1,
            "one single read and one spaced block for the whole sector");
    require(slot.read(0x4000, false, waits) == expected++ && board->singleReads == singles + 2,
            "after the sector the next read of 4000h is a single read again (CRC byte)");

    // Card register: 16 LDI at 4BEAh, then LD A,(HL) twice at 4010h - the block holds only the 15 bytes of the chain
    inOrder = true;
    for (unsigned i = 0; i < 16; ++i) {
        inOrder = ldi(0x4BEA, i, static_cast<std::uint16_t>(0x4000 + i)) == expected++ && inOrder;
    }
    require(board->spacedBlocks == 2 && board->lastSpacedAddress == 0x4001 && board->lastSpacedCount == 15,
            "a 16-byte LDI chain fetches only its 15 remaining bytes");
    (void)slot.read(0x4C0A, true, waits);
    inOrder = slot.read(0x4010, false, waits) == expected++ && inOrder;
    (void)slot.read(0x4C0B, true, waits);
    inOrder = slot.read(0x4010, false, waits) == expected++ && inOrder;
    require(inOrder && slot.spiBlocks() == 2 && slot.spiBlockErrors() == 0,
            "the two CRC reads behind the register chain are single reads, nothing broken off");

    // An access out of order inside a block breaks it off and is counted
    (void)ldi(0x5A73, 0, 0x4000);
    (void)ldi(0x5A73, 1, 0x4001);
    (void)slot.read(0x4010, false, waits);
    require(slot.spiBlocks() == 3 && slot.spiBlockErrors() == 1, "an unexpected address breaks the block off");

    // A read of 4001h that does not come from an LDI chain is a single read
    const int plain = board->spacedBlocks;
    (void)slot.read(0x5000, true, waits);
    (void)slot.read(0x4001, false, waits);
    require(board->spacedBlocks == plain, "without an LDI before it the read of 4001h stays single");

    // Firmware with protocol 2: no spaced reads, every byte is a single read as before
    board->spacedReads = false;
    const int before = board->singleReads;
    (void)ldi(0x5A73, 0, 0x4000);
    (void)ldi(0x5A73, 1, 0x4001);
    (void)ldi(0x5A73, 2, 0x4002);
    require(board->singleReads == before + 3 && slot.spiBlocks() == 3,
            "without spaced block reads the bridge reads every byte singly");
}

void testMegaram()
{
    auto transport = std::make_unique<SimulatedBoard>();
    auto* board = transport.get();
    board->megaramMode = true;
    usbslot::Bridge bridge(std::move(transport));
    const auto detection = usbslot::detectBoard(bridge);
    usbslot::BoardSlot slot(bridge, 2, detection.slots[1]);
    slot.reset();
    std::uint32_t waits = 0;
    std::uint32_t ioWaits = 0;

    slot.write(0xFFFF, 0x30);     // page 2 = subslot 3
    require(slot.read(0x8000, false, waits) == 1 && slot.read(0xA000, false, waits) == 2,
            "8000h and A000h show MegaRAM banks 0 and 1 after reset");
    bridge.writeIo(0x8E, 0, ioWaits);
    slot.ioWritten(0x8E, 0);
    slot.write(0x8000, 7);        // bank switching: register A14-A13 = 0 (window 8000h)
    require(slot.read(0x8000, false, waits) == 8, "a write in bank switching mode selects bank 7");
    (void)bridge.readIo(0x8E, ioWaits);
    slot.ioRead(0x8E);
    slot.write(0x8010, 0x99);
    require(slot.read(0x8010, false, waits) == 0x99 && board->ramAt(7 * 0x2000 + 0x10) == 0x99,
            "after IN 8Eh writes go to the RAM of the bank");
}

} // namespace

int main()
{
    testForwarding();
    testResetWakesTheBoard();
    testInterrupts();
    testSwitchedOffDuringPause();
    testHeartbeatAlongWithCycles();
    testLostConnection();
    testCycleTracker();
    testDetection();
    testRomCache();
    testExpanderBankAndMapper();
    testSdMapperSpiSwitch();
    testSdMapperSectorBlock();
    testMegaram();
    std::cout << "usbslot_tests passed\n";
    return EXIT_SUCCESS;
}
