#include "AY8910/AY8910.hpp"

#include <cstdlib>
#include <initializer_list>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void writeRegister(ay8910::AY8910& chip, std::uint8_t index, std::uint8_t value)
{
    chip.selectRegister(index);
    chip.writeData(value);
}

void testRegisterMasksAndSelection()
{
    ay8910::AY8910 chip;
    chip.selectRegister(0x12);
    require(chip.selectedRegister() == 2, "register select wraps to four bits");
    chip.writeData(0xFF);
    require(chip.readData() == 0xFF, "tone fine register retains all eight bits");

    writeRegister(chip, 0, 0xFF);
    require(chip.readData() == 0xFF, "channel A fine period is eight bits wide");
    writeRegister(chip, 1, 0xFF);
    require(chip.readData() == 0x0F, "tone coarse register masks to four bits");
    writeRegister(chip, 6, 0xFF);
    require(chip.readData() == 0x1F, "noise period masks to five bits");
    writeRegister(chip, 8, 0xFF);
    require(chip.readData() == 0x1F, "volume and envelope-enable bits are retained");
    writeRegister(chip, 13, 0xFF);
    require(chip.readData() == 0x0F, "envelope shape masks to four bits");
}

void testToneGenerationAndMixer()
{
    ay8910::AY8910 chip(16000);
    writeRegister(chip, 0, 1);
    writeRegister(chip, 1, 0);
    writeRegister(chip, 7, 0x38);
    writeRegister(chip, 8, 15);

    const auto samples = chip.render(8, 2000);
    bool sawHigh = false;
    bool sawLow = false;
    for (const auto& sample : samples) {
        sawHigh |= sample.channel[0] > 0;
        sawLow |= sample.channel[0] == 0;
        require(sample.channel[1] == 0 && sample.channel[2] == 0,
                "channels have independent mixer outputs");
    }
    require(sawHigh && sawLow, "tone generator toggles at its programmed period");

    writeRegister(chip, 7, 0x39);
    const auto gated = chip.render(4, 2000);
    for (const auto& sample : gated) {
        require(sample.channel[0] == 32767,
                "disabling tone and noise opens the channel gate");
    }
}

void testNoiseAndEnvelope()
{
    ay8910::AY8910 chip(16000);
    writeRegister(chip, 7, 0x07);
    writeRegister(chip, 8, 15);
    const auto noise = chip.render(64, 1000);
    bool noiseHigh = false;
    bool noiseLow = false;
    for (const auto& sample : noise) {
        noiseHigh |= sample.channel[0] > 0;
        noiseLow |= sample.channel[0] == 0;
    }
    require(noiseHigh && noiseLow, "17-bit noise generator changes its output");

    chip.reset();
    writeRegister(chip, 7, 0x3F);
    writeRegister(chip, 8, 0x10);
    writeRegister(chip, 11, 1);
    writeRegister(chip, 12, 0);
    writeRegister(chip, 13, 0x0C);
    const auto envelope = chip.render(5, 16000 / 16);
    require(envelope[0].channel[0] > 0, "rising envelope leaves its low endpoint");
    require(envelope[1].channel[0] > envelope[0].channel[0],
            "envelope advances at its programmed period");
    require(envelope[4].channel[0] > envelope[1].channel[0],
            "envelope continues through its ramp");
}

void testGpioDirection()
{
    ay8910::AY8910 chip;
    chip.setPortInputs(0xA5, 0x5A);
    auto ports = chip.ports();
    require(ports.portA == 0xA5 && ports.portADriveMask == 0,
        "reset register 7 configures port A as input");
    require(ports.portB == 0x5A && ports.portBDriveMask == 0,
        "reset register 7 configures port B as input");

    writeRegister(chip, 7, 0xC0);
    writeRegister(chip, 14, 0x3C);
    writeRegister(chip, 15, 0xC3);
    ports = chip.ports();
    require(ports.portA == 0x3C && ports.portADriveMask == 0xFF,
            "port A becomes a driven output");
    require(ports.portB == 0xC3 && ports.portBDriveMask == 0xFF,
            "port B becomes a driven output");

        writeRegister(chip, 7, 0x00);
    ports = chip.ports();
        require(ports.portA == 0xA5 && ports.portADriveMask == 0,
            "input port A exposes external pin values");
    require(ports.portB == 0x5A && ports.portBDriveMask == 0,
            "input port B exposes external pin values");

        writeRegister(chip, 7, 0x40);
        ports = chip.ports();
        require(ports.portA == 0x3C && ports.portADriveMask == 0xFF,
            "port A and port B directions are independently configurable");
        require(ports.portB == 0x5A && ports.portBDriveMask == 0,
            "port B remains an input while port A is an output");

    // The CPU reads the pins: an output port reads its register ANDed with the external levels.
    chip.selectRegister(14);
    require(chip.readData() == (0x3C & 0xA5), "output port A reads its register ANDed with the external pins");
    chip.setPortInputs(0xFF, 0x5A);
    require(chip.readData() == 0x3C, "output port A without external contacts reads its register");
    writeRegister(chip, 14, 0xFF);
    chip.setPortInputs(0xFE, 0x5A);
    require(chip.readData() == 0xFE, "a contact pulling an output pin low reads 0, as a joystick on the SVI-328");
}

