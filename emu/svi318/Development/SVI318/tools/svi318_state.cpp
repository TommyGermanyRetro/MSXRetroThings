// svi318_state: runs the SVI-318 without a window, as fast as the PC allows, and works through its
// arguments in order like a script: boot, insert a tape, type, wait for the tape, hold keys or
// joysticks, write pictures and snapshots. It makes snapshots that the frontend loads
// (System > Load state...), for example of a game after its five-minute tape load, and checks
// input on a snapshot without the frontend.

#include "SVI318/Keyboard.hpp"
#include "SVI318/SVI318.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 16> Palette = {
    0x000000, 0x000000, 0x21C842, 0x5EDC78,
    0x5455ED, 0x7D76FC, 0xD4524D, 0x42EBF5,
    0xFC5554, 0xFF7978, 0xD4C154, 0xE6CE80,
    0x21B03B, 0xC95BBA, 0xCCCCCC, 0xFFFFFF
};

// Fields a key or joystick direction stays pressed by default, and the pause after releasing it.
constexpr unsigned DefaultHoldFields = 5;
constexpr unsigned ReleaseFields = 5;
constexpr unsigned DefaultTapeLimitFields = 50 * 60 * 15;

constexpr svi318::KeyboardPosition SviEnter{6, 6};

int usage()
{
    std::cerr <<
        "svi318_state <basic.rom> [--ntsc] <action>...\n"
        "Actions run in the order given:\n"
        "  --load <file.sst>          continue from a snapshot\n"
        "  --cas <file.cas>           insert a cassette\n"
        "  --run <fields>             run fields (50 per second PAL, 60 NTSC)\n"
        "  --until <text> <fields>    run until the text screen shows <text>, at most <fields>\n"
        "  --type <text>              type on the SVI keyboard; \\n is Enter\n"
        "  --tape [fields]            run until the tape has been read to its end\n"
        "  --key <row> <bit> [fields] hold an SVI matrix contact, then release it\n"
        "  --joy <1|2> <UDLRF> [fields] hold joystick directions (U D L R) and fire (F), then release\n"
        "  --set-joy <UDLRF|-> <UDLRF|-> hold both joysticks until the next --set-joy (- released)\n"
        "  --bmp <file.bmp>           write the TV picture (256 x 192)\n"
        "  --text                     print the text screen\n"
        "  --profile <fields>         run fields and list the hottest code addresses and the I/O reads\n"
        "  --dump <start> <length>    print memory as the CPU sees it (hexadecimal)\n"
        "  --psg                      print the PSG registers\n"
        "  --save <file.sst>          write a snapshot\n";
    return 2;
}

std::vector<std::uint8_t> readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

bool writeFile(const std::string& path, const std::vector<std::uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

void runFields(svi318::SVI318& machine, unsigned fields)
{
    const auto ticks = machine.cpuTicksPerField();
    for (unsigned field = 0; field < fields; ++field) {
        for (std::uint32_t tick = 0; tick < ticks; ++tick) (void)machine.tick();
    }
}

// The SVI BASIC stores characters as ASCII - 20h (its font starts with the space).
std::string screenText(const svi318::SVI318& machine)
{
    const auto& vdp = machine.video();
    const bool text = (vdp.registerValue(1) & 0x10) != 0;
    const unsigned columns = text ? 40 : 32;
    const auto base = static_cast<std::uint16_t>((vdp.registerValue(2) & 0x0F) << 10);
    std::string screen;
    for (unsigned row = 0; row < 24; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            const auto c = vdp.peekVram(static_cast<std::uint16_t>(base + row * columns + column)) + 0x20;
            screen += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
        }
        screen += '\n';
    }
    return screen;
}

void pressContact(svi318::SVI318& machine, svi318::KeyboardPosition position, bool shift, unsigned holdFields)
{
    if (shift) machine.setKeyboardKey(svi318::SviShift.row, svi318::SviShift.bit, true);
    machine.setKeyboardKey(position.row, position.bit, true);
    runFields(machine, holdFields);
    machine.setKeyboardKey(position.row, position.bit, false);
    if (shift) machine.setKeyboardKey(svi318::SviShift.row, svi318::SviShift.bit, false);
    runFields(machine, ReleaseFields);
}

bool typeText(svi318::SVI318& machine, const std::string& text)
{
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\\' && index + 1 < text.size() && text[index + 1] == 'n') {
            pressContact(machine, SviEnter, false, 3);
            ++index;
            continue;
        }
        svi318::KeyboardPosition position{};
        svi318::KeyModifier modifier{};
        if (!svi318::mapCharacterToSvi(static_cast<char16_t>(static_cast<unsigned char>(text[index])), position, modifier) ||
            (modifier != svi318::KeyModifier::None && modifier != svi318::KeyModifier::Shift)) {
            std::cerr << "Character '" << text[index] << "' has no SVI key.\n";
            return false;
        }
        pressContact(machine, position, modifier == svi318::KeyModifier::Shift, 3);
    }
    return true;
}

