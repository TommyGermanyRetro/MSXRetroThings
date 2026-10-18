#include "AY8910/AY8910.hpp"

#include <cstring>
#include <type_traits>

#include <algorithm>
#include <array>
#include <utility>

namespace ay8910 {
namespace {

constexpr std::array<std::uint8_t, RegisterCount> RegisterMasks = {
    0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
    0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF
};

constexpr std::array<std::int16_t, 16> VolumeLevels = {
    0, 280, 400, 570, 810, 1150, 1630, 2310,
    3270, 4630, 6560, 9290, 13150, 18620, 26350, 32767
};

// YM2149: 32 logarithmic levels, approximated with 1.5 dB per step below full scale.
constexpr std::array<std::int16_t, 32> Ym2149VolumeLevels = {
    0, 184, 219, 260, 309, 368, 437, 519, 617, 734, 872, 1036, 1232, 1464, 1740, 2067,
    2457, 2920, 3471, 4125, 4903, 5827, 6925, 8231, 9782, 11626, 13818, 16422, 19518, 23197, 27570, 32767
};

constexpr std::uint32_t MinimumPeriod = 1;

} // namespace

struct AY8910::Impl {
    struct Tone {
        std::uint64_t phase = 0;
        bool high = false;
    };

    std::array<std::uint8_t, RegisterCount> registers{};
    std::array<Tone, ChannelCount> tones{};
    std::uint8_t selected = 0;
    std::uint8_t portAInput = 0xFF;
    std::uint8_t portBInput = 0xFF;
    std::uint32_t masterClock = 1773400;
    Model model = Model::AY8910;
    bool fixedPortDirections = false;
    std::uint32_t lastSampleRate = 0;
    std::uint64_t noisePhase = 0;
    std::uint64_t sampleClockRemainder = 0;
    std::uint32_t noiseLfsr = 1;
    bool noiseHigh = true;
    std::uint64_t envelopePhase = 0;
    std::int8_t envelopeStep = 0;
    std::uint8_t envelopeAttack = 0;
    bool envelopeHold = false;
    bool envelopeAlternate = false;
    bool envelopeHolding = true;
    bool envelopeContinue = false;
    std::uint8_t envelopeVolume = 0;

    Impl(std::uint32_t clock, Model chipModel)
        : masterClock(clock ? clock : 1)
        , model(chipModel)
    {
    }

    // The AY-3-8910 envelope has 16 steps per cycle, the YM2149 envelope 32 at twice the rate.
    [[nodiscard]] std::uint8_t envelopeMaximum() const noexcept { return model == Model::YM2149 ? 0x1F : 0x0F; }
    [[nodiscard]] std::uint32_t envelopeClocksPerStep() const noexcept { return model == Model::YM2149 ? 8 : 16; }

    [[nodiscard]] std::uint32_t tonePeriod(unsigned channel) const noexcept
    {
        const auto fine = registers[channel * 2];
        const auto coarse = registers[channel * 2 + 1] & 0x0F;
        const auto period = static_cast<std::uint32_t>(fine | (coarse << 8));
        return std::max(period, MinimumPeriod);
    }

    [[nodiscard]] std::uint32_t noisePeriod() const noexcept
    {
        return std::max<std::uint32_t>(registers[6] & 0x1F, MinimumPeriod);
    }

    [[nodiscard]] std::uint32_t envelopePeriod() const noexcept
    {
        const auto period = static_cast<std::uint32_t>(registers[11] |
                                                       (registers[12] << 8));
        return std::max(period, MinimumPeriod);
    }

    void restartEnvelope(std::uint8_t shape) noexcept
    {
        envelopeContinue = (shape & 0x08) != 0;
        envelopeAttack = (shape & 0x04) ? envelopeMaximum() : 0;
        if (!envelopeContinue) {
            envelopeHold = true;
            envelopeAlternate = envelopeAttack != 0;
        } else {
            envelopeHold = (shape & 0x01) != 0;
            envelopeAlternate = (shape & 0x02) != 0;
        }
        envelopeStep = static_cast<std::int8_t>(envelopeMaximum());
        envelopeVolume = static_cast<std::uint8_t>(envelopeStep) ^ envelopeAttack;
        envelopePhase = 0;
        envelopeHolding = false;
    }

