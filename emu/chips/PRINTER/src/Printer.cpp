#include "PRINTER/Printer.hpp"

namespace printer {

void Printer::reset() noexcept
{
    data = 0xFF;
    strobeHigh = false;
    dataPending = false;
}

void Printer::writeData(std::uint8_t value) noexcept
{
    data = value;
    dataPending = true;
}

void Printer::writeStrobe(bool high)
{
    if (high && !strobeHigh && dataPending) {
        printedBytes.push_back(data);
        dataPending = false;
    }
    strobeHigh = high;
}

std::uint8_t Printer::status() const noexcept
{
    return 0xFD;
}

const std::vector<std::uint8_t>& Printer::output() const noexcept
{
    return printedBytes;
}

void Printer::clearOutput() noexcept
{
    printedBytes.clear();
}

} // namespace printer
