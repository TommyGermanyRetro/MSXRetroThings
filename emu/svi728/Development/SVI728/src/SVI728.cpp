#include "SVI728/SVI728.hpp"

#include "PPI8255/PPI8255.hpp"
#include <cstring>
#include <type_traits>
#include <optional>
#include "TMS9918A/TMS9918A.hpp"
#include "TMS9929A/TMS9929A.hpp"
#include "USBSLOT/BoardSlot.hpp"

#if defined(SVI728_GUI_APP)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>

#include "SVI728/Keyboard.hpp"
#include "SVI728/ManualPage.hpp"
#include "SVI728/UiText.hpp"
#include "VIDEOREC/VideoRecorder.hpp"

#include <charconv>
#include <filesystem>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <string>

using svi728::fillIn;
using svi728::ui;
using svi728::uiLanguage;
using svi728::UiLanguage;

#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace svi728 {

struct SVI728::Impl {
    z80bus::Z80Bus cpu;
    ppi8255::PPI8255 ppi;
    VideoStandard standard;
    std::unique_ptr<tms99x8::TMS99X8> vdp;
    ay8910::AY8910 psg{AyMasterClockHz};
    printer::Printer printer;
    Cassette cassette;
    std::array<std::uint8_t, RamSize> ram{};
    std::array<std::uint8_t, MainRomSize> mainRom{};
    // Slots 2 and 3 take ROM cartridges.
    std::array<cartridge::Cartridge, 2> cartridges{};
    // A slot of a USB2SLOT board in slot 2 or 3 replaces the module there.
    std::array<usbslot::BoardSlot*, 2> boardSlots{};
    usbslot::CycleTracker boardReadCycle;
    usbslot::CycleTracker boardWriteCycle;
    usbslot::CycleTracker boardIoReadCycle;
    usbslot::CycleTracker boardIoWriteCycle;
    std::uint8_t boardData = 0xFF;
    // T-states the CPU stands still because a cartridge on the board held /WAIT low in the last cycle.
    // The Z80 core samples WAIT in the T-state that starts an access, before the board has answered,
    // so the wait states are inserted as whole T-states right after the access.
    std::uint32_t boardWaitTicks = 0;
    std::array<std::uint8_t, KeyboardRowCount> keyboardRows{};
    std::uint8_t primarySlots = 0;
    std::uint8_t joystickA = 0xFF;
    std::uint8_t joystickB = 0xFF;
    z80bus::Outputs cpuPins{};
    bool m1WaitInserted = false;

    explicit Impl(VideoStandard videoStandard)
        : standard(videoStandard),
          vdp(videoStandard == VideoStandard::Ntsc
                  ? std::unique_ptr<tms99x8::TMS99X8>(std::make_unique<tms9918a::TMS9918A>())
                  : std::unique_ptr<tms99x8::TMS99X8>(std::make_unique<tms9929a::TMS9929A>()))
    {
        mainRom.fill(0xFF);
        keyboardRows.fill(0xFF);
        psg.setFixedPortDirections(true);
        refreshPeripheralInputs();
    }

    // Board slot behind the address in the current slot selection, or none.
    [[nodiscard]] usbslot::BoardSlot* boardSlotAt(std::uint16_t address) const noexcept
    {
        const auto page = address / PageSize;
        const auto slot = static_cast<std::uint8_t>((primarySlots >> (page * 2)) & 0x03);
        return slot >= 2 ? boardSlots[slot - 2] : nullptr;
    }

    // /INT of a board in use, wired-OR with the other interrupt sources.
    [[nodiscard]] bool boardInterruptRequest() const noexcept
    {
        for (const auto* boardSlot : boardSlots) {
            if (boardSlot && boardSlot->bridge().interruptRequest()) return true;
        }
        return false;
    }

    // Connected board slot that claims the I/O port, or none.
    [[nodiscard]] usbslot::BoardSlot* boardSlotForPort(std::uint8_t port) const noexcept
    {
        for (auto* boardSlot : boardSlots) {
            if (boardSlot && boardSlot->claimsPort(port)) return boardSlot;
        }
        return nullptr;
    }

    // I/O cycle on the board; every board slot on the same board follows its switching ports.
    [[nodiscard]] std::uint8_t readBoardIo(usbslot::BoardSlot& claimer, std::uint8_t port, std::uint32_t& waits)
    {
        const auto value = claimer.bridge().readIo(port, waits);
        for (auto* boardSlot : boardSlots) {
            if (boardSlot && &boardSlot->bridge() == &claimer.bridge()) boardSlot->ioRead(port);
        }
        return value;
    }

    void writeBoardIo(usbslot::BoardSlot& claimer, std::uint8_t port, std::uint8_t value, std::uint32_t& waits)
    {
        claimer.bridge().writeIo(port, value, waits);
        for (auto* boardSlot : boardSlots) {
            if (boardSlot && &boardSlot->bridge() == &claimer.bridge()) boardSlot->ioWritten(port, value);
        }
    }

    // Reads without a bus cycle (debugging, tests): a board slot shows FFh, because reading a real
    // cartridge can change it.
    [[nodiscard]] std::uint8_t readMemory(std::uint16_t address) const noexcept
    {
        const auto page = address / PageSize;
        const auto slot = static_cast<std::uint8_t>((primarySlots >> (page * 2)) & 0x03);
        if (const auto* boardSlot = boardSlotAt(address)) return boardSlot->peek(address);
        switch (slot) {
        case 0:
            return mainRom[address & (MainRomSize - 1)];
        case 1:
            return ram[address];
        case 2:
        case 3:
            return cartridges[slot - 2].read(address);
        default:
            return 0xFF;
        }
    }

    void writeMemory(std::uint16_t address, std::uint8_t value) noexcept
    {
        const auto page = address / PageSize;
        const auto slot = static_cast<std::uint8_t>((primarySlots >> (page * 2)) & 0x03);
        if (slot == 1) ram[address] = value;
        if (slot >= 2 && !boardSlots[slot - 2]) cartridges[slot - 2].write(address, value);
    }

    void refreshPeripheralInputs()
    {
        const auto ppiOutputs = ppi.outputs();
        cassette.setPins((ppiOutputs.portCDriveMask & 0x10) != 0 &&
                 (ppiOutputs.portC & 0x10) == 0,
                 (ppiOutputs.portCDriveMask & 0x20) != 0 &&
                 (ppiOutputs.portC & 0x20) != 0);
        const auto selectedRow = static_cast<std::uint8_t>(ppiOutputs.portC & 0x0F);
        const auto keyboardValue = selectedRow < keyboardRows.size()
            ? keyboardRows[selectedRow] : static_cast<std::uint8_t>(0xFF);

        ppi8255::Inputs ppiInputs;
        ppiInputs.portA = primarySlots;
        ppiInputs.portB = keyboardValue;
        ppiInputs.portC = 0xFF;
        ppi.setInputs(ppiInputs);
        const auto selectedJoystick = (psg.ports().portB & 0x40) != 0
            ? joystickB : joystickA;
        const auto portA = static_cast<std::uint8_t>((selectedJoystick & 0x7F) |
                          (cassette.inputHigh() ? 0x80 : 0));
        psg.setPortInputs(portA, 0xFF);
    }

    [[nodiscard]] std::uint8_t readIo(std::uint8_t port)
    {
        if (auto* boardSlot = boardSlotForPort(port)) {
            std::uint32_t waits = 0;
            return readBoardIo(*boardSlot, port, waits);
        }
        if ((port & 0xF8) == 0x90) return printer.status();
        if ((port & 0xFC) == 0x98) return vdp->read(port & 1);
        if (port == 0xA2) {
            refreshPeripheralInputs();
            return psg.readAddressData();
        }
        if ((port & 0xFC) == 0xA8) {
            refreshPeripheralInputs();
            return ppi.read(port & 3);
        }
        return 0xFF;
    }

    void writeIo(std::uint8_t port, std::uint8_t value)
    {
        if (auto* boardSlot = boardSlotForPort(port)) {
            std::uint32_t waits = 0;
            writeBoardIo(*boardSlot, port, value, waits);
            return;
        }
        if ((port & 0xF8) == 0x90) {
            if (port & 1) printer.writeData(value);
            else printer.writeStrobe((value & 0x01) != 0);
            return;
        }
        if ((port & 0xFC) == 0x98) {
            vdp->write(port & 1, value);
            return;
        }
        if (port == 0xA0) {
            psg.writeAddressData(false, value);
            return;
        }
        if (port == 0xA1) {
            psg.writeAddressData(true, value);
            refreshPeripheralInputs();
            return;
        }
        if ((port & 0xFC) == 0xA8) {
            const auto portAOutput = ppi.outputs().portADriveMask != 0;
            ppi.write(port & 3, value);
            if ((port & 3) == 0 && portAOutput) primarySlots = value;
            if ((port & 3) == 3 && ppi.outputs().portADriveMask != 0) {
                primarySlots = ppi.outputs().portA;
            }
            refreshPeripheralInputs();
        }
    }

    void clockCpuTState()
    {
        if (boardWaitTicks > 0) {
            --boardWaitTicks;
            cassette.tick(CpuClockHz);
            return;
        }
        z80bus::Inputs inputs;

        const bool memoryRead = cpuPins.memoryRequest && cpuPins.read;
        const bool memoryWrite = cpuPins.memoryRequest && cpuPins.write;
        auto* board = (memoryRead || memoryWrite) ? boardSlotAt(cpuPins.address) : nullptr;
        const bool boardReadBegins = boardReadCycle.begins(memoryRead && board, cpuPins.address);
        const bool boardWriteBegins = boardWriteCycle.begins(memoryWrite && board, cpuPins.address);
        const auto port = static_cast<std::uint8_t>(cpuPins.address);
        const bool ioRead = cpuPins.ioRequest && cpuPins.read && !cpuPins.m1;
        const bool ioWrite = cpuPins.ioRequest && cpuPins.write;
        const bool boardPort = (ioRead || ioWrite) && boardSlotForPort(port) != nullptr;
        const bool boardIoReadBegins = boardIoReadCycle.begins(ioRead && boardPort, cpuPins.address);
        const bool boardIoWriteBegins = boardIoWriteCycle.begins(ioWrite && boardPort, cpuPins.address);

        if (memoryRead && board) {
            if (boardReadBegins) {
                std::uint32_t waits = 0;
                boardData = board->read(cpuPins.address, cpuPins.m1, waits);
                // An opcode fetch already gets the M1 wait state of the machine; /WAIT of the
                // cartridge overlaps it.
                boardWaitTicks = cpuPins.m1 && waits > 0 ? waits - 1 : waits;
            }
            inputs.dataBus = boardData;
            inputs.driveDataBus = true;
        } else if (memoryRead) {
            inputs.dataBus = readMemory(cpuPins.address);
            inputs.driveDataBus = true;
        } else if (cpuPins.ioRequest && cpuPins.m1) {
            inputs.dataBus = 0xFF;
            inputs.driveDataBus = true;
        } else if (ioRead && boardPort) {
            if (boardIoReadBegins) {
                std::uint32_t waits = 0;
                boardData = readBoardIo(*boardSlotForPort(port), port, waits);
                boardWaitTicks = waits;
            }
            inputs.dataBus = boardData;
            inputs.driveDataBus = true;
        } else if (ioRead) {
            inputs.dataBus = readIo(port);
            inputs.driveDataBus = true;
        }

        if (memoryWrite && board) {
            if (boardWriteBegins) board->write(cpuPins.address, cpuPins.dataBus);
        } else if (memoryWrite) {
            writeMemory(cpuPins.address, cpuPins.dataBus);
        } else if (ioWrite && boardPort) {
            if (boardIoWriteBegins) {
                std::uint32_t waits = 0;
                writeBoardIo(*boardSlotForPort(port), port, cpuPins.dataBus, waits);
                boardWaitTicks = waits;
            }
        } else if (ioWrite) {
            writeIo(port, cpuPins.dataBus);
        }

        // MSX wait generator (74x74 pair): /M1 is clocked into /WAIT at the start of T2 and
        // released one clock later, so every opcode or prefix fetch gets exactly one wait state.
        // Interrupt acknowledge (M1 with IORQ) has /WAIT high again when the Z80 samples it.
        const bool opcodeFetch = cpuPins.m1 && cpuPins.memoryRequest && cpuPins.read;
        if (opcodeFetch && !m1WaitInserted) {
            inputs.wait = true;
            m1WaitInserted = true;
        } else if (!opcodeFetch) {
            m1WaitInserted = false;
        }

        inputs.intRequest = vdp->outputs().interruptRequest || boardInterruptRequest();
        cpuPins = cpu.tick(inputs);
        cassette.tick(CpuClockHz);
    }

    [[nodiscard]] z80bus::Outputs tick()
    {
        for (std::uint8_t masterTick = 0;
             masterTick < VdpMasterTicksPerCpuTick; ++masterTick) {
            vdp->advanceMasterTick();
            if (masterTick + 1 == VdpMasterTicksPerCpuTick) clockCpuTState();
        }
        return cpuPins;
    }
};

SVI728::SVI728(VideoStandard standard)
    : impl(std::make_unique<Impl>(standard))
{
}

SVI728::~SVI728() = default;
SVI728::SVI728(SVI728&&) noexcept = default;
SVI728& SVI728::operator=(SVI728&&) noexcept = default;

void SVI728::reset()
{
    impl->cpu.reset();
    impl->ppi.reset();
    impl->vdp->reset();
    impl->psg.reset();
    impl->printer.reset();
    impl->ram.fill(0);
    impl->primarySlots = 0;
    for (auto& cart : impl->cartridges) cart.reset();
    // One /RESET pulse per board, also when both of its slots are connected.
    for (std::size_t index = 0; index < impl->boardSlots.size(); ++index) {
        auto* boardSlot = impl->boardSlots[index];
        if (!boardSlot) continue;
        boardSlot->reset();
        if (index == 0 || !impl->boardSlots[0] || &impl->boardSlots[0]->bridge() != &boardSlot->bridge()) {
            boardSlot->bridge().reset();
        }
    }
    impl->boardReadCycle = {};
    impl->boardWriteCycle = {};
    impl->boardIoReadCycle = {};
    impl->boardIoWriteCycle = {};
    impl->boardWaitTicks = 0;
    impl->cpuPins = {};
    impl->m1WaitInserted = false;
    impl->refreshPeripheralInputs();
}

bool SVI728::loadMainRom(const std::vector<std::uint8_t>& image)
{
    if (image.size() > MainRomSize) return false;
    impl->mainRom.fill(0xFF);
    std::copy(image.begin(), image.end(), impl->mainRom.begin());
    return true;
}

bool SVI728::loadSlotRom(std::uint8_t primarySlot, std::uint16_t baseAddress,
                         const std::vector<std::uint8_t>& image)
{
    if (primarySlot != 2 && primarySlot != 3) return false;
    return impl->cartridges[primarySlot - 2].insertAt(image, baseAddress);
}

bool SVI728::insertCartridge(std::uint8_t primarySlot, const std::vector<std::uint8_t>& image,
                             std::optional<cartridge::Mapper> mapper)
{
    if (primarySlot != 2 && primarySlot != 3) return false;
    return impl->cartridges[primarySlot - 2].insert(image, mapper ? *mapper : cartridge::detectMapper(image));
}

void SVI728::ejectCartridge(std::uint8_t primarySlot) noexcept
{
    if (primarySlot == 2 || primarySlot == 3) impl->cartridges[primarySlot - 2].eject();
}

const cartridge::Cartridge& SVI728::cartridgeSlot(std::uint8_t primarySlot) const noexcept
{
    return impl->cartridges[primarySlot == 3 ? 1 : 0];
}

bool SVI728::connectBridgeSlot(std::uint8_t primarySlot, usbslot::BoardSlot* boardSlot)
{
    if (primarySlot != 2 && primarySlot != 3) return false;
    impl->boardSlots[primarySlot - 2] = boardSlot;
    return true;
}

std::uint8_t SVI728::bridgeSlot(std::uint8_t primarySlot) const noexcept
{
    if (primarySlot != 2 && primarySlot != 3) return 0;
    const auto* boardSlot = impl->boardSlots[primarySlot - 2];
    return boardSlot ? boardSlot->number() : 0;
}

