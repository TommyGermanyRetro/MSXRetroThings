#pragma once

#include "SVI3X8/ExpansionPort.hpp"
#include "WD1002SHD/WD1002SHD.hpp"

#include <cstdint>

namespace lanhd01 {

// SASI host adapter of the SVI-608M (LAN-HD-01) with the WD1002-SHD and its hard disk behind it.
// 40h W: bus reset. 41h W: data bus; in the Command and Data Out phases the byte goes to the
// target with an automatic ACK, in bus free it is the selection ID. 42h R: data from the target
// with an automatic ACK (Data In, Status, Message). 43h W: SEL, selects the target whose ID bit is
// on the data bus. 44h W: further bus control, without effect here. 46h R: bits 0-4 BSY, REQ,
// MSG, C/D, I/O (phases 00h bus free, 0Bh Command, 03h Data Out, 13h Data In, 1Bh Status, 1Fh
// Message In); bit 5 set while a target is connected (used by the SVI-609 boot ROM).
class LANHD01 final : public svi3x8::ExpansionPort {
public:
    [[nodiscard]] wd1002shd::WD1002SHD& controller() noexcept;
    [[nodiscard]] const wd1002shd::WD1002SHD& controller() const noexcept;

    void reset() override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle readMemory(std::uint16_t address, std::uint8_t bankLines) const override;
    [[nodiscard]] svi3x8::ExpansionMemoryCycle writeMemory(std::uint16_t address, std::uint8_t value,
                                                           std::uint8_t bankLines) override;
    [[nodiscard]] std::uint8_t readIo(std::uint8_t port) override;
    void writeIo(std::uint8_t port, std::uint8_t value) override;
    void tick() override;
    [[nodiscard]] bool interruptRequest() const override;

private:
    wd1002shd::WD1002SHD target;
    std::uint8_t dataBus = 0;
};

} // namespace lanhd01
