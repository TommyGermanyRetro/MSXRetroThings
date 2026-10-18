#pragma once

#include "SVI3X8/ExpansionPort.hpp"
#include "SVI801/DiskUnit.hpp"

#include <cstdint>

namespace svi801 {

// SVI-801 floppy disk controller cartridge: the disk unit (FD1793 at 30h-34h and 38h) with two
// external SVI-902 drives on its flat cable. The drives are single-sided; a blank disk for
// formatting is single-sided.
class SVI801 final : public svi3x8::ExpansionPort {
public:
    SVI801();

    [[nodiscard]] DiskUnit& disks() noexcept;
    [[nodiscard]] const DiskUnit& disks() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    DiskUnit unit;
};

} // namespace svi801
