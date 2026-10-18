#pragma once

#include "WD2793/FloppyDrive.hpp"

#include <cstdint>
#include <vector>
#include <memory>

namespace wd2793 {

// Western Digital WD2793 floppy disk controller in double density (MFM, 250 kbit/s) with a
// 1 MHz clock: 32 us per byte, step rates of 6/12/20/30 ms and a 30 ms head settling time.
class WD2793 final {
public:
    // clockHz is the frequency at which the host calls advance().
    explicit WD2793(std::uint32_t clockHz);
    ~WD2793();

    WD2793(const WD2793&) = delete;
    WD2793& operator=(const WD2793&) = delete;
    WD2793(WD2793&&) noexcept;
    WD2793& operator=(WD2793&&) noexcept;

    // Master reset: clears the registers and executes a Restore with the slowest step rate.
    void reset();

    // The drive on the controller's drive interface, nullptr when no drive is selected.
    void selectDrive(FloppyDrive* drive) noexcept;

    [[nodiscard]] std::uint8_t readStatus() noexcept;
    [[nodiscard]] std::uint8_t readTrack() const noexcept;
    [[nodiscard]] std::uint8_t readSector() const noexcept;
    [[nodiscard]] std::uint8_t readData() noexcept;
    void writeCommand(std::uint8_t value) noexcept;
    void writeTrack(std::uint8_t value) noexcept;
    void writeSector(std::uint8_t value) noexcept;
    void writeData(std::uint8_t value) noexcept;

    // Register values without the side effects of a read.
    [[nodiscard]] std::uint8_t peekStatus() const noexcept;
    [[nodiscard]] std::uint8_t peekData() const noexcept;

    [[nodiscard]] bool interruptRequest() const noexcept;
    [[nodiscard]] bool dataRequest() const noexcept;
    [[nodiscard]] bool busy() const noexcept;

    // Advances time by elapsed clocks of the clockHz given to the constructor.
    void advance(std::uint32_t elapsed) noexcept;

    // Registers, command phase, timing and sector buffer for snapshots; the connected drive stays.
    [[nodiscard]] std::vector<std::uint8_t> saveState() const;
    [[nodiscard]] bool loadState(const std::vector<std::uint8_t>& state);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace wd2793
