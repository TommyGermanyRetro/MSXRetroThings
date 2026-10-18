#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace ppi8255 {

struct Inputs {
    std::uint8_t portA = 0xFF;
    std::uint8_t portB = 0xFF;
    std::uint8_t portC = 0xFF;

    bool strobeAActive = false;
    bool acknowledgeAActive = false;
    bool strobeBActive = false;
    bool acknowledgeBActive = false;
};

struct Outputs {
    std::uint8_t portA = 0;
    std::uint8_t portB = 0;
    std::uint8_t portC = 0;

    std::uint8_t portADriveMask = 0;
    std::uint8_t portBDriveMask = 0;
    std::uint8_t portCDriveMask = 0;

    bool inputBufferFullA = false;
    bool outputBufferFullA = false;
    bool interruptA = false;
    bool inputBufferFullB = false;
    bool outputBufferFullB = false;
    bool interruptB = false;
};

class PPI8255 final {
public:
    PPI8255();
    ~PPI8255();

    PPI8255(const PPI8255&) = delete;
    PPI8255& operator=(const PPI8255&) = delete;
    PPI8255(PPI8255&&) noexcept;
    PPI8255& operator=(PPI8255&&) noexcept;

    void reset();
    void setInputs(const Inputs& inputs);

    [[nodiscard]] std::uint8_t read(std::uint8_t registerAddress);
    void write(std::uint8_t registerAddress, std::uint8_t value);

    [[nodiscard]] Outputs outputs() const noexcept;
    [[nodiscard]] std::uint8_t controlWord() const noexcept;
    [[nodiscard]] bool isMode2A() const noexcept;

    // Complete register, latch and pin state for snapshots; loadState() rejects data of another size.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace ppi8255