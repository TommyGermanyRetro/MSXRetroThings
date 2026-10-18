#pragma once

#include "LANIF01/LANIF01.hpp"
#include "LANIF01/PiaPrinterPort.hpp"
#include "SVI3X8/ExpansionPort.hpp"

#include <cstdint>
#include <vector>

namespace lanif01 {

// LAN card of the SVI-609 Master Station: the fully fitted LAN-IF-01 board (a special version of the
// SVI-809). It carries the boot logic of the bootstrap card (4 KiB EPROM in place of the BASIC ROM
// after /RST, off after a write to 6Fh) and the printer PIA at 60h-63h, which can interrupt. The
// Z8530 SCC with its RS-422 drivers and the station DIP switches are not connected yet: their
// ports are not documented.
class LanCard final : public svi3x8::ExpansionPort {
public:
    [[nodiscard]] bool loadRom(const std::vector<std::uint8_t>& image);
    [[nodiscard]] bool romActive() const noexcept;

    // Printer port on the card.
    [[nodiscard]] PiaPrinterPort& printerPort() noexcept;
    [[nodiscard]] const PiaPrinterPort& printerPort() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    LANIF01 boot;
    PiaPrinterPort printer;
};

} // namespace lanif01