// Many games leave the motor on once they run, so the end of the tape signal marks the end of
// loading, not the motor.
bool waitForTape(svi318::SVI318& machine, unsigned limitFields)
{
    for (unsigned field = 0; field < limitFields; ++field) {
        runFields(machine, 1);
        const auto state = machine.cassetteState();
        if (!state.inserted) return false;
        if (state.atEnd) {
            std::cout << "Tape read to its end after " << field + 1 << " fields ("
                      << (field + 1) / machine.fieldRateHz() << " s emulated), motor "
                      << (state.motorOn ? "still on" : "off") << ".\n";
            return true;
        }
    }
    return false;
}

// Directions as letters U, D, L, R and fire F; the joystick bits are active low.
bool parseJoystick(const std::string& text, std::uint8_t& bits)
{
    bits = 0xFF;
    for (const char letter : text) {
        switch (letter) {
        case 'U': bits &= static_cast<std::uint8_t>(~svi318::JoystickUp); break;
        case 'D': bits &= static_cast<std::uint8_t>(~svi318::JoystickDown); break;
        case 'L': bits &= static_cast<std::uint8_t>(~svi318::JoystickLeft); break;
        case 'R': bits &= static_cast<std::uint8_t>(~svi318::JoystickRight); break;
        case 'F': bits &= static_cast<std::uint8_t>(~svi318::JoystickTrigger); break;
        default: return false;
        }
    }
    return true;
}

// Where the program runs and which inputs it reads: instruction fetches per address and I/O reads
// per port (PSG data reads per selected register), counted once per bus cycle.
void profile(svi318::SVI318& machine, unsigned fields)
{
    std::map<std::uint16_t, std::uint64_t> fetches;
    std::map<std::string, std::uint64_t> reads;
    bool previousFetch = false;
    bool previousRead = false;
    std::uint16_t instruction = 0;
    const auto ticks = machine.cpuTicksPerField();
    for (unsigned field = 0; field < fields; ++field) {
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto pins = machine.tick();
            const bool fetch = pins.m1 && pins.memoryRequest && pins.read;
            if (fetch && !previousFetch) {
                ++fetches[pins.address];
                instruction = pins.address;
            }
            const bool ioRead = pins.ioRequest && pins.read && !pins.m1;
            if (ioRead && !previousRead) {
                const auto port = static_cast<unsigned>(pins.address & 0xFF);
                char name[48];
                if (port == 0x90) {
                    std::snprintf(name, sizeof(name), "90h PSG R%u @%04X", machine.sound().selectedRegister(), instruction);
                } else {
                    std::snprintf(name, sizeof(name), "%02Xh @%04X", port, instruction);
                }
                ++reads[name];
            }
            previousFetch = fetch;
            previousRead = ioRead;
        }
    }
    std::vector<std::pair<std::uint64_t, std::uint16_t>> hot;
    for (const auto& [address, count] : fetches) hot.emplace_back(count, address);
    std::sort(hot.rbegin(), hot.rend());
    std::printf("Hottest instruction addresses over %u fields:\n", fields);
    for (std::size_t index = 0; index < hot.size() && index < 16; ++index) {
        std::printf("  %04Xh %10llu\n", hot[index].second, static_cast<unsigned long long>(hot[index].first));
    }
    std::printf("I/O reads:\n");
    for (const auto& [name, count] : reads) std::printf("  %-22s %10llu\n", name.c_str(), static_cast<unsigned long long>(count));
}

void dumpMemory(const svi318::SVI318& machine, unsigned start, unsigned length)
{
    for (unsigned offset = 0; offset < length; offset += 16) {
        std::printf("%04X:", (start + offset) & 0xFFFF);
        for (unsigned index = 0; index < 16 && offset + index < length; ++index) {
            std::printf(" %02X", machine.readMemory(static_cast<std::uint16_t>(start + offset + index)));
        }
        std::printf("\n");
    }
}

