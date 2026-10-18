#pragma once

#include "SVI3X8/ExpansionPort.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace svi807 {

// DIP switches S1-S5 of the SVI-807: the banks the card answers. Factory setting S2 and S3.
// The SVI-328 has bank 21 itself, so S1 stays off there; S5 (bank 02) replaces RAM bank 2.
struct SVI807Switches {
    bool bank21 = false;
    bool bank22 = true;
    bool bank31 = true;
    bool bank32 = false;
    bool bank02 = false;
};

// SVI-807 64 KiB RAM card: one 32 KiB half for 0000h-7FFFh (banks 21/31), one for 8000h-FFFFh
// (banks 22/32/02). While it answers, it switches the internal ROM or RAM bank 2 off. In bank 02 it
// stands in for RAM bank 2 and gives way to another card's /RAMDIS, for example the SVI-806 video RAM.
class SVI807 final : public svi3x8::ExpansionPort {
public:
    explicit SVI807(SVI807Switches switches = {}) noexcept;

    [[nodiscard]] const SVI807Switches& switches() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    [[nodiscard]] bool answers(std::uint16_t address, std::uint8_t bankLines) const noexcept;
    [[nodiscard]] bool onlyBank02(std::uint8_t bankLines) const noexcept;

    SVI807Switches settings;
    std::array<std::uint8_t, 0x8000> lowHalf{};
    std::array<std::uint8_t, 0x8000> highHalf{};
};

} // namespace svi807