void testResetAndZeroSampleRate()
{
    ay8910::AY8910 chip(2000000);
    writeRegister(chip, 0, 0x34);
    chip.reset();
    require(chip.registerValue(0) == 0, "reset clears register state");
    require(chip.masterClockHz() == 2000000, "reset preserves configured master clock");

    const auto silent = chip.render(3, 0);
    require(silent.size() == 3, "render preserves requested sample count");
    require(silent[0].channel[0] == 0, "zero sample rate returns silence");
}

void testRenderChunkContinuity()
{
    ay8910::AY8910 continuous(1773400);
    ay8910::AY8910 chunked(1773400);
    for (auto* chip : {&continuous, &chunked}) {
        writeRegister(*chip, 0, 0x17);
        writeRegister(*chip, 1, 0x00);
        writeRegister(*chip, 7, 0x38);
        writeRegister(*chip, 8, 0x0F);
    }

    const auto expected = continuous.render(37, 44100);
    auto actual = chunked.render(11, 44100);
    const auto remaining = chunked.render(26, 44100);
    actual.insert(actual.end(), remaining.begin(), remaining.end());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(expected[i].channel == actual[i].channel,
                "render output is invariant to host audio block size");
    }
}

void testYm2149Envelope()
{
    constexpr std::uint32_t Clock = 1000000;
    ay8910::AY8910 ym(Clock, ay8910::Model::YM2149);
    ay8910::AY8910 ay(Clock, ay8910::Model::AY8910);
    for (ay8910::AY8910* chip : {&ym, &ay}) {
        writeRegister(*chip, 11, 1);
        writeRegister(*chip, 12, 0);
        writeRegister(*chip, 13, 0x0D);
    }
    require(ym.envelopeLevel() == 0 && ay.envelopeLevel() == 0, "an attack envelope starts at step 0");
    (void)ym.render(8, Clock);
    (void)ay.render(8, Clock);
    require(ym.envelopeLevel() == 1 && ay.envelopeLevel() == 0, "the YM2149 envelope steps twice as fast");
    (void)ay.render(8, Clock);
    require(ay.envelopeLevel() == 1, "the AY-3-8910 envelope steps every 16 clocks per period unit");
    (void)ym.render(8 * 30, Clock);
    require(ym.envelopeLevel() == 31, "the YM2149 envelope has 32 steps");
    (void)ym.render(64, Clock);
    require(ym.envelopeLevel() == 31, "HOLD keeps the last YM2149 step");

    writeRegister(ym, 13, 0x08);
    require(ym.envelopeLevel() == 31, "a decaying envelope starts at step 31");
    (void)ym.render(8 * 32, Clock);
    require(ym.envelopeLevel() == 31, "a repeating YM2149 sawtooth restarts after 32 steps");
}

void testYm2149Volume()
{
    ay8910::AY8910 ym(1000000, ay8910::Model::YM2149);
    ay8910::AY8910 ay(1000000, ay8910::Model::AY8910);
    for (ay8910::AY8910* chip : {&ym, &ay}) writeRegister(*chip, 7, 0x3F);
    const auto level = [](ay8910::AY8910& chip, std::uint8_t volume) {
        writeRegister(chip, 8, volume);
        return chip.render(1, 44100)[0].channel[0];
    };
    require(level(ym, 15) == 32767 && level(ay, 15) == 32767, "level 15 is full scale on both chips");
    require(level(ym, 14) == 23197 && level(ay, 14) == 26350, "the YM2149 maps fixed level L to step 2L+1");
    require(level(ym, 0) == 0, "fixed level 0 is silent");
    writeRegister(ym, 11, 0xFF);
    writeRegister(ym, 12, 0xFF);
    writeRegister(ym, 13, 0x0D);
    require(level(ym, 0x10) == 0, "envelope step 0 is silent");
}

void testFixedPortDirections()
{
    ay8910::AY8910 chip(1000000, ay8910::Model::YM2149);
    chip.setFixedPortDirections(true);
    chip.setPortInputs(0xA5, 0x5A);
    writeRegister(chip, 14, 0x3C);
    writeRegister(chip, 15, 0xC3);
    writeRegister(chip, 7, 0x00);
    auto ports = chip.ports();
    require(ports.portB == 0xC3 && ports.portBDriveMask == 0xFF, "port B stays an output with R7 bit 7 clear");
    writeRegister(chip, 7, 0xC0);
    ports = chip.ports();
    require(ports.portA == 0xA5 && ports.portADriveMask == 0, "port A stays an input with R7 bit 6 set");
    chip.selectRegister(14);
    require(chip.readData() == 0xA5, "reading R14 returns the port A pins");
    chip.reset();
    require(chip.ports().portBDriveMask == 0xFF && chip.model() == ay8910::Model::YM2149,
            "reset keeps the model and the fixed port directions");
}

} // namespace

int main()
{
    testRegisterMasksAndSelection();
    testToneGenerationAndMixer();
    testNoiseAndEnvelope();
    testGpioDirection();
    testResetAndZeroSampleRate();
    testRenderChunkContinuity();
    testYm2149Envelope();
    testYm2149Volume();
    testFixedPortDirections();
    std::cout << "All AY8910 tests passed.\n";
}