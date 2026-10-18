#include "SVI728/SVI728.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint16_t PutPointer = 0xF3F8;
constexpr std::uint16_t GetPointer = 0xF3FA;
constexpr std::uint16_t KeyboardBegin = 0xFBF0;
constexpr std::uint16_t KeyboardEnd = 0xFC18;
constexpr std::uint16_t TextPointer = 0xF676;
constexpr std::uint16_t VariablesPointer = 0xF6C2;
constexpr std::uint16_t CompletionAddress = 0xD000;

class BasicSession {
public:
    svi728::SVI728 machine;
    std::uint64_t ticks = 0;
    unsigned motorStops = 0;
    std::uint16_t busAddress = 0;
    std::size_t previousPrinterOutputSize = 0;
    bool sawListingPrinterByte = false;
    bool sawDirectPrinterByte = false;
    bool sawProgramPrinterByte = false;
    bool trackProgramPrinterRun = false;
    bool programPrinterOutputStarted = false;
    bool directModeAfterProgramPrint = false;
    bool directModeBetweenProgramPrints = false;
    std::uint64_t lastPrinterByteTick = 0;
    std::uint64_t maxPrinterByteGapTicks = 0;

    std::uint16_t word(std::uint16_t address) const
    {
        return machine.readMemory(address) |
            (static_cast<std::uint16_t>(machine.readMemory(address + 1)) << 8);
    }

    void step()
    {
        const bool motorBefore = machine.cassetteState().motorOn;
        busAddress = machine.tick().address;
        if (motorBefore && !machine.cassetteState().motorOn) ++motorStops;
        const auto currentLine = word(0xF41C);
        if (trackProgramPrinterRun && programPrinterOutputStarted && currentLine == 0xFFFF) {
            directModeAfterProgramPrint = true;
        }
        const auto printerOutputSize = machine.printer().output().size();
        if (printerOutputSize < previousPrinterOutputSize) {
            previousPrinterOutputSize = printerOutputSize;
        } else if (printerOutputSize > previousPrinterOutputSize) {
            const bool listing = machine.readMemory(0xF416) != 0;
            if (lastPrinterByteTick != 0) {
                maxPrinterByteGapTicks = std::max(maxPrinterByteGapTicks, ticks - lastPrinterByteTick);
            }
            lastPrinterByteTick = ticks;
            sawListingPrinterByte = sawListingPrinterByte || listing;
            sawProgramPrinterByte = sawProgramPrinterByte || currentLine != 0xFFFF;
            sawDirectPrinterByte = sawDirectPrinterByte || currentLine == 0xFFFF;
            if (trackProgramPrinterRun && currentLine != 0xFFFF) {
                if (directModeAfterProgramPrint) directModeBetweenProgramPrints = true;
                programPrinterOutputStarted = true;
                directModeAfterProgramPrint = false;
            }
            previousPrinterOutputSize = printerOutputSize;
        }
        ++ticks;
    }

    template<class Predicate>
    void wait(Predicate finished, unsigned seconds, const std::string& stage)
    {
        const auto limit = ticks + static_cast<std::uint64_t>(svi728::CpuClockHz) * seconds;
        while (!finished() && ticks < limit) step();
        if (!finished()) {
            std::cerr << "Timeout: " << stage << " ticks=" << ticks
                      << " bus=0x" << std::hex << busAddress
                      << " GET=0x" << word(GetPointer) << " PUT=0x" << word(PutPointer)
                      << " TXTTAB=0x" << word(TextPointer)
                      << " VARTAB=0x" << word(VariablesPointer) << std::dec
                      << " motor=" << machine.cassetteState().motorOn
                      << " stops=" << motorStops << '\n';
            throw std::runtime_error(stage);
        }
    }