std::uint8_t SVI728::readMemory(std::uint16_t address) const noexcept
{
    return impl->readMemory(address);
}

void SVI728::writeMemory(std::uint16_t address, std::uint8_t value) noexcept
{
    impl->writeMemory(address, value);
}

std::uint8_t SVI728::readIo(std::uint8_t port)
{
    return impl->readIo(port);
}

void SVI728::writeIo(std::uint8_t port, std::uint8_t value)
{
    impl->writeIo(port, value);
}

z80bus::Outputs SVI728::tick()
{
    return impl->tick();
}

void SVI728::setKeyboardRow(std::uint8_t row, std::uint8_t activeLowKeys) noexcept
{
    if (row < KeyboardRowCount) impl->keyboardRows[row] = activeLowKeys;
}

void SVI728::setKeyboardKey(std::uint8_t row, std::uint8_t bit, bool pressed) noexcept
{
    if (row >= KeyboardRowCount || bit >= 8) return;
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    if (pressed) impl->keyboardRows[row] &= static_cast<std::uint8_t>(~mask);
    else impl->keyboardRows[row] |= mask;
}

void SVI728::setJoystickPorts(std::uint8_t portA, std::uint8_t portB) noexcept
{
    impl->joystickA = portA;
    impl->joystickB = portB;
    impl->refreshPeripheralInputs();
}

bool SVI728::loadCassette(const std::vector<std::uint8_t>& image)
{
    const bool loaded = impl->cassette.insert(image);
    impl->refreshPeripheralInputs();
    return loaded;
}

void SVI728::rewindCassette() noexcept
{
    impl->cassette.rewind();
    impl->refreshPeripheralInputs();
}

void SVI728::ejectCassette() noexcept
{
    impl->cassette.eject();
    impl->refreshPeripheralInputs();
}

CassetteState SVI728::cassetteState() const noexcept
{
    return impl->cassette.state();
}

void SVI728::startCassetteRecording()
{
    impl->cassette.startRecording();
    impl->refreshPeripheralInputs();
}

bool SVI728::stopCassetteRecording()
{
    const bool complete = impl->cassette.stopRecording();
    impl->refreshPeripheralInputs();
    return complete;
}

const std::vector<std::uint8_t>& SVI728::cassetteImage() const noexcept
{
    return impl->cassette.image();
}

void SVI728::markCassetteSaved() noexcept
{
    impl->cassette.markSaved();
}

bool SVI728::oneBitSoundLevel() const noexcept
{
    const auto outputs = impl->ppi.outputs();
    return (outputs.portCDriveMask & 0x80) != 0 && (outputs.portC & 0x80) != 0;
}

std::uint8_t SVI728::primarySlotRegister() const noexcept
{
    return impl->primarySlots;
}

const std::array<std::uint8_t, RamSize>& SVI728::ram() const noexcept
{
    return impl->ram;
}

tms99x8::TMS99X8& SVI728::video() noexcept
{
    return *impl->vdp;
}

const tms99x8::TMS99X8& SVI728::video() const noexcept
{
    return *impl->vdp;
}

VideoStandard SVI728::videoStandard() const noexcept
{
    return impl->standard;
}

std::uint32_t SVI728::cpuTicksPerField() const noexcept
{
    return impl->standard == VideoStandard::Ntsc ? CpuTicksPerNtscField : CpuTicksPerPalField;
}

std::uint32_t SVI728::vdpMasterTicksPerField() const noexcept
{
    return impl->standard == VideoStandard::Ntsc ? VdpMasterTicksPerNtscField : VdpMasterTicksPerPalField;
}

std::uint32_t SVI728::fieldRateHz() const noexcept
{
    return impl->vdp->fieldRateHz();
}


namespace {

// Snapshot layout: magic, version (32 bits, little endian), video standard (one byte), then one
// section per component, each a 32-bit little-endian length and its bytes: CPU, CPU pins, PPI,
// VDP, PSG, RAM, slot register and M1 wait flag, module slot 2, module slot 3.
constexpr std::array<std::uint8_t, 8> StateMagic = {{'S', 'V', 'I', '7', '2', '8', 'S', 'T'}};
constexpr std::uint32_t StateVersion = 1;
constexpr std::size_t StateHeaderSize = StateMagic.size() + 4 + 1;
constexpr std::size_t StateSectionCount = 9;

void appendLe32(std::vector<std::uint8_t>& data, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) data.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t readLe32(const std::vector<std::uint8_t>& data, std::size_t offset)
{
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) value |= static_cast<std::uint32_t>(data[offset + index]) << (8 * index);
    return value;
}

void appendStateSection(std::vector<std::uint8_t>& state, const std::uint8_t* data, std::size_t size)
{
    appendLe32(state, static_cast<std::uint32_t>(size));
    state.insert(state.end(), data, data + size);
}

void appendStateSection(std::vector<std::uint8_t>& state, const std::vector<std::uint8_t>& section)
{
    appendStateSection(state, section.data(), section.size());
}

bool readStateSection(const std::vector<std::uint8_t>& state, std::size_t& offset, std::vector<std::uint8_t>& section)
{
    if (state.size() - offset < 4) return false;
    const std::size_t size = readLe32(state, offset);
    offset += 4;
    if (state.size() - offset < size) return false;
    section.assign(state.begin() + static_cast<std::ptrdiff_t>(offset),
                   state.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return true;
}

} // namespace

std::vector<std::uint8_t> SVI728::saveState() const
{
    static_assert(std::is_trivially_copyable_v<z80bus::Outputs>, "the CPU pins are copied as bytes");
    std::vector<std::uint8_t> state(StateMagic.begin(), StateMagic.end());
    appendLe32(state, StateVersion);
    state.push_back(static_cast<std::uint8_t>(impl->standard));
    appendStateSection(state, impl->cpu.saveState());
    appendStateSection(state, reinterpret_cast<const std::uint8_t*>(&impl->cpuPins), sizeof(impl->cpuPins));
    appendStateSection(state, impl->ppi.saveState());
    appendStateSection(state, impl->vdp->saveState());
    appendStateSection(state, impl->psg.saveState());
    appendStateSection(state, impl->ram.data(), impl->ram.size());
    const std::vector<std::uint8_t> slots{impl->primarySlots, static_cast<std::uint8_t>(impl->m1WaitInserted ? 1 : 0)};
    appendStateSection(state, slots);
    appendStateSection(state, impl->cartridges[0].saveState());
    appendStateSection(state, impl->cartridges[1].saveState());
    return state;
}

std::optional<VideoStandard> SVI728::stateVideoStandard(const std::vector<std::uint8_t>& state)
{
    if (state.size() < StateHeaderSize || !std::equal(StateMagic.begin(), StateMagic.end(), state.begin()) ||
        readLe32(state, StateMagic.size()) != StateVersion) {
        return std::nullopt;
    }
    const auto standard = state[StateMagic.size() + 4];
    if (standard > static_cast<std::uint8_t>(VideoStandard::Ntsc)) return std::nullopt;
    return static_cast<VideoStandard>(standard);
}

// The components are restored into a fresh set first, so that a snapshot that does not fit leaves
// the running machine untouched. Main ROM, module images, cassette and host inputs stay as they are.
bool SVI728::loadState(const std::vector<std::uint8_t>& state)
{
    if (stateVideoStandard(state) != impl->standard) return false;
    std::size_t offset = StateHeaderSize;
    std::array<std::vector<std::uint8_t>, StateSectionCount> sections;
    for (auto& section : sections) {
        if (!readStateSection(state, offset, section)) return false;
    }
    if (offset != state.size()) return false;

    auto candidate = std::make_unique<Impl>(impl->standard);
    auto cartridges = impl->cartridges;
    if (!candidate->cpu.loadState(sections[0]) || sections[1].size() != sizeof(z80bus::Outputs) ||
        !candidate->ppi.loadState(sections[2]) || !candidate->vdp->loadState(sections[3]) ||
        !candidate->psg.loadState(sections[4]) || sections[5].size() != RamSize || sections[6].size() != 2 ||
        !cartridges[0].loadState(sections[7]) || !cartridges[1].loadState(sections[8])) {
        return false;
    }
    impl->cpu = std::move(candidate->cpu);
    std::memcpy(&impl->cpuPins, sections[1].data(), sizeof(impl->cpuPins));
    impl->ppi = std::move(candidate->ppi);
    impl->vdp = std::move(candidate->vdp);
    impl->psg = std::move(candidate->psg);
    std::copy(sections[5].begin(), sections[5].end(), impl->ram.begin());
    impl->primarySlots = sections[6][0];
    impl->m1WaitInserted = sections[6][1] != 0;
    impl->cartridges = std::move(cartridges);
    impl->refreshPeripheralInputs();
    return true;
}

ay8910::AY8910& SVI728::sound() noexcept
{
    return impl->psg;
}

const ay8910::AY8910& SVI728::sound() const noexcept
{
    return impl->psg;
}

printer::Printer& SVI728::printer() noexcept
{
    return impl->printer;
}

const printer::Printer& SVI728::printer() const noexcept
{
    return impl->printer;
}

} // namespace svi728

