#include "PPI8255/PPI8255.hpp"

#include <cstring>
#include <type_traits>
#include <utility>

namespace ppi8255 {

struct PPI8255::Impl {
    enum class ModeA : std::uint8_t { Mode0, Mode1, Mode2 };

    Inputs pins{};
    std::uint8_t control = 0x9B;
    std::uint8_t outputA = 0;
    std::uint8_t outputB = 0;
    std::uint8_t outputC = 0;
    std::uint8_t inputLatchA = 0xFF;
    std::uint8_t inputLatchB = 0xFF;

    bool inteAInput = false;
    bool inteAOutput = false;
    bool inteB = false;
    bool ibfA = false;
    bool obfA = false;
    bool intrA = false;
    bool ibfB = false;
    bool obfB = false;
    bool intrB = false;
    bool acknowledgeSeenA = false;
    bool acknowledgeCompleteA = false;
    bool acknowledgeSeenB = false;
    bool acknowledgeCompleteB = false;

    [[nodiscard]] ModeA modeA() const noexcept
    {
        if (control & 0x40) return ModeA::Mode2;
        return (control & 0x20) ? ModeA::Mode1 : ModeA::Mode0;
    }

    [[nodiscard]] bool inputA() const noexcept { return (control & 0x10) != 0; }
    [[nodiscard]] bool mode1B() const noexcept { return (control & 0x04) != 0; }
    [[nodiscard]] bool inputB() const noexcept { return (control & 0x02) != 0; }

    [[nodiscard]] bool mode1InputA() const noexcept
    {
        return modeA() == ModeA::Mode1 && inputA();
    }

    [[nodiscard]] bool mode1OutputA() const noexcept
    {
        return modeA() == ModeA::Mode1 && !inputA();
    }

    void updateInterruptA() noexcept
    {
        const bool inputInterrupt =
            (mode1InputA() || modeA() == ModeA::Mode2) && ibfA && inteAInput &&
            !pins.strobeAActive;
        const bool outputInterrupt =
            (mode1OutputA() || modeA() == ModeA::Mode2) && acknowledgeCompleteA &&
            inteAOutput && !pins.acknowledgeAActive;
        intrA = inputInterrupt || outputInterrupt;
    }

    void updateInterruptB() noexcept
    {
        if (!mode1B()) {
            intrB = false;
        } else if (inputB()) {
            intrB = ibfB && inteB && !pins.strobeBActive;
        } else {
            intrB = acknowledgeCompleteB && inteB && !pins.acknowledgeBActive;
        }
    }

    void setMode(std::uint8_t value) noexcept
    {
        control = value;
        outputA = outputB = outputC = 0;
        inputLatchA = inputLatchB = 0xFF;
        inteAInput = inteAOutput = inteB = false;
        ibfA = obfA = intrA = false;
        ibfB = obfB = intrB = false;
        acknowledgeSeenA = acknowledgeCompleteA = false;
        acknowledgeSeenB = acknowledgeCompleteB = false;
    }

    void writeBsr(std::uint8_t value) noexcept
    {
        const unsigned bit = (value >> 1) & 7;
        const bool set = (value & 1) != 0;
        const auto mask = static_cast<std::uint8_t>(1u << bit);

        if (bit == 4 && (mode1InputA() || modeA() == ModeA::Mode2)) {
            inteAInput = set;
            updateInterruptA();
            return;
        }
        if (bit == 6 && (mode1OutputA() || modeA() == ModeA::Mode2)) {
            inteAOutput = set;
            updateInterruptA();
            return;
        }
        if (bit == 2 && mode1B()) {
            inteB = set;
            updateInterruptB();
            return;
        }

        outputC = set ? static_cast<std::uint8_t>(outputC | mask)
                      : static_cast<std::uint8_t>(outputC & ~mask);
    }

    [[nodiscard]] std::uint8_t readA()
    {
        if (mode1InputA() || modeA() == ModeA::Mode2) {
            const auto value = inputLatchA;
            ibfA = false;
            updateInterruptA();
            return value;
        }
        return inputA() ? pins.portA : outputA;
    }

    [[nodiscard]] std::uint8_t readB()
    {
        if (mode1B() && inputB()) {
            const auto value = inputLatchB;
            ibfB = false;
            updateInterruptB();
            return value;
        }
        return inputB() ? pins.portB : outputB;
    }

