#pragma once

#include "SVI3X8/ExpansionPort.hpp"

#include <memory>

namespace svi602 {

// Spectravideo SVI-602 Mini Expander: one slot for an interface cartridge on the expansion
// connector. It passes every bus cycle, CPUCLK, /RST and /INT straight between the computer and the
// cartridge; with the slot empty the bus stays open (data FFh, no /ROMDIS, /RAMDIS or /INT).
class SVI602 final : public svi3x8::ExpansionPort {
public:
    // Places a cartridge in the slot and returns the one it replaces (nullptr if the slot was empty).
    std::unique_ptr<svi3x8::ExpansionPort> insertCard(std::unique_ptr<svi3x8::ExpansionPort> card);
    std::unique_ptr<svi3x8::ExpansionPort> removeCard();
    [[nodiscard]] svi3x8::ExpansionPort* card() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    std::unique_ptr<svi3x8::ExpansionPort> slot;
};

} // namespace svi602
