#pragma once

#include "MC6821/MC6821.hpp"
#include "PRINTER/Printer.hpp"
#include "SVI3X8/ExpansionPort.hpp"

#include <cstdint>

namespace lanif01 {

// Printer port on the LAN card of the SVI-609: an MC6821 (HD46821) at 60h-63h (PRA/DDRA, CRA, PRB/DDRB, CRB).
// Port A carries the data inverted (the CP/M 2.27 BIOS writes the complement), CA2 drives the
// printer's /STROBE through an inverter, the printer's /ACK goes to CA1 and BUSY to PB0; the
// other port B lines read high. The PIA interrupt (IRQA/IRQB) reaches /INT.
class PiaPrinterPort final : public svi3x8::ExpansionPort {
public:
    PiaPrinterPort();

    [[nodiscard]] printer::Printer& printer() noexcept;
    [[nodiscard]] const printer::Printer& printer() const noexcept;
    [[nodiscard]] const mc6821::MC6821& pia() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    void followPins();

    mc6821::MC6821 chip;
    printer::Printer device;
    bool lastStrobe = true;
};

} // namespace lanif01