#if defined(SVI728_GUI_APP)
namespace {

constexpr int NativeWidth = 256;
constexpr int NativeHeight = 192;
constexpr int Scale = 4;
constexpr int FrameWidth = 50;
constexpr int DisplayWidth = NativeWidth * Scale;
constexpr int DisplayHeight = NativeHeight * Scale;
constexpr int ClientWidth = DisplayWidth + FrameWidth * 2;
constexpr int ClientHeight = DisplayHeight + FrameWidth * 2;
constexpr UINT_PTR EmulationTimer = 1;
constexpr UINT TimerPeriodMs = 1;
// Joystick commands: base + 10 * port + source.
constexpr UINT JoystickCommandBase = 400;
constexpr UINT KeyboardJoystickToggleCommand = 430;
constexpr ULONGLONG ControllerProbeIntervalMs = 1000;

// What drives an MSX joystick port, as stored in the ini file. With the keyboard, the joystick keys
// act only on the joystick, as on the real machine where keyboard and joysticks are separate.
enum JoystickSource : int {
    JoystickOff = 0,
    JoystickKeyboard = 1,
    JoystickController = 2,
    JoystickSourceCount = 3
};

constexpr std::array<const char*, JoystickSourceCount> JoystickSourceNames = {{"off", "keyboard", "controller"}};

// Keyboard joystick keys per port by key position (scan code, 0x100 for extended keys): up, down,
// left, right, trigger A, trigger B. Port 2 fires with the two keys left of C: Z and X on a US keyboard,
// Y and X on a German one.
constexpr std::array<std::array<UINT, 6>, 2> JoystickKeys = {{
    {{0x148, 0x150, 0x14B, 0x14D, 0x2E, 0x2F}},
    {{0x11, 0x1F, 0x1E, 0x20, 0x2C, 0x2D}},
}};

// A PC game controller read through the Windows joystick interface (JOYSTICKID1 + port).
struct ControllerState {
    bool present = false;
    JOYCAPSW caps{};
    ULONGLONG nextProbe = 0;
};

constexpr UINT CassetteOpenCommand = 100;
constexpr UINT CassetteRewindCommand = 101;
constexpr UINT CassetteEjectCommand = 102;
constexpr UINT CassetteRecordCommand = 103;
constexpr UINT CassetteSaveCommand = 104;
constexpr UINT PrinterDirectoryCommand = 150;
// Video standard: PAL (TMS9929A) or NTSC (TMS9918A).
constexpr UINT VideoPalCommand = 160;
constexpr UINT VideoNtscCommand = 161;
constexpr UINT StateLoadCommand = 162;
constexpr UINT ManualCommand = 990;
constexpr UINT LanguageEnglishCommand = 991;
constexpr UINT LanguageGermanCommand = 992;
constexpr UINT StateSaveCommand = 163;
constexpr UINT VideoRecordCommand = 164;
constexpr UINT VideoStopCommand = 165;
constexpr UINT ResetCommand = 166;
constexpr UINT BridgeProbeCommand = 167;
constexpr UINT CardClockCommand = 168;
// Cartridge commands: base + 10 * (slot - 2) + choice, choice 0 = detect, 1-5 = fixed mapper, 9 = eject.
constexpr UINT CartridgeCommandBase = 130;
constexpr UINT CartridgeEjectChoice = 9;
// Choices 6 and 7 put slot 1 or 2 of the USB2SLOT board into the module slot.
constexpr UINT CartridgeBridgeChoice = 6;
constexpr const char* DefaultBridgePort = "COM16";
constexpr wchar_t SettingsFileName[] = L"svi728.ini";
std::wstring mapperText(cartridge::Mapper mapper);

std::filesystem::path mediaFolder(const wchar_t* name);

constexpr std::array<cartridge::Mapper, 5> CartridgeMappers = {
    cartridge::Mapper::None, cartridge::Mapper::Ascii8, cartridge::Mapper::Ascii16,
    cartridge::Mapper::Konami, cartridge::Mapper::KonamiScc};
constexpr wchar_t WindowTitle[] = L"SVI-728";
constexpr std::uint32_t AudioSampleRate = 44100;
constexpr std::size_t MaxAudioSamplesPerField = static_cast<std::size_t>(
    (static_cast<std::uint64_t>(AudioSampleRate) * svi728::VdpMasterTicksPerPalField +
     svi728::VdpMasterClockHz - 1) / svi728::VdpMasterClockHz);
constexpr std::size_t AudioBufferCount = 3;
constexpr ULONGLONG AudioRetryIntervalMs = 1000;
constexpr ULONGLONG TitleRefreshIntervalMs = 250;
constexpr std::uint16_t BasicCurrentLineAddress = 0xF41C;
constexpr std::uint64_t DirectPrinterIdleTicks = svi728::CpuClockHz / 4;

constexpr std::array<std::uint32_t, 16> Palette = {
    0x000000, 0x000000, 0x21C842, 0x5EDC78,
    0x5455ED, 0x7D76FC, 0xD4524D, 0x42EBF5,
    0xFC5554, 0xFF7978, 0xD4C154, 0xE6CE80,
    0x21B03B, 0xC95BBA, 0xCCCCCC, 0xFFFFFF
};

using KeyPosition = svi728::KeyboardPosition;

struct MatrixBinding {
    std::array<KeyPosition, 2> positions{};
    std::uint8_t count = 0;
    bool active = false;
    // A character the MSX types without Shift, held while the PC Shift key is down.
    bool suppressesShift = false;
};

struct SoundPortTransition {
    unsigned cycle;
    bool high;
};

struct AudioBlock {
    WAVEHDR header{};
    std::array<std::int16_t, MaxAudioSamplesPerField> samples{};
    bool prepared = false;
    bool inFlight = false;
};

struct AudioOutput {
    HWAVEOUT device = nullptr;
    std::array<AudioBlock, AudioBufferCount> blocks{};
    std::size_t nextBlock = 0;
    std::int32_t previousInput = 0;
    std::int32_t previousOutput = 0;
    ULONGLONG nextRetryTick = 0;
};

enum class PrinterFileMode {
    None,
    Direct,
    Program
};

struct AppState {
    svi728::SVI728 machine;
    std::vector<std::uint8_t> romImage;
    HWND window = nullptr;
    LARGE_INTEGER counterFrequency{};
    LARGE_INTEGER lastCounter{};
    std::uint64_t fieldPhase = 0;
    std::uint64_t audioSamplePhase = 0;
    bool timerResolutionRaised = false;
    AudioOutput audio;
    std::array<std::uint8_t, 14> savedAyRegisters{};
    std::uint8_t savedAyRegisterSelect = 0;
    unsigned audioTestField = 0;
    bool audioTestActive = false;
    std::array<bool, 512> keyDown{};
    std::array<int, 2> joystickSources{};
    bool keyboardJoystickOn = false;
    std::array<ControllerState, 2> controllers{};
    bool leftShiftDown = false;
    bool rightShiftDown = false;
    bool altGrDown = false;
    // Held characters that the MSX types without Shift although the PC Shift key is down.
    int shiftSuppressors = 0;
    std::filesystem::path cassettePath;
    std::array<std::filesystem::path, 2> cartridgePaths;
    // USB2SLOT board: real cartridges in place of modules, opened when a slot first uses it. Its two
    // slots, with the switching rules found by the probe, exist while a module slot uses them.
    std::unique_ptr<usbslot::Bridge> bridge;
    std::array<std::unique_ptr<usbslot::BoardSlot>, 2> boardSlots;
    // What the last probe found in each slot of the board (empty: nothing plugged in), for the module menu;
    // the probe pulses /RESET, so it runs only at start, when a slot is chosen and on "detect again".
    std::array<std::wstring, 2> boardContents;
    bool boardProbed = false;
    usbslot::Detection detection;
    // At the start, set the clock of RC2014 card A (PCF8584 at this I/O address) to the PC time.
    bool cardClock = false;
    std::uint8_t cardClockAddress = 0;
    std::string bridgePort = DefaultBridgePort;
    std::filesystem::path printerDirectory;
    std::filesystem::path printerOutputPath;
    std::ofstream printerFile;
    std::size_t printerOutputOffset = 0;
    PrinterFileMode printerFileMode = PrinterFileMode::None;
    std::uint64_t emulatedCpuTicks = 0;
    std::uint64_t lastDirectPrinterByteTick = 0;
    bool printerOutputPaused = false;
    bool printerErrorShown = false;
    bool printerWarningPending = false;
    std::filesystem::path settingsPath;
    std::wstring shownTitle;
    std::wstring shownStatus;
    HWND statusBar = nullptr;
    LARGE_INTEGER speedStart{};
    std::uint64_t speedStartTicks = 0;
    double cpuClockMHz = 0.0;
    ULONGLONG titleTick = 0;
    std::vector<SoundPortTransition> soundPortTransitions;
    std::array<MatrixBinding, 512> matrixBindings{};
    std::array<std::array<std::uint8_t, 8>, svi728::KeyboardRowCount> matrixCounts{};
    std::vector<std::uint32_t> pixels =
        std::vector<std::uint32_t>(ClientWidth * ClientHeight);
    // Video recording: the picture and the sound of every field.
    videorec::VideoRecorder recorder;
    bool videoRecording = false;
    std::vector<std::int16_t> fieldAudio;
};

void writeAyRegister(ay8910::AY8910& ay, std::uint8_t index, std::uint8_t value)
{
    ay.selectRegister(index);
    ay.writeData(value);
}

void startAyAudioTest(AppState& app)
{
    if (app.audioTestActive) return;

    auto& ay = app.machine.sound();
    app.savedAyRegisterSelect = ay.selectedRegister();
    for (std::uint8_t index = 0; index < app.savedAyRegisters.size(); ++index) {
        app.savedAyRegisters[index] = ay.registerValue(index);
    }
    app.audioTestField = 0;
    app.audioTestActive = true;
}

void restoreAyAfterAudioTest(AppState& app)
{
    auto& ay = app.machine.sound();
    for (std::uint8_t index = 0; index < app.savedAyRegisters.size(); ++index) {
        writeAyRegister(ay, index, app.savedAyRegisters[index]);
    }
    ay.selectRegister(app.savedAyRegisterSelect);
    app.audioTestActive = false;
}

void setAyAudioTestTone(AppState& app)
{
    constexpr std::array<std::uint32_t, 3> frequencies = {440, 554, 659};
    constexpr unsigned FieldsPerTone = 8;
    const auto note = app.audioTestField / FieldsPerTone;
    auto& ay = app.machine.sound();

    writeAyRegister(ay, 7, 0xBE);
    writeAyRegister(ay, 9, 0);
    writeAyRegister(ay, 10, 0);
    if (note >= frequencies.size()) {
        writeAyRegister(ay, 8, 0);
        return;
    }

    const auto divisor = 16 * frequencies[note];
    const auto period = (ay.masterClockHz() + divisor / 2) / divisor;
    writeAyRegister(ay, 0, static_cast<std::uint8_t>(period & 0xFF));
    writeAyRegister(ay, 1, static_cast<std::uint8_t>((period >> 8) & 0x0F));
    writeAyRegister(ay, 8, 12);
}

void processPendingKeyboardMessages(HWND window)
{
    MSG message{};
    while (PeekMessageW(&message, window, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
        DispatchMessageW(&message);
    }
}

void stopAudio(AudioOutput& audio)
{
    if (!audio.device) return;
    waveOutReset(audio.device);
    for (auto& block : audio.blocks) {
        if (block.prepared) {
            waveOutUnprepareHeader(audio.device, &block.header, sizeof(block.header));
            block.prepared = false;
            block.inFlight = false;
        }
    }
    waveOutClose(audio.device);
    audio.device = nullptr;
}

bool startAudio(AudioOutput& audio)
{
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = AudioSampleRate;
    format.nAvgBytesPerSec = AudioSampleRate * sizeof(std::int16_t);
    format.nBlockAlign = sizeof(std::int16_t);
    format.wBitsPerSample = 16;

    if (waveOutOpen(&audio.device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) !=
        MMSYSERR_NOERROR) {
        audio.device = nullptr;
        return false;
    }

    for (auto& block : audio.blocks) {
        block.header.lpData = reinterpret_cast<LPSTR>(block.samples.data());
        block.header.dwBufferLength = static_cast<DWORD>(sizeof(block.samples));
        if (waveOutPrepareHeader(audio.device, &block.header, sizeof(block.header)) !=
            MMSYSERR_NOERROR) {
            stopAudio(audio);
            return false;
        }
        block.prepared = true;
    }
    return true;
}

std::size_t nextAudioSampleCount(AppState& app)
{
    app.audioSamplePhase += static_cast<std::uint64_t>(AudioSampleRate) *
                            app.machine.vdpMasterTicksPerField();
    const auto sampleCount = app.audioSamplePhase / svi728::VdpMasterClockHz;
    app.audioSamplePhase %= svi728::VdpMasterClockHz;
    return static_cast<std::size_t>(sampleCount);
}

void queueAudioField(AppState& app, bool initialSoundPortLevel, std::size_t sampleCount)
{
    const auto samples = app.machine.sound().render(sampleCount, AudioSampleRate);
    auto& audio = app.audio;
    const bool recording = app.recorder.recording();
    if (!audio.device && !recording) return;

    AudioBlock* available = nullptr;
    std::size_t availableIndex = audio.nextBlock;
    for (std::size_t offset = 0; audio.device && offset < audio.blocks.size(); ++offset) {
        const auto index = (audio.nextBlock + offset) % audio.blocks.size();
        auto& block = audio.blocks[index];
        if (block.inFlight && (block.header.dwFlags & WHDR_DONE) != 0) {
            block.inFlight = false;
        }
        if (!block.inFlight) {
            available = &block;
            availableIndex = index;
            break;
        }
    }
    if (recording) app.fieldAudio.resize(samples.size());
    auto soundPortLevel = initialSoundPortLevel;
    std::size_t transitionIndex = 0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto sampleCycle = static_cast<unsigned>(
            ((index + 1) * app.machine.cpuTicksPerField()) / samples.size());
        while (transitionIndex < app.soundPortTransitions.size() &&
               app.soundPortTransitions[transitionIndex].cycle <= sampleCycle) {
            soundPortLevel = app.soundPortTransitions[transitionIndex].high;
            ++transitionIndex;
        }
        const auto sum = static_cast<std::int32_t>(samples[index].channel[0]) +
                         static_cast<std::int32_t>(samples[index].channel[1]) +
                         static_cast<std::int32_t>(samples[index].channel[2]) +
                         (soundPortLevel ? 32767 : 0);
        const auto average = sum / static_cast<std::int32_t>(ay8910::ChannelCount + 1);
        const auto filtered = average - audio.previousInput +
                              (audio.previousOutput * 32674 >> 15);
        audio.previousInput = average;
        audio.previousOutput = std::clamp(filtered, -32768, 32767);
        if (available) {
            available->samples[index] = static_cast<std::int16_t>(audio.previousOutput);
        }
        if (recording) app.fieldAudio[index] = static_cast<std::int16_t>(audio.previousOutput);
    }
    if (recording) app.recorder.addAudio(app.fieldAudio.data(), app.fieldAudio.size());

    if (!available) return;
    available->header.dwBufferLength = static_cast<DWORD>(
        samples.size() * sizeof(available->samples[0]));
    if (waveOutWrite(audio.device, &available->header, sizeof(available->header)) ==
        MMSYSERR_NOERROR) {
        available->inFlight = true;
        audio.nextBlock = (availableIndex + 1) % audio.blocks.size();
    } else {
        stopAudio(audio);
        audio.nextRetryTick = GetTickCount64() + AudioRetryIntervalMs;
    }
}

// Reopens the WinMM device while it is unavailable; returns true when the device state changed.
bool retryAudio(AudioOutput& audio)
{
    if (audio.device || GetTickCount64() < audio.nextRetryTick) return false;
    if (startAudio(audio)) return true;
    audio.nextRetryTick = GetTickCount64() + AudioRetryIntervalMs;
    return false;
}

// Keys without a character: GRAPH on the left Alt, CODE on the right Ctrl (left and right of the
// MSX space bar), SELECT on Page Up; F6-F10 are Shift+F1 to Shift+F5 as on the MSX keyboard.
bool matrixPositionFallback(WPARAM virtualKey, bool extended, KeyPosition& position, bool& shift)
{
    shift = false;
    switch (virtualKey) {
    case VK_SHIFT: return false;
    case VK_CONTROL:
        position = extended ? KeyPosition{6, 4} : KeyPosition{6, 1};
        return true;
    case VK_MENU:
        if (extended) return false;
        position = {6, 2};
        return true;
    case VK_LMENU: position = {6, 2}; return true;
    case VK_RMENU: return false;
    case VK_CAPITAL: position = {6, 3}; return true;
    case VK_RETURN: position = {7, 7}; return true;
    case VK_BACK: position = {7, 5}; return true;
    case VK_END: position = {7, 4}; return true;
    case VK_PRIOR: position = {7, 6}; return true;
    case VK_TAB: position = {7, 3}; return true;
    case VK_ESCAPE: position = {7, 2}; return true;
    case VK_F1: position = {6, 5}; return true;
    case VK_F6: position = {6, 5}; shift = true; return true;
    case VK_F2: position = {6, 6}; return true;
    case VK_F7: position = {6, 6}; shift = true; return true;
    case VK_F3: position = {6, 7}; return true;
    case VK_F8: position = {6, 7}; shift = true; return true;
    case VK_F4: position = {7, 0}; return true;
    case VK_F9: position = {7, 0}; shift = true; return true;
    case VK_F5: position = {7, 1}; return true;
    case VK_F10: position = {7, 1}; shift = true; return true;
    case VK_RIGHT: position = {8, 7}; return true;
    case VK_DOWN: position = {8, 6}; return true;
    case VK_UP: position = {8, 5}; return true;
    case VK_LEFT: position = {8, 4}; return true;
    case VK_DELETE: position = {8, 3}; return true;
    case VK_INSERT: position = {8, 2}; return true;
    case VK_HOME: position = {8, 1}; return true;
    case VK_SPACE: position = {8, 0}; return true;
    case VK_NUMPAD4: position = {9, 7}; return true;
    case VK_NUMPAD3: position = {9, 6}; return true;
    case VK_NUMPAD2: position = {9, 5}; return true;
    case VK_NUMPAD1: position = {9, 4}; return true;
    case VK_NUMPAD0: position = {9, 3}; return true;
    case VK_DIVIDE: position = {9, 2}; return true;
    case VK_ADD: position = {9, 1}; return true;
    case VK_MULTIPLY: position = {9, 0}; return true;
    case VK_DECIMAL: position = {10, 7}; return true;
    case VK_SEPARATOR: position = {10, 6}; return true;
    case VK_SUBTRACT: position = {10, 5}; return true;
    case VK_NUMPAD9: position = {10, 4}; return true;
    case VK_NUMPAD8: position = {10, 3}; return true;
    case VK_NUMPAD7: position = {10, 2}; return true;
    case VK_NUMPAD6: position = {10, 1}; return true;
    case VK_NUMPAD5: position = {10, 0}; return true;
    default: return false;
    }
}

// F11 switches the keyboard joystick on and off; while it is off, the joystick keys type on the MSX
// keyboard again.
bool keyboardJoystickEnabled(const AppState& app)
{
    return app.keyboardJoystickOn;
}

bool keyboardJoystickChosen(const AppState& app)
{
    return app.joystickSources[0] == JoystickKeyboard || app.joystickSources[1] == JoystickKeyboard;
}

bool joystickKey(const AppState& app, std::size_t keyId)
{
    if (!keyboardJoystickEnabled(app)) return false;
    for (std::size_t port = 0; port < JoystickKeys.size(); ++port) {
        if (app.joystickSources[port] != JoystickKeyboard) continue;
        for (const auto key : JoystickKeys[port]) {
            if (key == keyId) return true;
        }
    }
    return false;
}

// MSX joystick port value: directions in bits 0-3, triggers A and B in bits 4 and 5, active low.
std::uint8_t joystickBits(bool up, bool down, bool left, bool right, bool triggerA, bool triggerB)
{
    std::uint8_t result = 0xFF;
    if (up) result &= static_cast<std::uint8_t>(~0x01);
    if (down) result &= static_cast<std::uint8_t>(~0x02);
    if (left) result &= static_cast<std::uint8_t>(~0x04);
    if (right) result &= static_cast<std::uint8_t>(~0x08);
    if (triggerA) result &= static_cast<std::uint8_t>(~0x10);
    if (triggerB) result &= static_cast<std::uint8_t>(~0x20);
    return result;
}

void probeController(AppState& app, std::size_t port)
{
    auto& controller = app.controllers[port];
    controller.nextProbe = GetTickCount64() + ControllerProbeIntervalMs;
    controller.present = joyGetDevCapsW(JOYSTICKID1 + static_cast<UINT>(port), &controller.caps,
                                        sizeof(controller.caps)) == JOYERR_NOERROR;
}

// Axes beyond a quarter of their range and the point-of-view hat give the directions; the second
// button is trigger B, any other button trigger A.
std::uint8_t controllerJoystick(AppState& app, std::size_t port)
{
    auto& controller = app.controllers[port];
    if (!controller.present && GetTickCount64() >= controller.nextProbe) probeController(app, port);
    if (!controller.present) return 0xFF;
    JOYINFOEX info{};
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(JOYSTICKID1 + static_cast<UINT>(port), &info) != JOYERR_NOERROR) {
        controller.present = false;
        return 0xFF;
    }
    const auto& caps = controller.caps;
    const auto xQuarter = (caps.wXmax - caps.wXmin) / 4;
    const auto yQuarter = (caps.wYmax - caps.wYmin) / 4;
    bool up = info.dwYpos < caps.wYmin + yQuarter;
    bool down = info.dwYpos > caps.wYmax - yQuarter;
    bool left = info.dwXpos < caps.wXmin + xQuarter;
    bool right = info.dwXpos > caps.wXmax - xQuarter;
    if ((caps.wCaps & JOYCAPS_HASPOV) != 0 && info.dwPOV != JOY_POVCENTERED) {
        const auto pov = info.dwPOV;
        up = up || pov >= 31500 || pov <= 4500;
        right = right || (pov >= 4500 && pov <= 13500);
        down = down || (pov >= 13500 && pov <= 22500);
        left = left || (pov >= 22500 && pov <= 31500);
    }
    return joystickBits(up, down, left, right, (info.dwButtons & ~JOY_BUTTON2) != 0, (info.dwButtons & JOY_BUTTON2) != 0);
}

void updateJoysticks(AppState& app)
{
    std::array<std::uint8_t, 2> ports{0xFF, 0xFF};
    for (std::size_t port = 0; port < ports.size(); ++port) {
        if (app.joystickSources[port] == JoystickKeyboard && keyboardJoystickEnabled(app)) {
            const auto& keys = JoystickKeys[port];
            ports[port] = joystickBits(app.keyDown[keys[0]], app.keyDown[keys[1]], app.keyDown[keys[2]],
                                       app.keyDown[keys[3]], app.keyDown[keys[4]], app.keyDown[keys[5]]);
        } else if (app.joystickSources[port] == JoystickController) {
            ports[port] = controllerJoystick(app, port);
        }
    }
    app.machine.setJoystickPorts(ports[0], ports[1]);
}

