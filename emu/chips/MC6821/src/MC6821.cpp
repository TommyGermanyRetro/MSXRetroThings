#include "MC6821/MC6821.hpp"

#include <initializer_list>

namespace mc6821 {

namespace {

constexpr std::uint8_t C1Enable = 0x01;
constexpr std::uint8_t C1Rising = 0x02;
constexpr std::uint8_t SelectPeripheral = 0x04;
constexpr std::uint8_t C2Bit3 = 0x08;
constexpr std::uint8_t C2Bit4 = 0x10;
constexpr std::uint8_t C2Output = 0x20;
constexpr std::uint8_t Irq2Flag = 0x40;
constexpr std::uint8_t Irq1Flag = 0x80;

} // namespace

MC6821::MC6821()
{
    reset();
}

void MC6821::reset() noexcept
{
    for (auto* side : {&a, &b}) {
        side->output = 0;
        side->direction = 0;
        side->control = 0;
        side->c2Out = true;
    }
}

std::uint8_t MC6821::pins(const Side& side) noexcept
{
    return static_cast<std::uint8_t>((side.output & side.direction) | (side.inputs & ~side.direction));
}

bool MC6821::irq(const Side& side) noexcept
{
    const bool c1 = (side.control & Irq1Flag) && (side.control & C1Enable);
    const bool c2 = (side.control & Irq2Flag) && !(side.control & C2Output) && (side.control & C2Bit3);
    return c1 || c2;
}

void MC6821::updateC2Output(Side& side) noexcept
{
    if ((side.control & C2Output) && (side.control & C2Bit4)) side.c2Out = (side.control & C2Bit3) != 0;
    if (!(side.control & C2Output)) side.c2Out = true;
}

void MC6821::edgeC1(Side& side, bool level) noexcept
{
    const bool rising = !side.c1 && level;
    const bool falling = side.c1 && !level;
    side.c1 = level;
    if ((side.control & C1Rising) ? rising : falling) {
        side.control |= Irq1Flag;
        // Handshake mode with read/write strobe: the active C1 edge ends the C2 low phase.
        if ((side.control & (C2Output | C2Bit4 | C2Bit3)) == C2Output) side.c2Out = true;
    }
}

void MC6821::edgeC2(Side& side, bool level) noexcept
{
    const bool rising = !side.c2In && level;
    const bool falling = side.c2In && !level;
    side.c2In = level;
    if (!(side.control & C2Output) && ((side.control & C2Bit4) ? rising : falling)) side.control |= Irq2Flag;
}

std::uint8_t MC6821::read(std::uint8_t reg) noexcept
{
    switch (reg & 3) {
    case 0:
        if (!(a.control & SelectPeripheral)) return a.direction;
        a.control &= static_cast<std::uint8_t>(~(Irq1Flag | Irq2Flag));
        if ((a.control & (C2Output | C2Bit4)) == C2Output) {
            a.c2Out = false;
            if (a.control & C2Bit3) a.c2Out = true;
        }
        return pins(a);
    case 1: return a.control;
    case 2:
        if (!(b.control & SelectPeripheral)) return b.direction;
        b.control &= static_cast<std::uint8_t>(~(Irq1Flag | Irq2Flag));
        return static_cast<std::uint8_t>((b.output & b.direction) | (b.inputs & ~b.direction));
    default: return b.control;
    }
}

void MC6821::write(std::uint8_t reg, std::uint8_t value) noexcept
{
    switch (reg & 3) {
    case 0:
        if (a.control & SelectPeripheral) a.output = value;
        else a.direction = value;
        break;
    case 1:
        a.control = static_cast<std::uint8_t>((a.control & (Irq1Flag | Irq2Flag)) | (value & 0x3F));
        updateC2Output(a);
        break;
    case 2:
        if (b.control & SelectPeripheral) {
            b.output = value;
            if ((b.control & (C2Output | C2Bit4)) == C2Output) {
                b.c2Out = false;
                if (b.control & C2Bit3) b.c2Out = true;
            }
        } else {
            b.direction = value;
        }
        break;
    default:
        b.control = static_cast<std::uint8_t>((b.control & (Irq1Flag | Irq2Flag)) | (value & 0x3F));
        updateC2Output(b);
        break;
    }
}

void MC6821::setPortAInputs(std::uint8_t value) noexcept
{
    a.inputs = value;
}

void MC6821::setPortBInputs(std::uint8_t value) noexcept
{
    b.inputs = value;
}

void MC6821::setCA1(bool level) noexcept
{
    edgeC1(a, level);
}

void MC6821::setCB1(bool level) noexcept
{
    edgeC1(b, level);
}

void MC6821::setCA2(bool level) noexcept
{
    edgeC2(a, level);
}

void MC6821::setCB2(bool level) noexcept
{
    edgeC2(b, level);
}

std::uint8_t MC6821::portA() const noexcept
{
    return pins(a);
}

std::uint8_t MC6821::portB() const noexcept
{
    return pins(b);
}

bool MC6821::ca2() const noexcept
{
    return a.c2Out;
}

bool MC6821::cb2() const noexcept
{
    return b.c2Out;
}

bool MC6821::irqA() const noexcept
{
    return irq(a);
}

bool MC6821::irqB() const noexcept
{
    return irq(b);
}

} // namespace mc6821
