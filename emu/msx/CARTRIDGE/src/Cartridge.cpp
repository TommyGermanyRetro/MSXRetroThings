#include "CARTRIDGE/Cartridge.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace cartridge {

namespace {

constexpr std::size_t RegionSize = 0x2000;
constexpr std::size_t PlainMaximum = 0x10000;

// Page of a plain ROM with an "AB" header: the init, statement or device address points there.
std::uint16_t plainBaseOf(const std::vector<std::uint8_t>& image) noexcept
{
    if (image.size() > 0xC000) return 0x0000;
    if (image.size() >= 4 && image[0] == 'A' && image[1] == 'B') {
        for (const std::size_t field : {2u, 4u, 6u}) {
            const unsigned address = image[field] | (image[field + 1] << 8);
            if (address >= 0x8000 && address < 0xC000 && image.size() <= 0x4000) return 0x8000;
        }
    }
    return 0x4000;
}

} // namespace

std::string_view mapperName(Mapper mapper) noexcept
{
    switch (mapper) {
    case Mapper::Ascii8:
        return "ASCII8";
    case Mapper::Ascii16:
        return "ASCII16";
    case Mapper::Konami:
        return "Konami";
    case Mapper::KonamiScc:
        return "Konami SCC";
    default:
        return "no mapper";
    }
}

Mapper detectMapper(const std::vector<std::uint8_t>& image) noexcept
{
    if (image.size() <= PlainMaximum) return Mapper::None;
    // Votes per mapper for each LD (nnnn),A to one of its bank registers.
    std::array<unsigned, 5> votes{};
    const auto vote = [&](Mapper mapper) { ++votes[static_cast<std::size_t>(mapper)]; };
    for (std::size_t index = 0; index + 2 < image.size(); ++index) {
        if (image[index] != 0x32) continue;
        const unsigned address = image[index + 1] | (image[index + 2] << 8);
        switch (address) {
        case 0x5000:
        case 0x9000:
        case 0xB000:
            vote(Mapper::KonamiScc);
            break;
        case 0x4000:
        case 0x8000:
        case 0xA000:
            vote(Mapper::Konami);
            break;
        case 0x6800:
        case 0x7800:
            vote(Mapper::Ascii8);
            break;
        case 0x6000:
            vote(Mapper::Konami);
            vote(Mapper::Ascii8);
            vote(Mapper::Ascii16);
            break;
        case 0x7000:
            vote(Mapper::KonamiScc);
            vote(Mapper::Ascii8);
            vote(Mapper::Ascii16);
            break;
        case 0x77FF:
            vote(Mapper::Ascii16);
            break;
        default:
            break;
        }
    }
    // Without writes to #6800/#7800 an ASCII mapper uses 16 KiB banks.
    if (votes[static_cast<std::size_t>(Mapper::Ascii8)] == votes[static_cast<std::size_t>(Mapper::Ascii16)]) {
        votes[static_cast<std::size_t>(Mapper::Ascii8)] = 0;
    }
    // Ties go to the earlier mapper: a ROM writing only to #6000 is an ASCII16 ROM.
    Mapper best = Mapper::Ascii16;
    unsigned bestVotes = 0;
    for (const Mapper candidate : {Mapper::Ascii16, Mapper::Ascii8, Mapper::Konami, Mapper::KonamiScc}) {
        if (votes[static_cast<std::size_t>(candidate)] > bestVotes) {
            bestVotes = votes[static_cast<std::size_t>(candidate)];
            best = candidate;
        }
    }
    return best;
}

bool Cartridge::insert(std::vector<std::uint8_t> image, Mapper mapper)
{
    if (image.empty()) return false;
    if (mapper == Mapper::None) {
        const std::uint16_t base = plainBaseOf(image);
        return insertAt(std::move(image), base);
    }
    const std::size_t bank = (mapper == Mapper::Ascii16) ? 0x4000 : 0x2000;
    if (image.size() % bank != 0) return false;
    rom = std::move(image);
    type = mapper;
    plainBase = 0;
    reset();
    return true;
}

bool Cartridge::insertAt(std::vector<std::uint8_t> image, std::uint16_t baseAddress)
{
    if (image.empty() || baseAddress + image.size() > PlainMaximum || baseAddress % RegionSize != 0) return false;
    rom = std::move(image);
    type = Mapper::None;
    plainBase = baseAddress;
    reset();
    return true;
}

