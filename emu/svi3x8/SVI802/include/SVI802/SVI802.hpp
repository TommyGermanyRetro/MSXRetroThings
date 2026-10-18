#pragma once

#include "SVI3X8/ExpansionPort.hpp"
#include "SVI802/CentronicsPort.hpp"

#include <cstdint>

namespace svi802 {

// SVI-802 Centronics interface cartridge: the printer port at 10h-12h.
class SVI802 final : public svi3x8::ExpansionPort {
public:
    [[nodiscard]] CentronicsPort& port() noexcept;
    [[nodiscard]] const CentronicsPort& port() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    CentronicsPort centronics;
};

} // namespace svi802
