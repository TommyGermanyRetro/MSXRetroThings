#pragma once

#include "SVI3X8/ExpansionPort.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace svi803 {

// SVI-803 16 KiB RAM card for the SVI-318: always in 8000h-BFFFh, without /RAMDIS. In an SVI-328
// the internal RAM bank 2 answers the same addresses first, so reads return the internal RAM.
class SVI803 final : public svi3x8::ExpansionPort {
public:
    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    std::array<std::uint8_t, 0x4000> ram{};
};

} // namespace svi803