void Cartridge::eject() noexcept
{
    rom.clear();
    type = Mapper::None;
    std::fill(std::begin(banks), std::end(banks), -1);
}

bool Cartridge::inserted() const noexcept
{
    return !rom.empty();
}

Mapper Cartridge::mapper() const noexcept
{
    return type;
}

std::size_t Cartridge::bankSize() const noexcept
{
    return type == Mapper::Ascii16 ? 0x4000 : 0x2000;
}

void Cartridge::selectBank(unsigned region, unsigned bank) noexcept
{
    const std::size_t count = rom.size() / bankSize();
    const int selected = static_cast<int>(bank % count);
    if (type == Mapper::Ascii16) {
        banks[region] = selected * 2;
        banks[region + 1] = selected * 2 + 1;
    } else {
        banks[region] = selected;
    }
}

void Cartridge::reset() noexcept
{
    std::fill(std::begin(banks), std::end(banks), -1);
    if (rom.empty()) return;
    switch (type) {
    case Mapper::None:
        for (std::size_t offset = 0; offset < rom.size(); offset += RegionSize) {
            banks[(plainBase + offset) / RegionSize] = static_cast<int>(offset / RegionSize);
        }
        break;
    case Mapper::Ascii8:
        for (unsigned region = 2; region < 6; ++region) selectBank(region, 0);
        break;
    case Mapper::Ascii16:
        selectBank(2, 0);
        selectBank(4, 0);
        break;
    case Mapper::Konami:
    case Mapper::KonamiScc:
        for (unsigned region = 2; region < 6; ++region) selectBank(region, region - 2);
        break;
    }
}

std::uint8_t Cartridge::read(std::uint16_t address) const noexcept
{
    const int bank = banks[address / RegionSize];
    if (bank < 0) return 0xFF;
    const std::size_t offset = static_cast<std::size_t>(bank) * RegionSize + (address % RegionSize);
    return offset < rom.size() ? rom[offset] : 0xFF;
}

void Cartridge::write(std::uint16_t address, std::uint8_t value) noexcept
{
    switch (type) {
    case Mapper::Ascii8:
        if (address >= 0x6000 && address < 0x8000) selectBank(2 + ((address - 0x6000) >> 11), value);
        break;
    case Mapper::Ascii16:
        if (address >= 0x6000 && address < 0x6800) selectBank(2, value);
        if (address >= 0x7000 && address < 0x7800) selectBank(4, value);
        break;
    case Mapper::Konami:
        if (address >= 0x6000 && address < 0xC000) selectBank(address / RegionSize, value);
        break;
    case Mapper::KonamiScc:
        if (address >= 0x5000 && address < 0xC000 && (address & 0x1800) == 0x1000) {
            selectBank(address / RegionSize, value);
        }
        break;
    default:
        break;
    }
}

int Cartridge::bankAt(std::uint16_t address) const noexcept
{
    return banks[address / RegionSize];
}

std::vector<std::uint8_t> Cartridge::saveState() const
{
    std::vector<std::uint8_t> state;
    const auto put32 = [&state](std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) state.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    state.push_back(static_cast<std::uint8_t>(type));
    put32(plainBase);
    put32(static_cast<std::uint32_t>(rom.size()));
    for (const int bank : banks) put32(static_cast<std::uint32_t>(bank));
    return state;
}

bool Cartridge::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != 1 + 4 + 4 + 8 * 4) return false;
    const auto get32 = [&state](std::size_t offset) {
        std::uint32_t value = 0;
        for (unsigned index = 0; index < 4; ++index) value |= static_cast<std::uint32_t>(state[offset + index]) << (8 * index);
        return value;
    };
    if (state[0] != static_cast<std::uint8_t>(type) || get32(1) != plainBase || get32(5) != rom.size()) return false;
    int restored[8];
    const auto regions = static_cast<int>(rom.size() / RegionSize);
    for (unsigned region = 0; region < 8; ++region) {
        restored[region] = static_cast<int>(get32(9 + 4 * region));
        if (restored[region] < -1 || restored[region] >= regions) return false;
    }
    std::copy(std::begin(restored), std::end(restored), std::begin(banks));
    return true;
}

} // namespace cartridge
