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

// SD Mapper/Megaram 512K: Nextor driver header in ROM bank 7 of subslot 1 - "NEXTOR_DRIVER" at 4100h and the
// driver name "SDMapper Driver" at 4110h. Selects subslot 1 for page 1 and bank 7 for the check, then the
// reset values again (bank 0, expander register 00h).
bool hasSdMapperDriver(Bridge& bridge, std::uint8_t slot)
{
    static constexpr char Signature[] = "NEXTOR_DRIVER";
    static constexpr char Name[] = "SDMapper Driver";
    bridge.writeMemory(slot, 0xFFFF, 0x04);
    bridge.writeMemory(slot, 0x6000, 0x07);
    std::vector<std::uint8_t> header;
    const bool read = bridge.readBlock(slot, 0x4100, 0x20, header);
    bridge.writeMemory(slot, 0x6000, 0x00);
    bridge.writeMemory(slot, 0xFFFF, 0x00);
    if (!read) return false;
    return std::equal(Signature, Signature + sizeof Signature, header.begin()) &&
           std::equal(Name, Name + sizeof Name - 1, header.begin() + 0x10);
}

// RCX ROM: UNAPI specification identifier "RCX_INTERFACE" right behind the cartridge header at 4010h, in the
// slot or subslot selected now.
bool hasRcxRom(Bridge& bridge, std::uint8_t slot)
{
    static constexpr char Signature[] = "RCX_INTERFACE";
    std::vector<std::uint8_t> header;
    if (!bridge.readBlock(slot, 0x4010, sizeof Signature, header)) return false;
    return std::equal(Signature, Signature + sizeof Signature, header.begin());
}

// Switched I/O of the RCX card: 40h enables it with the ID C0h; 46h latches the address of an RC2014
// card register, 47h reads or writes it.
constexpr std::uint8_t SwitchedIoPort = 0x40;
constexpr std::uint8_t SwitchedIoId = 0xC0;
constexpr std::uint8_t AddressLatch = 0x46;
constexpr std::uint8_t DataPort = 0x47;

// PCF8584 registers S0 (A0 = 0) and S1 (A0 = 1) behind the switched I/O.
class Pcf8584 {
public:
    Pcf8584(Bridge& bridge, std::uint8_t address) : bridge_(bridge), s0_(address), s1_(static_cast<std::uint8_t>(address + 1)) {}

    void write(bool s1, std::uint8_t value)
    {
        bridge_.writeIo(AddressLatch, s1 ? s1_ : s0_, waits_);
        bridge_.writeIo(DataPort, value, waits_);
    }
    std::uint8_t status()
    {
        bridge_.writeIo(AddressLatch, s1_, waits_);
        return bridge_.readIo(DataPort, waits_);
    }
    // Up to 255 reads of S1 until the bits in mask match wanted, like I2CBCTRL and I2CPCTRL of the RCX ROM.
    bool waitStatus(std::uint8_t mask, std::uint8_t wanted, std::uint8_t& value)
    {
        for (int count = 0; count < 255; ++count) {
            value = status();
            if ((value & mask) == wanted) return true;
        }
        return false;
    }
    // I2C write of a block like CORE8 of the RCX ROM: START with the chip address, the bytes, STOP. False
    // when the bus stays busy, a transfer does not complete or a byte is not acknowledged.
    bool writeBlock(std::uint8_t chip, const std::vector<std::uint8_t>& bytes)
    {
        std::uint8_t value = 0;
        if (!waitStatus(0x01, 0x01, value)) return false;
        write(false, chip);
        write(true, 0xC5);
        bool acknowledged = true;
        for (std::size_t index = 0;; ++index) {
            if (!waitStatus(0x80, 0x00, value) || (value & 0x08) != 0) {
                acknowledged = false;
                break;
            }
            if (index == bytes.size()) break;
            write(false, bytes[index]);
        }
        write(true, 0xC3);
        return acknowledged;
    }

private:
    Bridge& bridge_;
    std::uint8_t s0_;
    std::uint8_t s1_;
    std::uint32_t waits_ = 0;
};