std::wstring joystickStatus(const AppState& app)
{
    std::wstring text;
    for (std::size_t port = 0; port < 2; ++port) {
        text += (port == 0 ? L"Joystick 1: " : L" | Joystick 2: ");
        switch (app.joystickSources[port]) {
        case JoystickKeyboard: text += port == 0 ? ui(L"arrows + C/V", L"Pfeile + C/V") : ui(L"W A S D + Z/X", L"W A S D + Y/X"); break;
        case JoystickController:
            text += ui(L"Game controller ", L"Gamecontroller ") + std::to_wstring(port + 1);
            if (!app.controllers[port].present) text += ui(L" (not found)", L" (nicht gefunden)");
            break;
        default: text += ui(L"off", L"aus"); break;
        }
    }
    if (keyboardJoystickChosen(app)) text += keyboardJoystickEnabled(app) ? ui(L" (F11: on)", L" (F11: an)") : ui(L" (F11: off)", L" (F11: aus)");
    return text;
}

// The MSX Shift contact is held by a key that needs it, or by the PC Shift key alone, as on the real
// keyboard (BASIC shows the F6-F10 labels while Shift is down); a held character that the MSX types
// without Shift takes it back meanwhile.
void refreshShift(AppState& app)
{
    const auto shift = KeyPosition{6, 0};
    const bool pcShift = (app.leftShiftDown || app.rightShiftDown) && app.shiftSuppressors == 0;
    app.machine.setKeyboardKey(shift.row, shift.bit, app.matrixCounts[shift.row][shift.bit] != 0 || pcShift);
}

void releaseMatrixBinding(AppState& app, MatrixBinding& binding)
{
    if (!binding.active) return;
    binding.active = false;
    for (std::uint8_t index = 0; index < binding.count; ++index) {
        const auto contact = binding.positions[index];
        auto& count = app.matrixCounts[contact.row][contact.bit];
        if (count > 0) --count;
        app.machine.setKeyboardKey(contact.row, contact.bit, count != 0);
    }
    binding.count = 0;
    if (binding.suppressesShift) {
        binding.suppressesShift = false;
        if (app.shiftSuppressors > 0) --app.shiftSuppressors;
    }
    refreshShift(app);
}

void releaseAllInput(AppState& app)
{
    for (auto& binding : app.matrixBindings) {
        releaseMatrixBinding(app, binding);
    }
    app.keyDown.fill(false);
    app.leftShiftDown = false;
    app.rightShiftDown = false;
    app.altGrDown = false;
    app.shiftSuppressors = 0;
    refreshShift(app);
    app.machine.setJoystickPorts(0xFF, 0xFF);
}

void updateInput(AppState& app, WPARAM virtualKey, LPARAM keyData, bool pressed)
{
    const auto scanCode = static_cast<std::uint8_t>((keyData >> 16) & 0xFF);
    const bool extended = ((keyData >> 24) & 1) != 0;
    if (scanCode == 0x2A) app.leftShiftDown = pressed;
    if (scanCode == 0x36) app.rightShiftDown = pressed;
    if (extended && scanCode == 0x38) {
        app.altGrDown = pressed;
        if (pressed) releaseMatrixBinding(app, app.matrixBindings[0x1D]);
    }
    const auto matrixKeyId = static_cast<std::size_t>(scanCode) | (extended ? 0x100u : 0u);
    app.keyDown[matrixKeyId] = pressed;
    auto& binding = app.matrixBindings[matrixKeyId];
    if (joystickKey(app, matrixKeyId)) {
        releaseMatrixBinding(app, binding);
    } else if (pressed) {
        if (!binding.active) {
            std::array<KeyPosition, 2> positions{};
            std::uint8_t contactCount = 0;
            KeyPosition position{};
            bool shiftRequired = false;
            const bool mappedScanCode = (uiLanguage() == UiLanguage::German ? svi728::mapGermanScanCodeToMsx
                                                                           : svi728::mapUsScanCodeToMsx)(
                scanCode, extended, app.leftShiftDown || app.rightShiftDown,
                app.altGrDown, position, shiftRequired);
            bool fallbackShift = false;
            const bool mapped = mappedScanCode ||
                                matrixPositionFallback(virtualKey, extended, position, fallbackShift);
            if (mapped && position.row < svi728::KeyboardRowCount && position.bit < 8) {
                positions[contactCount++] = position;
                if ((mappedScanCode && shiftRequired) || fallbackShift) positions[contactCount++] = {6, 0};

                binding.positions = positions;
                binding.count = contactCount;
                binding.active = true;
                binding.suppressesShift = mappedScanCode && !shiftRequired && (app.leftShiftDown || app.rightShiftDown);
                if (binding.suppressesShift) ++app.shiftSuppressors;
                for (std::uint8_t index = 0; index < binding.count; ++index) {
                    const auto contact = binding.positions[index];
                    auto& count = app.matrixCounts[contact.row][contact.bit];
                    if (count < 0xFF) ++count;
                    app.machine.setKeyboardKey(contact.row, contact.bit, true);
                }
            }
        }
    } else {
        releaseMatrixBinding(app, binding);
    }
    refreshShift(app);

    updateJoysticks(app);
}

// Maps a client coordinate outside the active display to the raster dot the beam drew there.
int nativeBorderCoordinate(int client)
{
    const auto offset = client - FrameWidth;
    return offset >= 0 ? offset / Scale : -((-offset + Scale - 1) / Scale);
}

void updateDisplayPixels(AppState& app, const tms99x8::FrameBuffer& frame)
{
    const auto& video = app.machine.video();
    const auto& backdrop = video.backdropRaster();
    const auto displayStart = static_cast<int>(video.outputs().displayStartScanline);
    const auto scanlines = static_cast<int>(video.outputs().scanlinesPerFrame);
    for (int clientY = 0; clientY < ClientHeight; ++clientY) {
        const bool activeRow = clientY >= FrameWidth && clientY < FrameWidth + DisplayHeight;
        const auto line = std::clamp(displayStart + nativeBorderCoordinate(clientY), 0, scanlines - 1);
        auto* row = app.pixels.data() + static_cast<std::size_t>(clientY) * ClientWidth;
        for (int clientX = 0; clientX < ClientWidth; ++clientX) {
            if (activeRow && clientX >= FrameWidth && clientX < FrameWidth + DisplayWidth) {
                clientX = FrameWidth + DisplayWidth - 1;
                continue;
            }
            const auto dot = std::clamp(
                tms99x8::HorizontalDisplayStart + nativeBorderCoordinate(clientX), 0,
                static_cast<int>(tms99x8::DotsPerScanline) - 1);
            row[clientX] = Palette[backdrop[static_cast<std::size_t>(line) *
                                            tms99x8::DotsPerScanline + dot] & 0x0F];
        }
    }

    for (int y = 0; y < NativeHeight; ++y) {
        for (int x = 0; x < NativeWidth; ++x) {
            const auto color = Palette[frame[static_cast<std::size_t>(y * NativeWidth + x)] & 0x0F];
            for (int scaleY = 0; scaleY < Scale; ++scaleY) {
                const auto destination = static_cast<std::size_t>(
                    (FrameWidth + y * Scale + scaleY) * ClientWidth + FrameWidth + x * Scale);
                std::fill_n(app.pixels.begin() + destination, Scale, color);
            }
        }
    }
}

std::uint16_t basicCurrentLine(const svi728::SVI728& machine)
{
    return static_cast<std::uint16_t>(machine.readMemory(BasicCurrentLineAddress) |
        (static_cast<std::uint16_t>(machine.readMemory(BasicCurrentLineAddress + 1)) << 8));
}

