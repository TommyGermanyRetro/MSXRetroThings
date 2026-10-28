#include "USBSLOT/BoardSlot.hpp"

#include <algorithm>
#include <utility>

namespace usbslot {

namespace {

enum class Content : std::uint8_t { Empty, Data, Rom };

const char* contentName(Content content) noexcept
{
    switch (content) {
    case Content::Rom:
        return "ROM";
    case Content::Data:
        return "data";
    default:
        return "empty";
    }
}

// ROM: "AB" at 4000h or 8000h; data: any byte other than FFh in the first 64 bytes of 4000h, 8000h or
// C000h; empty otherwise (an empty slot reads FFh, the board precharges the data bus).
Content classify(Bridge& bridge, std::uint8_t slot)
{
    bool rom = false;
    bool data = false;
    for (const std::uint16_t base : {std::uint16_t{0x4000}, std::uint16_t{0x8000}, std::uint16_t{0xC000}}) {
        std::uint32_t waits = 0;
        if (base != 0xC000 && bridge.readMemory(slot, base, false, waits) == 0x41 &&
            bridge.readMemory(slot, static_cast<std::uint16_t>(base + 1), false, waits) == 0x42) {
            rom = true;
        }
        std::vector<std::uint8_t> block;
        if (bridge.readBlock(slot, base, 64, block)) {
            data = data || std::any_of(block.begin(), block.end(), [](std::uint8_t value) { return value != 0xFF; });
        }
    }
    return rom ? Content::Rom : (data ? Content::Data : Content::Empty);
}

} // namespace

void Rules::noCache(std::uint16_t base, std::uint32_t size)
{
    if (size == 0) return;
    const std::uint32_t last = std::min<std::uint32_t>(base + size - 1, 0xFFFF);
    for (std::uint32_t line = base / LineSize; line <= last / LineSize; ++line) uncached.set(line);
}

Detection detectBoard(Bridge& bridge)
{
    Detection detection;
    bridge.reset();
    std::array<bool, 2> expander{};
    std::array<std::array<Content, 4>, 2> contents{};
    for (std::uint8_t slot = 1; slot <= 2; ++slot) {
        std::uint32_t waits = 0;
        const auto old = bridge.readMemory(slot, 0xFFFF, false, waits);
        bridge.writeMemory(slot, 0xFFFF, 0x5A);
        expander[slot - 1] = bridge.readMemory(slot, 0xFFFF, false, waits) == 0xA5;
        if (expander[slot - 1]) {
            for (unsigned sub = 0; sub < 4; ++sub) {
                bridge.writeMemory(slot, 0xFFFF, static_cast<std::uint8_t>(sub * 0x55));
                contents[slot - 1][sub] = classify(bridge, slot);
            }
            bridge.writeMemory(slot, 0xFFFF, 0x00);
        } else {
            bridge.writeMemory(slot, 0xFFFF, old);
            contents[slot - 1][0] = classify(bridge, slot);
        }
    }

    std::uint32_t waits = 0;
    bridge.writeIo(0x40, 0xC0, waits);
    detection.switchedIo = bridge.readIo(0x40, waits) == 0x3F;
    bridge.writeIo(0x40, 0x00, waits);
    // After /RESET a Memory Mapper reads back its segments 0-3 with the unused high bits set (E0h-E3h);
    // a port without answer reads FFh.
    bool mapperAnswers = true;
    for (unsigned port = 0xFC; port <= 0xFF; ++port) {
        const auto value = bridge.readIo(static_cast<std::uint8_t>(port), waits);
        if (value == 0xFF || (value & 0xE0) != 0xE0) mapperAnswers = false;
    }

    for (std::size_t index = 0; index < 2; ++index) {
        auto& config = detection.slots[index];
        const auto& sub = contents[index];
        if (!expander[index]) {
            if (sub[0] == Content::Empty) continue;
            config.description = "Cartridge";
        } else if (sub[1] == Content::Rom && sub[3] == Content::Data) {
            // SD Mapper/Megaram 512K: Nextor ROM with its bank register at 6000h and the SD card
            // interface at 4000h-4800h in subslot 1, 512 KiB RAM in subslot 3
            config.expander = true;
            config.rules[1].bankPage = 1;
            config.rules[1].bankAddress = 0x6000;
            config.rules[1].bankMask = 0x07;
            config.rules[1].noCache(0x4000, 0x0801);
            if (mapperAnswers) {
                config.rules[3].mapper = true;
                config.rules[3].mapperPort = 0xFC;
                config.rules[3].mapperMask = 0x1F;
                config.ioPorts = {0xFC, 0xFD, 0xFE, 0xFF};
                config.description = "SD Mapper (Memory Mapper)";
            } else {
                config.rules[3].megaram = true;
                config.rules[3].megaramPort = 0x8E;
                config.ioPorts = {0x8E, 0x8F};
                config.description = "SD Mapper (MegaRAM)";
            }
        } else {
            config.expander = true;
            config.description = "Slot expander (";
            for (unsigned s = 0; s < 4; ++s) {
                config.description += std::to_string(s) + ": " + contentName(sub[s]) + (s < 3 ? ", " : ")");
            }
        }
        if (detection.switchedIo) {
            for (std::uint8_t port = 0x40; port <= 0x47; ++port) config.ioPorts.push_back(port);
        }
        detection.occupied[index] = true;
    }
    return detection;
}

BoardSlot::BoardSlot(Bridge& bridge, std::uint8_t number, SlotConfig config)
    : board(bridge), slotNumber(number), settings(std::move(config))
{
}

Bridge& BoardSlot::bridge() noexcept
{
    return board;
}

const Bridge& BoardSlot::bridge() const noexcept
{
    return board;
}

std::uint8_t BoardSlot::number() const noexcept
{
    return slotNumber;
}

const SlotConfig& BoardSlot::config() const noexcept
{
    return settings;
}

unsigned BoardSlot::subslotOf(unsigned page) const noexcept
{
    return settings.expander ? (expanderRegister >> (2 * page)) & 3 : 0;
}

// Key of the window image (8 KiB) that the cartridge shows in the window now: a mapper segment and a
// MegaRAM bank are the same memory wherever they appear, a ROM bank belongs to its page, everything
// else is fixed.
std::uint32_t BoardSlot::imageKey(unsigned window) const noexcept
{
    const unsigned page = window / 2;
    const unsigned half = window & 1;
    const unsigned sub = subslotOf(page);
    const Rules& rules = settings.rules[sub];
    if (rules.mapper) return (sub << 24) | (1u << 20) | (static_cast<unsigned>(segment[page]) << 1) | half;
    if (rules.megaram) return (sub << 24) | (3u << 20) | megaramBank[window & 3];
    if (rules.bankPage == static_cast<int>(page)) {
        return (sub << 24) | (2u << 20) | (page << 16) | (static_cast<unsigned>(bank[sub]) << 1) | half;
    }
    return (sub << 24) | (window << 16);
}

const BoardSlot::WindowCache* BoardSlot::findImage(unsigned window) const noexcept
{
    const auto it = images.find(imageKey(window));
    return it != images.end() ? it->second.get() : nullptr;
}

BoardSlot::WindowCache& BoardSlot::image(unsigned window)
{
    auto& cache = images[imageKey(window)];
    if (!cache) cache = std::make_unique<WindowCache>();
    return *cache;
}

bool BoardSlot::isUncached(std::uint16_t address) const noexcept
{
    return settings.rules[subslotOf(address / PageSize)].uncached.test(address / LineSize);
}

bool BoardSlot::fetchLine(WindowCache& cache, std::uint16_t address)
{
    const auto start = static_cast<std::uint16_t>(address & 0xFF00);
    std::vector<std::uint8_t> block;
    if (!board.readBlock(slotNumber, start, LineSize, block)) return false;
    const unsigned offset = start % WindowSize;
    const unsigned line = offset / LineSize;
    std::copy(block.begin(), block.end(), cache.data.begin() + offset);
    for (unsigned byte = offset; byte < offset + LineSize; ++byte) cache.stale.reset(byte);
    cache.staleCount[line] = 0;
    cache.valid.set(line);
    return true;
}

std::uint8_t BoardSlot::readCycle(std::uint16_t address, bool m1, std::uint32_t& waits)
{
    return board.readMemory(slotNumber, address, m1, waits);
}

std::uint8_t BoardSlot::read(std::uint16_t address, bool m1, std::uint32_t& waits)
{
    waits = 0;
    if (!board.connected()) return 0xFF;
    if (settings.expander && address == 0xFFFF) return static_cast<std::uint8_t>(~expanderRegister);
    if (isUncached(address)) return readCycle(address, m1, waits);
    WindowCache& cache = image(address / WindowSize);
    const unsigned offset = address % WindowSize;
    const unsigned line = offset / LineSize;
    if (cache.valid.test(line)) {
        if (cache.stale.test(offset)) {
            const auto value = readCycle(address, m1, waits);
            cache.data[offset] = value;
            cache.stale.reset(offset);
            --cache.staleCount[line];
            return value;
        }
        return cache.data[offset];
    }
    if (fetchLine(cache, address)) return cache.data[offset];
    return readCycle(address, m1, waits);
}

void BoardSlot::write(std::uint16_t address, std::uint8_t value)
{
    board.writeMemory(slotNumber, address, value);
    if (settings.expander && address == 0xFFFF) {
        expanderRegister = value;
        return;
    }
    const unsigned page = address / PageSize;
    const unsigned sub = subslotOf(page);
    const Rules& rules = settings.rules[sub];
    if (rules.bankPage >= 0 && address == rules.bankAddress) {
        bank[sub] = static_cast<std::uint8_t>(value & rules.bankMask);
        return;
    }
    if (rules.megaram && !megaramWrite) {
        // Bank switching mode: every write sets the bank register selected by A14-A13
        megaramBank[(address >> 13) & 3] = static_cast<std::uint8_t>(value & 0x3F);
        return;
    }
    const auto it = images.find(imageKey(address / WindowSize));
    if (it == images.end()) return;
    WindowCache& cache = *it->second;
    const unsigned offset = address % WindowSize;
    const unsigned line = offset / LineSize;
    if (!cache.valid.test(line)) return;
    if (rules.mapper || rules.megaram) {
        // RAM: the cartridge now holds the written value
        cache.data[offset] = value;
        if (cache.stale.test(offset)) {
            cache.stale.reset(offset);
            --cache.staleCount[line];
        }
    } else if (!cache.stale.test(offset)) {
        cache.stale.set(offset);
        ++cache.staleCount[line];
    }
}

std::uint8_t BoardSlot::peek(std::uint16_t address) const noexcept
{
    if (settings.expander && address == 0xFFFF) return static_cast<std::uint8_t>(~expanderRegister);
    const WindowCache* cache = findImage(address / WindowSize);
    const unsigned offset = address % WindowSize;
    return (cache && cache->valid.test(offset / LineSize) && !cache->stale.test(offset)) ? cache->data[offset] : 0xFF;
}

bool BoardSlot::claimsPort(std::uint8_t port) const noexcept
{
    return std::find(settings.ioPorts.begin(), settings.ioPorts.end(), port) != settings.ioPorts.end();
}

void BoardSlot::ioRead(std::uint8_t port)
{
    // MegaRAM: reading its port enables RAM writes (the bank registers keep their values)
    for (const auto& rules : settings.rules) {
        if (rules.megaram && (port & 0xFE) == rules.megaramPort) {
            megaramWrite = true;
            return;
        }
    }
}

void BoardSlot::ioWritten(std::uint8_t port, std::uint8_t value)
{
    for (const auto& rules : settings.rules) {
        if (rules.megaram && (port & 0xFE) == rules.megaramPort) {
            // MegaRAM: writing its port enables bank switching
            megaramWrite = false;
            return;
        }
        if (rules.mapper && (port & 0xFC) == rules.mapperPort) {
            segment[port & 3] = static_cast<std::uint8_t>(value & rules.mapperMask);
            return;
        }
    }
}

void BoardSlot::reset()
{
    images.clear();
    expanderRegister = 0;
    bank.fill(0);
    segment = {0, 1, 2, 3};
    megaramBank = {0, 1, 2, 3};
    megaramWrite = false;
}

} // namespace usbslot