    void writeA(std::uint8_t value) noexcept
    {
        outputA = value;
        if (mode1OutputA() || modeA() == ModeA::Mode2) {
            obfA = true;
            acknowledgeSeenA = false;
            acknowledgeCompleteA = false;
            updateInterruptA();
        }
    }

    void writeB(std::uint8_t value) noexcept
    {
        outputB = value;
        if (mode1B() && !inputB()) {
            obfB = true;
            acknowledgeSeenB = false;
            acknowledgeCompleteB = false;
            updateInterruptB();
        }
    }

    [[nodiscard]] std::uint8_t readC() const noexcept
    {
        std::uint8_t inputMask = 0;
        if (control & 0x08) inputMask |= 0xF0;
        if (control & 0x01) inputMask |= 0x0F;

        std::uint8_t handshakeMask = 0;
        std::uint8_t handshakeValues = 0;

        if (modeA() == ModeA::Mode1) {
            if (inputA()) {
                handshakeMask |= 0x38;
                handshakeValues |= static_cast<std::uint8_t>((!pins.strobeAActive) ? 0x10 : 0);
                handshakeValues |= static_cast<std::uint8_t>(ibfA ? 0x20 : 0);
            } else {
                handshakeMask |= 0xC8;
                handshakeValues |= static_cast<std::uint8_t>((!pins.acknowledgeAActive) ? 0x40 : 0);
                handshakeValues |= static_cast<std::uint8_t>(obfA ? 0 : 0x80);
            }
            handshakeValues |= static_cast<std::uint8_t>(intrA ? 0x08 : 0);
        } else if (modeA() == ModeA::Mode2) {
            handshakeMask |= 0xF8;
            handshakeValues |= static_cast<std::uint8_t>((!pins.strobeAActive) ? 0x10 : 0);
            handshakeValues |= static_cast<std::uint8_t>(ibfA ? 0x20 : 0);
            handshakeValues |= static_cast<std::uint8_t>((!pins.acknowledgeAActive) ? 0x40 : 0);
            handshakeValues |= static_cast<std::uint8_t>(obfA ? 0 : 0x80);
            handshakeValues |= static_cast<std::uint8_t>(intrA ? 0x08 : 0);
        }

        if (mode1B()) {
            handshakeMask |= 0x07;
            handshakeValues |= static_cast<std::uint8_t>(
                inputB() ? (pins.strobeBActive ? 0 : 0x04)
                         : (pins.acknowledgeBActive ? 0 : 0x04));
            handshakeValues |= static_cast<std::uint8_t>(inputB() ? (ibfB ? 0x02 : 0)
                                                                  : (obfB ? 0 : 0x02));
            handshakeValues |= static_cast<std::uint8_t>(intrB ? 0x01 : 0);
        }

        const auto generalInputMask = static_cast<std::uint8_t>(inputMask & ~handshakeMask);
        const auto generalOutputMask = static_cast<std::uint8_t>(~inputMask & ~handshakeMask);
        auto value = static_cast<std::uint8_t>((pins.portC & generalInputMask) |
                                               (outputC & generalOutputMask));
        value = static_cast<std::uint8_t>((value & ~handshakeMask) |
                                          (handshakeValues & handshakeMask));
        return value;
    }

    [[nodiscard]] Outputs getOutputs() const noexcept
    {
        Outputs result;
        result.portA = outputA;
        result.portB = outputB;
        result.portC = readC();

        const auto currentModeA = modeA();
        if (currentModeA == ModeA::Mode2) {
            result.portADriveMask = obfA ? 0xFF : 0;
        } else if (!inputA()) {
            result.portADriveMask = 0xFF;
        }

        if (!inputB()) result.portBDriveMask = 0xFF;

        std::uint8_t inputMask = 0;
        if (control & 0x08) inputMask |= 0xF0;
        if (control & 0x01) inputMask |= 0x0F;
        std::uint8_t handshakeMask = 0;
        std::uint8_t handshakeOutputMask = 0;

        if (currentModeA == ModeA::Mode1) {
            if (inputA()) {
                handshakeMask |= 0x38;
                handshakeOutputMask |= 0x28;
            } else {
                handshakeMask |= 0xC8;
                handshakeOutputMask |= 0x88;
            }
        } else if (currentModeA == ModeA::Mode2) {
            handshakeMask |= 0xF8;
            handshakeOutputMask |= 0xA8;
        }
        if (mode1B()) {
            handshakeMask |= 0x07;
            handshakeOutputMask |= 0x03;
        }
        result.portCDriveMask = static_cast<std::uint8_t>(
            ((~inputMask) & (~handshakeMask)) | handshakeOutputMask);

        result.inputBufferFullA = ibfA;
        result.outputBufferFullA = obfA;
        result.interruptA = intrA;
        result.inputBufferFullB = ibfB;
        result.outputBufferFullB = obfB;
        result.interruptB = intrB;
        return result;
    }
};

PPI8255::PPI8255()
    : impl(std::make_unique<Impl>())
{
    reset();
}

PPI8255::~PPI8255() = default;
PPI8255::PPI8255(PPI8255&&) noexcept = default;
PPI8255& PPI8255::operator=(PPI8255&&) noexcept = default;

void PPI8255::reset()
{
    const auto pins = impl->pins;
    *impl = Impl{};
    impl->pins = pins;
}

std::vector<std::uint8_t> PPI8255::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the PPI state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool PPI8255::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    std::memcpy(impl.get(), state.data(), sizeof(Impl));
    return true;
}