// Guesses the ROM mapper like openMSX (RomFactory): counts LD (nnnn),A (32h nn nn) to the bank registers in the
// 32 KiB seen at 4000h-BFFFh after /RESET. The winner needs at least three hits, otherwise it is a plain ROM - a
// 16 or 32 KiB game may store to such an address by chance.
RomMapper guessRomMapper(Bridge& bridge, std::uint8_t slot)
{
    std::vector<std::uint8_t> rom;
    for (std::uint32_t base = 0x4000; base < 0xC000; base += 0x400) {
        std::vector<std::uint8_t> block;
        if (!bridge.readBlock(slot, static_cast<std::uint16_t>(base), 0x400, block) || block.size() != 0x400) {
            return RomMapper::None;
        }
        rom.insert(rom.end(), block.begin(), block.end());
    }
    std::array<unsigned, 5> hits{};
    const auto count = [&hits](RomMapper mapper) { ++hits[static_cast<unsigned>(mapper)]; };
    for (std::size_t i = 0; i + 2 < rom.size(); ++i) {
        if (rom[i] != 0x32) continue;
        switch (rom[i + 1] | (rom[i + 2] << 8)) {
        case 0x5000:
        case 0x9000:
        case 0xB000:
            count(RomMapper::KonamiScc);
            break;
        case 0x4000:
        case 0x8000:
        case 0xA000:
            count(RomMapper::Konami);
            break;
        case 0x6800:
        case 0x7800:
            count(RomMapper::Ascii8);
            break;
        case 0x6000:
            count(RomMapper::Konami);
            count(RomMapper::Ascii8);
            count(RomMapper::Ascii16);
            break;
        case 0x7000:
            count(RomMapper::KonamiScc);
            count(RomMapper::Ascii8);
            count(RomMapper::Ascii16);
            break;
        case 0x77FF:
            count(RomMapper::Ascii16);
            break;
        default:
            break;
        }
    }
    auto& ascii8 = hits[static_cast<unsigned>(RomMapper::Ascii8)];
    if (ascii8 > 0) --ascii8;
    unsigned best = 0;
    for (unsigned mapper = 1; mapper < hits.size(); ++mapper) {
        if (hits[mapper] > 0 && hits[mapper] >= hits[best]) best = mapper;
    }
    return hits[best] >= 3 ? static_cast<RomMapper>(best) : RomMapper::None;
}

const char* mapperName(RomMapper mapper) noexcept
{
    switch (mapper) {
    case RomMapper::Konami:
        return "Konami";
    case RomMapper::KonamiScc:
        return "Konami SCC";
    case RomMapper::Ascii8:
        return "ASCII8";
    case RomMapper::Ascii16:
        return "ASCII16";
    default:
        return "";
    }
}

std::uint8_t bcd(int value) noexcept
{
    return static_cast<std::uint8_t>((((value / 10) % 10) << 4) | (value % 10));
}

} // namespace

void Rules::noCache(std::uint16_t base, std::uint32_t size)
{
    if (size == 0) return;
    const std::uint32_t last = std::min<std::uint32_t>(base + size - 1, 0xFFFF);
    for (std::uint32_t line = base / LineSize; line <= last / LineSize; ++line) uncached.set(line);
}

void Rules::noCacheWhenSwitched(std::uint16_t base, std::uint32_t size)
{
    if (size == 0) return;
    const std::uint32_t last = std::min<std::uint32_t>(base + size - 1, 0xFFFF);
    for (std::uint32_t line = base / LineSize; line <= last / LineSize; ++line) switchedUncached.set(line);
}

