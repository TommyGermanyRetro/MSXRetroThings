#pragma once

#include "SVI3X8/ExpansionPort.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace svi605b {

// Card slots of a Spectravideo expander on the SVI-318/328 expansion connector. Every card sees each
// memory and I/O cycle, CPUCLK and /RST; data lines combine as on an open bus (AND of all
// drivers, FFh when none drives), /ROMDIS, /RAMDIS and /INT are wired-OR. A card standing in for
// RAM bank 2 stays off the bus while another card pulls /RAMDIS.
class CardCage : public svi3x8::ExpansionPort {
public:
    explicit CardCage(std::size_t slotCount);

    [[nodiscard]] std::size_t slotCount() const noexcept;
    // Places a card in a slot below slotCount() and returns the card it replaces (nullptr if the
    // slot was empty); for a slot beyond slotCount() it returns the card itself.
    std::unique_ptr<svi3x8::ExpansionPort> insertCard(std::size_t slot, std::unique_ptr<svi3x8::ExpansionPort> card);
    std::unique_ptr<svi3x8::ExpansionPort> removeCard(std::size_t slot);
    [[nodiscard]] svi3x8::ExpansionPort* card(std::size_t slot) const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    [[nodiscard]] bool ramDisabledByOthers(std::uint16_t address, std::uint8_t bankLines) const;

    std::vector<std::unique_ptr<svi3x8::ExpansionPort>> slots;
};

} // namespace svi605b