void PPI8255::setInputs(const Inputs& inputs)
{
    const auto oldPins = impl->pins;
    impl->pins = inputs;

    const bool inputModeA = impl->mode1InputA() || impl->modeA() == Impl::ModeA::Mode2;
    if (inputModeA && !oldPins.strobeAActive && inputs.strobeAActive) {
        impl->inputLatchA = inputs.portA;
        impl->ibfA = true;
        impl->updateInterruptA();
    }
    if (inputModeA && oldPins.strobeAActive && !inputs.strobeAActive) {
        impl->updateInterruptA();
    }

    const bool outputModeA = impl->mode1OutputA() || impl->modeA() == Impl::ModeA::Mode2;
    if (outputModeA && !oldPins.acknowledgeAActive && inputs.acknowledgeAActive && impl->obfA) {
        impl->obfA = false;
        impl->acknowledgeSeenA = true;
        impl->acknowledgeCompleteA = false;
        impl->updateInterruptA();
    }
    if (outputModeA && oldPins.acknowledgeAActive && !inputs.acknowledgeAActive &&
        impl->acknowledgeSeenA) {
        impl->acknowledgeSeenA = false;
        impl->acknowledgeCompleteA = true;
        impl->updateInterruptA();
    }

    if (impl->mode1B()) {
        if (impl->inputB() && !oldPins.strobeBActive && inputs.strobeBActive) {
            impl->inputLatchB = inputs.portB;
            impl->ibfB = true;
            impl->updateInterruptB();
        }
        if (impl->inputB() && oldPins.strobeBActive && !inputs.strobeBActive) {
            impl->updateInterruptB();
        }
        if (!impl->inputB() && !oldPins.acknowledgeBActive &&
            inputs.acknowledgeBActive && impl->obfB) {
            impl->obfB = false;
            impl->acknowledgeSeenB = true;
            impl->acknowledgeCompleteB = false;
            impl->updateInterruptB();
        }
        if (!impl->inputB() && oldPins.acknowledgeBActive &&
            !inputs.acknowledgeBActive && impl->acknowledgeSeenB) {
            impl->acknowledgeSeenB = false;
            impl->acknowledgeCompleteB = true;
            impl->updateInterruptB();
        }
    }
}

std::uint8_t PPI8255::read(std::uint8_t registerAddress)
{
    switch (registerAddress & 3) {
    case 0: return impl->readA();
    case 1: return impl->readB();
    case 2: return impl->readC();
    default: return 0xFF;
    }
}

void PPI8255::write(std::uint8_t registerAddress, std::uint8_t value)
{
    switch (registerAddress & 3) {
    case 0:
        impl->writeA(value);
        break;
    case 1:
        impl->writeB(value);
        break;
    case 2:
        impl->outputC = value;
        break;
    default:
        if (value & 0x80) {
            impl->setMode(value);
        } else {
            impl->writeBsr(value);
        }
        break;
    }
}

Outputs PPI8255::outputs() const noexcept
{
    return impl->getOutputs();
}

std::uint8_t PPI8255::controlWord() const noexcept
{
    return impl->control;
}

bool PPI8255::isMode2A() const noexcept
{
    return impl->modeA() == Impl::ModeA::Mode2;
}

} // namespace ppi8255