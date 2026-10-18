#include "Z80Bus/Z80Bus.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct Machine {
    std::array<std::uint8_t, 65536> memory{};
    z80bus::Inputs pendingResponse{};
    bool sawM1 = false;
    bool sawRefresh = false;
    bool sawWrite = false;
    bool sawHalt = false;
    bool sawInterruptAcknowledge = false;
};

z80bus::Outputs step(z80bus::Z80Bus& cpu, Machine& machine, z80bus::Inputs inputs = {})
{
    inputs.driveDataBus = machine.pendingResponse.driveDataBus;
    inputs.dataBus = machine.pendingResponse.dataBus;
    auto outputs = cpu.tick(inputs);
    machine.pendingResponse = {};

    machine.sawM1 |= outputs.m1;
    machine.sawRefresh |= outputs.refresh;
    machine.sawHalt |= outputs.halt;
    machine.sawInterruptAcknowledge |= outputs.ioRequest && outputs.m1;

    if (outputs.memoryRequest && outputs.read) {
        machine.pendingResponse.driveDataBus = true;
        machine.pendingResponse.dataBus = machine.memory[outputs.address];
    } else if (outputs.ioRequest && outputs.m1) {
        machine.pendingResponse.driveDataBus = true;
        machine.pendingResponse.dataBus = 0xFF;
    }

    if (outputs.memoryRequest && outputs.write) {
        machine.memory[outputs.address] = outputs.dataBus;
        machine.sawWrite = true;
    }

    return outputs;
}

void testInstructionBus()
{
    z80bus::Z80Bus cpu;
    Machine machine;
    machine.memory[0] = 0x3E;
    machine.memory[1] = 0x42;
    machine.memory[2] = 0x32;
    machine.memory[3] = 0x00;
    machine.memory[4] = 0x40;
    machine.memory[5] = 0x76;

    for (unsigned cycle = 0; cycle < 200 && !machine.sawHalt; ++cycle) {
        step(cpu, machine);
    }

    require(machine.sawM1, "opcode fetch asserts M1");
    require(machine.sawRefresh, "opcode fetch exposes refresh cycles");
    require(machine.sawWrite, "memory write cycle is exposed");
    require(machine.memory[0x4000] == 0x42, "LD (nn),A reaches external memory");
    require(machine.sawHalt, "HALT output is exposed");
}

void testMaskableInterruptAcknowledge()
{
    z80bus::Z80Bus cpu;
    Machine machine;
    machine.memory[0] = 0xFB;
    machine.memory[1] = 0x00;

    z80bus::Inputs inputs;
    inputs.intRequest = true;

    for (unsigned cycle = 0; cycle < 100 && !machine.sawInterruptAcknowledge; ++cycle) {
        step(cpu, machine, inputs);
    }

    require(machine.sawInterruptAcknowledge, "INT acknowledge asserts IORQ and M1");
}

void testWaitAndBusRequest()
{
    z80bus::Z80Bus cpu;
    Machine machine;
    machine.memory[0] = 0x00;

    z80bus::Inputs inputs;
    auto outputs = step(cpu, machine, inputs);
    require(outputs.m1 && outputs.memoryRequest && outputs.read, "reset starts with opcode fetch");

    inputs.wait = true;
    outputs = step(cpu, machine, inputs);
    require(outputs.m1 && outputs.memoryRequest && outputs.read, "WAIT holds the current bus cycle");

    inputs.wait = false;
    inputs.busRequest = true;
    bool sawBusAcknowledge = false;
    for (unsigned cycle = 0; cycle < 20 && !sawBusAcknowledge; ++cycle) {
        outputs = step(cpu, machine, inputs);
        sawBusAcknowledge = outputs.busAcknowledge;
    }
    require(sawBusAcknowledge, "BUSACK follows a completed instruction boundary");
        require(!outputs.memoryRequest && !outputs.ioRequest && !outputs.read && !outputs.write,
            "bus control outputs are idle while BUSACK is active");

    inputs.busRequest = false;
    outputs = cpu.tick(inputs);
    require(!outputs.busAcknowledge, "BUSACK clears when BUSREQ is released");
}

} // namespace

int main()
{
    testInstructionBus();
    testMaskableInterruptAcknowledge();
    testWaitAndBusRequest();
    std::cout << "All Z80Bus tests passed.\n";
}