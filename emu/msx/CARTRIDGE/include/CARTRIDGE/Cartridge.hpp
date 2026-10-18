#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cartridge {

enum class Mapper : std::uint8_t {
    None,      // plain ROM up to 64 KiB without bank switching
    Ascii8,    // 8 KiB banks at #4000/#6000/#8000/#A000, registers #6000/#6800/#7000/#7800
    Ascii16,   // 16 KiB banks at #4000/#8000, registers #6000/#7000
    Konami,    // 8 KiB banks, #4000 fixed to bank 0, registers #6000/#8000/#A000
    KonamiScc, // 8 KiB banks at #4000-#BFFF, registers #5000/#7000/#9000/#B000 (no SCC sound)
};

[[nodiscard]] std::string_view mapperName(Mapper mapper) noexcept;

// Picks the mapper from the image size and the bank register addresses the code writes to with
// LD (nnnn),A; plain ROMs up to 64 KiB have no mapper.
[[nodiscard]] Mapper detectMapper(const std::vector<std::uint8_t>& image) noexcept;

// A ROM cartridge in one primary slot.
class Cartridge final {
public:
    Cartridge() = default;

    // Plain ROMs start at the page of their "AB" header, or at #0000 when larger than 48 KiB.
    // Mapped ROMs need a size that is a multiple of the bank size.
    [[nodiscard]] bool insert(std::vector<std::uint8_t> image, Mapper mapper);
    // Plain ROM at a fixed address.
    [[nodiscard]] bool insertAt(std::vector<std::uint8_t> image, std::uint16_t baseAddress);
    void eject() noexcept;
    [[nodiscard]] bool inserted() const noexcept;
    [[nodiscard]] Mapper mapper() const noexcept;

    // Selects the power-on banks.
    void reset() noexcept;

    [[nodiscard]] std::uint8_t read(std::uint16_t address) const noexcept;
    void write(std::uint16_t address, std::uint8_t value) noexcept;

    // Bank shown in an 8 KiB region (#0000, #2000, ... #E000); -1 when nothing is mapped there.
    [[nodiscard]] int bankAt(std::uint16_t address) const noexcept;

    // Bank registers for snapshots, with mapper, base address and ROM size of the image; loadState()
    // refuses data saved with another image or mapper. The ROM itself is not part of the state.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    [[nodiscard]] std::size_t bankSize() const noexcept;
    void selectBank(unsigned region, unsigned bank) noexcept;

    std::vector<std::uint8_t> rom;
    Mapper type = Mapper::None;
    std::uint16_t plainBase = 0;
    // Bank per 8 KiB region; 16 KiB mappers use two consecutive regions.
    int banks[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

} // namespace cartridge