void closePrinterFile(AppState& app)
{
    if (app.printerFile.is_open()) {
        app.printerFile.flush();
        if (!app.printerFile) {
            MessageBoxW(app.window, ui(L"Error while writing the printer file.", L"Fehler beim Schreiben der Druckdatei."),
                        ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
        }
        app.printerFile.close();
    }
    app.printerOutputPath.clear();
    app.printerFileMode = PrinterFileMode::None;
    app.lastDirectPrinterByteTick = 0;
}

bool openPrinterFile(AppState& app)
{
    if (app.printerFile.is_open()) return true;
    if (app.printerDirectory.empty()) {
        app.printerOutputPaused = true;
        if (!app.printerErrorShown) app.printerWarningPending = true;
        return false;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::wstring stem =
        std::to_wstring(now.wYear) + L"-" +
        (now.wMonth < 10 ? L"0" : L"") + std::to_wstring(now.wMonth) + L"-" +
        (now.wDay < 10 ? L"0" : L"") + std::to_wstring(now.wDay) + L"_" +
        (now.wHour < 10 ? L"0" : L"") + std::to_wstring(now.wHour) + L"-" +
        (now.wMinute < 10 ? L"0" : L"") + std::to_wstring(now.wMinute) + L"-" +
        (now.wSecond < 10 ? L"0" : L"") + std::to_wstring(now.wSecond) + L"-" +
        (now.wMilliseconds < 100 ? (now.wMilliseconds < 10 ? L"00" : L"0") : L"") +
        std::to_wstring(now.wMilliseconds);

    for (unsigned suffix = 0; suffix < 1000; ++suffix) {
        const auto candidate = app.printerDirectory /
            (stem + (suffix == 0 ? L"" : L"-" + std::to_wstring(suffix)) + L".txt");
        std::error_code error;
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) break;
        if (exists) continue;
        app.printerFile.open(candidate, std::ios::binary | std::ios::out | std::ios::trunc);
        if (app.printerFile) {
            app.printerOutputPath = candidate;
            return true;
        }
        break;
    }

    app.printerOutputPaused = true;
    if (!app.printerErrorShown) {
        MessageBoxW(app.window,
            ui(L"The printer file could not be created in the chosen output folder.", L"Die Druckdatei konnte im gewaehlten Ablageordner nicht angelegt werden."),
            ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
        app.printerErrorShown = true;
    }
    return false;
}

void processPrinterOutput(AppState& app)
{
    if (app.printerOutputPaused) return;

    const auto& output = app.machine.printer().output();
    const auto currentLine = basicCurrentLine(app.machine);

    while (app.printerOutputOffset < output.size()) {
        if (app.printerFileMode == PrinterFileMode::None) {
            app.printerFileMode = currentLine != 0xFFFF ? PrinterFileMode::Program
                : PrinterFileMode::Direct;
        }
        if (!openPrinterFile(app)) return;

        const auto byte = output[app.printerOutputOffset];
        app.printerFile.put(static_cast<char>(byte));
        if (!app.printerFile) {
            app.printerFile.close();
            app.printerOutputPaused = true;
            if (!app.printerErrorShown) {
                MessageBoxW(app.window, ui(L"Error while writing the printer file.", L"Fehler beim Schreiben der Druckdatei."),
                            ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
                app.printerErrorShown = true;
            }
            return;
        }
        ++app.printerOutputOffset;
        if (app.printerFileMode == PrinterFileMode::Direct) {
            app.lastDirectPrinterByteTick = app.emulatedCpuTicks;
        }
    }

    if (app.printerOutputOffset == output.size() && !output.empty()) {
        app.machine.printer().clearOutput();
        app.printerOutputOffset = 0;
    }

    if (app.printerFileMode == PrinterFileMode::Program && currentLine == 0xFFFF) {
        closePrinterFile(app);
    } else if (app.printerFileMode == PrinterFileMode::Direct &&
               app.lastDirectPrinterByteTick != 0 &&
               app.emulatedCpuTicks - app.lastDirectPrinterByteTick >= DirectPrinterIdleTicks) {
        closePrinterFile(app);
    }

}

void stopVideoRecording(AppState& app);

// Hands the picture of the field just finished to the video recording.
void captureVideoField(AppState& app)
{
    if (!app.recorder.recording()) return;
    if (app.machine.vdpMasterTicksPerField() != app.recorder.settings().frameRateDenominator) {
        // The video standard and with it the field rate changed: the recording ends here.
        stopVideoRecording(app);
        return;
    }
    updateDisplayPixels(app, app.machine.video().scanoutFrame());
    app.recorder.addFrame(app.pixels.data(), ClientWidth);
}

void runEmulationField(AppState& app)
{
    if (app.joystickSources[0] == JoystickController || app.joystickSources[1] == JoystickController) {
        updateJoysticks(app);
    }
    const auto sampleCount = nextAudioSampleCount(app);
    if (app.audioTestActive) {
        app.soundPortTransitions.clear();
        setAyAudioTestTone(app);
        queueAudioField(app, app.machine.oneBitSoundLevel(), sampleCount);
        if (++app.audioTestField >= 32) restoreAyAfterAudioTest(app);
        captureVideoField(app);
        return;
    }

    app.soundPortTransitions.clear();
    const bool initialSoundPortLevel = app.machine.oneBitSoundLevel();
    bool previousSoundPortLevel = initialSoundPortLevel;
    const auto ticksPerField = app.machine.cpuTicksPerField();
    for (std::uint32_t tick = 0; tick < ticksPerField; ++tick) {
        if ((tick & 0xFFu) == 0) processPendingKeyboardMessages(app.window);
        (void)app.machine.tick();
        ++app.emulatedCpuTicks;
        processPrinterOutput(app);
        const bool soundPortLevel = app.machine.oneBitSoundLevel();
        if (soundPortLevel != previousSoundPortLevel) {
            app.soundPortTransitions.push_back({tick + 1, soundPortLevel});
            previousSoundPortLevel = soundPortLevel;
        }
    }
    queueAudioField(app, initialSoundPortLevel, sampleCount);
    captureVideoField(app);
}

// Effective CPU clock: emulated CPU T-states per second of wall time, measured once per second.
void measureCpuClock(AppState& app, LONGLONG now)
{
    const auto elapsed = now - app.speedStart.QuadPart;
    if (elapsed < app.counterFrequency.QuadPart) return;
    app.cpuClockMHz = static_cast<double>(app.emulatedCpuTicks - app.speedStartTicks) *
                      static_cast<double>(app.counterFrequency.QuadPart) /
                      static_cast<double>(elapsed) / 1e6;
    app.speedStart.QuadPart = now;
    app.speedStartTicks = app.emulatedCpuTicks;
}

std::wstring formatMHz(double mhz)
{
    wchar_t text[32];
    swprintf(text, 32, L"CPU@%.2fMHz", mhz);
    std::wstring result(text);
    std::replace(result.begin(), result.end(), L'.', L',');
    return result;
}

bool advanceWallClock(AppState& app)
{
    LARGE_INTEGER now{};
    if (!QueryPerformanceCounter(&now) || app.counterFrequency.QuadPart <= 0) return false;

    auto elapsed = now.QuadPart - app.lastCounter.QuadPart;
    app.lastCounter = now;
    if (elapsed <= 0) return false;

    const auto maximumElapsed = app.counterFrequency.QuadPart / 4;
    elapsed = std::min(elapsed, maximumElapsed);
    app.fieldPhase += static_cast<std::uint64_t>(elapsed) * svi728::VdpMasterClockHz;
    const auto fieldThreshold = static_cast<std::uint64_t>(app.counterFrequency.QuadPart) *
                                app.machine.vdpMasterTicksPerField();

    bool advanced = false;
    while (app.fieldPhase >= fieldThreshold) {
        app.fieldPhase -= fieldThreshold;
        runEmulationField(app);
        advanced = true;
    }
    measureCpuClock(app, now.QuadPart);
    return advanced;
}

// While the motor runs, shows how far the tape has been read or how much has been recorded.
std::wstring cassetteProgress(const svi728::CassetteState& state)
{
    if (!state.motorOn) return {};
    const auto seconds = state.positionTicks / svi728::CpuClockHz;
    const auto secondsText = std::to_wstring(seconds % 60);
    const auto tapeTime = ui(L", tape ", L", Band ") + std::to_wstring(seconds / 60) + L":" +
                          (secondsText.size() < 2 ? L"0" : L"") + secondsText;
    if (state.recording) {
        return ui(L" | Saving: ", L" | Speichern: ") + std::to_wstring(state.bytePosition) + L" Bytes" + tapeTime;
    }
    if (!state.inserted || state.byteCount == 0) return {};
    const auto block = std::min(state.blockIndex + 1, state.blockCount);
    const auto percent = state.bytePosition * 100 / state.byteCount;
    return ui(L" | Loading: block ", L" | Laden: Block ") + std::to_wstring(block) + L"/" +
           std::to_wstring(state.blockCount) + L", " + std::to_wstring(state.bytePosition) +
           L"/" + std::to_wstring(state.byteCount) + L" Bytes (" + std::to_wstring(percent) +
           L" %)" + tapeTime;
}

void updateWindowTitle(AppState& app)
{
    const auto state = app.machine.cassetteState();
    auto title = joystickStatus(app);
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (const auto boardSlot = app.machine.bridgeSlot(slot)) {
            const auto& description = app.boardSlots[boardSlot - 1]->config().description;
            title += ui(L" | Module ", L" | Modul ") + std::to_wstring(slot) + L": USB2SLOT " + std::to_wstring(boardSlot) +
                     L" " + std::wstring(description.begin(), description.end());
            if (!app.bridge || !app.bridge->connected()) title += ui(L" (no connection)", L" (keine Verbindung)");
            continue;
        }
        const auto& cart = app.machine.cartridgeSlot(slot);
        if (!cart.inserted()) continue;
        title += ui(L" | Module ", L" | Modul ") + std::to_wstring(slot) + L": " + app.cartridgePaths[slot - 2].filename().wstring() +
                 L" (" + mapperText(cart.mapper()) + L")";
    }
    if (state.recording) {
        title += ui(L" | CAS: recording", L" | CAS: Aufnahme");
    } else if (!app.cassettePath.empty()) {
        title += L" | CAS: " + app.cassettePath.filename().wstring();
    }
    if (!app.printerDirectory.empty()) {
        title += ui(L" | Printer: ", L" | Drucker: ") + app.printerDirectory.filename().wstring();
    }
    title += app.machine.videoStandard() == svi728::VideoStandard::Ntsc ? L" | NTSC 60 Hz" : L" | PAL 50 Hz";
    title += cassetteProgress(state);
    if (app.recorder.recording()) {
        const auto seconds = static_cast<unsigned>(app.recorder.seconds());
        wchar_t text[32];
        swprintf(text, 32, L" | REC %u:%02u", seconds / 60, seconds % 60);
        title += text;
    }
    if (!app.audio.device) title += ui(L" | Audio: off", L" | Audio: aus");
    if (state.modified) title += L" *";
    app.titleTick = GetTickCount64();
    if (app.statusBar && title != app.shownStatus) {
        app.shownStatus = title;
        SetWindowTextW(app.statusBar, title.c_str());
    }
    auto caption = std::wstring(WindowTitle);
    if (app.cpuClockMHz > 0.0) caption += L" | " + formatMHz(app.cpuClockMHz);
    if (caption == app.shownTitle) return;
    app.shownTitle = caption;
    SetWindowTextW(app.window, caption.c_str());
}

void updateCassetteMenu(AppState& app)
{
    const auto state = app.machine.cassetteState();
    const auto enabled = state.inserted && !state.recording ? MF_ENABLED : MF_GRAYED;
    const auto menu = GetMenu(app.window);
    EnableMenuItem(menu, CassetteRewindCommand, MF_BYCOMMAND | enabled);
    EnableMenuItem(menu, CassetteEjectCommand, MF_BYCOMMAND | enabled);
    EnableMenuItem(menu, CassetteRecordCommand,
                   MF_BYCOMMAND | (state.recording ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, CassetteSaveCommand,
                   MF_BYCOMMAND | (state.modified ? MF_ENABLED : MF_GRAYED));
    updateWindowTitle(app);
    DrawMenuBar(app.window);
}

void resumeAfterCassetteDialog(AppState& app)
{
    QueryPerformanceCounter(&app.lastCounter);
    app.fieldPhase = 0;
    SetTimer(app.window, EmulationTimer, TimerPeriodMs, nullptr);
}

bool confirmCassetteDiscard(AppState& app)
{
    if (!app.machine.cassetteState().modified) return true;
    KillTimer(app.window, EmulationTimer);
    const auto answer = MessageBoxW(app.window,
        ui(L"Discard the unsaved CAS recording?", L"Die ungespeicherte CAS-Aufnahme verwerfen?"), ui(L"Cassette", L"Kassette"),
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    resumeAfterCassetteDialog(app);
    return answer == IDYES;
}

bool writeCassetteFile(const std::filesystem::path& path,
                       const std::vector<std::uint8_t>& image)
{
    auto temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
                 std::to_wstring(GetTickCount64());
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool saved = WriteFile(file, image.data(), static_cast<DWORD>(image.size()),
                           &written, nullptr) != FALSE && written == image.size();
    if (saved) saved = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) saved = false;
    if (saved) {
        saved = MoveFileExW(temporary.c_str(), path.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!saved) DeleteFileW(temporary.c_str());
    return saved;
}

void saveCassetteImage(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    const bool stopped = !app.machine.cassetteState().recording ||
                         app.machine.stopCassetteRecording();
    updateCassetteMenu(app);
    if (!stopped || app.machine.cassetteState().recordingError ||
        app.machine.cassetteImage().empty()) {
        MessageBoxW(app.window,
            ui(L"There is no complete MSX CAS recording. Invalid or incomplete signals, or signals CAS cannot represent, are not saved.", L"Keine vollstaendige MSX-CAS-Aufnahme vorhanden. Ungueltige, unvollstaendige "
            L"oder nicht als CAS darstellbare Signale werden nicht gespeichert."),
            ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        resumeAfterCassetteDialog(app);
        return;
    }
    std::vector<wchar_t> fileName(32768, L'\0');
    const auto suggestedName = svi728::suggestedCassetteFileName(app.machine.cassetteImage());
    std::copy(suggestedName.begin(), suggestedName.end(), fileName.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"MSX cassettes (*.cas)\0*.cas\0\0", L"MSX-Kassetten (*.cas)\0*.cas\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrDefExt = L"cas";
    const std::wstring initialFolder = mediaFolder(L"CAS").wstring();
    dialog.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
    dialog.lpstrTitle = ui(L"Save CAS recording", L"CAS-Aufnahme speichern");
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        if (writeCassetteFile(path, app.machine.cassetteImage())) {
            app.machine.markCassetteSaved();
            app.cassettePath = path;
            updateCassetteMenu(app);
        } else {
            MessageBoxW(app.window,
                ui(L"The CAS image could not be saved. The recording stays in memory.", L"Das CAS-Image konnte nicht gespeichert werden. Die Aufnahme bleibt im Speicher."),
                ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The save dialog could not be opened.", L"Der Speicherdialog konnte nicht geoeffnet werden."),
                    ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
    }
    resumeAfterCassetteDialog(app);
}

void selectCassetteImage(AppState& app)
{
    if (!confirmCassetteDiscard(app)) return;
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"MSX cassettes (*.cas)\0*.cas\0All files (*.*)\0*.*\0\0", L"MSX-Kassetten (*.cas)\0*.cas\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    const std::wstring initialFolder = mediaFolder(L"CAS").wstring();
    dialog.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
    dialog.lpstrTitle = ui(L"Insert CAS image", L"CAS-Image einlegen");
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto size = file ? file.tellg() : std::streampos(-1);
        bool loaded = false;
        if (size > std::streampos(0) && size <= std::streampos(16 * 1024 * 1024)) {
            file.seekg(0);
            std::vector<std::uint8_t> image(static_cast<std::size_t>(size));
            if (file.read(reinterpret_cast<char*>(image.data()),
                          static_cast<std::streamsize>(image.size()))) {
                svi728::Cassette candidate;
                if (candidate.insert(image)) {
                    app.machine.ejectCassette();
                    loaded = app.machine.loadCassette(image);
                }
            }
        }
        if (loaded) {
            app.cassettePath = path;
            updateCassetteMenu(app);
        } else {
            MessageBoxW(app.window,
                ui(L"The file is not a readable MSX CAS image (at most 16 MiB).", L"Die Datei ist kein lesbares MSX-CAS-Image (maximal 16 MiB)."),
                ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."),
                    ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
    }
    resumeAfterCassetteDialog(app);
}

bool openBridge(AppState& app, bool showError);
void refreshMenu(AppState& app);
void saveMedia(AppState& app);

// Resets the machine; a lost connection to the USB2SLOT board is opened again first.
void resetMachine(AppState& app)
{
    if (app.bridge && !app.bridge->connected()) (void)openBridge(app, false);
    app.machine.reset();
}

// Changing a cartridge resets the machine like switching it off and on.
void selectCartridge(AppState& app, std::uint8_t slot, unsigned choice)
{
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"MSX modules (*.rom)\0*.rom\0All files (*.*)\0*.*\0\0", L"MSX-Module (*.rom)\0*.rom\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    const std::wstring initialFolder = mediaFolder(L"CARD").wstring();
    dialog.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
    dialog.lpstrTitle = slot == 2 ? ui(L"Insert module in slot 2", L"Modul in Slot 2 einlegen") : ui(L"Insert module in slot 3", L"Modul in Slot 3 einlegen");
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto size = file ? file.tellg() : std::streampos(-1);
        std::vector<std::uint8_t> image;
        if (size > std::streampos(0) && size <= std::streampos(4 * 1024 * 1024)) {
            file.seekg(0);
            image.resize(static_cast<std::size_t>(size));
            if (!file.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()))) {
                image.clear();
            }
        }
        std::optional<cartridge::Mapper> mapper;
        if (choice >= 1 && choice <= CartridgeMappers.size()) mapper = CartridgeMappers[choice - 1];
        if (!image.empty() && app.machine.insertCartridge(slot, image, mapper)) {
            app.cartridgePaths[slot - 2] = path;
            (void)app.machine.connectBridgeSlot(slot, nullptr);
            resetMachine(app);
        } else {
            MessageBoxW(app.window,
                ui(L"The file does not fit the chosen module type (without mapper up to 64 KiB, otherwise whole banks, at most 4 MiB).", L"Die Datei passt nicht zum gewaehlten Modultyp (ohne Mapper bis 64 KiB, sonst ganze Baenke, "
                L"maximal 4 MiB)."),
                ui(L"Module", L"Modul"), MB_OK | MB_ICONERROR);
        }
        refreshMenu(app);
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."),
                    ui(L"Module", L"Modul"), MB_OK | MB_ICONERROR);
    }
    resumeAfterCassetteDialog(app);
}

void ejectCartridge(AppState& app, std::uint8_t slot)
{
    app.machine.ejectCartridge(slot);
    app.cartridgePaths[slot - 2].clear();
    (void)app.machine.connectBridgeSlot(slot, nullptr);
    resetMachine(app);
    refreshMenu(app);
}

// Opens the USB2SLOT board on its COM port unless it is connected already; the board slots in use keep
// their rules and move to the new connection.
bool openBridge(AppState& app, bool showError)
{
    if (app.bridge && app.bridge->connected()) return true;
    std::string error;
    auto transport = usbslot::openSerial(app.bridgePort, error);
    if (!transport) {
        if (showError) {
            const std::wstring text = ui(L"The USB2SLOT board does not answer:\n\n", L"Die USB2SLOT-Platine antwortet nicht:\n\n") +
                                      std::wstring(error.begin(), error.end());
            MessageBoxW(app.window, text.c_str(), L"USB2SLOT", MB_OK | MB_ICONERROR);
        }
        return false;
    }
    auto bridge = std::make_unique<usbslot::Bridge>(std::move(transport));
    std::array<std::unique_ptr<usbslot::BoardSlot>, 2> boardSlots;
    for (std::uint8_t number = 1; number <= 2; ++number) {
        if (const auto& old = app.boardSlots[number - 1]) {
            boardSlots[number - 1] = std::make_unique<usbslot::BoardSlot>(*bridge, number, old->config());
        }
    }
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (const auto number = app.machine.bridgeSlot(slot)) {
            (void)app.machine.connectBridgeSlot(slot, boardSlots[number - 1].get());
        }
    }
    app.boardSlots = std::move(boardSlots);
    app.bridge = std::move(bridge);
    return true;
}

// Keeps what a probe of the board found, for the module menu.
void noteDetection(AppState& app, const usbslot::Detection& detection)
{
    for (std::size_t index = 0; index < 2; ++index) {
        const auto& description = detection.slots[index].description;
        app.boardContents[index] = detection.occupied[index] ? std::wstring(description.begin(), description.end())
                                                             : std::wstring();
    }
    app.boardProbed = true;
    app.detection = detection;
}

// Probes the board and sets up its slot with the rules for what was found; false when the slot is empty.
bool setUpBoardSlot(AppState& app, std::uint8_t number)
{
    const auto detection = usbslot::detectBoard(*app.bridge);
    noteDetection(app, detection);
    if (!detection.occupied[number - 1]) return false;
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (app.machine.bridgeSlot(slot) == number) (void)app.machine.connectBridgeSlot(slot, nullptr);
    }
    app.boardSlots[number - 1] = std::make_unique<usbslot::BoardSlot>(*app.bridge, number, detection.slots[number - 1]);
    return true;
}

// Puts slot 1 or 2 of the board into a module slot; like changing a module, the machine is reset.
void selectBridgeSlot(AppState& app, std::uint8_t slot, std::uint8_t boardSlot)
{
    KillTimer(app.window, EmulationTimer);
    if (openBridge(app, true)) {
        if (setUpBoardSlot(app, boardSlot)) {
            app.machine.ejectCartridge(slot);
            app.cartridgePaths[slot - 2].clear();
            (void)app.machine.connectBridgeSlot(slot, app.boardSlots[boardSlot - 1].get());
        } else {
            const std::wstring text = ui(L"Slot ", L"Steckplatz ") + std::to_wstring(boardSlot) +
                                      ui(L" of the USB2SLOT board is empty.", L" der USB2SLOT-Platine ist leer.");
            MessageBoxW(app.window, text.c_str(), L"USB2SLOT", MB_OK | MB_ICONWARNING);
        }
        resetMachine(app);
    }
    refreshMenu(app);
    resumeAfterCassetteDialog(app);
}

// Probes the board again (pulses /RESET): module slots keep their board slot with the new rules, or lose it when
// the board slot is empty now. False when the board does not answer.
bool probeBoard(AppState& app, bool showError)
{
    if (!openBridge(app, showError)) {
        app.boardContents = {};
        app.boardProbed = false;
        return false;
    }
    const auto detection = usbslot::detectBoard(*app.bridge);
    noteDetection(app, detection);
    for (std::uint8_t number = 1; number <= 2; ++number) {
        app.boardSlots[number - 1] = detection.occupied[number - 1]
            ? std::make_unique<usbslot::BoardSlot>(*app.bridge, number, detection.slots[number - 1]) : nullptr;
    }
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (const auto number = app.machine.bridgeSlot(slot)) {
            (void)app.machine.connectBridgeSlot(slot, app.boardSlots[number - 1].get());
        }
    }
    return true;
}

// Menu command "detect USB2SLOT again"; like changing a module, the machine is reset.
void reprobeBoard(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    if (probeBoard(app, true)) resetMachine(app);
    refreshMenu(app);
    saveMedia(app);
    resumeAfterCassetteDialog(app);
}

constexpr int CardClockAddressEdit = 100;

INT_PTR CALLBACK cardClockAddressProcedure(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* address = reinterpret_cast<std::uint8_t*>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        SetDlgItemInt(dialog, CardClockAddressEdit, *reinterpret_cast<std::uint8_t*>(lParam), FALSE);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            BOOL valid = FALSE;
            const UINT value = GetDlgItemInt(dialog, CardClockAddressEdit, &valid, FALSE);
            if (!valid || value > 254) {
                MessageBoxW(dialog, ui(L"Please enter an I/O address from 0 to 254.", L"Bitte eine IO-Adresse von 0 bis 254 eingeben."),
                            ui(L"Clock of card A", L"Uhr der Karte A"), MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            *address = static_cast<std::uint8_t>(value);
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

// Asks for the I/O address of the PCF8584 on card A (decimal, as in CALL I2C INIT); false when cancelled.
bool askCardClockAddress(AppState& app)
{
    std::vector<WORD> data;
    const auto text = [&data](const wchar_t* value) {
        for (; *value; ++value) data.push_back(static_cast<WORD>(*value));
        data.push_back(0);
    };
    const auto align = [&data] {
        if (data.size() % 2 != 0) data.push_back(0);
    };
    const auto item = [&](DWORD style, short x, short y, short cx, short cy, WORD id, WORD classAtom, const wchar_t* title) {
        align();
        DLGITEMTEMPLATE entry{style | WS_CHILD | WS_VISIBLE, 0, x, y, cx, cy, id};
        const auto* words = reinterpret_cast<const WORD*>(&entry);
        data.insert(data.end(), words, words + sizeof(entry) / sizeof(WORD));
        data.push_back(0xFFFF);
        data.push_back(classAtom);
        text(title);
        data.push_back(0);
    };
    DLGTEMPLATE header{DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 4, 0, 0, 190, 62};
    const auto* words = reinterpret_cast<const WORD*>(&header);
    data.insert(data.end(), words, words + sizeof(header) / sizeof(WORD));
    data.push_back(0);
    data.push_back(0);
    text(ui(L"Clock of card A", L"Uhr der Karte A"));
    data.push_back(9);
    text(L"Segoe UI");
    item(SS_LEFT, 8, 8, 174, 18, 0xFFFF, 0x0082,
         ui(L"I/O address of the PCF8584 on card A (0-254, as in CALL I2C INIT):",
            L"IO-Adresse des PCF8584 auf Karte A (0-254, wie bei CALL I2C INIT):"));
    item(ES_NUMBER | ES_LEFT | WS_BORDER | WS_TABSTOP, 8, 28, 40, 12, CardClockAddressEdit, 0x0081, L"");
    item(BS_DEFPUSHBUTTON | WS_TABSTOP, 78, 44, 50, 14, IDOK, 0x0080, L"OK");
    item(BS_PUSHBUTTON | WS_TABSTOP, 132, 44, 50, 14, IDCANCEL, 0x0080, ui(L"Cancel", L"Abbrechen"));
    std::uint8_t address = app.cardClockAddress;
    KillTimer(app.window, EmulationTimer);
    const INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), reinterpret_cast<LPCDLGTEMPLATEW>(data.data()),
                                                   app.window, cardClockAddressProcedure, reinterpret_cast<LPARAM>(&address));
    resumeAfterCassetteDialog(app);
    if (result != IDOK) return false;
    app.cardClockAddress = address;
    return true;
}

// Sets the clock of RC2014 card A to the local PC time; leaves it alone when a check of the board fails.
void setCardClockToPc(AppState& app)
{
    if (!app.bridge || !app.bridge->connected()) return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const usbslot::ClockTime time{now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond};
    (void)usbslot::setCardClock(*app.bridge, app.detection, app.cardClockAddress, time);
}

bool writeSettingsAtomically(const std::filesystem::path& path, const std::string& text)
{
    auto temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
                 std::to_wstring(GetTickCount64());
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool saved = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE &&
                 written == text.size();
    if (saved) saved = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) saved = false;
    if (saved) {
        saved = MoveFileExW(temporary.c_str(), path.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!saved) DeleteFileW(temporary.c_str());
    return saved;
}

void saveMedia(AppState& app)
{
    std::string text = "video=" + std::string(app.machine.videoStandard() == svi728::VideoStandard::Ntsc ? "NTSC" : "PAL") + "\n";
    text += std::string("language=") + (uiLanguage() == UiLanguage::German ? "de" : "en") + "\n";
    for (std::size_t port = 0; port < 2; ++port) {
        text += "joystick" + std::to_string(port + 1) + "=" +
                JoystickSourceNames[static_cast<std::size_t>(app.joystickSources[port])] + "\n";
    }
    if (!app.printerDirectory.empty()) {
        text += "printerDirectory=" + app.printerDirectory.u8string() + "\n";
    }
    text += "usb2slotPort=" + app.bridgePort + "\n";
    text += std::string("cardClock=") + (app.cardClock ? "1" : "0") + "\n";
    text += "cardClockAddress=" + std::to_string(app.cardClockAddress) + "\n";
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (const auto boardSlot = app.machine.bridgeSlot(slot)) {
            text += "slot" + std::to_string(slot) + "=usb2slot," + std::to_string(boardSlot) + "\n";
            continue;
        }
        const auto& cart = app.machine.cartridgeSlot(slot);
        if (!cart.inserted() || app.cartridgePaths[slot - 2].empty()) continue;
        text += "slot" + std::to_string(slot) + "=" +
                std::to_string(static_cast<int>(cart.mapper())) + "," +
                app.cartridgePaths[slot - 2].u8string() + "\n";
    }

    if (!writeSettingsAtomically(app.settingsPath, text)) {
        MessageBoxW(app.window,
            ui(L"The settings could not be saved to svi728.ini.", L"Die Einstellungen konnten nicht in svi728.ini gespeichert werden."),
            L"SVI-728", MB_OK | MB_ICONERROR);
    }
}

// Fits the machine for a video standard (TMS9929A for PAL, TMS9918A for NTSC) and puts the BASIC
// ROM, the modules and the cassette back in.
void rebuildMachine(AppState& app, svi728::VideoStandard standard)
{
    const auto tape = app.machine.cassetteImage();
    const bool tapeInserted = app.machine.cassetteState().inserted && !app.machine.cassetteState().recording;
    std::array<cartridge::Mapper, 2> mappers{};
    std::array<bool, 2> inserted{};
    std::array<std::uint8_t, 2> boardSlots{};
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        inserted[slot - 2] = app.machine.cartridgeSlot(slot).inserted();
        mappers[slot - 2] = app.machine.cartridgeSlot(slot).mapper();
        boardSlots[slot - 2] = app.machine.bridgeSlot(slot);
    }
    app.machine = svi728::SVI728(standard);
    (void)app.machine.loadMainRom(app.romImage);
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        if (boardSlots[slot - 2]) {
            (void)app.machine.connectBridgeSlot(slot, app.boardSlots[boardSlots[slot - 2] - 1].get());
            continue;
        }
        auto& path = app.cartridgePaths[slot - 2];
        if (!inserted[slot - 2] || path.empty()) continue;
        std::ifstream file(path, std::ios::binary);
        const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file), {}};
        if (image.empty() || !app.machine.insertCartridge(slot, image, mappers[slot - 2])) path.clear();
    }
    if (tapeInserted && !app.machine.loadCassette(tape)) app.cassettePath.clear();
    if (!tapeInserted) app.cassettePath.clear();
}