Detection detectBoard(Bridge& bridge)
{
    Detection detection;
    bridge.reset();
    std::array<bool, 2> expander{};
    std::array<bool, 2> sdMapper{};
    std::array<std::array<Content, 4>, 2> contents{};
    std::array<RomMapper, 2> romMapper{};
    for (std::uint8_t slot = 1; slot <= 2; ++slot) {
        std::uint32_t waits = 0;
        const auto old = bridge.readMemory(slot, 0xFFFF, false, waits);
        bridge.writeMemory(slot, 0xFFFF, 0x5A);
        expander[slot - 1] = bridge.readMemory(slot, 0xFFFF, false, waits) == 0xA5;
        if (expander[slot - 1]) {
            for (unsigned sub = 0; sub < 4; ++sub) {
                bridge.writeMemory(slot, 0xFFFF, static_cast<std::uint8_t>(sub * 0x55));
                contents[slot - 1][sub] = classify(bridge, slot);
                if (contents[slot - 1][sub] == Content::Rom && hasRcxRom(bridge, slot)) detection.rcxRom = true;
            }
            bridge.writeMemory(slot, 0xFFFF, 0x00);
            sdMapper[slot - 1] = contents[slot - 1][1] == Content::Rom && contents[slot - 1][3] == Content::Data &&
                                 hasSdMapperDriver(bridge, slot);
        } else {
            bridge.writeMemory(slot, 0xFFFF, old);
            contents[slot - 1][0] = classify(bridge, slot);
            if (contents[slot - 1][0] == Content::Rom && hasRcxRom(bridge, slot)) detection.rcxRom = true;
            if (contents[slot - 1][0] == Content::Rom) romMapper[slot - 1] = guessRomMapper(bridge, slot);
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
            if (romMapper[index] != RomMapper::None) {
                config.rules[0].romMapper = romMapper[index];
                config.description += std::string(" (") + mapperName(romMapper[index]) + ")";
            }
        } else if (sdMapper[index]) {
            // SD Mapper/Megaram 512K: Nextor ROM with its bank register at 6000h in subslot 1; a write
            // to 6001h (bit 0) switches 4000h-47FFh and 4800h to the SPI port of the SD cards. Its driver
            // reads a sector with LD HL,4000h and 512 LDI (18 clocks each on the MSX): the read of 4001h
            // starts the rest of the sector, 511 reads 24 clocks apart (one SPI byte takes about 19
            // clocks). 512 KiB RAM in subslot 3.
            config.expander = true;
            config.rules[1].bankPage = 1;
            config.rules[1].bankAddress = 0x6000;
            config.rules[1].bankMask = 0x07;
            config.rules[1].switchAddress = 0x6001;
            config.rules[1].noCacheWhenSwitched(0x4000, 0x0801);
            config.rules[1].spiBlockStart = 0x4001;
            config.rules[1].spiBlockEnd = 0x41FF;
            config.rules[1].spiBlockGap = 24;
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

ClockResult setCardClock(Bridge& bridge, const Detection& detection, std::uint8_t cardAddress, const ClockTime& time)
{
    if (!detection.switchedIo) return ClockResult::NoSwitchedIo;
    if (!detection.rcxRom) return ClockResult::NoRcxRom;
    std::uint32_t waits = 0;
    bridge.writeIo(SwitchedIoPort, SwitchedIoId, waits);
    Pcf8584 controller(bridge, cardAddress);
    // Initialisation like CORE6 (CALL I2C INIT): own address 55h, clock 8 MHz / SCL 90 kHz, bus idle.
    controller.write(true, 0x80);
    controller.write(false, 0x55);
    controller.write(true, 0xA0);
    controller.write(false, 0x18);
    controller.write(true, 0xC1);
    auto result = ClockResult::Set;
    if ((controller.status() & 0x81) != 0x81) {
        result = ClockResult::NoController;
    } else if (!controller.writeBlock(0xA0, {0x00, 0x00, 0x00, bcd(time.second), bcd(time.minute), bcd(time.hour)})) {
        // CALL SET TIME: from register 0 control, hundredths, seconds, minutes, hours.
        result = ClockResult::NoClock;
    } else if (!controller.writeBlock(0xA0, {0x05, bcd(time.day), bcd(time.month)}) ||
               !controller.writeBlock(0xA0, {0x10, bcd(time.year % 100)})) {
        // CALL SET DATE: day in register 5, month in register 6, year in RAM 10h.
        result = ClockResult::NoClock;
    }
    bridge.writeIo(SwitchedIoPort, 0x00, waits);
    return result;
}

BoardSlot::BoardSlot(Bridge& bridge, std::uint8_t number, SlotConfig config)
    : board(bridge), slotNumber(number), settings(std::move(config))
{
    for (unsigned sub = 0; sub < 4; ++sub) resetRomMapper(sub);
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
    // A MegaROM bank is the same 8 KiB of ROM in whichever window it is shown
    if (rules.romMapper != RomMapper::None && window >= 2 && window <= 5) {
        return (sub << 24) | (4u << 20) | romBank[sub][window - 2];
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
    const unsigned sub = subslotOf(address / PageSize);
    const Rules& rules = settings.rules[sub];
    const unsigned line = address / LineSize;
    if (rules.romMapper != RomMapper::None) {
        if (address < 0x4000 || address >= 0xC000) return true;
        if (rules.romMapper == RomMapper::KonamiScc && sccOn[sub] && address >= 0x9800 && address < 0xA000) return true;
    }
    return rules.uncached.test(line) || (switchedOn[sub] && rules.switchedUncached.test(line));
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
    if (m1) {
        lastFetch = address;
        fetched = true;
    }
    if (settings.expander && address == 0xFFFF) return static_cast<std::uint8_t>(~expanderRegister);
    if (isUncached(address)) {
        if (!spiBuffer.empty()) {
            if (!m1 && address == spiNext) {
                const auto value = spiBuffer[spiPosition++];
                ++spiNext;
                if (spiPosition == spiBuffer.size()) spiBuffer.clear();
                return value;
            }
            ++spiErrorCount;
            dropSpiBlock();
        }
        const unsigned sub = subslotOf(address / PageSize);
        const Rules& rules = settings.rules[sub];
        const unsigned count = (!m1 && switchedOn[sub] && rules.spiBlockStart == static_cast<int>(address))
            ? ldiChainLength(address, rules.spiBlockEnd - address + 1u) : 0u;
        if (count > 1) {
            std::vector<std::uint8_t> block;
            if (board.readBlockSpaced(slotNumber, address, static_cast<std::uint16_t>(count), rules.spiBlockGap, block)) {
                ++spiBlockCount;
                spiBuffer = std::move(block);
                spiPosition = 1;
                spiNext = static_cast<std::uint16_t>(address + 1);
                return spiBuffer[0];
            }
            if (!board.connected()) return 0xFF;
        }
        return readCycle(address, m1, waits);
    }
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
    if (rules.romMapper != RomMapper::None) {
        if (address >= 0x4000 && address < 0xC000) romMapperWrite(sub, rules.romMapper, address, value);
        return;
    }
    if (rules.bankPage >= 0 && address == rules.bankAddress) {
        bank[sub] = static_cast<std::uint8_t>(value & rules.bankMask);
        return;
    }
    if (!spiBuffer.empty() && (isUncached(address) || (rules.switchAddress >= 0 && address == rules.switchAddress))) {
        // The sector read was broken off: the board has clocked bytes the CPU will not read
        ++spiErrorCount;
        dropSpiBlock();
    }
    if (rules.switchAddress >= 0 && address == rules.switchAddress) {
        switchedOn[sub] = (value & 1) != 0;
        return;
    }
    // A write to a line read on the board (registers, the SPI port) leaves the cache alone.
    if (isUncached(address)) return;
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
    switchedOn.fill(false);
    fetched = false;
    dropSpiBlock();
    segment = {0, 1, 2, 3};
    megaramBank = {0, 1, 2, 3};
    megaramWrite = false;
    for (unsigned sub = 0; sub < 4; ++sub) resetRomMapper(sub);
}

void BoardSlot::resetRomMapper(unsigned sub)
{
    switch (settings.rules[sub].romMapper) {
    case RomMapper::Konami:
    case RomMapper::KonamiScc:
        romBank[sub] = {0, 1, 2, 3};
        break;
    case RomMapper::Ascii16:
        romBank[sub] = {0, 1, 0, 1};
        break;
    default:
        romBank[sub] = {0, 0, 0, 0};
        break;
    }
    sccOn[sub] = false;
}

void BoardSlot::romMapperWrite(unsigned sub, RomMapper mapper, std::uint16_t address, std::uint8_t value)
{
    auto& banks = romBank[sub];
    switch (mapper) {
    case RomMapper::Konami:
        // 4000h-5FFFh is fixed at bank 0
        if (address >= 0x6000) banks[(address >> 13) - 2] = value;
        break;
    case RomMapper::KonamiScc:
        if (sccOn[sub] && address >= 0x9800 && address < 0xA000) break; // SCC register
        if ((address & 0xF800) == 0x9000) sccOn[sub] = (value & 0x3F) == 0x3F;
        if ((address & 0x1800) == 0x1000) banks[(address >> 13) - 2] = value;
        break;
    case RomMapper::Ascii8:
        if (address >= 0x6000 && address < 0x8000) banks[(address >> 11) & 3] = value;
        break;
    case RomMapper::Ascii16:
        if (address >= 0x6000 && address < 0x7800 && (address & 0x0800) == 0) {
            const unsigned half = ((address >> 12) & 1) * 2;
            banks[half] = static_cast<std::uint16_t>(value * 2);
            banks[half + 1] = static_cast<std::uint16_t>(value * 2 + 1);
        }
        break;
    default:
        break;
    }
}

void BoardSlot::dropSpiBlock()
{
    spiBuffer.clear();
    spiPosition = 0;
}

int BoardSlot::cachedByte(std::uint16_t address)
{
    if (isUncached(address) || (settings.expander && address == 0xFFFF)) return -1;
    WindowCache& cache = image(address / WindowSize);
    const unsigned offset = address % WindowSize;
    if (!cache.valid.test(offset / LineSize) && !fetchLine(cache, address)) return -1;
    if (cache.stale.test(offset)) return -1;
    return cache.data[offset];
}

// The instruction that reads now is the LDI whose second opcode byte (A0h) was the last fetch from this slot;
// every further ED A0 directly behind it reads the next byte. Anything else (no LDI, code not readable from
// the cache) gives 1: only this byte.
unsigned BoardSlot::ldiChainLength(std::uint16_t address, unsigned limit)
{
    (void)address;
    if (!fetched || lastFetch == 0) return 1;
    if (cachedByte(static_cast<std::uint16_t>(lastFetch - 1)) != 0xED || cachedByte(lastFetch) != 0xA0) return 1;
    unsigned count = 1;
    std::uint16_t next = static_cast<std::uint16_t>(lastFetch + 1);
    while (count < limit && cachedByte(next) == 0xED && cachedByte(static_cast<std::uint16_t>(next + 1)) == 0xA0) {
        ++count;
        next = static_cast<std::uint16_t>(next + 2);
    }
    return count;
}

unsigned long BoardSlot::spiBlocks() const noexcept
{
    return spiBlockCount;
}

unsigned long BoardSlot::spiBlockErrors() const noexcept
{
    return spiErrorCount;
}

} // namespace usbslot
