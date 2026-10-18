#pragma once

#include "PRINTER/Printer.hpp"

#include <cstdint>

namespace svi802 {

// Spectravideo Centronics printer port: 10h data (write), 11h bit 0 /STROBE (write), 12h bit 0 BUSY
// (read, 1 = busy, other bits 1). The printer latches the data byte on the rising /STROBE edge.
class CentronicsPort {
public:
    [[nodiscard]] printer::Printer& printer() noexcept;
    [[nodiscard]] const printer::Printer& printer() const noexcept;

    void reset();
    // FFh for ports the port does not answer.
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) const;
    void writeIo(std::uint8_t port, std::uint8_t value);

private:
    printer::Printer device;
};

} // namespace svi802