void loadMedia(AppState& app)
{
    std::ifstream file(app.settingsPath, std::ios::binary);
    if (!file) return;

    bool restoreFailed = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = line.substr(0, equals);
        if (key == "joystick1" || key == "joystick2") {
            const auto port = static_cast<std::size_t>(key[8] - '1');
            for (int source = 0; source < JoystickSourceCount; ++source) {
                if (line.substr(equals + 1) == JoystickSourceNames[static_cast<std::size_t>(source)]) {
                    app.joystickSources[port] = source;
                }
            }
            if (app.joystickSources[port] == JoystickController) probeController(app, port);
            continue;
        }
        if (key == "printerDirectory") {
            app.printerDirectory = std::filesystem::u8path(line.substr(equals + 1));
            continue;
        }
        if (key == "video") {
            if (line.substr(equals + 1) == "NTSC") rebuildMachine(app, svi728::VideoStandard::Ntsc);
            continue;
        }
        if (key == "usb2slotPort") {
            if (equals + 1 < line.size()) app.bridgePort = line.substr(equals + 1);
            continue;
        }
        if (key == "cardClock") {
            app.cardClock = line.substr(equals + 1) == "1";
            continue;
        }
        if (key == "cardClockAddress") {
            unsigned address = 0;
            const auto text = line.substr(equals + 1);
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), address);
            if (error == std::errc{} && end == text.data() + text.size() && address <= 0xFE) {
                app.cardClockAddress = static_cast<std::uint8_t>(address);
            }
            continue;
        }
        if (key != "slot2" && key != "slot3") continue;

        const std::string value = line.substr(equals + 1);
        if (value == "usb2slot,1" || value == "usb2slot,2") {
            const auto number = static_cast<std::uint8_t>(value.back() - '0');
            if (!openBridge(app, false) || (!app.boardSlots[number - 1] && !setUpBoardSlot(app, number)) ||
                !app.machine.connectBridgeSlot(static_cast<std::uint8_t>(key[4] - '0'), app.boardSlots[number - 1].get())) {
                restoreFailed = true;
            }
            continue;
        }
        const auto comma = value.find(',');
        if (comma == std::string::npos) {
            restoreFailed = true;
            continue;
        }
        int mapper = -1;
        const auto mapperText = value.substr(0, comma);
        const auto parsed = std::from_chars(mapperText.data(),
            mapperText.data() + mapperText.size(), mapper);
        if (parsed.ec != std::errc{} || parsed.ptr != mapperText.data() + mapperText.size() ||
            mapper < 0 || mapper > static_cast<int>(cartridge::Mapper::KonamiScc)) {
            restoreFailed = true;
            continue;
        }

        const auto path = std::filesystem::u8path(value.substr(comma + 1));
        std::ifstream imageFile(path, std::ios::binary | std::ios::ate);
        const auto size = imageFile ? imageFile.tellg() : std::streampos(-1);
        if (size <= std::streampos(0) || size > std::streampos(4 * 1024 * 1024)) {
            restoreFailed = true;
            continue;
        }
        imageFile.seekg(0);
        std::vector<std::uint8_t> image(static_cast<std::size_t>(size));
        if (!imageFile.read(reinterpret_cast<char*>(image.data()),
                            static_cast<std::streamsize>(image.size())) ||
            !app.machine.insertCartridge(static_cast<std::uint8_t>(key[4] - '0'), image,
                                         static_cast<cartridge::Mapper>(mapper))) {
            restoreFailed = true;
            continue;
        }
        app.cartridgePaths[key[4] - '0' - 2] = path;
    }
    if (restoreFailed) {
        MessageBoxW(nullptr,
            ui(L"At least one saved module could not be restored. Check the paths and entries in svi728.ini.", L"Mindestens ein gespeichertes Modul konnte nicht wiederhergestellt werden. "
            L"Pruefen Sie die Pfade und Eintraege in svi728.ini."),
            L"SVI-728", MB_OK | MB_ICONWARNING);
    }
}

void updateVideoMenu(AppState& app)
{
    const auto ntsc = app.machine.videoStandard() == svi728::VideoStandard::Ntsc;
    CheckMenuRadioItem(GetMenu(app.window), VideoPalCommand, VideoNtscCommand,
                       ntsc ? VideoNtscCommand : VideoPalCommand, MF_BYCOMMAND);
}

// Switching the video standard is like switching on another machine: it restarts.
void selectVideoStandard(AppState& app, svi728::VideoStandard standard)
{
    if (app.machine.videoStandard() == standard || !confirmCassetteDiscard(app)) return;
    rebuildMachine(app, standard);
    resetMachine(app);
    app.fieldPhase = 0;
    app.audioSamplePhase = 0;
    updateVideoMenu(app);
    updateCassetteMenu(app);
    updateWindowTitle(app);
    saveMedia(app);
}

std::filesystem::path executableDirectory();
HMENU buildMenu(const AppState& app);

// Name of a module mapper in the frontend language.
std::wstring mapperText(cartridge::Mapper mapper)
{
    if (mapper == cartridge::Mapper::None) return ui(L"no mapper", L"ohne Mapper");
    const auto name = cartridge::mapperName(mapper);
    return std::wstring(name.begin(), name.end());
}

// Folder that holds the project's ROM folders: the Development folder of the project when the program
// runs from its Build folder (or from Build\x64\<Configuration>), otherwise the program folder, where
// the setup places the ROMs.
std::filesystem::path sourceDirectory()
{
    std::error_code error;
    auto folder = executableDirectory();
    for (int level = 0; level < 3 && !folder.empty(); ++level, folder = folder.parent_path()) {
        const auto development = folder.parent_path() / L"Development";
        if (std::filesystem::is_directory(development, error)) return development;
    }
    return executableDirectory();
}

// Media folder "2100 - MSX - MEDIEN" of the development tree, in the program folder or above it;
// empty when the program runs elsewhere.
std::filesystem::path developmentMediaRoot()
{
    std::error_code error;
    for (auto folder = executableDirectory(); !folder.empty(); folder = folder.parent_path()) {
        const auto candidate = folder / L"2100 - MSX - MEDIEN";
        if (std::filesystem::is_directory(candidate, error)) return candidate;
        if (folder == folder.parent_path()) break;
    }
    return {};
}

// Folder of this machine in the user's documents, for example Documents\SVI-728.
std::filesystem::path userDataFolder()
{
    std::array<wchar_t, MAX_PATH> documents{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents.data()))) return {};
    return std::filesystem::path(documents.data()) / L"SVI-728";
}

// Media folder <name>: in the development tree "2100 - MSX - MEDIEN\<name>", otherwise
// Documents\SVI-728\<name> (CAS, CARD, ROM, STATE, PRINT, VIDEO), created when missing. Empty when the folder does not exist.
std::filesystem::path mediaFolder(const wchar_t* name)
{
    std::error_code error;
    const auto development = developmentMediaRoot();
    std::filesystem::path folder;
    if (!development.empty()) {
        folder = development / name;
        if (std::wcscmp(name, L"STATE") == 0 || std::wcscmp(name, L"VIDEO") == 0) {
            std::filesystem::create_directories(folder, error);
        }
    } else {
        const auto user = userDataFolder();
        if (user.empty()) return {};
        folder = user / name;
        std::filesystem::create_directories(folder, error);
    }
    return std::filesystem::is_directory(folder, error) ? folder : std::filesystem::path{};
}

// A ROM image by its file name and size: first the rom folder of its component in the development
// tree, then the ROM media folder, then the program folder. Empty when no folder holds the file with
// that size.
std::filesystem::path findRom(const std::filesystem::path& componentFolder, const wchar_t* fileName, std::uintmax_t size)
{
    std::error_code error;
    for (const auto& folder : {componentFolder, mediaFolder(L"ROM"), executableDirectory()}) {
        if (folder.empty()) continue;
        const auto candidate = folder / fileName;
        if (std::filesystem::is_regular_file(candidate, error) && std::filesystem::file_size(candidate, error) == size) {
            return candidate;
        }
    }
    return {};
}

// Outside the development tree the program creates its folders in the user's documents.
void createUserFolders()
{
    if (!developmentMediaRoot().empty()) return;
    for (const wchar_t* name : {L"CAS", L"CARD", L"ROM", L"STATE", L"PRINT", L"VIDEO"}) (void)mediaFolder(name);
}

