#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace z80bus {

struct Inputs {
    bool intRequest = false;
    bool nmiRequest = false;
    bool wait = false;
    bool reset = false;
    bool busRequest = false;
    bool interruptEnableIn = false;
    bool driveDataBus = false;
    std::uint8_t dataBus = 0xFF;
};

struct Outputs {
    std::uint16_t address = 0;
    std::uint8_t dataBus = 0xFF;

    bool m1 = false;
    bool memoryRequest = false;
    bool ioRequest = false;
    bool read = false;
    bool write = false;
    bool halt = false;
    bool refresh = false;

    bool busAcknowledge = false;
    bool interruptEnableOut = false;
    bool reti = false;
};

class Z80Bus final {
public:
    Z80Bus();
    ~Z80Bus();

    Z80Bus(const Z80Bus&) = delete;
    Z80Bus& operator=(const Z80Bus&) = delete;
    Z80Bus(Z80Bus&&) noexcept;
    Z80Bus& operator=(Z80Bus&&) noexcept;

    void reset();

    // Advance exactly one emulated Z80 T-state and return its resulting bus pins.
    [[nodiscard]] Outputs tick(const Inputs& inputs = {});

    // True at the core's instruction-fetch boundary; prefixes are not reported complete.
    [[nodiscard]] bool instructionComplete() const noexcept;

    // Complete core state (registers, decoder step, pins) for snapshots; loadState() rejects data
    // of another size, as saved by a different build of the core.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace z80bus