    void clockEnvelope() noexcept
    {
        if (envelopeHolding) return;
        --envelopeStep;
        if (envelopeStep < 0) {
            if (envelopeHold) {
                if (envelopeAlternate) envelopeAttack ^= envelopeMaximum();
                envelopeHolding = true;
                envelopeStep = 0;
            } else {
                if (envelopeAlternate) envelopeAttack ^= envelopeMaximum();
                envelopeStep = static_cast<std::int8_t>(envelopeMaximum());
            }
        }
        envelopeVolume = static_cast<std::uint8_t>(envelopeStep) ^ envelopeAttack;
    }

    [[nodiscard]] bool portIsOutput(unsigned port) const noexcept
    {
        if (fixedPortDirections) return port == 1;
        return (registers[7] & (port == 0 ? 0x40 : 0x80)) != 0;
    }

    // The CPU reads the pins. An output port drives them through open-collector stages, so an
    // external contact that pulls a pin low still reads 0: the result is the output register ANDed
    // with the external levels. The SVI game Chess (JTM, 1986) writes FFh to register 7 and then
    // reads the joystick on port A. With the fixed directions of an MSX engine port B reads its
    // register and port A its pins.
    [[nodiscard]] std::uint8_t portValue(unsigned port) const noexcept
    {
        const auto dataRegister = port == 0 ? 14 : 15;
        const auto input = port == 0 ? portAInput : portBInput;
        if (!portIsOutput(port)) return input;
        if (fixedPortDirections) return registers[dataRegister];
        return static_cast<std::uint8_t>(registers[dataRegister] & input);
    }

    [[nodiscard]] std::uint8_t readSelected() const noexcept
    {
        if (selected == 14) return portValue(0);
        if (selected == 15) return portValue(1);
        return registers[selected] & RegisterMasks[selected];
    }

    void writeSelected(std::uint8_t value) noexcept
    {
        value &= RegisterMasks[selected];
        registers[selected] = value;
        if (selected == 13) restartEnvelope(value);
    }

    void clockTone(unsigned channel, std::uint64_t elapsedMasterClocks) noexcept
    {
        auto& tone = tones[channel];
        tone.phase += elapsedMasterClocks;
        const auto threshold = static_cast<std::uint64_t>(tonePeriod(channel)) * 8;
        while (tone.phase >= threshold) {
            tone.phase -= threshold;
            tone.high = !tone.high;
        }
    }

    void clockNoise(std::uint64_t elapsedMasterClocks) noexcept
    {
        noisePhase += elapsedMasterClocks;
        const auto threshold = static_cast<std::uint64_t>(noisePeriod()) * 16;
        while (noisePhase >= threshold) {
            noisePhase -= threshold;
            const auto feedback = (noiseLfsr ^ (noiseLfsr >> 3)) & 1;
            noiseLfsr = (noiseLfsr >> 1) | (feedback << 16);
            noiseHigh = (noiseLfsr & 1) != 0;
        }
    }