bool writeBmp(const svi318::SVI318& machine, const std::string& path)
{
    const auto& frame = machine.video().scanoutFrame();
    constexpr std::uint32_t width = 256;
    constexpr std::uint32_t height = 192;
    constexpr std::uint32_t pixelBytes = width * height * 3;
    std::vector<std::uint8_t> bmp(54 + pixelBytes, 0);
    const auto put32 = [&](std::size_t offset, std::uint32_t value) {
        for (unsigned index = 0; index < 4; ++index) bmp[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(2, static_cast<std::uint32_t>(bmp.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    bmp[26] = 1;
    bmp[28] = 24;
    put32(34, pixelBytes);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto rgb = Palette[frame[y * width + x] & 0x0F];
            const auto offset = 54 + ((height - 1 - y) * width + x) * 3;
            bmp[offset] = static_cast<std::uint8_t>(rgb);
            bmp[offset + 1] = static_cast<std::uint8_t>(rgb >> 8);
            bmp[offset + 2] = static_cast<std::uint8_t>(rgb >> 16);
        }
    }
    return writeFile(path, bmp);
}

unsigned optionalCount(int argc, char** argv, int& index, unsigned fallback)
{
    if (index + 1 < argc && argv[index + 1][0] != '-') return static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10));
    return fallback;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) return usage();
    const auto rom = readFile(argv[1]);
    int index = 2;
    auto standard = svi318::VideoStandard::Pal;
    if (index < argc && std::string(argv[index]) == "--ntsc") {
        standard = svi318::VideoStandard::Ntsc;
        ++index;
    }
    svi318::SVI318 machine(standard);
    if (rom.size() != svi318::RomSize || !machine.loadRom(rom)) {
        std::cerr << "BASIC ROM not found or not 32 KiB: " << argv[1] << '\n';
        return 1;
    }
    machine.reset();

    for (; index < argc; ++index) {
        const std::string action = argv[index];
        const bool hasValue = index + 1 < argc;
        if (action == "--load" && hasValue) {
            const auto state = readFile(argv[++index]);
            if (!machine.loadState(state)) {
                std::cerr << "Snapshot does not fit this machine: " << argv[index] << '\n';
                return 1;
            }
        } else if (action == "--cas" && hasValue) {
            if (!machine.loadCassette(readFile(argv[++index]))) {
                std::cerr << "No SVI CAS image: " << argv[index] << '\n';
                return 1;
            }
        } else if (action == "--run" && hasValue) {
            runFields(machine, static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10)));
        } else if (action == "--until" && index + 2 < argc) {
            const std::string text = argv[++index];
            const auto limit = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10));
            unsigned field = 0;
            while (field < limit && screenText(machine).find(text) == std::string::npos) {
                runFields(machine, 1);
                ++field;
            }
            if (field >= limit) {
                std::cerr << "Text \"" << text << "\" did not appear within " << limit << " fields.\n";
                return 1;
            }
        } else if (action == "--type" && hasValue) {
            if (!typeText(machine, argv[++index])) return 1;
        } else if (action == "--tape") {
            if (!waitForTape(machine, optionalCount(argc, argv, index, DefaultTapeLimitFields))) {
                const auto state = machine.cassetteState();
                std::cerr << "Tape did not finish: block " << state.blockIndex << " of " << state.blockCount
                          << ", byte " << state.bytePosition << " of " << state.byteCount << ".\n";
                return 1;
            }
        } else if (action == "--key" && index + 2 < argc) {
            const auto row = static_cast<std::uint8_t>(std::strtoul(argv[++index], nullptr, 10));
            const auto bit = static_cast<std::uint8_t>(std::strtoul(argv[++index], nullptr, 10));
            pressContact(machine, {row, bit}, false, optionalCount(argc, argv, index, DefaultHoldFields));
        } else if (action == "--joy" && index + 2 < argc) {
            const std::string port = argv[++index];
            std::uint8_t bits = 0xFF;
            if ((port != "1" && port != "2") || !parseJoystick(argv[++index], bits)) return usage();
            const auto hold = optionalCount(argc, argv, index, DefaultHoldFields);
            machine.setJoysticks(port == "1" ? bits : 0xFF, port == "2" ? bits : 0xFF);
            runFields(machine, hold);
            machine.setJoysticks(0xFF, 0xFF);
            runFields(machine, ReleaseFields);
        } else if (action == "--set-joy" && index + 2 < argc) {
            // Holds both joysticks until the next --set-joy; "-" for released.
            std::uint8_t bits1 = 0xFF;
            std::uint8_t bits2 = 0xFF;
            const std::string text1 = argv[++index];
            const std::string text2 = argv[++index];
            if ((text1 != "-" && !parseJoystick(text1, bits1)) || (text2 != "-" && !parseJoystick(text2, bits2))) return usage();
            machine.setJoysticks(bits1, bits2);
        } else if (action == "--bmp" && hasValue) {
            if (!writeBmp(machine, argv[++index])) {
                std::cerr << "Cannot write " << argv[index] << '\n';
                return 1;
            }
        } else if (action == "--profile" && hasValue) {
            profile(machine, static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10)));
        } else if (action == "--dump" && index + 2 < argc) {
            const auto start = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 16));
            const auto length = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 16));
            dumpMemory(machine, start, length);
        } else if (action == "--psg") {
            std::printf("PSG:");
            for (std::uint8_t reg = 0; reg < 16; ++reg) std::printf(" R%u=%02X", reg, machine.sound().registerValue(reg));
            std::printf("\n");
        } else if (action == "--text") {
            std::cout << screenText(machine);
        } else if (action == "--save" && hasValue) {
            if (!writeFile(argv[++index], machine.saveState())) {
                std::cerr << "Cannot write " << argv[index] << '\n';
                return 1;
            }
            std::cout << "Snapshot written: " << argv[index] << '\n';
        } else {
            return usage();
        }
    }
    return 0;
}
