#include "SVI728/SVI728.hpp"
#include "PPI8255/PPI8255.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

bool runProgram(std::array<std::uint8_t, svi728::AddressSpaceSize>& memory,
                svi728::Cassette& tape)
{
    z80bus::Z80Bus cpu;
    ppi8255::PPI8255 ppi;
    ay8910::AY8910 psg;
    z80bus::Outputs pins{};
    for (std::uint32_t tick = 0; tick < svi728::CpuClockHz * 12 && !pins.halt; ++tick) {
        z80bus::Inputs inputs;
        if (pins.memoryRequest && pins.read) {
            inputs.driveDataBus = true;
            inputs.dataBus = memory[pins.address];
        } else if (pins.ioRequest && pins.read) {
            inputs.driveDataBus = true;
            const auto port = static_cast<std::uint8_t>(pins.address);
            if (port == 0xA2) {
                psg.setPortInputs(tape.inputHigh() ? 0xFF : 0x7F, 0xFF);
                inputs.dataBus = psg.readData();
            } else if ((port & 0xFC) == 0xA8) {
                inputs.dataBus = ppi.read(port & 3);
            }
        }
        if (pins.memoryRequest && pins.write && pins.address >= 0x8000) {
            memory[pins.address] = pins.dataBus;
        } else if (pins.ioRequest && pins.write) {
            const auto port = static_cast<std::uint8_t>(pins.address);
            if (port == 0xA0) psg.selectRegister(pins.dataBus);
            else if (port == 0xA1) psg.writeData(pins.dataBus);
            else if ((port & 0xFC) == 0xA8) {
                ppi.write(port & 3, pins.dataBus);
                const auto output = ppi.outputs();
                tape.setPins((output.portCDriveMask & 0x10) != 0 &&
                             (output.portC & 0x10) == 0,
                             (output.portCDriveMask & 0x20) != 0 &&
                             (output.portC & 0x20) != 0);
            }
        }
        pins = cpu.tick(inputs);
        tape.tick(svi728::CpuClockHz);
    }
    if (!pins.halt) {
        std::cerr << "BIOS timeout: busAddress=" << std::hex << pins.address << std::dec
                  << " LOW=" << unsigned(memory[0xF406]) + 256u * memory[0xF407]
                  << " HIGH=" << unsigned(memory[0xF408]) + 256u * memory[0xF409]
                  << " HEADER=" << unsigned(memory[0xF40A])
                  << " motor=" << tape.state().motorOn << '\n';
    }
    return pins.halt;
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount != 2) return EXIT_FAILURE;
    std::ifstream file(arguments[1], std::ios::binary);
    const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>{file},
                                      std::istreambuf_iterator<char>{}};
    if (rom.size() != svi728::MainRomSize) return EXIT_FAILURE;
    std::array<std::uint8_t, svi728::AddressSpaceSize> memory{};
    const auto initialize = [&](const std::vector<std::uint8_t>& program) {
        memory.fill(0);
        std::copy(rom.begin(), rom.end(), memory.begin());
        memory[0] = 0xC3;
        memory[1] = 0x00;
        memory[2] = 0xC0;
        std::copy(program.begin(), program.end(), memory.begin() + 0xC000);
        memory[0xC100] = memory[0xC101] = memory[0xC102] = 0xFF;
    };
    const std::vector<std::uint8_t> readProgram = {
        0x31, 0x00, 0xF0,
        0x3E, 0x82, 0xD3, 0xAB,
        0x3E, 0x10, 0xD3, 0xAA,
        0x3E, 0x07, 0xD3, 0xA0,
        0x3E, 0x80, 0xD3, 0xA1,
        0xCD, 0xE1, 0x00,
        0x3E, 0x00, 0xCE, 0x00, 0x32, 0x01, 0xC1,
        0xCD, 0xE4, 0x00,
        0x32, 0x00, 0xC1,
        0x3E, 0x00, 0xCE, 0x00, 0x32, 0x02, 0xC1,
        0xCD, 0xE7, 0x00,
        0x76
    };
    initialize(readProgram);
    svi728::Cassette tape;
    const std::vector<std::uint8_t> marker = {0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74};
    auto image = marker;
    image.push_back(0xA5);
    if (!tape.insert(image)) return EXIT_FAILURE;
    const bool halted = runProgram(memory, tape);
    if (!halted || memory[0xC100] != 0xA5 || memory[0xC101] != 0 ||
        memory[0xC102] != 0 || tape.state().motorOn) {
        std::cerr << "FAIL: SVI BIOS TAPION/TAPIN/TAPIOF: halt=" << halted
                  << " byte=" << unsigned(memory[0xC100])
                  << " headerCarry=" << unsigned(memory[0xC101])
                  << " byteCarry=" << unsigned(memory[0xC102])
                  << " motor=" << tape.state().motorOn << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "SVI BIOS read CAS byte A5 through PPI/PSG pins.\n";

    for (const bool fast : {false, true}) {
        std::vector<std::uint8_t> writeProgram = {
            0x31, 0x00, 0xF0,
            0x3E, 0x82, 0xD3, 0xAB,
            0x3E, 0x10, 0xD3, 0xAA,
            0x21, static_cast<std::uint8_t>(fast ? 0xA8 : 0xA3), 0x7F,
            0x11, 0x06, 0xF4, 0x01, 0x05, 0x00, 0xED, 0xB0,
            0x3E, 0x01, 0xCD, 0xEA, 0x00
        };
        std::vector<std::uint8_t> header(10, 0xD3);
        header.insert(header.end(), {'T', 'E', 'S', 'T', ' ', ' '});
        const auto writeBytes = [&](const std::vector<std::uint8_t>& data) {
            for (const auto value : data) {
                writeProgram.insert(writeProgram.end(), {0x3E, value, 0xCD, 0xED, 0x00});
            }
        };
        writeBytes(header);
        writeProgram.insert(writeProgram.end(), {0xCD, 0xF0, 0x00, 0xAF, 0xCD, 0xEA, 0x00});
        const std::vector<std::uint8_t> payload = {0xA5, 0x00, 0xFF, 0x81, 0x55, 0x7F, 0x10, 0x20};
        writeBytes(payload);
        writeProgram.insert(writeProgram.end(), {0xCD, 0xF0, 0x00, 0x76});
        initialize(writeProgram);
        tape.startRecording();
        const bool wrote = runProgram(memory, tape);
        const bool decoded = tape.stopRecording();
        auto expected = marker;
        expected.insert(expected.end(), header.begin(), header.end());
        expected.insert(expected.end(), marker.begin(), marker.end());
        expected.insert(expected.end(), payload.begin(), payload.end());
        if (!wrote || !decoded || tape.image() != expected || tape.state().motorOn) {
            std::cerr << "FAIL: BIOS CAS recording " << (fast ? 2400 : 1200)
                      << " baud: halt=" << wrote << " decoded=" << decoded
                      << " bytes=" << tape.image().size()
                      << " error=" << tape.state().recordingError << '\n';
            return EXIT_FAILURE;
        }
        initialize(readProgram);
        if (!runProgram(memory, tape) || memory[0xC100] != 0xD3 ||
            memory[0xC101] != 0 || memory[0xC102] != 0) {
            std::cerr << "FAIL: recorded CAS cannot be read back by SVI BIOS\n";
            return EXIT_FAILURE;
        }
        std::cout << "SVI BIOS CAS write/read round trip at " << (fast ? 2400 : 1200)
                  << " baud passed.\n";
    }
}