    [[nodiscard]] std::int16_t channelSample(unsigned channel) const noexcept
    {
        const auto mixer = registers[7];
        const bool toneEnabled = (mixer & (1u << channel)) == 0;
        const bool noiseEnabled = (mixer & (1u << (channel + 3))) == 0;
        const bool tonePasses = !toneEnabled || tones[channel].high;
        const bool noisePasses = !noiseEnabled || noiseHigh;
        if (!tonePasses || !noisePasses) return 0;

        const auto volumeRegister = registers[8 + channel];
        if (model == Model::YM2149) {
            // A fixed level L selects step 2L+1 of the 32-step curve.
            const unsigned fixed = volumeRegister & 0x0F;
            const unsigned step = (volumeRegister & 0x10) ? envelopeVolume : (fixed ? fixed * 2 + 1 : 0);
            return Ym2149VolumeLevels[step & 0x1F];
        }
        const auto volume = (volumeRegister & 0x10)
            ? envelopeVolume
            : static_cast<std::uint8_t>(volumeRegister & 0x0F);
        return VolumeLevels[volume & 0x0F];
    }
};

AY8910::AY8910(std::uint32_t masterClockHz, Model model)
    : impl(std::make_unique<Impl>(masterClockHz, model))
{
}

AY8910::~AY8910() = default;
AY8910::AY8910(AY8910&&) noexcept = default;
AY8910& AY8910::operator=(AY8910&&) noexcept = default;

void AY8910::reset() noexcept
{
    const auto clock = impl->masterClock;
    impl->registers.fill(0);
    impl->tones = {};
    impl->selected = 0;
    impl->portAInput = 0xFF;
    impl->portBInput = 0xFF;
    impl->lastSampleRate = 0;
    impl->noisePhase = 0;
    impl->sampleClockRemainder = 0;
    impl->noiseLfsr = 1;
    impl->noiseHigh = true;
    impl->envelopePhase = 0;
    impl->envelopeStep = 0;
    impl->envelopeAttack = 0;
    impl->envelopeHold = false;
    impl->envelopeAlternate = false;
    impl->envelopeHolding = true;
    impl->envelopeContinue = false;
    impl->envelopeVolume = 0;
    impl->masterClock = clock;
}

void AY8910::selectRegister(std::uint8_t index) noexcept
{
    impl->selected = index & 0x0F;
}

void AY8910::writeData(std::uint8_t value) noexcept
{
    impl->writeSelected(value);
}

std::uint8_t AY8910::readData() const noexcept
{
    return impl->readSelected();
}

void AY8910::writeAddressData(bool dataSelect, std::uint8_t value) noexcept
{
    if (dataSelect) writeData(value);
    else selectRegister(value);
}

std::uint8_t AY8910::readAddressData() const noexcept
{
    return readData();
}

void AY8910::setPortInputs(std::uint8_t portA, std::uint8_t portB) noexcept
{
    impl->portAInput = portA;
    impl->portBInput = portB;
}

PortState AY8910::ports() const noexcept
{
    PortState result;
    const bool portAOutput = impl->portIsOutput(0);
    const bool portBOutput = impl->portIsOutput(1);
    result.portA = portAOutput ? impl->registers[14] : impl->portAInput;
    result.portB = portBOutput ? impl->registers[15] : impl->portBInput;
    result.portADriveMask = portAOutput ? 0xFF : 0;
    result.portBDriveMask = portBOutput ? 0xFF : 0;
    return result;
}

std::vector<Sample> AY8910::render(std::size_t sampleCount, std::uint32_t sampleRate)
{
    std::vector<Sample> samples(sampleCount);
    if (sampleRate == 0) return samples;
    if (impl->lastSampleRate != sampleRate) {
        impl->lastSampleRate = sampleRate;
        impl->sampleClockRemainder = 0;
    }

    for (auto& sample : samples) {
        impl->sampleClockRemainder += impl->masterClock;
        const auto elapsed = impl->sampleClockRemainder / sampleRate;
        impl->sampleClockRemainder %= sampleRate;

        for (unsigned channel = 0; channel < ChannelCount; ++channel) {
            impl->clockTone(channel, elapsed);
        }
        impl->clockNoise(elapsed);

        impl->envelopePhase += elapsed;
        const auto envelopeThreshold = static_cast<std::uint64_t>(impl->envelopePeriod()) * impl->envelopeClocksPerStep();
        while (impl->envelopePhase >= envelopeThreshold) {
            impl->envelopePhase -= envelopeThreshold;
            impl->clockEnvelope();
        }

        for (unsigned channel = 0; channel < ChannelCount; ++channel) {
            sample.channel[channel] = impl->channelSample(channel);
        }
    }
    return samples;
}

std::uint8_t AY8910::registerValue(std::uint8_t index) const noexcept
{
    index &= 0x0F;
    return impl->registers[index] & RegisterMasks[index];
}

std::uint8_t AY8910::selectedRegister() const noexcept
{
    return impl->selected;
}

std::uint32_t AY8910::masterClockHz() const noexcept
{
    return impl->masterClock;
}

Model AY8910::model() const noexcept
{
    return impl->model;
}

void AY8910::setFixedPortDirections(bool fixed) noexcept
{
    impl->fixedPortDirections = fixed;
}

std::uint8_t AY8910::envelopeLevel() const noexcept
{
    return impl->envelopeVolume;
}

std::vector<std::uint8_t> AY8910::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the PSG state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool AY8910::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    std::memcpy(impl.get(), state.data(), sizeof(Impl));
    return true;
}

} // namespace ay8910