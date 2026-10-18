#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>

namespace rp5c01 {

inline constexpr std::size_t BlockCount = 4;
inline constexpr std::size_t RegistersPerBlock = 13;
inline constexpr std::size_t ImageSize = BlockCount * RegistersPerBlock;

// Battery-backed contents: registers 0-12 of the time, alarm and both RAM blocks, one nibble per byte.
using Image = std::array<std::uint8_t, ImageSize>;

struct DateTime {
    unsigned year = 0;   // 0-99
    unsigned month = 1;  // 1-12
    unsigned day = 1;    // 1-31
    unsigned weekday = 0; // 0-6
    unsigned hour = 0;   // 0-23
    unsigned minute = 0;
    unsigned second = 0;
};

class RP5C01 final {
public:
    RP5C01();
    ~RP5C01();

    RP5C01(const RP5C01&) = delete;
    RP5C01& operator=(const RP5C01&) = delete;
    RP5C01(RP5C01&&) noexcept;
    RP5C01& operator=(RP5C01&&) noexcept;

    // Selects block 0 with the timer running; time, alarm and RAM survive.
    void reset() noexcept;

    // Address 0-15 (A3-A0), 4-bit data (D3-D0); unimplemented bits read 0.
    [[nodiscard]] std::uint8_t read(std::uint8_t address) const noexcept;
    void write(std::uint8_t address, std::uint8_t value) noexcept;

    // Advances the 1 Hz time base by elapsed clocks of a clockHz signal.
    void advance(std::uint32_t elapsed, std::uint32_t clockHz) noexcept;

    // Time in the current 12/24-hour mode of block 1 register A.
    void setDateTime(const DateTime& value) noexcept;
    [[nodiscard]] DateTime dateTime() const noexcept;

    [[nodiscard]] Image image() const noexcept;
    void loadImage(const Image& value) noexcept;

    // Complete register and time-base state for snapshots; loadState() rejects data of another size.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace rp5c01