    void input(const std::string& text)
    {
        for (const unsigned char character : text) {
            wait([&] {
                auto next = word(PutPointer) + 1;
                if (next == KeyboardEnd) next = KeyboardBegin;
                return next != word(GetPointer);
            }, 5, "keyboard buffer space");
            const auto put = word(PutPointer);
            const auto get = word(GetPointer);
            if (put < KeyboardBegin || put >= KeyboardEnd ||
                get < KeyboardBegin || get >= KeyboardEnd) {
                throw std::runtime_error("invalid BIOS keyboard pointers");
            }
            machine.writeMemory(put, character);
            const auto next = static_cast<std::uint16_t>(
                put + 1 == KeyboardEnd ? KeyboardBegin : put + 1);
            machine.writeMemory(PutPointer, static_cast<std::uint8_t>(next));
            machine.writeMemory(PutPointer + 1, static_cast<std::uint8_t>(next >> 8));
        }
    }

    void command(const std::string& text, unsigned seconds = 5, unsigned minimumStops = 0)
    {
        const auto stopsBefore = motorStops;
        ++completion;
        input(text + "\r");
        wait([&] { return word(GetPointer) == word(PutPointer); }, seconds,
             text + " input consumption");
        input("POKE &HD000," + std::to_string(completion) + "\r");
        wait([&] { return machine.readMemory(CompletionAddress) == completion; },
             seconds, text + " BASIC completion");
        if (machine.cassetteState().motorOn || motorStops - stopsBefore < minimumStops) {
            throw std::runtime_error(text + " did not finish header and data motor phases");
        }
        std::cout << "Completed " << text << "; motor stops="
                  << motorStops - stopsBefore << "; ticks=" << ticks << '\n';
    }

    std::vector<std::uint8_t> program() const
    {
        const auto begin = word(TextPointer);
        const auto end = word(VariablesPointer);
        if (begin < 0x8000 || end < begin + 2 || end >= CompletionAddress) {
            throw std::runtime_error("invalid BASIC program pointers");
        }
        auto line = begin;
        while (word(line) != 0) {
            const auto next = word(line);
            if (next <= line + 4 || next + 2 > end || machine.readMemory(next - 1) != 0) {
                throw std::runtime_error("invalid tokenized BASIC line chain");
            }
            line = next;
        }
        const auto programEnd = static_cast<std::uint16_t>(line + 2);
        if (end < programEnd || end > programEnd + 1 ||
            (end > programEnd && machine.readMemory(programEnd) != 0)) {
            throw std::runtime_error("invalid BASIC terminator/VARTAB padding");
        }
        std::vector<std::uint8_t> bytes;
        for (auto address = begin; address < programEnd; ++address) {
            bytes.push_back(machine.readMemory(address));
        }
        return bytes;
    }

private:
    std::uint8_t completion = 0;
};

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

bool referencesAddress(const std::vector<std::uint8_t>& rom, std::uint16_t address)
{
    for (std::size_t offset = 0; offset + 2 < rom.size(); ++offset) {
        const auto opcode = rom[offset];
        if ((opcode == 0x21 || opcode == 0x11 || opcode == 0x2A || opcode == 0x22 ||
             opcode == 0x3A || opcode == 0x32) &&
            rom[offset + 1] == (address & 0xFF) && rom[offset + 2] == (address >> 8)) {
            return true;
        }
    }
    return false;
}
}