// ? menu: MANUAL.md next to the program (installed) or in the Distribution folder of the project
// (program in Build or Build\x64\<Configuration>) as an HTML page in the default browser.
void showManual(HWND window)
{
    std::filesystem::path manual;
    auto folder = executableDirectory();
    for (int level = 0; level < 4 && !folder.empty() && manual.empty(); ++level, folder = folder.parent_path()) {
        for (const auto& candidate : {folder / L"MANUAL.md", folder / L"Distribution" / L"MANUAL.md"}) {
            if (std::filesystem::exists(candidate)) {
                manual = candidate;
                break;
            }
        }
    }
    if (manual.empty()) {
        MessageBoxW(window, ui(L"MANUAL.md was not found next to the program.", L"MANUAL.md wurde neben dem Programm nicht gefunden."), WindowTitle,
                    MB_OK | MB_ICONWARNING);
        return;
    }
    std::ifstream input(manual, std::ios::binary);
    std::string markdown{std::istreambuf_iterator<char>(input), {}};
    if (markdown.rfind("\xEF\xBB\xBF", 0) == 0) markdown.erase(0, 3);
    std::string baseUrl = "file:///";
    for (const char c : manual.parent_path().generic_u8string()) {
        if (c == ' ') baseUrl += "%20";
        else if (c == '%') baseUrl += "%25";
        else if (c == '#') baseUrl += "%23";
        else baseUrl += c;
    }
    baseUrl += '/';
    std::array<wchar_t, MAX_PATH + 1> temp{};
    GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    const auto page = std::filesystem::path(temp.data()) / L"SVI728-Manual.html";
    std::ofstream output(page, std::ios::binary | std::ios::trunc);
    output << svi728::manualToHtml(markdown, baseUrl);
    output.close();
    if (!output) {
        MessageBoxW(window, ui(L"The manual page could not be written.", L"Die Handbuchseite konnte nicht geschrieben werden."), WindowTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(window, L"open", page.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        MessageBoxW(window, ui(L"The default browser could not be started.", L"Der Standardbrowser konnte nicht gestartet werden."), WindowTitle,
                    MB_OK | MB_ICONWARNING);
    }
}

// Restores a snapshot; a snapshot of the other video standard first switches the machine over.
bool loadStateFile(AppState& app, const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    const std::vector<std::uint8_t> state{std::istreambuf_iterator<char>(file), {}};
    const auto standard = svi728::SVI728::stateVideoStandard(state);
    if (!standard) {
        MessageBoxW(app.window, ui(L"The file is not an SVI-728 state.", L"Die Datei ist kein SVI-728-Zustand."), ui(L"Load state", L"Zustand laden"), MB_OK | MB_ICONERROR);
        return false;
    }
    if (app.machine.videoStandard() != *standard) {
        rebuildMachine(app, *standard);
        resetMachine(app);
        updateVideoMenu(app);
        updateCassetteMenu(app);
        saveMedia(app);
    }
    if (!app.machine.loadState(state)) {
        MessageBoxW(app.window, ui(L"The state does not fit this emulator (other version) or the inserted modules.", L"Der Zustand passt nicht zu diesem Emulator (andere Version) oder nicht zu den "
                    L"eingelegten Modulen."), ui(L"Load state", L"Zustand laden"), MB_OK | MB_ICONERROR);
        return false;
    }
    app.fieldPhase = 0;
    app.audioSamplePhase = 0;
    updateWindowTitle(app);
    return true;
}

// Snapshots live in the snapshot media folder.
void selectStateFile(AppState& app, bool save)
{
    KillTimer(app.window, EmulationTimer);
    const std::wstring folder = mediaFolder(L"STATE").wstring();
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI-728 states (*.sst)\0*.sst\0All files (*.*)\0*.*\0\0", L"SVI-728-Zustaende (*.sst)\0*.sst\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = save ? ui(L"Save state", L"Zustand speichern") : ui(L"Load state", L"Zustand laden");
    dialog.lpstrDefExt = L"sst";
    dialog.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        if (!save) {
            (void)loadStateFile(app, path);
        } else {
            const auto state = app.machine.saveState();
            if (!writeSettingsAtomically(path, std::string(state.begin(), state.end()))) {
                MessageBoxW(app.window, ui(L"The state could not be saved.", L"Der Zustand konnte nicht gespeichert werden."), ui(L"Save state", L"Zustand speichern"),
                            MB_OK | MB_ICONERROR);
            }
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"State", L"Zustand"),
                    MB_OK | MB_ICONERROR);
    }
    refreshMenu(app);
    resumeAfterCassetteDialog(app);
}

void updateVideoRecordMenu(AppState& app)
{
    const auto menu = GetMenu(app.window);
    EnableMenuItem(menu, VideoRecordCommand, MF_BYCOMMAND | (app.videoRecording ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, VideoStopCommand, MF_BYCOMMAND | (app.videoRecording ? MF_ENABLED : MF_GRAYED));
    DrawMenuBar(app.window);
}

// Records picture and sound of every field into an MP4 file (video media folder) until stopped.
void startVideoRecording(AppState& app)
{
    if (app.videoRecording) return;
    KillTimer(app.window, EmulationTimer);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    // The suggested name carries the folder; otherwise Windows opens the folder used last.
    const std::wstring folder = mediaFolder(L"VIDEO").wstring();
    std::vector<wchar_t> fileName(32768, L'\0');
    swprintf(fileName.data(), fileName.size(), L"%ls%lsSVI728_%04u%02u%02u_%02u%02u%02u.mp4", folder.c_str(),
             folder.empty() ? L"" : L"\\", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"MP4 video (*.mp4)\0*.mp4\0All files (*.*)\0*.*\0\0", L"MP4-Video (*.mp4)\0*.mp4\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = ui(L"Record video", L"Video aufnehmen");
    dialog.lpstrDefExt = L"mp4";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&dialog)) {
        videorec::Settings settings;
        settings.path = fileName.data();
        settings.width = ClientWidth;
        settings.height = ClientHeight;
        settings.frameRateNumerator = svi728::VdpMasterClockHz;
        settings.frameRateDenominator = app.machine.vdpMasterTicksPerField();
        settings.sampleRate = AudioSampleRate;
        std::wstring error;
        if (app.recorder.start(settings, error)) {
            app.videoRecording = true;
        } else {
            MessageBoxW(app.window, (ui(L"The video recording could not be started.\n\n", L"Die Videoaufnahme konnte nicht gestartet werden.\n\n") + error).c_str(),
                        ui(L"Record video", L"Video aufnehmen"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"Record video", L"Video aufnehmen"),
                    MB_OK | MB_ICONERROR);
    }
    updateVideoRecordMenu(app);
    updateWindowTitle(app);
    resumeAfterCassetteDialog(app);
}

// Finishes the MP4 file; reports when writing failed.
void stopVideoRecording(AppState& app)
{
    if (!app.videoRecording) return;
    app.videoRecording = false;
    std::wstring error;
    if (!app.recorder.stop(error)) {
        MessageBoxW(app.window, (ui(L"The video could not be written completely.\n\n", L"Das Video konnte nicht vollstaendig geschrieben werden.\n\n") + error).c_str(),
                    ui(L"Record video", L"Video aufnehmen"), MB_OK | MB_ICONERROR);
    }
    updateVideoRecordMenu(app);
    updateWindowTitle(app);
}

void selectPrinterDirectory(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        MessageBoxW(app.window, ui(L"The folder dialog could not be initialised.", L"Der Ordnerdialog konnte nicht initialisiert werden."),
                    ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
        resumeAfterCassetteDialog(app);
        return;
    }

    IFileOpenDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(result)) {
        DWORD options = 0;
        result = dialog->GetOptions(&options);
        if (SUCCEEDED(result)) {
            result = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(result)) result = dialog->SetTitle(ui(L"Choose the folder for printer output", L"Ablageordner fuer Druckdateien waehlen"));
        if (SUCCEEDED(result) && !app.printerDirectory.empty()) {
            IShellItem* initialFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(app.printerDirectory.c_str(), nullptr,
                                                      IID_PPV_ARGS(&initialFolder)))) {
                (void)dialog->SetFolder(initialFolder);
                initialFolder->Release();
            }
        }
        if (SUCCEEDED(result)) result = dialog->Show(app.window);

        if (SUCCEEDED(result)) {
            IShellItem* selectedFolder = nullptr;
            result = dialog->GetResult(&selectedFolder);
            if (SUCCEEDED(result)) {
                PWSTR selectedPath = nullptr;
                result = selectedFolder->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
                if (SUCCEEDED(result)) {
                    app.printerDirectory = selectedPath;
                    CoTaskMemFree(selectedPath);
                    app.printerOutputPaused = false;
                    app.printerErrorShown = false;
                    app.printerWarningPending = false;
                    saveMedia(app);
                }
                selectedFolder->Release();
            }
        } else if (result != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            MessageBoxW(app.window, ui(L"The printer output folder could not be chosen.", L"Der Drucker-Ablageordner konnte nicht ausgewaehlt werden."),
                        ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
        }
        dialog->Release();
    } else {
        MessageBoxW(app.window, ui(L"The printer folder dialog could not be opened.", L"Der Drucker-Ordnerdialog konnte nicht geoeffnet werden."),
                    ui(L"Printer", L"Drucker"), MB_OK | MB_ICONERROR);
    }

    CoUninitialize();
    resumeAfterCassetteDialog(app);
}

void paint(AppState& app, HWND window)
{
    PAINTSTRUCT paintState{};
    const HDC dc = BeginPaint(window, &paintState);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = ClientWidth;
    info.bmiHeader.biHeight = -ClientHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, ClientWidth, ClientHeight,
                  0, 0, ClientWidth, ClientHeight,
                  app.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    EndPaint(window, &paintState);
}

void updateJoystickMenu(AppState& app)
{
    CheckMenuItem(GetMenu(app.window), KeyboardJoystickToggleCommand,
                  MF_BYCOMMAND | (app.keyboardJoystickOn ? MF_CHECKED : MF_UNCHECKED));
    for (UINT port = 0; port < 2; ++port) {
        const UINT first = JoystickCommandBase + 10 * port;
        CheckMenuRadioItem(GetMenu(app.window), first, first + JoystickSourceCount - 1,
                           first + static_cast<UINT>(app.joystickSources[port]), MF_BYCOMMAND);
    }
}

void toggleKeyboardJoystick(AppState& app)
{
    releaseAllInput(app);
    app.keyboardJoystickOn = !app.keyboardJoystickOn;
    updateJoysticks(app);
    updateJoystickMenu(app);
    updateWindowTitle(app);
}

void selectJoystickSource(AppState& app, std::size_t port, int source)
{
    releaseAllInput(app);
    app.joystickSources[port] = source;
    if (source == JoystickController) probeController(app, port);
    updateJoysticks(app);
    updateJoystickMenu(app);
    updateWindowTitle(app);
    saveMedia(app);
}

// Builds the menu bar again (language, module contents) and sets all checks and enabled states.
void refreshMenu(AppState& app)
{
    const HMENU previous = GetMenu(app.window);
    const HMENU menu = buildMenu(app);
    if (!menu) return;
    SetMenu(app.window, menu);
    if (previous) DestroyMenu(previous);
    updateCassetteMenu(app);
    updateVideoMenu(app);
    updateJoystickMenu(app);
    updateVideoRecordMenu(app);
    updateWindowTitle(app);
}

