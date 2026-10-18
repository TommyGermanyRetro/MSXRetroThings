#pragma once

#include <cstdint>

namespace svi3x8 {

// CPUCLK on the expansion connector: the Z80 clock, master clock 21,477,270 Hz / 6.
inline constexpr std::uint32_t CpuClockHz = 3579545;

// One memory cycle on the expansion bus: the data a device drives (FFh when none does) and the
// /ROMDIS and /RAMDIS lines with which a device switches off the internal ROM or RAM bank 2.
struct ExpansionMemoryCycle {
    std::uint8_t data = 0xFF;
    bool romDisable = false;
    bool ramDisable = false;
    // The device stands in for RAM bank 2 and, like the internal RAM, gives way while another
    // device pulls /RAMDIS.
    bool ramBank2 = false;
};

// Device side of the 50-pin expansion connector of the SVI-318 and SVI-328. bankLines is PSG port
// B with /BK21 (bit 1), /BK22 (bit 2), /BK31 (bit 3) and /BK32 (bit 4), active low. The machine
// passes every memory cycle outside the cartridge banks and every I/O cycle below port 80h.
class ExpansionPort {
public:
    virtual ~ExpansionPort() = default;

    // /RST.
    virtual void reset() = 0;
    [[nodiscard]] virtual ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const = 0;
    [[nodiscard]] virtual ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) = 0;
    [[nodiscard]] virtual std::uint8_t readIo(std::uint8_t port) = 0;
    virtual void writeIo(std::uint8_t port, std::uint8_t value) = 0;
    // CPUCLK: one Z80 T-state.
    virtual void tick() = 0;
    // /INT, shared with the VDP interrupt.
    [[nodiscard]] virtual bool interruptRequest() const = 0;
};

} // namespace svi3x8
