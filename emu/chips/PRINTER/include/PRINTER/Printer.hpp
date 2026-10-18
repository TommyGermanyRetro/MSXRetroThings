#pragma once

#include <cstdint>
#include <vector>

namespace printer {

class Printer final {
public:
    void reset() noexcept;
    void writeData(std::uint8_t value) noexcept;
    void writeStrobe(bool high);

    [[nodiscard]] std::uint8_t status() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& output() const noexcept;
    void clearOutput() noexcept;

private:
    std::uint8_t data = 0xFF;
    bool strobeHigh = false;
    bool dataPending = false;
    std::vector<std::uint8_t> printedBytes;
};

} // namespace printer
