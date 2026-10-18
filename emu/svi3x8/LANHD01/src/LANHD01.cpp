#include "LANHD01/LANHD01.hpp"

namespace lanhd01 {

namespace {

constexpr std::uint8_t TargetPresent = 0x20;

} // namespace

wd1002shd::WD1002SHD& LANHD01::controller() noexcept
{
    return target;
}

const wd1002shd::WD1002SHD& LANHD01::controller() const noexcept
{
    return target;
}

void LANHD01::reset()
{
    target.reset();
    dataBus = 0;
}

svi3x8::ExpansionMemoryCycle LANHD01::readMemory(std::uint16_t, std::uint8_t) const
{
    return {};
}

svi3x8::ExpansionMemoryCycle LANHD01::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t LANHD01::readIo(std::uint8_t port)
{
    if (port == 0x42) {
        const auto signals = target.signals();
        if ((signals & wd1002shd::SignalReq) && (signals & wd1002shd::SignalIo)) dataBus = target.readData();
        return dataBus;
    }
    if (port == 0x46) return static_cast<std::uint8_t>(target.signals() | TargetPresent);
    return 0xFF;
}

void LANHD01::writeIo(std::uint8_t port, std::uint8_t value)
{
    switch (port) {
    case 0x40:
        target.reset();
        break;
    case 0x41: {
        dataBus = value;
        const auto signals = target.signals();
        if ((signals & wd1002shd::SignalReq) && !(signals & wd1002shd::SignalIo)) target.writeData(value);
        break;
    }
    case 0x43:
        (void)target.select(dataBus);
        break;
    default:
        break;
    }
}

void LANHD01::tick()
{
}

bool LANHD01::interruptRequest() const
{
    return false;
}

} // namespace lanhd01