int main(int argumentCount, char** arguments)
{
    try {
        require(argumentCount == 2, "expected BIOS ROM path");
        std::ifstream file(arguments[1], std::ios::binary);
        const std::vector<std::uint8_t> rom{std::istreambuf_iterator<char>{file},
                                          std::istreambuf_iterator<char>{}};
        require(rom.size() == svi728::MainRomSize, "invalid BIOS ROM size");
        for (const auto address : {PutPointer, GetPointer, KeyboardBegin, KeyboardEnd,
                                   TextPointer, VariablesPointer}) {
            require(referencesAddress(rom, address), "system address not referenced by ROM");
        }
        BasicSession session;
        require(session.machine.loadMainRom(rom), "ROM load failed");
        session.machine.reset();
        session.wait([&] {
            return session.word(GetPointer) == KeyboardBegin &&
                session.word(PutPointer) == KeyboardBegin &&
                session.word(TextPointer) >= 0x8000 &&
                session.word(VariablesPointer) == session.word(TextPointer) + 2;
        }, 10, "BASIC boot");
        require(session.machine.printer().output().empty(),
                "BIOS startup must not produce printer output");
        session.command("10 PRINT 123");
        session.command("20 END");
        const auto original = session.program();
        const auto textBegin = session.word(TextPointer);
        const auto originalVariables = session.word(VariablesPointer);
        require(original.size() > 10, "BASIC program was not entered");
        require(session.machine.readMemory(textBegin + 2) == 10 &&
                session.machine.readMemory(textBegin + 3) == 0,
                "first tokenized BASIC line is not line 10");
        const auto nextLine = session.word(textBegin);
        require(nextLine > textBegin && nextLine < session.word(VariablesPointer) &&
                session.word(nextLine + 2) == 20, "second BASIC line is not line 20");

        session.lastPrinterByteTick = 0;
        session.maxPrinterByteGapTicks = 0;
        session.command("LLIST");
        const std::vector<std::uint8_t> listedLine = {
            '1', '0', ' ', 'P', 'R', 'I', 'N', 'T', ' ', '1', '2', '3'
        };
        const auto& listedOutput = session.machine.printer().output();
        require(std::search(listedOutput.begin(), listedOutput.end(),
                            listedLine.begin(), listedLine.end()) != listedOutput.end(),
                "BASIC LLIST sends the stored program text to the printer");
        require(session.sawListingPrinterByte,
                "BASIC sets PRTFLG while LLIST sends bytes to the printer");
        require(session.maxPrinterByteGapTicks < svi728::CpuClockHz / 4,
                "LLIST output stays within the direct-print job idle window");
        session.machine.printer().clearOutput();

        session.command("LPRINT \"PRINTER TEST\"");
        const std::vector<std::uint8_t> printedText = {
            'P', 'R', 'I', 'N', 'T', 'E', 'R', ' ', 'T', 'E', 'S', 'T'
        };
        const auto& printedOutput = session.machine.printer().output();
        require(std::search(printedOutput.begin(), printedOutput.end(),
                            printedText.begin(), printedText.end()) != printedOutput.end(),
                "BASIC LPRINT sends its text to the printer");
        require(session.sawDirectPrinterByte,
                "direct-mode LPRINT runs with CURLIN set to direct mode");

        session.command("NEW");
        session.command("10 LPRINT \"PROGRAM\"");
        session.command("20 LPRINT USING \"##\";12");
        session.command("30 END");
        session.machine.printer().clearOutput();
        session.trackProgramPrinterRun = true;
        session.command("RUN");
        session.trackProgramPrinterRun = false;
        const std::vector<std::uint8_t> programText = {
            'P', 'R', 'O', 'G', 'R', 'A', 'M'
        };
        const std::vector<std::uint8_t> formattedText = {'1', '2'};
        const auto& programOutput = session.machine.printer().output();
        require(std::search(programOutput.begin(), programOutput.end(),
                            programText.begin(), programText.end()) != programOutput.end() &&
                std::search(programOutput.begin(), programOutput.end(),
                            formattedText.begin(), formattedText.end()) != programOutput.end(),
                "program-mode LPRINT and LPRINT USING print into the same run");
        require(session.sawProgramPrinterByte,
                "program-mode LPRINT runs with CURLIN set to a BASIC line");
        require(!session.directModeBetweenProgramPrints,
                "CURLIN stays in program mode between consecutive program LPRINT statements");

        constexpr std::uint16_t printerRoutine = 0xC100;
        const std::vector<std::uint8_t> printerAssembly = {
            0x3E, 'M', 0xCD, 0xA5, 0x00,
            0x3E, 'X', 0xCD, 0x4D, 0x01,
            0xC9
        };
        for (std::size_t offset = 0; offset < printerAssembly.size(); ++offset) {
            session.command("POKE " + std::to_string(printerRoutine + offset) + "," +
                            std::to_string(printerAssembly[offset]));
        }
        session.command("DEFUSR0=&HC100");
        session.machine.printer().clearOutput();
        session.command("X=USR0(0)");
        const auto& assemblyOutput = session.machine.printer().output();
        require(std::find(assemblyOutput.begin(), assemblyOutput.end(), 'M') != assemblyOutput.end() &&
                std::find(assemblyOutput.begin(), assemblyOutput.end(), 'X') != assemblyOutput.end(),
                "assembly calls to BIOS LPTOUT and OUTDLP send bytes to the printer");

        session.command("NEW");
        session.command("10 PRINT 123");
        session.command("20 END");
        require(session.program() == original, "printer tests restore the original BASIC program");

        session.machine.startCassetteRecording();
        session.command("CSAVE \"TEST\"", 30, 2);
        require(session.machine.stopCassetteRecording(), "CSAVE capture decoding failed");
        const auto image = session.machine.cassetteImage();
        require(image.size() > original.size() && session.machine.cassetteState().modified &&
                !session.machine.cassetteState().recordingError, "invalid recorded CAS state");
        session.machine.markCassetteSaved();
        require(!session.machine.cassetteState().modified, "markCassetteSaved failed");
        session.command("NEW");
        require(session.program() == std::vector<std::uint8_t>({0, 0}),
                "NEW did not erase BASIC program");
        session.machine.rewindCassette();
        require(session.machine.cassetteState().positionTicks == 0, "rewind failed");
        require(session.machine.loadCassette(image), "recorded CAS reinsertion failed");
        session.command("CLOAD \"TEST\"", 30, 1);
        const auto restored = session.program();
        if (session.word(TextPointer) != textBegin || restored != original) {
            std::cerr << "Original TXTTAB=0x" << std::hex << textBegin
                      << " restored TXTTAB=0x" << session.word(TextPointer)
                      << " VARTAB=0x" << session.word(VariablesPointer) << '\n';
            for (const auto& entry : {original, restored, image}) {
                std::cerr << "Bytes(" << std::dec << entry.size() << "):" << std::hex;
                for (const auto byte : entry) std::cerr << ' ' << unsigned(byte);
                std::cerr << std::dec << '\n';
            }
        }
        require(session.word(TextPointer) == textBegin && restored == original,
                "CLOAD did not restore exact BASIC program bytes");
        std::cout << "PASS: actual BASIC CSAVE/NEW/CLOAD restored " << original.size()
                  << " tokenized bytes; CAS bytes=" << image.size()
                  << "; TXTTAB=0x" << std::hex << textBegin
                  << "; VARTAB before=0x" << originalVariables
                  << " after=0x" << session.word(VariablesPointer) << std::dec << '\n';

        constexpr std::uint16_t binaryBegin = 0xC100;
        const std::vector<std::uint8_t> binary = {0xA5, 0x00, 0xFF, 0x5A};
        const auto binaryMatches = [&](const std::vector<std::uint8_t>& expected) {
            for (std::size_t offset = 0; offset < expected.size(); ++offset) {
                if (session.machine.readMemory(static_cast<std::uint16_t>(binaryBegin + offset))
                    != expected[offset]) return false;
            }
            return true;
        };
        for (std::size_t offset = 0; offset < binary.size(); ++offset) {
            session.command("POKE " + std::to_string(binaryBegin + offset) + "," +
                            std::to_string(binary[offset]));
        }
        require(binaryMatches(binary), "BASIC POKE did not initialize binary area");
        session.machine.startCassetteRecording();
        session.command("BSAVE \"CAS:BIN\",&HC100,&HC103", 30, 2);
        require(session.machine.stopCassetteRecording(), "BSAVE capture decoding failed");
        const auto binaryImage = session.machine.cassetteImage();
        require(binaryImage.size() > binary.size(), "BSAVE image is empty");
        session.machine.markCassetteSaved();
        for (std::size_t offset = 0; offset < binary.size(); ++offset) {
            session.command("POKE " + std::to_string(binaryBegin + offset) + ",0");
        }
        require(binaryMatches(std::vector<std::uint8_t>(binary.size(), 0)),
                "BASIC POKE did not clear binary area");
        session.machine.rewindCassette();
        require(session.machine.loadCassette(binaryImage), "BSAVE CAS reinsertion failed");
        session.command("BLOAD \"CAS:BIN\"", 30, 1);
        require(binaryMatches(binary), "BLOAD did not restore exact binary bytes");
        require(session.program() == original, "binary round trip altered BASIC program");
        std::cout << "PASS: actual BASIC BSAVE/BLOAD restored A5 00 FF 5A at C100; CAS bytes="
                  << binaryImage.size() << "; total CPU ticks=" << session.ticks << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}