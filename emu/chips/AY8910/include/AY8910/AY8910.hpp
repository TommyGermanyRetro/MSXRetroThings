#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ay8910 {

inline constexpr std::size_t RegisterCount = 16;
inline constexpr std::size_t ChannelCount = 3;

// AY-3-8910: 16-step envelope; YM2149: 32-step envelope and 32-level D/A converter.
enum class Model : std::uint8_t {
    AY8910,
    YM2149
};

struct Sample {
    std::array<std::int16_t, ChannelCount> channel{};
};

struct PortState {
    std::uint8_t portA = 0xFF;
    std::uint8_t portB = 0xFF;
    std::uint8_t portADriveMask = 0;
    std::uint8_t portBDriveMask = 0;
};

class AY8910 final {
public:
    explicit AY8910(std::uint32_t masterClockHz = 1773400, Model model = Model::AY8910);
    ~AY8910();

    AY8910(const AY8910&) = delete;
    AY8910& operator=(const AY8910&) = delete;
    AY8910(AY8910&&) noexcept;
    AY8910& operator=(AY8910&&) noexcept;

    void reset() noexcept;

    void selectRegister(std::uint8_t index) noexcept;
    void writeData(std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readData() const noexcept;

    // YM-style address/data bus: even selects a register, odd reads/writes it.
    void writeAddressData(bool dataSelect, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t readAddressData() const noexcept;

    void setPortInputs(std::uint8_t portA, std::uint8_t portB) noexcept;
    [[nodiscard]] PortState ports() const noexcept;

    // Generate three independent, unsigned 16-bit DAC-level streams.
    [[nodiscard]] std::vector<Sample> render(std::size_t sampleCount,
                                             std::uint32_t sampleRate);

    [[nodiscard]] std::uint8_t registerValue(std::uint8_t index) const noexcept;
    [[nodiscard]] std::uint8_t selectedRegister() const noexcept;
    [[nodiscard]] std::uint32_t masterClockHz() const noexcept;
    [[nodiscard]] Model model() const noexcept;

    // MSX engines wire port A as input and port B as output whatever register 7 bits 6-7 say.
    void setFixedPortDirections(bool fixed) noexcept;

    // Current envelope step: 0-15 (AY-3-8910) or 0-31 (YM2149).
    [[nodiscard]] std::uint8_t envelopeLevel() const noexcept;

    // Complete register, generator and port state for snapshots; loadState() rejects data of
    // another size.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace ay8910