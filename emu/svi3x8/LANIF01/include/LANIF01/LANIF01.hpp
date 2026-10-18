#pragma once

#include "SVI3X8/ExpansionPort.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lanif01 {

inline constexpr std::size_t BootRomSize = 4096;

// Bootstrap card of the SVI-608M (LAN-IF-01, a cut-down SVI-609 LAN card): after /RST it pulls
// /ROMDIS and supplies its 4 KiB EPROM ("NETROM 1.0") in place of the BASIC ROM, mirrored every
// 4 KiB through 0000h-7FFFh, for reads only. A write to port 6Fh switches the EPROM off until the
// next reset. The EPROM copies itself to 8000h, initialises the BASIC work area, and starts from
// floppy drive A if a disk answers there, otherwise from the SASI hard disk (LBA 0-1 to C100h).
class LANIF01 final : public svi3x8::ExpansionPort {
public:
    [[nodiscard]] bool loadRom(const std::vector<std::uint8_t>& image);
    [[nodiscard]] bool romActive() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    std::array<std::uint8_t, BootRomSize> rom{};
    bool active = true;
};

} // namespace lanif01
