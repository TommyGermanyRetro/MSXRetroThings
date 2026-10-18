#include "Z80Bus/Z80Bus.hpp"

#include <chips/z80.h>

#include <cstring>
#include <type_traits>
#include <utility>

namespace z80bus {

struct Z80Bus::Impl {
    z80_t cpu{};
    std::uint64_t pins = 0;
    bool busAcknowledge = false;
};

namespace {

Outputs decodePins(std::uint64_t pins, bool busAcknowledge)
{
    Outputs result;
    result.address = Z80_GET_ADDR(pins);
    result.dataBus = Z80_GET_DATA(pins);
    if (!busAcknowledge) {
        result.m1 = (pins & Z80_M1) != 0;
        result.memoryRequest = (pins & Z80_MREQ) != 0;
        result.ioRequest = (pins & Z80_IORQ) != 0;
        result.read = (pins & Z80_RD) != 0;
        result.write = (pins & Z80_WR) != 0;
        result.halt = (pins & Z80_HALT) != 0;
        result.refresh = (pins & Z80_RFSH) != 0;
    }
    result.busAcknowledge = busAcknowledge;
    result.interruptEnableOut = (pins & Z80_IEIO) != 0;
    result.reti = !busAcknowledge && ((pins & Z80_RETI) != 0);
    return result;
}

} // namespace

Z80Bus::Z80Bus()
    : impl(std::make_unique<Impl>())
{
    reset();
}

Z80Bus::~Z80Bus() = default;
Z80Bus::Z80Bus(Z80Bus&&) noexcept = default;
Z80Bus& Z80Bus::operator=(Z80Bus&&) noexcept = default;

void Z80Bus::reset()
{
    impl->pins = z80_reset(&impl->cpu);
    impl->busAcknowledge = false;
}

Outputs Z80Bus::tick(const Inputs& inputs)
{
    if (inputs.reset) {
        reset();
        return decodePins(impl->pins, false);
    }

    if (inputs.busRequest && !impl->busAcknowledge && z80_opdone(&impl->cpu)) {
        impl->busAcknowledge = true;
    } else if (!inputs.busRequest) {
        impl->busAcknowledge = false;
    }

    if (!impl->busAcknowledge) {
        const auto previousPins = impl->pins;
        auto incomingPins = impl->pins & ~(Z80_INT | Z80_NMI | Z80_WAIT | Z80_IEIO);
        if (inputs.intRequest) incomingPins |= Z80_INT;
        if (inputs.nmiRequest) incomingPins |= Z80_NMI;
        if (inputs.wait) incomingPins |= Z80_WAIT;
        if (inputs.interruptEnableIn) incomingPins |= Z80_IEIO;
        if (inputs.driveDataBus) Z80_SET_DATA(incomingPins, inputs.dataBus);
        impl->pins = z80_tick(&impl->cpu, incomingPins);
        if (inputs.wait && (previousPins & (Z80_MREQ | Z80_IORQ))) {
            constexpr std::uint64_t BusCycleMask = 0xFFFFULL | 0xFF0000ULL |
                Z80_M1 | Z80_MREQ | Z80_IORQ | Z80_RD | Z80_WR | Z80_RFSH;
            const auto heldBus = previousPins & BusCycleMask;
            impl->pins = (impl->pins & ~BusCycleMask) | heldBus;
        }
    }

    return decodePins(impl->pins, impl->busAcknowledge);
}

bool Z80Bus::instructionComplete() const noexcept
{
    return z80_opdone(&impl->cpu);
}

std::vector<std::uint8_t> Z80Bus::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the CPU state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool Z80Bus::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    std::memcpy(impl.get(), state.data(), sizeof(Impl));
    return true;
}

} // namespace z80bus