// Switches the frontend language: rebuilds the menu bar, refreshes menus and status bar and keeps the
// choice in the settings.
void setLanguage(AppState& app, UiLanguage language)
{
    if (uiLanguage() == language) return;
    uiLanguage() = language;
    refreshMenu(app);
    saveMedia(app);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        app = static_cast<AppState*>(create->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    switch (message) {
    case WM_COMMAND:
        if (!app) break;
        switch (LOWORD(wParam)) {
        case JoystickCommandBase + JoystickOff:
        case JoystickCommandBase + JoystickKeyboard:
        case JoystickCommandBase + JoystickController:
        case JoystickCommandBase + 10 + JoystickOff:
        case JoystickCommandBase + 10 + JoystickKeyboard:
        case JoystickCommandBase + 10 + JoystickController:
            selectJoystickSource(*app, (LOWORD(wParam) - JoystickCommandBase) / 10,
                                 static_cast<int>((LOWORD(wParam) - JoystickCommandBase) % 10));
            return 0;
        case KeyboardJoystickToggleCommand:
            toggleKeyboardJoystick(*app);
            return 0;
        case CassetteOpenCommand:
            selectCassetteImage(*app);
            return 0;
        case CassetteRewindCommand:
            app->machine.rewindCassette();
            return 0;
        case CassetteEjectCommand:
            if (!confirmCassetteDiscard(*app)) return 0;
            app->machine.ejectCassette();
            app->cassettePath.clear();
            updateCassetteMenu(*app);
            return 0;
        case CassetteRecordCommand:
            if (!confirmCassetteDiscard(*app)) return 0;
            app->machine.startCassetteRecording();
            app->cassettePath.clear();
            updateCassetteMenu(*app);
            return 0;
        case CassetteSaveCommand:
            saveCassetteImage(*app);
            return 0;
        case PrinterDirectoryCommand:
            selectPrinterDirectory(*app);
            return 0;
        case VideoPalCommand:
        case VideoNtscCommand:
            selectVideoStandard(*app, LOWORD(wParam) == VideoNtscCommand ? svi728::VideoStandard::Ntsc
                                                                         : svi728::VideoStandard::Pal);
            return 0;
        case BridgeProbeCommand:
            reprobeBoard(*app);
            return 0;
        case CardClockCommand:
            if (!app->cardClock && !askCardClockAddress(*app)) return 0;
            app->cardClock = !app->cardClock;
            CheckMenuItem(GetMenu(window), CardClockCommand, MF_BYCOMMAND | (app->cardClock ? MF_CHECKED : MF_UNCHECKED));
            saveMedia(*app);
            return 0;
        case ResetCommand:
            releaseAllInput(*app);
            resetMachine(*app);
            return 0;
        case StateLoadCommand:
        case StateSaveCommand:
            selectStateFile(*app, LOWORD(wParam) == StateSaveCommand);
            return 0;
        case VideoRecordCommand:
            startVideoRecording(*app);
            return 0;
        case VideoStopCommand:
            stopVideoRecording(*app);
            return 0;
        case LanguageEnglishCommand:
        case LanguageGermanCommand:
            setLanguage(*app, LOWORD(wParam) == LanguageGermanCommand ? UiLanguage::German : UiLanguage::English);
            return 0;
        case ManualCommand:
            showManual(window);
            return 0;
        case CartridgeCommandBase + 0: case CartridgeCommandBase + 1: case CartridgeCommandBase + 2:
        case CartridgeCommandBase + 3: case CartridgeCommandBase + 4: case CartridgeCommandBase + 5:
            selectCartridge(*app, 2, LOWORD(wParam) - CartridgeCommandBase);
            return 0;
        case CartridgeCommandBase + 10: case CartridgeCommandBase + 11: case CartridgeCommandBase + 12:
        case CartridgeCommandBase + 13: case CartridgeCommandBase + 14: case CartridgeCommandBase + 15:
            selectCartridge(*app, 3, LOWORD(wParam) - CartridgeCommandBase - 10);
            return 0;
        case CartridgeCommandBase + CartridgeEjectChoice:
            ejectCartridge(*app, 2);
            return 0;
        case CartridgeCommandBase + 10 + CartridgeEjectChoice:
            ejectCartridge(*app, 3);
            return 0;
        case CartridgeCommandBase + CartridgeBridgeChoice:
        case CartridgeCommandBase + CartridgeBridgeChoice + 1:
            selectBridgeSlot(*app, 2, static_cast<std::uint8_t>(LOWORD(wParam) - CartridgeCommandBase - CartridgeBridgeChoice + 1));
            return 0;
        case CartridgeCommandBase + 10 + CartridgeBridgeChoice:
        case CartridgeCommandBase + 10 + CartridgeBridgeChoice + 1:
            selectBridgeSlot(*app, 3, static_cast<std::uint8_t>(LOWORD(wParam) - CartridgeCommandBase - 10 - CartridgeBridgeChoice + 1));
            return 0;
        default: break;
        }
        break;
    case WM_KEYDOWN:
        if (app && wParam == VK_F11) {
            if ((static_cast<unsigned long>(lParam) & (1ul << 30)) != 0) return 0;
            toggleKeyboardJoystick(*app);
            return 0;
        }
        if (app && wParam == VK_F12 &&
            (static_cast<unsigned long>(lParam) & (1ul << 30)) == 0) {
            startAyAudioTest(*app);
            return 0;
        }
        if (app && wParam < app->keyDown.size()) updateInput(*app, wParam, lParam, true);
        return 0;
    case WM_KEYUP:
        if (app && wParam < app->keyDown.size()) updateInput(*app, wParam, lParam, false);
        return 0;
    case WM_SYSKEYDOWN:
        if (app && wParam == VK_F11) {
            if ((static_cast<unsigned long>(lParam) & (1ul << 30)) != 0) return 0;
            toggleKeyboardJoystick(*app);
            return 0;
        }
        if (app && wParam == VK_F12 &&
            (static_cast<unsigned long>(lParam) & (1ul << 30)) == 0) {
            startAyAudioTest(*app);
            return 0;
        }
        if (app && wParam < app->keyDown.size()) updateInput(*app, wParam, lParam, true);
        return 0;
    case WM_SYSKEYUP:
        if (app && wParam < app->keyDown.size()) updateInput(*app, wParam, lParam, false);
        return 0;
    case WM_KILLFOCUS:
        if (app) releaseAllInput(*app);
        return 0;
    case WM_TIMER:
        if (app && wParam == EmulationTimer) {
            const bool hadAudio = app->audio.device != nullptr;
            (void)retryAudio(app->audio);
            if (app->bridge) {
                app->bridge->heartbeat();
                // The board switched the cartridges off while the emulation stood still (menu, dialog):
                // they lost their state, so the machine starts again as after switching it on.
                if (app->bridge->takeSwitchedOff()) resetMachine(*app);
            }
            const bool advanced = advanceWallClock(*app);
            if (app->printerWarningPending) {
                app->printerWarningPending = false;
                app->printerErrorShown = true;
                MessageBoxW(app->window,
                    ui(L"Please choose an output folder in the Printer menu first. Printer output stays buffered until then.", L"Bitte zuerst im Menue Drucker einen Ablageordner auswaehlen. "
                    L"Die Druckausgabe bleibt bis dahin gepuffert."),
                    ui(L"Printer", L"Drucker"), MB_OK | MB_ICONWARNING);
            }
            if ((app->audio.device != nullptr) != hadAudio) {
                updateCassetteMenu(*app);
            } else if (GetTickCount64() - app->titleTick >= TitleRefreshIntervalMs) {
                updateCassetteMenu(*app);
            }
            if (app->videoRecording && !app->recorder.recording()) stopVideoRecording(*app);
            if (advanced) {
                updateDisplayPixels(*app, app->machine.video().scanoutFrame());
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        return 0;
    case WM_SIZE:
        if (app && app->statusBar) SendMessageW(app->statusBar, WM_SIZE, 0, 0);
        break;
    case WM_PAINT:
        if (app) paint(*app, window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        if (app && !confirmCassetteDiscard(*app)) return 0;
        if (app) stopVideoRecording(*app);
        break;
    case WM_DESTROY:
        KillTimer(window, EmulationTimer);
        if (app) {
            closePrinterFile(*app);
            saveMedia(*app);
            stopAudio(app->audio);
        }
        if (app && app->timerResolutionRaised) timeEndPeriod(1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool loadMainRom(AppState& app, const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    const std::istreambuf_iterator<char> romEnd{};
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>{file}, romEnd};
    if (image.size() != svi728::MainRomSize || !app.machine.loadMainRom(image)) return false;
    app.romImage = image;
    return true;
}

std::filesystem::path executableDirectory()
{
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

// Menu text with "&" shown as such (file names).
std::wstring literalMenuText(const std::wstring& text)
{
    std::wstring result;
    for (const auto c : text) {
        if (c == L'&') result += L'&';
        result += c;
    }
    return result;
}

// What a module slot holds, for the title of its submenu.
std::wstring slotContents(const AppState& app, std::uint8_t slot)
{
    if (const auto boardSlot = app.machine.bridgeSlot(slot)) {
        const auto& found = app.boardContents[boardSlot - 1];
        return L"USB2SLOT " + std::to_wstring(boardSlot) + (found.empty() ? L"" : L" - " + found);
    }
    if (app.machine.cartridgeSlot(slot).inserted()) {
        return app.cartridgePaths[slot - 2].empty() ? ui(L"module", L"Modul")
                                                    : literalMenuText(app.cartridgePaths[slot - 2].filename().wstring());
    }
    return ui(L"empty", L"leer");
}

// Submenu of a module slot: ROM file, slots of the USB2SLOT board (checked when in use here, grayed when the
// board did not answer, the slot is empty or another module slot uses it), remove.
HMENU buildSlotMenu(const AppState& app, std::uint8_t slot, std::uint8_t otherSlot, UINT base)
{
    const HMENU slotMenu = CreatePopupMenu();
    const HMENU mapperMenu = CreatePopupMenu();
    for (std::size_t choice = 0; choice < CartridgeMappers.size(); ++choice) {
        AppendMenuW(mapperMenu, MF_STRING, base + 1u + static_cast<UINT>(choice), mapperText(CartridgeMappers[choice]).c_str());
    }
    AppendMenuW(slotMenu, MF_STRING, base, ui(L"&Insert ROM file...", L"ROM-Datei &einlegen..."));
    AppendMenuW(slotMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(mapperMenu), ui(L"Insert ROM file &as", L"ROM-Datei einlegen &als"));
    AppendMenuW(slotMenu, MF_SEPARATOR, 0, nullptr);
    for (std::uint8_t boardSlot = 1; boardSlot <= 2; ++boardSlot) {
        const auto& found = app.boardContents[boardSlot - 1];
        const bool usedHere = app.machine.bridgeSlot(slot) == boardSlot;
        const bool usedThere = app.machine.bridgeSlot(otherSlot) == boardSlot;
        std::wstring text = L"USB2SLOT &" + std::to_wstring(boardSlot) + L": ";
        if (!app.boardProbed) {
            text += ui(L"no board", L"keine Platine");
        } else {
            text += found.empty() ? ui(L"empty", L"leer") : found;
        }
        if (usedThere) text += ui(L" (in slot ", L" (in Slot ") + std::to_wstring(otherSlot) + L")";
        const bool available = app.boardProbed && !found.empty() && !usedThere;
        AppendMenuW(slotMenu, MF_STRING | (usedHere ? MF_CHECKED : 0u) | (available || usedHere ? 0u : MF_GRAYED),
                    base + CartridgeBridgeChoice + boardSlot - 1, text.c_str());
    }
    AppendMenuW(slotMenu, MF_SEPARATOR, 0, nullptr);
    const bool occupied = app.machine.cartridgeSlot(slot).inserted() || app.machine.bridgeSlot(slot) != 0;
    AppendMenuW(slotMenu, MF_STRING | (occupied ? 0u : MF_GRAYED), base + CartridgeEjectChoice, ui(L"&Remove", L"E&ntfernen"));
    return slotMenu;
}

// Menu bar in the frontend language, with the current contents of the module slots.
HMENU buildMenu(const AppState& app)
{
    const HMENU menu = CreateMenu();
    const HMENU systemMenu = CreatePopupMenu();
    const HMENU settingsMenu = CreatePopupMenu();
    const HMENU cassetteMenu = CreatePopupMenu();
    const HMENU printerMenu = CreatePopupMenu();
    if (!menu || !systemMenu || !settingsMenu || !cassetteMenu || !printerMenu) {
        if (menu) DestroyMenu(menu);
        if (systemMenu) DestroyMenu(systemMenu);
        if (settingsMenu) DestroyMenu(settingsMenu);
        if (cassetteMenu) DestroyMenu(cassetteMenu);
        if (printerMenu) DestroyMenu(printerMenu);
        return nullptr;
    }
    AppendMenuW(systemMenu, MF_STRING, ResetCommand, L"&Reset");
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, StateLoadCommand, ui(L"&Load state...", L"Zustand &laden..."));
    AppendMenuW(systemMenu, MF_STRING, StateSaveCommand, ui(L"&Save state...", L"Zustand &speichern..."));
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, VideoRecordCommand, ui(L"Record &video...", L"&Video aufnehmen..."));
    AppendMenuW(systemMenu, MF_STRING | MF_GRAYED, VideoStopCommand, ui(L"S&top video", L"Video &beenden"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(systemMenu), L"&System");
    AppendMenuW(settingsMenu, MF_STRING, VideoPalCommand, L"&PAL (TMS9929A, 50 Hz)");
    AppendMenuW(settingsMenu, MF_STRING, VideoNtscCommand, L"&NTSC (TMS9918A, 60 Hz)");
    AppendMenuW(settingsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settingsMenu, MF_STRING, LanguageEnglishCommand, L"&English");
    AppendMenuW(settingsMenu, MF_STRING, LanguageGermanCommand, L"&Deutsch");
    CheckMenuRadioItem(settingsMenu, LanguageEnglishCommand, LanguageGermanCommand,
                       uiLanguage() == UiLanguage::German ? LanguageGermanCommand : LanguageEnglishCommand, MF_BYCOMMAND);
    AppendMenuW(settingsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settingsMenu, MF_STRING | (app.cardClock ? MF_CHECKED : 0u), CardClockCommand,
                ui(L"Set &clock of card A at start", L"&Uhr der Karte A beim Start stellen"));
    const HMENU joystickMenu = CreatePopupMenu();
    for (UINT port = 0; port < 2; ++port) {
        const HMENU portMenu = CreatePopupMenu();
        const UINT first = JoystickCommandBase + 10 * port;
        AppendMenuW(portMenu, MF_STRING, first + JoystickOff, ui(L"&off", L"&aus"));
        AppendMenuW(portMenu, MF_STRING, first + JoystickKeyboard,
                    port == 0 ? ui(L"&Keyboard (arrows + C/V)", L"&Tastatur (Pfeile + C/V)") : ui(L"&Keyboard (W A S D + Z/X)", L"&Tastatur (W A S D + Y/X)"));
        AppendMenuW(portMenu, MF_STRING, first + JoystickController,
                    port == 0 ? ui(L"PC &game controller 1", L"PC-&Gamecontroller 1") : ui(L"PC &game controller 2", L"PC-&Gamecontroller 2"));
        AppendMenuW(joystickMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(portMenu),
                    port == 0 ? ui(L"Port &1", L"Anschluss &1") : ui(L"Port &2", L"Anschluss &2"));
    }
    AppendMenuW(joystickMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(joystickMenu, MF_STRING, KeyboardJoystickToggleCommand, ui(L"Keyboard joystick &active\tF11", L"Tastatur-Joystick &aktiv\tF11"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(joystickMenu), L"&Joystick");
    AppendMenuW(cassetteMenu, MF_STRING, CassetteOpenCommand, ui(L"&Open image...", L"Image &oeffnen..."));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteRewindCommand, ui(L"&Rewind", L"&Zurueckspulen"));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteEjectCommand, ui(L"&Eject", L"&Auswerfen"));
    AppendMenuW(cassetteMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(cassetteMenu, MF_STRING, CassetteRecordCommand, ui(L"&New recording", L"Neue &Aufnahme"));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteSaveCommand,
                ui(L"&Stop recording and save...", L"Aufnahme &beenden und speichern..."));
    const HMENU cartridgeMenu = CreatePopupMenu();
    for (std::uint8_t slot = 2; slot <= 3; ++slot) {
        const UINT base = CartridgeCommandBase + 10u * (slot - 2u);
        const HMENU slotMenu = buildSlotMenu(app, slot, slot == 2 ? 3 : 2, base);
        const std::wstring title = L"Slot &" + std::to_wstring(slot) + L": " + slotContents(app, slot);
        AppendMenuW(cartridgeMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(slotMenu), title.c_str());
    }
    AppendMenuW(cartridgeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(cartridgeMenu, MF_STRING, BridgeProbeCommand, ui(L"&Detect USB2SLOT again", L"USB2SLOT neu &erkennen"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(cartridgeMenu), ui(L"&Modules", L"&Module"));
    AppendMenuW(printerMenu, MF_STRING, PrinterDirectoryCommand, ui(L"Output &folder...", L"Ablage&ordner waehlen..."));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(printerMenu), ui(L"&Printer", L"&Drucker"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(cassetteMenu), ui(L"&Cassette", L"&Kassette"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(settingsMenu), ui(L"S&ettings", L"&Einstellungen"));
    AppendMenuW(menu, MF_STRING | MF_RIGHTJUSTIFY, ManualCommand, L"&?");
    return menu;
}

// Language from the settings file ("language=de" for German), read before the menu is built.
void loadLanguage(const std::filesystem::path& settingsPath)
{
    std::ifstream file(settingsPath);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "language=de") uiLanguage() = UiLanguage::German;
        if (line == "language=en") uiLanguage() = UiLanguage::English;
    }
}

} // namespace

// The window shows the picture pixel for pixel: the program tells Windows that it takes the display
// scaling into account itself, so that Windows does not stretch the window (blurred) at 125 % or 150 %.
void declareDpiAware()
{
    using SetContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (const auto setContext = reinterpret_cast<SetContext>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
            if (setContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) return;
        }
    }
    SetProcessDPIAware();
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    declareDpiAware();
    AppState app;
    createUserFolders();
    app.soundPortTransitions.reserve(16);
    if (!QueryPerformanceFrequency(&app.counterFrequency) ||
        !QueryPerformanceCounter(&app.lastCounter)) {
        MessageBoxW(nullptr, ui(L"The high-resolution system clock could not be initialised.", L"Die hochauflösende Systemuhr konnte nicht initialisiert werden."),
                    L"SVI-728", MB_OK | MB_ICONERROR);
        return 1;
    }
    const auto appDirectory = executableDirectory();
    // svi728.exe [rom] [-state file.sst]
    std::filesystem::path romArgument;
    std::filesystem::path stateArgument;
    int argumentCount = 0;
    if (LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) {
            if (_wcsicmp(arguments[index], L"-state") == 0 && index + 1 < argumentCount) {
                stateArgument = arguments[++index];
            } else {
                romArgument = arguments[index];
            }
        }
        LocalFree(arguments);
    }
    loadLanguage(appDirectory / SettingsFileName);
    const std::filesystem::path romPath = !romArgument.empty()
        ? std::filesystem::path(romArgument)
        : findRom(sourceDirectory() / L"SVI728" / L"rom", L"SVI-MSX-BIOS+Basic.bin", svi728::MainRomSize);
    if (romPath.empty() || !loadMainRom(app, romPath)) {
        std::wstring text = ui(L"The SVI-728 ROM (SVI-MSX-BIOS+Basic.bin, 32 KiB) is missing. Place it in the ROM folder and start the emulator again:\n\n",
                               L"Das SVI-728-ROM (SVI-MSX-BIOS+Basic.bin, 32 KiB) fehlt. Legen Sie es in den ROM-Ordner und starten Sie den Emulator neu:\n\n");
        text += mediaFolder(L"ROM").wstring();
        MessageBoxW(nullptr, text.c_str(), L"SVI-728", MB_OK | MB_ICONERROR);
        return 1;
    }
    app.settingsPath = appDirectory / SettingsFileName;
    loadMedia(app);
    if (!app.boardProbed) (void)probeBoard(app, false);
    if (app.cardClock && app.boardProbed) setCardClockToPc(app);
    if (app.printerDirectory.empty() && developmentMediaRoot().empty()) app.printerDirectory = mediaFolder(L"PRINT");
    resetMachine(app);

    const wchar_t* className = L"SVI728KeyboardWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass)) return 1;

    RECT rect{0, 0, ClientWidth, ClientHeight};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    loadLanguage(app.settingsPath);
    const HMENU menu = buildMenu(app);
    if (!menu) return 1;
    AdjustWindowRect(&rect, style, TRUE);
    const HWND window = CreateWindowExW(0, className,
        WindowTitle,
        style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, menu, instance, &app);
    if (!window) {
        DestroyMenu(menu);
        return 1;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    app.statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE,
                                    0, 0, 0, 0, window, nullptr, instance, nullptr);
    if (app.statusBar) {
        RECT statusRect{};
        GetWindowRect(app.statusBar, &statusRect);
        RECT frame{0, 0, ClientWidth, ClientHeight + (statusRect.bottom - statusRect.top)};
        AdjustWindowRect(&frame, style, TRUE);
        SetWindowPos(window, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    app.speedStart = app.lastCounter;

    if (!startAudio(app.audio)) {
        app.audio.nextRetryTick = GetTickCount64() + AudioRetryIntervalMs;
    }
    updateCassetteMenu(app);
    updateVideoMenu(app);
    updateWindowTitle(app);
    updateJoystickMenu(app);
    updateWindowTitle(app);
    ShowWindow(window, showCommand);
    if (!stateArgument.empty()) (void)loadStateFile(app, stateArgument);
    app.timerResolutionRaised = timeBeginPeriod(1) == TIMERR_NOERROR;
    SetTimer(window, EmulationTimer, TimerPeriodMs, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
#endif