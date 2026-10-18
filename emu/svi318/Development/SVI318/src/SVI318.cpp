#include "SVI318/SVI318.hpp"

#include "PPI8255/PPI8255.hpp"
#include "TMS9918A/TMS9918A.hpp"
#include "TMS9929A/TMS9929A.hpp"

#if defined(SVI318_GUI_APP)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <commdlg.h>

#include "SVI318/Keyboard.hpp"
#include "SVI318/ManualPage.hpp"
#include "SVI318/UiText.hpp"
#include "LANIF01/LANIF01.hpp"
#include "LANIF01/LanCard.hpp"
#include "SVI608M/SVI608M.hpp"
#include "SVI605BHD/SVI605BHD.hpp"
#include "SVI602/SVI602.hpp"
#include "SVI601/SVI601.hpp"
#include "SVI801/SVI801.hpp"
#include "SVI802/SVI802.hpp"
#include "LANHD01/LANHD01.hpp"
#include "SVI803/SVI803.hpp"
#include "SVI807/SVI807.hpp"
#include "SVI605B/SVI605B.hpp"
#include "SVI805/SVI805.hpp"
#include "SVI806/SVI806.hpp"
#include "VIDEOREC/VideoRecorder.hpp"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <filesystem>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <string>

using svi318::fillIn;
using svi318::ui;
using svi318::uiLanguage;
using svi318::UiLanguage;

#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

namespace svi318 {

static_assert(CpuClockHz == svi3x8::CpuClockHz, "CPUCLK on the expansion connector is the Z80 clock.");


namespace {

// PSG port B bits, active low. /BK21, /BK22, /BK31 and /BK32 only reach the expansion bus.
constexpr std::uint8_t BankCart = 0x01;
constexpr std::uint8_t BankRam21 = 0x02;
// The internal 16 KiB of RAM bank 2 answer at C000h-FFFFh only.
constexpr std::uint16_t InternalRamStart = 0xC000;
constexpr std::uint8_t BankRomEn0 = 0x40;
constexpr std::uint8_t BankRomEn1 = 0x80;

enum class LowBank { Rom, Cartridge };

} // namespace

struct SVI318::Impl {
    z80bus::Z80Bus cpu;
    ppi8255::PPI8255 ppi;
    VideoStandard standard;
    std::unique_ptr<tms99x8::TMS99X8> vdp;
    ay8910::AY8910 psg{AyMasterClockHz};
    Cassette cassette;
    std::array<std::uint8_t, RomSize> rom{};
    std::array<std::uint8_t, InternalRamSize> ram2{};
    std::vector<std::uint8_t> cartridge;
    std::array<std::uint8_t, KeyboardRowCount> keyboardRows{};
    std::uint8_t joystick1 = 0xFF;
    std::uint8_t joystick2 = 0xFF;
    z80bus::Outputs cpuPins{};
    svi3x8::ExpansionPort* expansion = nullptr;

    explicit Impl(VideoStandard videoStandard)
        : standard(videoStandard),
          vdp(videoStandard == VideoStandard::Ntsc
                  ? std::unique_ptr<tms99x8::TMS99X8>(std::make_unique<tms9918a::TMS9918A>())
                  : std::unique_ptr<tms99x8::TMS99X8>(std::make_unique<tms9929a::TMS9929A>()))
    {
        rom.fill(0xFF);
        keyboardRows.fill(0xFF);
        refreshPeripheralInputs();
    }

    [[nodiscard]] std::uint8_t bankRegister() const noexcept
    {
        const auto ports = psg.ports();
        return ports.portBDriveMask != 0 ? ports.portB : static_cast<std::uint8_t>(0xFF);
    }

    // /CART or /BK21 alone selects its bank; none or both leave the BASIC ROM.
    [[nodiscard]] LowBank lowBank() const noexcept
    {
        switch (static_cast<std::uint8_t>(~bankRegister() & (BankCart | BankRam21))) {
        case BankCart: return LowBank::Cartridge;
        default: return LowBank::Rom;
        }
    }

    // With /CART selected, /ROMEN0 maps cartridge bank 12 into 8000h-BFFFh and /ROMEN1 into C000h-FFFFh.
    [[nodiscard]] bool cartridgeHigh(std::uint16_t address) const noexcept
    {
        const auto romEnable = address < 0xC000 ? BankRomEn0 : BankRomEn1;
        return lowBank() == LowBank::Cartridge && (~bankRegister() & romEnable) != 0;
    }

    [[nodiscard]] bool cartridgeCycle(std::uint16_t address) const noexcept
    {
        return address < BankSize ? lowBank() == LowBank::Cartridge : cartridgeHigh(address);
    }

    // The internal BASIC ROM and the 16 KiB of RAM bank 2 at C000h-FFFFh answer unless an expansion
    // device asserts /ROMDIS or /RAMDIS; a selected bank that no device provides leaves them visible.
    // 8000h-BFFFh has no internal memory: expansion data counts there (FFh on an empty bus).
    [[nodiscard]] std::uint8_t readMemory(std::uint16_t address) const noexcept
    {
        if (cartridgeCycle(address)) return cartridgeByte(address);
        const auto cycle = expansion ? expansion->readMemory(address, bankRegister()) : svi3x8::ExpansionMemoryCycle{};
        if (address < BankSize) {
            return cycle.romDisable ? cycle.data : rom[address];
        }
        if (address < InternalRamStart) return cycle.data;
        return cycle.ramDisable ? cycle.data : ram2[address - InternalRamStart];
    }

    // Bank 11 holds the first 32 KiB of the image, bank 12 the second; missing bytes read FFh.
    [[nodiscard]] std::uint8_t cartridgeByte(std::uint16_t address) const noexcept
    {
        return address < cartridge.size() ? cartridge[address] : static_cast<std::uint8_t>(0xFF);
    }

    void writeMemory(std::uint16_t address, std::uint8_t value) noexcept
    {
        if (cartridgeCycle(address)) return;
        const auto cycle = expansion ? expansion->writeMemory(address, value, bankRegister()) : svi3x8::ExpansionMemoryCycle{};
        if (address < InternalRamStart) return;
        if (!cycle.ramDisable) ram2[address - InternalRamStart] = value;
    }

    [[nodiscard]] std::uint8_t joystickLines() const noexcept
    {
        return static_cast<std::uint8_t>((joystick1 & 0x0F) | ((joystick2 & 0x0F) << 4));
    }

    void refreshPeripheralInputs()
    {
        const auto ppiOutputs = ppi.outputs();
        cassette.setPins((ppiOutputs.portCDriveMask & 0x10) != 0 && (ppiOutputs.portC & 0x10) == 0,
                         (ppiOutputs.portCDriveMask & 0x20) != 0 && (ppiOutputs.portC & 0x20) != 0);
        const auto selectedRow = static_cast<std::uint8_t>(ppiOutputs.portC & 0x0F);
        ppi8255::Inputs ppiInputs;
        ppiInputs.portA = static_cast<std::uint8_t>(((joystick1 & JoystickTrigger) ? 0x10 : 0) |
                                                    ((joystick2 & JoystickTrigger) ? 0x20 : 0) |
                                                    (cassette.inputHigh() ? 0x80 : 0));
        ppiInputs.portB = selectedRow < keyboardRows.size() ? keyboardRows[selectedRow]
                                                            : static_cast<std::uint8_t>(0xFF);
        ppiInputs.portC = 0xFF;
        ppi.setInputs(ppiInputs);
        psg.setPortInputs(joystickLines(), 0xFF);
    }

    [[nodiscard]] std::uint8_t readIo(std::uint8_t port)
    {
        switch (port) {
        case 0x84: return vdp->read(0);
        case 0x85: return vdp->read(1);
        case 0x90:
            refreshPeripheralInputs();
            // The joystick contacts reach the CPU through register 14 whatever the PSG drives on
            // port A: Chess (JTM, 1986) playing black sets port A to output with 21h and still reads
            // the joystick there.
            if (psg.selectedRegister() == 14) return joystickLines();
            return psg.readAddressData();
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
            refreshPeripheralInputs();
            return ppi.read(port & 3);
        default:
            return port < 0x80 && expansion ? expansion->readIo(port) : static_cast<std::uint8_t>(0xFF);
        }
    }

    void writeIo(std::uint8_t port, std::uint8_t value)
    {
        switch (port) {
        case 0x80: vdp->write(0, value); break;
        case 0x81: vdp->write(1, value); break;
        case 0x88: psg.writeAddressData(false, value); break;
        case 0x8C: psg.writeAddressData(true, value); break;
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
            ppi.write(port & 3, value);
            refreshPeripheralInputs();
            break;
        default:
            if (port < 0x80 && expansion) expansion->writeIo(port, value);
            break;
        }
    }

    void clockCpuTState()
    {
        z80bus::Inputs inputs;
        if (cpuPins.memoryRequest && cpuPins.read) {
            inputs.dataBus = readMemory(cpuPins.address);
            inputs.driveDataBus = true;
        } else if (cpuPins.ioRequest && cpuPins.m1) {
            inputs.dataBus = 0xFF;
            inputs.driveDataBus = true;
        } else if (cpuPins.ioRequest && cpuPins.read) {
            inputs.dataBus = readIo(static_cast<std::uint8_t>(cpuPins.address));
            inputs.driveDataBus = true;
        }
        if (cpuPins.memoryRequest && cpuPins.write) {
            writeMemory(cpuPins.address, cpuPins.dataBus);
        } else if (cpuPins.ioRequest && cpuPins.write) {
            writeIo(static_cast<std::uint8_t>(cpuPins.address), cpuPins.dataBus);
        }
        inputs.intRequest = vdp->outputs().interruptRequest || (expansion && expansion->interruptRequest());
        cpuPins = cpu.tick(inputs);
        cassette.tick(CpuClockHz);
        if (expansion) expansion->tick();
    }

    [[nodiscard]] z80bus::Outputs tick()
    {
        for (std::uint8_t masterTick = 0; masterTick < VdpMasterTicksPerCpuTick; ++masterTick) {
            vdp->advanceMasterTick();
            if (masterTick + 1 == VdpMasterTicksPerCpuTick) clockCpuTState();
        }
        return cpuPins;
    }
};

SVI318::SVI318(VideoStandard standard)
    : impl(std::make_unique<Impl>(standard))
{
}

SVI318::~SVI318() = default;
SVI318::SVI318(SVI318&&) noexcept = default;
SVI318& SVI318::operator=(SVI318&&) noexcept = default;

void SVI318::reset()
{
    impl->cpu.reset();
    impl->ppi.reset();
    impl->vdp->reset();
    impl->psg.reset();
    impl->ram2.fill(0);
    impl->cpuPins = {};
    if (impl->expansion) impl->expansion->reset();
    impl->refreshPeripheralInputs();
}

void SVI318::connectExpansion(svi3x8::ExpansionPort* port) noexcept
{
    impl->expansion = port;
}

bool SVI318::loadRom(const std::vector<std::uint8_t>& image)
{
    if (image.size() > RomSize) return false;
    impl->rom.fill(0xFF);
    std::copy(image.begin(), image.end(), impl->rom.begin());
    return true;
}

bool SVI318::insertCartridge(const std::vector<std::uint8_t>& image)
{
    if (image.empty() || image.size() > CartridgeMaxSize) return false;
    impl->cartridge = image;
    return true;
}

void SVI318::ejectCartridge() noexcept
{
    impl->cartridge.clear();
}

bool SVI318::cartridgeInserted() const noexcept
{
    return !impl->cartridge.empty();
}

std::uint8_t SVI318::readMemory(std::uint16_t address) const noexcept
{
    return impl->readMemory(address);
}

void SVI318::writeMemory(std::uint16_t address, std::uint8_t value) noexcept
{
    impl->writeMemory(address, value);
}

std::uint8_t SVI318::readIo(std::uint8_t port)
{
    return impl->readIo(port);
}

void SVI318::writeIo(std::uint8_t port, std::uint8_t value)
{
    impl->writeIo(port, value);
}

z80bus::Outputs SVI318::tick()
{
    return impl->tick();
}

void SVI318::setKeyboardRow(std::uint8_t row, std::uint8_t activeLowKeys) noexcept
{
    if (row < KeyboardRowCount) impl->keyboardRows[row] = activeLowKeys;
}

void SVI318::setKeyboardKey(std::uint8_t row, std::uint8_t bit, bool pressed) noexcept
{
    if (row >= KeyboardRowCount || bit >= 8) return;
    const auto mask = static_cast<std::uint8_t>(1u << bit);
    if (pressed) impl->keyboardRows[row] &= static_cast<std::uint8_t>(~mask);
    else impl->keyboardRows[row] |= mask;
}

void SVI318::setJoysticks(std::uint8_t joystick1, std::uint8_t joystick2) noexcept
{
    impl->joystick1 = joystick1;
    impl->joystick2 = joystick2;
    impl->refreshPeripheralInputs();
}

bool SVI318::loadCassette(const std::vector<std::uint8_t>& image)
{
    const bool loaded = impl->cassette.insert(image);
    impl->refreshPeripheralInputs();
    return loaded;
}

void SVI318::rewindCassette() noexcept
{
    impl->cassette.rewind();
}

void SVI318::ejectCassette() noexcept
{
    impl->cassette.eject();
}

CassetteState SVI318::cassetteState() const noexcept
{
    return impl->cassette.state();
}

const std::vector<std::uint8_t>& SVI318::cassetteImage() const noexcept
{
    return impl->cassette.image();
}

void SVI318::startCassetteRecording()
{
    impl->cassette.startRecording();
    impl->refreshPeripheralInputs();
}

bool SVI318::stopCassetteRecording()
{
    const bool complete = impl->cassette.stopRecording();
    impl->refreshPeripheralInputs();
    return complete;
}

void SVI318::markCassetteSaved() noexcept
{
    impl->cassette.markSaved();
}

std::uint8_t SVI318::bankRegister() const noexcept
{
    return impl->bankRegister();
}

bool SVI318::clickLevel() const noexcept
{
    const auto outputs = impl->ppi.outputs();
    return (outputs.portCDriveMask & 0x80) != 0 && (outputs.portC & 0x80) != 0;
}

VideoStandard SVI318::videoStandard() const noexcept
{
    return impl->standard;
}

std::uint32_t SVI318::cpuTicksPerField() const noexcept
{
    return impl->standard == VideoStandard::Ntsc ? CpuTicksPerNtscField : CpuTicksPerPalField;
}

std::uint32_t SVI318::vdpMasterTicksPerField() const noexcept
{
    return impl->standard == VideoStandard::Ntsc ? VdpMasterTicksPerNtscField : VdpMasterTicksPerPalField;
}

std::uint32_t SVI318::fieldRateHz() const noexcept
{
    return impl->vdp->fieldRateHz();
}

tms99x8::TMS99X8& SVI318::video() noexcept
{
    return *impl->vdp;
}

const tms99x8::TMS99X8& SVI318::video() const noexcept
{
    return *impl->vdp;
}

ay8910::AY8910& SVI318::sound() noexcept
{
    return impl->psg;
}

const ay8910::AY8910& SVI318::sound() const noexcept
{
    return impl->psg;
}

namespace {

// Snapshot layout: magic, version (32 bits, little endian), video standard (one byte), then one
// section per component, each a 32-bit little-endian length and its bytes: CPU, CPU pins, PPI,
// VDP, PSG, RAM bank 2, RAM bank 21.
constexpr std::array<std::uint8_t, 8> StateMagic = {{'S', 'V', 'I', '3', '1', '8', 'S', 'T'}};
constexpr std::uint32_t StateVersion = 1;
constexpr std::size_t StateHeaderSize = StateMagic.size() + 4 + 1;
constexpr std::size_t StateSectionCount = 6;

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

std::vector<std::uint8_t> SVI318::saveState() const
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
    appendStateSection(state, impl->ram2.data(), impl->ram2.size());
    return state;
}

std::optional<VideoStandard> SVI318::stateVideoStandard(const std::vector<std::uint8_t>& state)
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
// the running machine untouched. ROM, module, cassette, expansion and host inputs stay as they are.
bool SVI318::loadState(const std::vector<std::uint8_t>& state)
{
    if (stateVideoStandard(state) != impl->standard) return false;
    std::size_t offset = StateHeaderSize;
    std::array<std::vector<std::uint8_t>, StateSectionCount> sections;
    for (auto& section : sections) {
        if (!readStateSection(state, offset, section)) return false;
    }
    if (offset != state.size()) return false;

    auto candidate = std::make_unique<Impl>(impl->standard);
    if (!candidate->cpu.loadState(sections[0]) || sections[1].size() != sizeof(z80bus::Outputs) ||
        !candidate->ppi.loadState(sections[2]) || !candidate->vdp->loadState(sections[3]) ||
        !candidate->psg.loadState(sections[4]) || sections[5].size() != InternalRamSize) {
        return false;
    }
    impl->cpu = std::move(candidate->cpu);
    std::memcpy(&impl->cpuPins, sections[1].data(), sizeof(impl->cpuPins));
    impl->ppi = std::move(candidate->ppi);
    impl->vdp = std::move(candidate->vdp);
    impl->psg = std::move(candidate->psg);
    std::copy(sections[5].begin(), sections[5].end(), impl->ram2.begin());
    impl->refreshPeripheralInputs();
    return true;
}

} // namespace svi318

#if defined(SVI318_GUI_APP)
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
constexpr UINT ResetCommand = 100;
constexpr UINT NewDiskCommand = 101;
constexpr UINT VideoPalCommand = 102;
constexpr UINT VideoNtscCommand = 103;
constexpr UINT StateLoadCommand = 104;
constexpr UINT ManualCommand = 990;
constexpr UINT LanguageEnglishCommand = 991;
constexpr UINT LanguageGermanCommand = 992;
constexpr UINT StateSaveCommand = 105;
constexpr UINT VideoRecordCommand = 106;
constexpr UINT VideoStopCommand = 107;
constexpr UINT CassetteOpenCommand = 110;
constexpr UINT CassetteRewindCommand = 111;
constexpr UINT CassetteEjectCommand = 112;
constexpr UINT CassetteRecordCommand = 113;
constexpr UINT CassetteSaveCommand = 114;
constexpr UINT CartridgeInsertCommand = 120;
constexpr UINT CartridgeEjectCommand = 121;
// Expander choice: none, SVI-601, SVI-602, SVI-605B, SVI-608M.
constexpr UINT ExpanderNoneCommand = 200;
constexpr UINT Expander601Command = 201;
constexpr UINT Expander602Command = 202;
constexpr UINT Expander605bCommand = 203;
constexpr UINT Expander608mCommand = 204;
constexpr UINT Expander605bHdCommand = 205;
// Drive commands: base + 10 * drive (0 = A, 1 = B).
constexpr UINT DriveInsertCommand = 210;
constexpr UINT DriveEjectCommand = 211;
constexpr UINT DriveProtectCommand = 212;
constexpr UINT HddInsertCommand = 230;
constexpr UINT HddNewCommand = 231;
constexpr UINT HddEjectCommand = 232;
constexpr UINT PrinterFolderCommand = 290;
// RS-232: none, then one command per listed COM port.
constexpr UINT ComPortNoneCommand = 300;
constexpr UINT ComPortLastCommand = 399;
// Joystick commands: base + 10 * port + source.
constexpr UINT JoystickCommandBase = 400;
constexpr UINT KeyboardJoystickToggleCommand = 430;
// Slot commands: base + SlotCommandStride * slot + card type.
constexpr UINT SlotCommandBase = 500;
constexpr UINT SlotCommandStride = 20;
// Most slots of any expander (SVI-601).
constexpr std::size_t MaxSlotCount = svi601::SlotCount;
constexpr ULONGLONG ControllerProbeIntervalMs = 1000;
// 80-column picture of the SVI-806 beside the TV picture: 1:1 horizontally, lines doubled.
constexpr int PanelMargin = 20;
constexpr int PanelPictureWidth = 640;
constexpr int PanelWidth = PanelPictureWidth + 2 * PanelMargin;
constexpr int PanelLineScale = 2;
constexpr std::uint32_t PanelLit = 0x33FF33;
constexpr ULONGLONG DiskFlushIntervalMs = 1000;
constexpr ULONGLONG PrinterIdleMs = 3000;
constexpr wchar_t SettingsFileName[] = L"svi318.ini";
constexpr wchar_t WindowTitle[] = L"SVI-318";
constexpr std::uint32_t AudioSampleRate = 44100;
// The PAL field is the longer one and sizes the audio blocks for both video standards.
constexpr std::size_t MaxAudioSamplesPerField = static_cast<std::size_t>(
    (static_cast<std::uint64_t>(AudioSampleRate) * svi318::VdpMasterTicksPerPalField +
     svi318::VdpMasterClockHz - 1) / svi318::VdpMasterClockHz);
constexpr std::size_t AudioBufferCount = 3;
constexpr ULONGLONG AudioRetryIntervalMs = 1000;
constexpr ULONGLONG TitleRefreshIntervalMs = 250;

constexpr std::array<std::uint32_t, 16> Palette = {
    0x000000, 0x000000, 0x21C842, 0x5EDC78,
    0x5455ED, 0x7D76FC, 0xD4524D, 0x42EBF5,
    0xFC5554, 0xFF7978, 0xD4C154, 0xE6CE80,
    0x21B03B, 0xC95BBA, 0xCCCCCC, 0xFFFFFF
};

using KeyPosition = svi318::KeyboardPosition;

// What drives an SVI joystick port, as stored in svi318.ini. With the keyboard, the joystick keys
// act only on the joystick, as on the real machine where keyboard and joysticks are separate.
enum JoystickSource : int {
    JoystickOff = 0,
    JoystickKeyboard = 1,
    JoystickController = 2,
    JoystickSourceCount = 3
};

constexpr std::array<const char*, JoystickSourceCount> JoystickSourceNames = {{"off", "keyboard", "controller"}};

// Keyboard joystick keys per port by key position (scan code, 0x100 for extended keys): up, down,
// left, right, trigger. Port 2 fires with the key left of X: Z on a US keyboard, Y on a German one.
constexpr std::array<std::array<UINT, 5>, 2> JoystickKeys = {{
    {{0x148, 0x150, 0x14B, 0x14D, 0x2E}},
    {{0x11, 0x1F, 0x1E, 0x20, 0x2C}},
}};

// A PC game controller read through the Windows joystick interface (JOYSTICKID1 + port).
struct ControllerState {
    bool present = false;
    JOYCAPSW caps{};
    ULONGLONG nextProbe = 0;
};

// Card numbers as stored in svi318.ini.
enum CardType : int {
    CardEmpty = 0,
    CardRam807S2S3 = 1,
    CardRam807S2 = 2,
    CardRam807S3S4 = 3,
    CardSerial805 = 4,
    CardVideo806 = 5,
    CardSasi = 6,
    CardBoot = 7,
    CardRam803 = 8,
    CardRam807S1S5 = 9,
    CardSvi801 = 10,
    CardSvi802 = 11,
    CardTypeCount = 12
};

struct CardChoice {
    int type;
    const wchar_t* name;
    const wchar_t* germanName;
};

// Menu order by card number; the SVI-807 settings are the switch combinations without overlapping banks.
constexpr std::array<CardChoice, 12> CardChoices = {{
    {CardEmpty, L"&empty", L"&leer"},
    {CardSvi801, L"SVI-80&1 disk controller (2x SVI-902)", L"SVI-80&1 Diskettencontroller (2x SVI-902)"},
    {CardSvi802, L"SVI-802 &Centronics printer", L"SVI-802 &Centronics-Drucker"},
    {CardRam803, L"SVI-80&3 16 KB RAM (8000h-BFFFh, bank 02)", L"SVI-80&3 16 KB RAM (8000h-BFFFh, Bank 02)"},
    {CardSerial805, L"SVI-80&5 RS-232", L"SVI-80&5 RS-232"},
    {CardVideo806, L"SVI-80&6 80 columns", L"SVI-80&6 80 Zeichen"},
    {CardRam807S1S5, L"SVI-807 64 KB RAM, S1+S&5 (banks 21, 02: 64 KB, CP/M)", L"SVI-807 64 KB RAM, S1+S&5 (Baenke 21, 02: 64 KB, CP/M)"},
    {CardRam807S2S3, L"SVI-807 64 KB RAM, S2+S3 (banks &22, 31)", L"SVI-807 64 KB RAM, S2+S3 (Baenke &22, 31)"},
    {CardRam807S2, L"SVI-807 64 KB RAM, S&2 only (bank 22)", L"SVI-807 64 KB RAM, nur S&2 (Bank 22)"},
    {CardRam807S3S4, L"SVI-807 64 KB RAM, S3+S&4 (banks 31, 32)", L"SVI-807 64 KB RAM, S3+S&4 (Baenke 31, 32)"},
    {CardSasi, L"LAN-HD-01 &hard disk adapter (SVI-608M)", L"LAN-HD-01 &Festplattenadapter (SVI-608M)"},
    {CardBoot, L"LAN-IF-01 &boot card (SVI-608M)", L"LAN-IF-01 &Bootkarte (SVI-608M)"},
}};

// RAM banks a RAM card answers: bit 0 bank 22, bit 1 bank 31, bit 2 bank 32, bit 3 bank 21,
// bit 4 bank 02 above the internal 16 KiB.
constexpr unsigned cardBanks(int type)
{
    switch (type) {
    case CardRam807S2S3: return 0x03u;
    case CardRam807S2: return 0x01u;
    case CardRam807S3S4: return 0x06u;
    case CardRam803: return 0x10u;
    case CardRam807S1S5: return 0x18u;
    default: return 0u;
    }
}

struct MatrixBinding {
    std::array<KeyPosition, 2> positions{};
    std::uint8_t count = 0;
    bool active = false;
    // A character the SVI types without Shift, held while the PC Shift key is down.
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

struct AppState {
    svi318::SVI318 machine;
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
    // Held characters that the SVI types without Shift although the PC Shift key is down.
    int shiftSuppressors = 0;
    std::wstring shownTitle;
    std::filesystem::path cassettePath;
    std::filesystem::path cartridgePath;
    std::vector<std::uint8_t> romImage;
    std::wstring shownStatus;
    HWND statusBar = nullptr;
    LARGE_INTEGER speedStart{};
    std::uint64_t speedStartTicks = 0;
    double cpuClockMHz = 0.0;
    std::uint64_t emulatedCpuTicks = 0;
    ULONGLONG titleTick = 0;
    std::vector<SoundPortTransition> soundPortTransitions;
    std::array<MatrixBinding, 512> matrixBindings{};
    std::array<std::array<std::uint8_t, 8>, svi318::KeyboardRowCount> matrixCounts{};
    std::vector<std::uint32_t> pixels =
        std::vector<std::uint32_t>(ClientWidth * ClientHeight);
    std::unique_ptr<svi605b::ExpanderBoard> expander = std::make_unique<svi605b::SVI605B>();
    bool expanderConnected = false;
    // The SVI-602 Mini Expander instead of the expander board.
    std::unique_ptr<svi602::SVI602> miniExpander = std::make_unique<svi602::SVI602>();
    bool miniFitted = false;
    // The SVI-601 Super Expander instead of the expander board.
    std::unique_ptr<svi601::SVI601> superExpander = std::make_unique<svi601::SVI601>();
    bool superFitted = false;
    std::array<int, MaxSlotCount> slotCards{};
    std::array<std::filesystem::path, svi605b::DriveCount> diskPaths;
    std::array<bool, svi605b::DriveCount> diskWriteFailed{};
    svi805::ComPortLink comLink;
    std::vector<std::wstring> comPorts;
    std::filesystem::path printerDirectory;
    std::filesystem::path printerFile;
    ULONGLONG printerLastByte = 0;
    std::filesystem::path settingsPath;
    std::vector<std::uint8_t> characterRom;
    std::vector<std::uint8_t> bootRom;
    // ROM files the settings asked for that were not found at start (file and part).
    std::vector<std::wstring> missingRoms;
    std::filesystem::path hddPath;
    bool hddReadOnly = false;
    bool hddWriteFailed = false;
    bool panelVisible = false;
    std::vector<std::uint32_t> panelPixels = std::vector<std::uint32_t>(PanelWidth * ClientHeight);
    // Video recording: the picture of every field (beside it the SVI-806 panel when it was shown at
    // the start) and the sound of that field.
    videorec::VideoRecorder recorder;
    bool videoRecording = false;
    bool recordPanel = false;
    std::vector<std::uint32_t> recordPixels;
    std::vector<std::int16_t> fieldAudio;
    ULONGLONG diskFlushTick = 0;
    HMENU expanderMenu = nullptr;
    std::array<HMENU, svi605b::DriveCount> driveMenus{};
    std::array<HMENU, MaxSlotCount> slotMenus{};
    HMENU hddMenu = nullptr;
    HMENU comMenu = nullptr;
    int statusHeight = 0;
    DWORD windowStyle = 0;
};

// The chosen expander is an SVI-605B or SVI-608M with built-in disk unit and printer port
// (otherwise the SVI-602 or SVI-601, which take these as cards).
bool boardFitted(const AppState& app)
{
    return !app.miniFitted && !app.superFitted;
}

// The chosen expander is an SVI-608M.
bool svi608mFitted(const AppState& app)
{
    return boardFitted(app) && dynamic_cast<const svi608m::SVI608M*>(app.expander.get()) != nullptr;
}

// The chosen expander is an SVI-605B with hard disk.
bool svi605bHdFitted(const AppState& app)
{
    return boardFitted(app) && dynamic_cast<const svi605bhd::SVI605BHD*>(app.expander.get()) != nullptr;
}

// Card ROMs found at start: the character ROM of the SVI-806 and the EPROM of the boot card (also
// on the LAN card). Without them the cards, and the SVI-608M with its factory cards, are not offered.
bool characterRomPresent(const AppState& app)
{
    return app.characterRom.size() == svi806::SVI806CharacterRomSize;
}

bool bootRomPresent(const AppState& app)
{
    return app.bootRom.size() == lanif01::BootRomSize;
}

bool factoryRomsPresent(const AppState& app)
{
    return characterRomPresent(app) && bootRomPresent(app);
}

// Records a ROM the settings asked for that is missing, once per file.
void noteMissingRom(AppState& app, const wchar_t* file)
{
    if (std::find(app.missingRoms.begin(), app.missingRoms.end(), file) == app.missingRoms.end()) app.missingRoms.emplace_back(file);
}

// What sits on the expansion connector.
svi3x8::ExpansionPort* connectedDevice(AppState& app)
{
    if (!app.expanderConnected) return nullptr;
    if (app.miniFitted) return app.miniExpander.get();
    if (app.superFitted) return app.superExpander.get();
    return app.expander.get();
}

std::size_t slotCount(const AppState& app)
{
    if (app.miniFitted) return 1;
    if (app.superFitted) return app.superExpander->slotCount();
    return app.expander->slotCount();
}

svi3x8::ExpansionPort* slotCard(const AppState& app, std::size_t slot)
{
    if (app.miniFitted) return slot == 0 ? app.miniExpander->card() : nullptr;
    if (app.superFitted) return app.superExpander->card(slot);
    return app.expander->card(slot);
}

void placeCard(AppState& app, std::size_t slot, std::unique_ptr<svi3x8::ExpansionPort> card)
{
    if (app.miniFitted) {
        if (slot == 0) (void)app.miniExpander->insertCard(std::move(card));
        return;
    }
    if (app.superFitted) {
        app.superExpander->insertCard(slot, std::move(card)).reset();
        return;
    }
    app.expander->insertCard(slot, std::move(card)).reset();
}

// SVI-801 floppy disk controller in the SVI-602 or SVI-601.
svi801::SVI801* diskCard(const AppState& app)
{
    if (boardFitted(app)) return nullptr;
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        if (auto* card = dynamic_cast<svi801::SVI801*>(slotCard(app, slot))) return card;
    }
    return nullptr;
}

// SVI-602 and SVI-601 have drives only through an SVI-801.
std::size_t driveCount(const AppState& app)
{
    if (boardFitted(app)) return app.expander->driveCount();
    const auto* card = diskCard(app);
    return card ? card->disks().driveCount() : 0;
}

// Drive A (0) or B (1) of the built-in disk unit or the SVI-801; without either, a drive of the
// unused expander board that is never connected.
svi801::FloppyDrive& driveAt(AppState& app, std::size_t drive)
{
    if (auto* card = diskCard(app)) return card->disks().drive(drive);
    return app.expander->drive(drive);
}

// Drive letter as CP/M uses it: started from the hard disk of an SVI-608M, CP/M 2.27 names the
// hard disk A: to D: and the floppy E:.
wchar_t driveLetter(const AppState& app, std::size_t drive)
{
    const auto* expander608m = svi608mFitted(app) ? dynamic_cast<const svi608m::SVI608M*>(app.expander.get()) : nullptr;
    if (drive == 0 && expander608m && expander608m->bootSource() == svi608m::BootSource::HardDisk) return L'E';
    return static_cast<wchar_t>(L'A' + drive);
}

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

// Register 7 keeps port B an output, so the memory banks stay selected during the test.
void setAyAudioTestTone(AppState& app)
{
    constexpr std::array<std::uint32_t, 3> frequencies = {440, 554, 659};
    constexpr unsigned FieldsPerTone = 8;
    const auto note = app.audioTestField / FieldsPerTone;
    auto& ay = app.machine.sound();

    writeAyRegister(ay, 7, static_cast<std::uint8_t>(0xBE | (app.savedAyRegisters[7] & 0x80)));
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
    const auto sampleCount = app.audioSamplePhase / svi318::VdpMasterClockHz;
    app.audioSamplePhase %= svi318::VdpMasterClockHz;
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

// SVI keys without a character: contact and whether SVI Shift is pressed with it.
bool matrixPositionFallback(WPARAM virtualKey, bool extended, KeyPosition& position, bool& shift)
{
    shift = false;
    switch (virtualKey) {
    case VK_SHIFT: return false;
    case VK_CONTROL:
        position = extended ? KeyPosition{6, 3} : KeyPosition{6, 1};
        return true;
    case VK_MENU:
        if (extended) return false;
        position = {6, 2};
        return true;
    case VK_ESCAPE: position = {6, 4}; return true;
    case VK_END: position = {6, 5}; return true;
    case VK_RETURN: position = {6, 6}; return true;
    case VK_LEFT: position = {6, 7}; return true;
    case VK_F1: position = {7, 0}; return true;
    case VK_F2: position = {7, 1}; return true;
    case VK_F3: position = {7, 2}; return true;
    case VK_F4: position = {7, 3}; return true;
    case VK_F5: position = {7, 4}; return true;
    case VK_F6: position = {7, 0}; shift = true; return true;
    case VK_F7: position = {7, 1}; shift = true; return true;
    case VK_F8: position = {7, 2}; shift = true; return true;
    case VK_F9: position = {7, 3}; shift = true; return true;
    case VK_F10: position = {7, 4}; shift = true; return true;
    case VK_HOME: position = {7, 5}; return true;
    case VK_INSERT: position = {7, 6}; return true;
    case VK_DOWN: position = {7, 7}; return true;
    case VK_SPACE: position = {8, 0}; return true;
    case VK_TAB: position = {8, 1}; return true;
    case VK_DELETE: position = {8, 2}; return true;
    case VK_CAPITAL: position = {8, 3}; return true;
    case VK_PRIOR: position = {8, 4}; return true;
    case VK_NEXT: position = {8, 5}; return true;
    case VK_RIGHT: position = {8, 7}; return true;
    case VK_UP: position = {5, 7}; return true;
    case VK_BACK: position = {5, 6}; return true;
    default: break;
    }
    // The SVI-318 has no numeric keypad: the PC keypad types its characters on the main keys.
    char16_t character = 0;
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) character = static_cast<char16_t>(u'0' + (virtualKey - VK_NUMPAD0));
    if (virtualKey == VK_ADD) character = u'+';
    if (virtualKey == VK_SUBTRACT) character = u'-';
    if (virtualKey == VK_MULTIPLY) character = u'*';
    if (virtualKey == VK_DIVIDE) character = u'/';
    if (virtualKey == VK_DECIMAL) character = u'.';
    if (virtualKey == VK_SEPARATOR) character = u',';
    svi318::KeyModifier modifier{};
    if (character == 0 || !svi318::mapCharacterToSvi(character, position, modifier)) return false;
    if (modifier != svi318::KeyModifier::None && modifier != svi318::KeyModifier::Shift) return false;
    shift = modifier == svi318::KeyModifier::Shift;
    return true;
}

// The SVI Shift contact is held by a key that needs it, or by the PC Shift key alone, as on the real
// keyboard (BASIC shows the F6-F10 labels while Shift is down); a held character that the SVI types
// without Shift takes it back meanwhile.
void refreshShift(AppState& app)
{
    const auto shift = svi318::SviShift;
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

// F11 switches the keyboard joystick on and off; while it is off, the joystick keys type on the SVI
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

std::uint8_t joystickBits(bool up, bool down, bool left, bool right, bool trigger)
{
    std::uint8_t result = 0xFF;
    if (up) result &= static_cast<std::uint8_t>(~svi318::JoystickUp);
    if (down) result &= static_cast<std::uint8_t>(~svi318::JoystickDown);
    if (left) result &= static_cast<std::uint8_t>(~svi318::JoystickLeft);
    if (right) result &= static_cast<std::uint8_t>(~svi318::JoystickRight);
    if (trigger) result &= static_cast<std::uint8_t>(~svi318::JoystickTrigger);
    return result;
}

void probeController(AppState& app, std::size_t port)
{
    auto& controller = app.controllers[port];
    controller.nextProbe = GetTickCount64() + ControllerProbeIntervalMs;
    controller.present = joyGetDevCapsW(JOYSTICKID1 + static_cast<UINT>(port), &controller.caps,
                                        sizeof(controller.caps)) == JOYERR_NOERROR;
}

// Axes beyond a quarter of their range, the point-of-view hat and any button.
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
    return joystickBits(up, down, left, right, info.dwButtons != 0);
}

void updateJoysticks(AppState& app)
{
    std::array<std::uint8_t, 2> ports{0xFF, 0xFF};
    for (std::size_t port = 0; port < ports.size(); ++port) {
        if (app.joystickSources[port] == JoystickKeyboard && keyboardJoystickEnabled(app)) {
            const auto& keys = JoystickKeys[port];
            ports[port] = joystickBits(app.keyDown[keys[0]], app.keyDown[keys[1]], app.keyDown[keys[2]],
                                       app.keyDown[keys[3]], app.keyDown[keys[4]]);
        } else if (app.joystickSources[port] == JoystickController) {
            ports[port] = controllerJoystick(app, port);
        }
    }
    app.machine.setJoysticks(ports[0], ports[1]);
}

std::wstring joystickStatus(const AppState& app)
{
    std::wstring text;
    for (std::size_t port = 0; port < 2; ++port) {
        text += (port == 0 ? L"Joystick 1: " : L" | Joystick 2: ");
        switch (app.joystickSources[port]) {
        case JoystickKeyboard: text += port == 0 ? ui(L"arrows + C", L"Pfeile + C") : ui(L"W A S D + Z", L"W A S D + Y"); break;
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
            svi318::KeyModifier modifier = svi318::KeyModifier::None;
            bool fallbackShift = false;
            const bool characterKey =
                (uiLanguage() == UiLanguage::German ? svi318::mapGermanScanCodeToSvi : svi318::mapUsScanCodeToSvi)(
                    scanCode, extended, app.leftShiftDown || app.rightShiftDown, app.altGrDown, position, modifier);
            const bool mapped = characterKey || matrixPositionFallback(virtualKey, extended, position, fallbackShift);
            if (mapped && position.row < svi318::KeyboardRowCount && position.bit < 8) {
                positions[contactCount++] = position;
                if (modifier == svi318::KeyModifier::Shift || fallbackShift) {
                    positions[contactCount++] = svi318::SviShift;
                } else if (modifier == svi318::KeyModifier::LeftGraph) {
                    positions[contactCount++] = svi318::SviLeftGraph;
                } else if (modifier == svi318::KeyModifier::RightGraph) {
                    positions[contactCount++] = svi318::SviRightGraph;
                }

                binding.positions = positions;
                binding.count = contactCount;
                binding.active = true;
                binding.suppressesShift = characterKey && modifier != svi318::KeyModifier::Shift && (app.leftShiftDown || app.rightShiftDown);
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

void updatePanelPixels(AppState& app);
void stopVideoRecording(AppState& app);

// Hands the picture of the field just finished to the video recording, beside it the SVI-806 panel
// when it was shown at the start of the recording.
void captureVideoField(AppState& app)
{
    if (!app.recorder.recording()) return;
    if (app.machine.vdpMasterTicksPerField() != app.recorder.settings().frameRateDenominator) {
        // The video standard and with it the field rate changed: the recording ends here.
        stopVideoRecording(app);
        return;
    }
    updateDisplayPixels(app, app.machine.video().scanoutFrame());
    if (!app.recordPanel) {
        app.recorder.addFrame(app.pixels.data(), ClientWidth);
        return;
    }
    if (app.panelVisible) updatePanelPixels(app);
    constexpr int width = ClientWidth + PanelWidth;
    app.recordPixels.resize(static_cast<std::size_t>(width) * ClientHeight);
    for (int y = 0; y < ClientHeight; ++y) {
        auto* row = app.recordPixels.data() + static_cast<std::size_t>(y) * width;
        std::copy_n(app.pixels.data() + static_cast<std::size_t>(y) * ClientWidth, ClientWidth, row);
        std::copy_n(app.panelPixels.data() + static_cast<std::size_t>(y) * PanelWidth, PanelWidth, row + ClientWidth);
    }
    app.recorder.addFrame(app.recordPixels.data(), width);
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
        queueAudioField(app, app.machine.clickLevel(), sampleCount);
        if (++app.audioTestField >= 32) restoreAyAfterAudioTest(app);
        captureVideoField(app);
        return;
    }

    app.soundPortTransitions.clear();
    const bool initialSoundPortLevel = app.machine.clickLevel();
    bool previousSoundPortLevel = initialSoundPortLevel;
    const auto ticksPerField = app.machine.cpuTicksPerField();
    for (std::uint32_t tick = 0; tick < ticksPerField; ++tick) {
        if ((tick & 0xFFu) == 0) processPendingKeyboardMessages(app.window);
        (void)app.machine.tick();
        ++app.emulatedCpuTicks;
        const bool soundPortLevel = app.machine.clickLevel();
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
    app.fieldPhase += static_cast<std::uint64_t>(elapsed) * svi318::VdpMasterClockHz;
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
std::wstring cassetteProgress(const svi318::CassetteState& state)
{
    if (!state.motorOn) return {};
    const auto seconds = state.positionTicks / svi318::CpuClockHz;
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
    title += app.machine.videoStandard() == svi318::VideoStandard::Ntsc ? L" | NTSC 60 Hz" : L" | PAL 50 Hz";
    if (!app.cartridgePath.empty()) title += ui(L" | Module: ", L" | Modul: ") + app.cartridgePath.filename().wstring();
    if (state.recording) {
        title += ui(L" | CAS: recording", L" | CAS: Aufnahme");
    } else if (!app.cassettePath.empty()) {
        title += L" | CAS: " + app.cassettePath.filename().wstring();
    }
    title += cassetteProgress(state);
    if (app.expanderConnected) {
        title += app.miniFitted ? L" | SVI-602" : app.superFitted ? L" | SVI-601" : svi608mFitted(app) ? L" | SVI-608M" : svi605bHdFitted(app) ? L" | SVI-605B HD" : L" | SVI-605B";
        for (std::size_t drive = 0; drive < driveCount(app); ++drive) {
            const auto& floppy = driveAt(app, drive);
            title += L" | " + std::wstring(1, driveLetter(app, drive)) + L": ";
            if (!floppy.hasDisk()) title += ui(L"empty", L"leer");
            else if (floppy.hasBlankDisk()) title += ui(L"new (no file)", L"neu (ohne Datei)");
            else title += app.diskPaths[drive].filename().wstring();
            if (floppy.hasDisk() && floppy.writeProtected()) title += L" [WP]";
            if (floppy.motorOn() && floppy.hasDisk()) title += ui(L" (motor)", L" (Motor)");
        }
        if (!app.hddPath.empty()) {
            title += L" | HD: " + app.hddPath.filename().wstring();
            if (app.hddReadOnly) title += ui(L" [read-only]", L" [nur Lesen]");
        }
        if (app.comLink.isOpen()) title += L" | RS-232: " + app.comLink.name();
    }
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
    EnableMenuItem(menu, CassetteRecordCommand, MF_BYCOMMAND | (state.recording ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, CassetteSaveCommand, MF_BYCOMMAND | (state.modified ? MF_ENABLED : MF_GRAYED));
    updateWindowTitle(app);
    DrawMenuBar(app.window);
}

void resumeAfterDialog(AppState& app)
{
    QueryPerformanceCounter(&app.lastCounter);
    app.fieldPhase = 0;
    SetTimer(app.window, EmulationTimer, TimerPeriodMs, nullptr);
}

bool confirmCassetteDiscard(AppState& app)
{
    if (!app.machine.cassetteState().modified) return true;
    KillTimer(app.window, EmulationTimer);
    const auto answer = MessageBoxW(app.window, ui(L"Discard the unsaved CAS recording?", L"Die ungespeicherte CAS-Aufnahme verwerfen?"),
                                    ui(L"Cassette", L"Kassette"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    resumeAfterDialog(app);
    return answer == IDYES;
}

bool writeFileAtomically(const std::filesystem::path& path, const std::vector<std::uint8_t>& image)
{
    auto temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool saved = WriteFile(file, image.data(), static_cast<DWORD>(image.size()), &written, nullptr) != FALSE &&
                 written == image.size();
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
    const bool stopped = !app.machine.cassetteState().recording || app.machine.stopCassetteRecording();
    updateCassetteMenu(app);
    if (!stopped || app.machine.cassetteState().recordingError || app.machine.cassetteImage().empty()) {
        MessageBoxW(app.window,
            ui(L"There is no complete SVI CAS recording. Invalid or incomplete signals are not saved.", L"Keine vollstaendige SVI-CAS-Aufnahme vorhanden. Ungueltige oder unvollstaendige "
            L"Signale werden nicht gespeichert."),
            ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        resumeAfterDialog(app);
        return;
    }
    std::vector<wchar_t> fileName(32768, L'\0');
    const auto suggestedName = svi318::suggestedCassetteFileName(app.machine.cassetteImage());
    std::copy(suggestedName.begin(), suggestedName.end(), fileName.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI cassettes (*.cas)\0*.cas\0\0", L"SVI-Kassetten (*.cas)\0*.cas\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrDefExt = L"cas";
    dialog.lpstrTitle = ui(L"Save CAS recording", L"CAS-Aufnahme speichern");
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        if (writeFileAtomically(path, app.machine.cassetteImage())) {
            app.machine.markCassetteSaved();
            app.cassettePath = path;
            updateCassetteMenu(app);
        } else {
            MessageBoxW(app.window, ui(L"The CAS image could not be saved. The recording stays in memory.", L"Das CAS-Image konnte nicht gespeichert werden. Die Aufnahme bleibt im Speicher."),
                        ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The save dialog could not be opened.", L"Der Speicherdialog konnte nicht geoeffnet werden."), ui(L"Cassette", L"Kassette"),
                    MB_OK | MB_ICONERROR);
    }
    resumeAfterDialog(app);
}

std::filesystem::path executableDirectory();
HMENU buildMenu(AppState& app);

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

// Folder of the shared SVI-318/328 expanders and cards, which holds the card ROMs in the
// development tree (next to the machine's project folder).
std::filesystem::path componentDirectory()
{
    return sourceDirectory().parent_path().parent_path() / L"svi3x8";
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

// Folder of this machine in the user's documents, for example Documents\SVI-318.
std::filesystem::path userDataFolder()
{
    std::array<wchar_t, MAX_PATH> documents{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents.data()))) return {};
    return std::filesystem::path(documents.data()) / L"SVI-318";
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
    const auto page = std::filesystem::path(temp.data()) / L"SVI318-Manual.html";
    std::ofstream output(page, std::ios::binary | std::ios::trunc);
    output << svi318::manualToHtml(markdown, baseUrl);
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

// Media folders by their name in the development tree (SVICAS, SVIDSK, SVIHDD, SVIMOD, SVIPRINT,
// SVIROM, SVISTATE, SVIVIDEO) and in the user's documents.
constexpr std::array<std::pair<const wchar_t*, const wchar_t*>, 8> MediaFolderNames{{
    {L"SVICAS", L"CAS"}, {L"SVIDSK", L"DSK"}, {L"SVIHDD", L"HDD"}, {L"SVIMOD", L"MOD"}, {L"SVIPRINT", L"PRINT"},
    {L"SVIROM", L"ROM"}, {L"SVISTATE", L"STATE"}, {L"SVIVIDEO", L"VIDEO"},
}};

// A media folder: in the development tree the folder of "2100 - MSX - MEDIEN", otherwise the folder
// in Documents\SVI-318 (CAS, DSK, HDD, MOD, PRINT, ROM, STATE, VIDEO), created when missing. Without either the
// program folder is used.
std::filesystem::path mediaFolder(const wchar_t* name)
{
    std::error_code error;
    const auto development = developmentMediaRoot();
    if (!development.empty()) {
        const auto candidate = development / name;
        return std::filesystem::is_directory(candidate, error) ? candidate : executableDirectory();
    }
    const auto user = userDataFolder();
    if (user.empty()) return executableDirectory();
    for (const auto& [developmentName, userName] : MediaFolderNames) {
        if (std::wcscmp(developmentName, name) != 0) continue;
        const auto folder = user / userName;
        std::filesystem::create_directories(folder, error);
        return std::filesystem::is_directory(folder, error) ? folder : executableDirectory();
    }
    return executableDirectory();
}

// A ROM image by its file name and size: first the rom folder of its component in the development
// tree, then the ROM media folder, then the program folder. Empty when no folder holds the file with
// that size.
std::filesystem::path findRom(const std::filesystem::path& componentFolder, const wchar_t* fileName, std::uintmax_t size)
{
    std::error_code error;
    for (const auto& folder : {componentFolder, mediaFolder(L"SVIROM"), executableDirectory()}) {
        const auto candidate = folder / fileName;
        if (std::filesystem::is_regular_file(candidate, error) && std::filesystem::file_size(candidate, error) == size) {
            return candidate;
        }
    }
    return {};
}

// Outside the development tree the program creates its folders in the user's documents, with the
// subfolder DSK\Original for write-protected original disks.
void createUserFolders()
{
    if (!developmentMediaRoot().empty()) return;
    for (const auto& names : MediaFolderNames) (void)mediaFolder(names.first);
    const auto user = userDataFolder();
    std::error_code error;
    if (!user.empty()) std::filesystem::create_directories(user / L"DSK" / L"Original", error);
}

void selectCassetteImage(AppState& app)
{
    if (!confirmCassetteDiscard(app)) return;
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI cassettes (*.cas)\0*.cas\0All files (*.*)\0*.*\0\0", L"SVI-Kassetten (*.cas)\0*.cas\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = ui(L"Insert CAS image", L"CAS-Image einlegen");
    const std::wstring cassetteFolder = mediaFolder(L"SVICAS").wstring();
    dialog.lpstrInitialDir = cassetteFolder.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto size = file ? file.tellg() : std::streampos(-1);
        bool loaded = false;
        if (size > std::streampos(0) && size <= std::streampos(16 * 1024 * 1024)) {
            file.seekg(0);
            std::vector<std::uint8_t> image(static_cast<std::size_t>(size));
            if (file.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()))) {
                svi318::Cassette candidate;
                if (candidate.insert(image)) {
                    app.machine.ejectCassette();
                    loaded = app.machine.loadCassette(image);
                }
            }
        }
        if (loaded) {
            app.cassettePath = path;
        } else {
            MessageBoxW(app.window,
                ui(L"The file is not an SVI CAS image (blocks starting with 16 x 55h and 7Fh).", L"Die Datei ist kein SVI-CAS-Image (Bloecke mit 16 x 55h und 7Fh am Anfang)."),
                ui(L"Cassette", L"Kassette"), MB_OK | MB_ICONERROR);
        }
        updateCassetteMenu(app);
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"Cassette", L"Kassette"),
                    MB_OK | MB_ICONERROR);
    }
    resumeAfterDialog(app);
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
    app.machine.setJoysticks(0xFF, 0xFF);
}

void updateCartridgeMenu(AppState& app)
{
    EnableMenuItem(GetMenu(app.window), CartridgeEjectCommand,
                   MF_BYCOMMAND | (app.machine.cartridgeInserted() ? MF_ENABLED : MF_GRAYED));
    updateWindowTitle(app);
    DrawMenuBar(app.window);
}

// Inserting or removing a module resets the machine like switching it off and on; the BASIC ROM
// then starts a module that begins with F3h 31h.
void restartWithCartridge(AppState& app)
{
    releaseAllInput(app);
    app.machine.reset();
    updateCartridgeMenu(app);
}

void selectCartridge(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI modules (*.bin;*.rom)\0*.bin;*.rom\0All files (*.*)\0*.*\0\0", L"SVI-Module (*.bin;*.rom)\0*.bin;*.rom\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = ui(L"Insert module", L"Modul einstecken");
    const std::wstring moduleFolder = mediaFolder(L"SVIMOD").wstring();
    dialog.lpstrInitialDir = moduleFolder.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        std::ifstream file(path, std::ios::binary);
        const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file), {}};
        if (file.bad() || !app.machine.insertCartridge(image)) {
            MessageBoxW(app.window, ui(L"The file is not an SVI module (1 byte to 64 KiB).", L"Die Datei ist kein SVI-Modul (1 Byte bis 64 KiB)."), ui(L"Module", L"Modul"),
                        MB_OK | MB_ICONERROR);
        } else {
            app.cartridgePath = path;
            restartWithCartridge(app);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"Module", L"Modul"),
                    MB_OK | MB_ICONERROR);
    }
    resumeAfterDialog(app);
}

// SVI disk images live in the disk media folder, the write-protected originals in its subfolder
// "Original".
std::filesystem::path diskFolder()
{
    return mediaFolder(L"SVIDSK");
}

// "Neue Diskette": copies an original disk image under a new name; the copy is writable.
void createDiskCopy(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    const auto folder = diskFolder();
    const auto originals = std::filesystem::is_directory(folder / L"Original") ? folder / L"Original" : folder;
    const std::wstring originalsText = originals.wstring();
    const std::wstring folderText = folder.wstring();
    std::vector<wchar_t> source(32768, L'\0');
    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = app.window;
    open.lpstrFilter = ui(L"SVI disks (*.dsk)\0*.dsk\0All files (*.*)\0*.*\0\0", L"SVI-Disketten (*.dsk)\0*.dsk\0Alle Dateien (*.*)\0*.*\0\0");
    open.lpstrFile = source.data();
    open.nMaxFile = static_cast<DWORD>(source.size());
    open.lpstrInitialDir = originalsText.c_str();
    open.lpstrTitle = ui(L"New disk: choose template", L"Neue Diskette: Vorlage waehlen");
    open.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&open)) {
        if (CommDlgExtendedError() != 0) {
            MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"New disk", L"Neue Diskette"),
                        MB_OK | MB_ICONERROR);
        }
        resumeAfterDialog(app);
        return;
    }
    const std::filesystem::path sourcePath(source.data());
    std::vector<wchar_t> target(32768, L'\0');
    const auto suggestion = sourcePath.filename().wstring();
    std::copy(suggestion.begin(), suggestion.end(), target.begin());
    OPENFILENAMEW save{};
    save.lStructSize = sizeof(save);
    save.hwndOwner = app.window;
    save.lpstrFilter = ui(L"SVI disks (*.dsk)\0*.dsk\0\0", L"SVI-Disketten (*.dsk)\0*.dsk\0\0");
    save.lpstrFile = target.data();
    save.nMaxFile = static_cast<DWORD>(target.size());
    save.lpstrInitialDir = folderText.c_str();
    save.lpstrDefExt = L"dsk";
    save.lpstrTitle = ui(L"Save new disk as", L"Neue Diskette speichern unter");
    save.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&save)) {
        const std::filesystem::path targetPath(target.data());
        std::error_code error;
        if (std::filesystem::equivalent(sourcePath, targetPath, error)) {
            MessageBoxW(app.window, ui(L"The new disk needs a name other than the template's.", L"Die neue Diskette braucht einen anderen Namen als die Vorlage."),
                        ui(L"New disk", L"Neue Diskette"), MB_OK | MB_ICONERROR);
        } else {
            const auto attributes = GetFileAttributesW(targetPath.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(targetPath.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
            }
            bool copied = CopyFileW(sourcePath.c_str(), targetPath.c_str(), FALSE) != FALSE;
            if (copied) {
                const auto copiedAttributes = GetFileAttributesW(targetPath.c_str());
                copied = copiedAttributes != INVALID_FILE_ATTRIBUTES &&
                         SetFileAttributesW(targetPath.c_str(), copiedAttributes & ~FILE_ATTRIBUTE_READONLY) != FALSE;
            }
            if (!copied) {
                MessageBoxW(app.window, ui(L"The disk could not be created.", L"Die Diskette konnte nicht angelegt werden."), ui(L"New disk", L"Neue Diskette"),
                            MB_OK | MB_ICONERROR);
            }
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The save dialog could not be opened.", L"Der Speicherdialog konnte nicht geoeffnet werden."), ui(L"New disk", L"Neue Diskette"),
                    MB_OK | MB_ICONERROR);
    }
    resumeAfterDialog(app);
}

void ejectCartridge(AppState& app)
{
    app.machine.ejectCartridge();
    app.cartridgePath.clear();
    restartWithCartridge(app);
}

std::vector<std::uint8_t> readWholeFile(const std::filesystem::path& path, std::size_t limit)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file ? file.tellg() : std::streampos(-1);
    if (size <= std::streampos(0) || size > static_cast<std::streampos>(limit)) return {};
    file.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return {};
    return bytes;
}

bool readOnlyFile(const std::filesystem::path& path)
{
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
}

svi806::SVI806* videoCard(AppState& app)
{
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        if (auto* card = dynamic_cast<svi806::SVI806*>(slotCard(app, slot))) return card;
    }
    return nullptr;
}

// SASI host adapter: built into the SVI-605B with hard disk, a card in the SVI-608M.
lanhd01::LANHD01* sasiCard(AppState& app)
{
    if (boardFitted(app)) {
        if (auto* expander = dynamic_cast<svi605bhd::SVI605BHD*>(app.expander.get())) return &expander->sasi();
    }
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        if (auto* card = dynamic_cast<lanhd01::LANHD01*>(slotCard(app, slot))) return card;
    }
    return nullptr;
}

// Hard disk images: sectors of 256 bytes in logical order, at most a full ST-212.
std::vector<std::uint8_t> readHddFile(const std::filesystem::path& path)
{
    auto image = readWholeFile(path, wd1002shd::St212Size);
    if (image.size() % 256 != 0) image.clear();
    return image;
}

void connectSerialCards(AppState& app)
{
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        if (auto* card = dynamic_cast<svi805::SVI805*>(slotCard(app, slot))) {
            card->setLink(app.comLink.isOpen() ? &app.comLink : nullptr);
        }
    }
}

std::unique_ptr<svi3x8::ExpansionPort> makeCard(AppState& app, int type)
{
    switch (type) {
    case CardRam807S2S3: return std::make_unique<svi807::SVI807>();
    case CardRam807S2: return std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, true, false, false, false});
    case CardRam807S3S4: return std::make_unique<svi807::SVI807>(svi807::SVI807Switches{false, false, true, true, false});
    case CardRam803: return std::make_unique<svi803::SVI803>();
    case CardRam807S1S5: return std::make_unique<svi807::SVI807>(svi807::SVI807Switches{true, false, false, false, true});
    case CardSerial805: return std::make_unique<svi805::SVI805>(app.comLink.isOpen() ? &app.comLink : nullptr);
    case CardVideo806: {
        auto card = std::make_unique<svi806::SVI806>();
        if (!card->loadCharacterRom(app.characterRom)) return nullptr;
        return card;
    }
    case CardSasi: {
        auto card = std::make_unique<lanhd01::LANHD01>();
        if (!app.hddPath.empty()) {
            auto image = readHddFile(app.hddPath);
            if (!image.empty()) card->controller().attachDisk(std::move(image));
        }
        return card;
    }
    case CardBoot: {
        auto card = std::make_unique<lanif01::LANIF01>();
        if (!card->loadRom(app.bootRom)) return nullptr;
        return card;
    }
    case CardSvi801: return std::make_unique<svi801::SVI801>();
    case CardSvi802: return std::make_unique<svi802::SVI802>();
    default: return nullptr;
    }
}

// Widens the window for the 80-column picture while an SVI-806 sits in the connected expander.
void applyLayout(AppState& app)
{
    const bool wanted = app.expanderConnected && videoCard(app) != nullptr;
    if (wanted == app.panelVisible) return;
    app.panelVisible = wanted;
    std::fill(app.panelPixels.begin(), app.panelPixels.end(), 0u);
    RECT frame{0, 0, ClientWidth + (wanted ? PanelWidth : 0), ClientHeight + app.statusHeight};
    AdjustWindowRect(&frame, app.windowStyle, TRUE);
    SetWindowPos(app.window, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(app.window, nullptr, FALSE);
}

void updatePanelPixels(AppState& app)
{
    auto* card = videoCard(app);
    if (!card) return;
    const auto frame = card->render();
    std::fill(app.panelPixels.begin(), app.panelPixels.end(), 0u);
    const int width = std::min(static_cast<int>(frame.width), PanelPictureWidth);
    const int height = std::min(static_cast<int>(frame.height), ClientHeight / PanelLineScale);
    const int left = PanelMargin + (PanelPictureWidth - width) / 2;
    const int top = (ClientHeight - height * PanelLineScale) / 2;
    for (int y = 0; y < height; ++y) {
        const auto* source = frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
        for (int line = 0; line < PanelLineScale; ++line) {
            auto* row = app.panelPixels.data() + static_cast<std::size_t>(top + y * PanelLineScale + line) * PanelWidth + left;
            for (int x = 0; x < width; ++x) row[x] = source[x] ? PanelLit : 0u;
        }
    }
}

void saveSettings(AppState& app)
{
    std::string text = "video=" + std::string(app.machine.videoStandard() == svi318::VideoStandard::Ntsc ? "NTSC" : "PAL") + "\n";
    text += std::string("language=") + (uiLanguage() == UiLanguage::German ? "de" : "en") + "\n";
    for (std::size_t port = 0; port < 2; ++port) {
        text += "joystick" + std::to_string(port + 1) + "=" +
                JoystickSourceNames[static_cast<std::size_t>(app.joystickSources[port])] + "\n";
    }
    text += "expander=" + std::string(app.expanderConnected ? "1" : "0") + "\n";
    text += "model=" + std::string(app.miniFitted ? "602" : app.superFitted ? "601" : svi608mFitted(app) ? "608M" : svi605bHdFitted(app) ? "605BHD" : "605B") + "\n";
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        text += "slot" + std::to_string(slot + 1) + "=" + std::to_string(app.slotCards[slot]) + "\n";
    }
    for (std::size_t drive = 0; drive < driveCount(app); ++drive) {
        if (!app.diskPaths[drive].empty()) {
            text += std::string("disk") + static_cast<char>('A' + drive) + "=" + app.diskPaths[drive].u8string() + "\n";
        }
    }
    if (!app.hddPath.empty()) text += "hdd=" + app.hddPath.u8string() + "\n";
    if (app.comLink.isOpen()) text += "comPort=" + std::filesystem::path(app.comLink.name()).u8string() + "\n";
    if (!app.printerDirectory.empty()) text += "printerDirectory=" + app.printerDirectory.u8string() + "\n";
    (void)writeFileAtomically(app.settingsPath, std::vector<std::uint8_t>(text.begin(), text.end()));
}

bool insertDiskFile(AppState& app, std::size_t drive, const std::filesystem::path& path)
{
    svi801::SviDisk disk;
    if (!disk.load(readWholeFile(path, svi801::SviDiskDoubleSidedSize))) return false;
    auto& floppy = driveAt(app, drive);
    floppy.insert(disk);
    floppy.setWriteProtected(readOnlyFile(path));
    app.diskPaths[drive] = path;
    app.diskWriteFailed[drive] = false;
    return true;
}

// Writes changed disks back to their files; new disks from formatting stay in memory.
void flushDisks(AppState& app)
{
    for (std::size_t drive = 0; drive < svi605b::DriveCount; ++drive) {
        auto& floppy = driveAt(app, drive);
        if (!floppy.hasDisk() || floppy.hasBlankDisk() || app.diskPaths[drive].empty() || !floppy.disk().modified()) continue;
        if (writeFileAtomically(app.diskPaths[drive], floppy.disk().image())) {
            floppy.disk().markSaved();
            app.diskWriteFailed[drive] = false;
        } else if (!app.diskWriteFailed[drive]) {
            app.diskWriteFailed[drive] = true;
            const std::wstring text = fillIn(ui(L"The disk in drive %1 could not be written to its file:\n", L"Die Diskette in Laufwerk %1 konnte nicht in ihre Datei geschrieben werden:\n"), std::wstring(1, driveLetter(app, drive))) + app.diskPaths[drive].wstring();
            MessageBoxW(app.window, text.c_str(), ui(L"Disk", L"Diskette"), MB_OK | MB_ICONERROR);
        }
    }
}

// Writes a changed hard disk back to its file; a read-only file keeps the changes in memory.
void flushHdd(AppState& app)
{
    auto* card = sasiCard(app);
    if (!card || app.hddPath.empty() || app.hddReadOnly || !card->controller().modified()) return;
    if (writeFileAtomically(app.hddPath, card->controller().image())) {
        card->controller().markSaved();
        app.hddWriteFailed = false;
    } else if (!app.hddWriteFailed) {
        app.hddWriteFailed = true;
        const std::wstring text = ui(L"The hard disk could not be written to its file:\n", L"Die Festplatte konnte nicht in ihre Datei geschrieben werden:\n") + app.hddPath.wstring();
        MessageBoxW(app.window, text.c_str(), ui(L"Hard disk", L"Festplatte"), MB_OK | MB_ICONERROR);
    }
}

bool saveDiskAs(AppState& app, std::size_t drive)
{
    std::vector<wchar_t> fileName(32768, L'\0');
    const std::wstring suggestion = std::wstring(ui(L"New_", L"Neu_")) + driveLetter(app, drive) + L".dsk";
    std::copy(suggestion.begin(), suggestion.end(), fileName.begin());
    const std::wstring folder = diskFolder().wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI disks (*.dsk)\0*.dsk\0\0", L"SVI-Disketten (*.dsk)\0*.dsk\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrInitialDir = folder.c_str();
    dialog.lpstrDefExt = L"dsk";
    dialog.lpstrTitle = ui(L"Save new disk as", L"Neue Diskette speichern unter");
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) return false;
    const std::filesystem::path path(fileName.data());
    auto& floppy = driveAt(app, drive);
    if (!writeFileAtomically(path, floppy.disk().image())) {
        MessageBoxW(app.window, ui(L"The disk could not be saved.", L"Die Diskette konnte nicht gespeichert werden."), ui(L"Disk", L"Diskette"), MB_OK | MB_ICONERROR);
        return false;
    }
    auto disk = floppy.disk();
    disk.markSaved();
    floppy.insert(disk);
    app.diskPaths[drive] = path;
    saveSettings(app);
    return true;
}

// A disk created by formatting an empty drive has no file yet: offer to save it.
bool confirmBlankDisk(AppState& app, std::size_t drive)
{
    if (!driveAt(app, drive).hasBlankDisk()) return true;
    KillTimer(app.window, EmulationTimer);
    const std::wstring text = fillIn(ui(L"Drive %1 holds a newly formatted disk without a file. Save it?", L"In Laufwerk %1 liegt eine neu formatierte Diskette ohne Datei. Speichern?"), std::wstring(1, driveLetter(app, drive)));
    const auto answer = MessageBoxW(app.window, text.c_str(), ui(L"Disk", L"Diskette"), MB_YESNOCANCEL | MB_ICONQUESTION);
    bool proceed = answer == IDNO;
    if (answer == IDYES) proceed = saveDiskAs(app, drive);
    resumeAfterDialog(app);
    return proceed;
}

bool cardAllowed(const AppState& app, std::size_t slot, int type);
void flushPrinter(AppState& app);

void updateExpanderMenu(AppState& app)
{
    const auto menu = GetMenu(app.window);
    if (!menu) return;
    const bool fitted608m = svi608mFitted(app);
    const UINT chosen = !app.expanderConnected ? ExpanderNoneCommand
        : app.miniFitted ? Expander602Command : app.superFitted ? Expander601Command
        : fitted608m ? Expander608mCommand : svi605bHdFitted(app) ? Expander605bHdCommand : Expander605bCommand;
    CheckMenuRadioItem(menu, ExpanderNoneCommand, Expander605bHdCommand, chosen, MF_BYCOMMAND);
    EnableMenuItem(menu, Expander608mCommand, MF_BYCOMMAND | (factoryRomsPresent(app) ? MF_ENABLED : MF_GRAYED));
    // Without the expander only the model can be chosen; drives, slots, hard disk, RS-232 and
    // printer follow the model and its cards.
    auto* sasi = sasiCard(app);
    bool printerPort = boardFitted(app);
    bool serialCard = false;
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        printerPort = printerPort || dynamic_cast<svi802::SVI802*>(slotCard(app, slot)) != nullptr;
        serialCard = serialCard || dynamic_cast<svi805::SVI805*>(slotCard(app, slot)) != nullptr;
    }
    const int items = GetMenuItemCount(app.expanderMenu);
    // Positions 0-5 choose the expander model; the drives, hard disk, slots and ports follow.
    for (int position = 6; position < items; ++position) {
        const HMENU popup = GetSubMenu(app.expanderMenu, position);
        if (popup == app.driveMenus[0]) {
            std::wstring text = ui(L"Drive &", L"Laufwerk &");
            text += driveLetter(app, 0);
            MENUITEMINFOW item{};
            item.cbSize = sizeof(item);
            item.fMask = MIIM_STRING;
            item.dwTypeData = text.data();
            SetMenuItemInfoW(app.expanderMenu, static_cast<UINT>(position), TRUE, &item);
        }
        bool enabled = app.expanderConnected;
        bool occupied = false;
        for (std::size_t drive = 0; drive < svi605b::DriveCount; ++drive) {
            if (popup == app.driveMenus[drive]) enabled = enabled && drive < driveCount(app);
        }
        for (std::size_t slot = 0; slot < MaxSlotCount; ++slot) {
            if (popup != app.slotMenus[slot]) continue;
            enabled = enabled && slot < slotCount(app);
            occupied = enabled && app.slotCards[slot] != CardEmpty;
        }
        // A slot holding a card shows its title in bold (default item state), so the fit is visible
        // without opening the slot's menu.
        MENUITEMINFOW state{};
        state.cbSize = sizeof(state);
        state.fMask = MIIM_STATE;
        if (GetMenuItemInfoW(app.expanderMenu, static_cast<UINT>(position), TRUE, &state)) {
            state.fState = occupied ? (state.fState | MFS_DEFAULT) : (state.fState & ~static_cast<UINT>(MFS_DEFAULT));
            SetMenuItemInfoW(app.expanderMenu, static_cast<UINT>(position), TRUE, &state);
        }
        if (popup == app.hddMenu) enabled = enabled && sasi != nullptr;
        if (GetMenuItemID(app.expanderMenu, position) == PrinterFolderCommand) enabled = enabled && printerPort;
        if (popup == app.comMenu) enabled = enabled && serialCard;
        EnableMenuItem(app.expanderMenu, static_cast<UINT>(position), MF_BYPOSITION | (enabled ? MF_ENABLED : MF_GRAYED));
    }
    EnableMenuItem(menu, HddEjectCommand,
                   MF_BYCOMMAND | (sasi && sasi->controller().hasDisk() ? MF_ENABLED : MF_GRAYED));
    for (UINT drive = 0; drive < svi605b::DriveCount; ++drive) {
        const auto& floppy = driveAt(app, drive);
        EnableMenuItem(menu, DriveEjectCommand + 10 * drive, MF_BYCOMMAND | (floppy.hasDisk() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(menu, DriveProtectCommand + 10 * drive, MF_BYCOMMAND | (floppy.hasDisk() ? MF_ENABLED : MF_GRAYED));
        CheckMenuItem(menu, DriveProtectCommand + 10 * drive,
                      MF_BYCOMMAND | (floppy.writeProtected() ? MF_CHECKED : MF_UNCHECKED));
    }
    for (UINT slot = 0; slot < MaxSlotCount; ++slot) {
        const UINT first = SlotCommandBase + SlotCommandStride * slot;
        UINT chosenPosition = 0;
        for (UINT position = 0; position < CardChoices.size(); ++position) {
            if (CardChoices[position].type == app.slotCards[slot]) chosenPosition = position;
        }
        CheckMenuRadioItem(app.slotMenus[slot], 0, static_cast<UINT>(CardChoices.size() - 1), chosenPosition, MF_BYPOSITION);
        for (const auto& choice : CardChoices) {
            EnableMenuItem(menu, first + static_cast<UINT>(choice.type),
                           MF_BYCOMMAND | (cardAllowed(app, slot, choice.type) ? MF_ENABLED : MF_GRAYED));
        }
    }
}

// A card fits in a slot of the model unless another slot already holds a card with fixed ports
// (SVI-805, SVI-806, SASI host adapter, boot card, SVI-801, SVI-802) of the same type, or a RAM card
// answering one of the same banks. The SASI host adapter and the boot card belong to the SVI-608M;
// SVI-801 and SVI-802 to the SVI-602 and SVI-601, whose SK6 takes only the SVI-801.
bool cardAllowed(const AppState& app, std::size_t slot, int type)
{
    if (type == CardEmpty) return true;
    if (slot >= slotCount(app)) return false;
    if (type == CardVideo806 && !characterRomPresent(app)) return false;
    if (type == CardBoot && !bootRomPresent(app)) return false;
    if ((type == CardSasi || type == CardBoot) && !svi608mFitted(app)) return false;
    if ((type == CardSvi801 || type == CardSvi802) && boardFitted(app)) return false;
    if (app.superFitted && (slot == svi601::DiskControllerSlot) != (type == CardSvi801)) return false;
    for (std::size_t other = 0; other < slotCount(app); ++other) {
        if (other == slot) continue;
        const int present = app.slotCards[other];
        if ((type == CardSerial805 || type == CardVideo806 || type == CardSasi || type == CardBoot ||
             type == CardSvi801 || type == CardSvi802) && present == type) return false;
        if ((cardBanks(type) & cardBanks(present)) != 0) return false;
    }
    return true;
}

void restartMachine(AppState& app)
{
    releaseAllInput(app);
    app.machine.connectExpansion(connectedDevice(app));
    app.machine.reset();
    applyLayout(app);
    updateExpanderMenu(app);
    updateWindowTitle(app);
    saveSettings(app);
}

void reportMissingRom(AppState& app, int type)
{
    MessageBoxW(app.window, type == CardBoot ? ui(L"The EPROM of the boot card (svi609.rom) is missing.", L"Das EPROM der Bootkarte (svi609.rom) fehlt.")
                                             : ui(L"The character ROM of the SVI-806 (svi806.rom) is missing.", L"Das Zeichen-ROM der SVI-806 (svi806.rom) fehlt."),
                L"Expander", MB_OK | MB_ICONERROR);
}

// Builds a new expander of the given model with the disks of the old one; the SVI-608M comes with
// its factory cards SVI-806, SASI host adapter and boot card.
// Inserts the disks of the saved paths into the current drives; paths without a drive are dropped.
void reloadDisks(AppState& app)
{
    for (std::size_t drive = 0; drive < svi605b::DriveCount; ++drive) {
        if (drive >= driveCount(app) || app.diskPaths[drive].empty() ||
            !insertDiskFile(app, drive, app.diskPaths[drive])) {
            app.diskPaths[drive].clear();
        }
    }
}

// Fits a new SVI-605B, SVI-605B with hard disk or SVI-608M (model: its menu command), the SVI-608M with
// its factory cards when factoryCards is set.
void replaceExpander(AppState& app, UINT model, bool factoryCards)
{
    app.machine.connectExpansion(nullptr);
    app.miniFitted = false;
    app.superFitted = false;
    if (model == Expander608mCommand) {
        app.expander = std::make_unique<svi608m::SVI608M>();
    } else if (model == Expander605bHdCommand) {
        app.expander = std::make_unique<svi605bhd::SVI605BHD>();
    } else {
        app.expander = std::make_unique<svi605b::SVI605B>();
    }
    app.slotCards.fill(CardEmpty);
    reloadDisks(app);
    if (!factoryCards || model != Expander608mCommand) return;
    const std::array<std::pair<std::size_t, int>, 3> factory = {{
        {svi608m::VideoSlot, CardVideo806}, {svi608m::SasiSlot, CardSasi}, {svi608m::BootSlot, CardBoot}}};
    for (const auto& [slot, type] : factory) {
        auto card = makeCard(app, type);
        if (!card) {
            reportMissingRom(app, type);
            continue;
        }
        placeCard(app, slot, std::move(card));
        app.slotCards[slot] = type;
    }
}

// Fits the SVI-602 (or the SVI-601) with empty slots; the unused expander board is replaced by a
// new one without disks.
void fitCardExpander(AppState& app, bool model601)
{
    app.machine.connectExpansion(nullptr);
    app.expander = std::make_unique<svi605b::SVI605B>();
    app.miniExpander = std::make_unique<svi602::SVI602>();
    app.superExpander = std::make_unique<svi601::SVI601>();
    app.miniFitted = !model601;
    app.superFitted = model601;
    app.slotCards.fill(CardEmpty);
}

// Connects no expander, an SVI-605B, an SVI-605B with hard disk, an SVI-608M, an SVI-602 or an
// SVI-601; changing the model fits a new one.
void selectExpander(AppState& app, UINT command)
{
    const bool connect = command != ExpanderNoneCommand;
    if (command == Expander608mCommand && !factoryRomsPresent(app)) return;
    const bool want608m = command == Expander608mCommand;
    const bool want605bHd = command == Expander605bHdCommand;
    const bool want602 = command == Expander602Command;
    const bool want601 = command == Expander601Command;
    const bool newModel = connect && (want602 != app.miniFitted || want601 != app.superFitted ||
                                      (!want602 && !want601 &&
                                       (want608m != svi608mFitted(app) || want605bHd != svi605bHdFitted(app))));
    if (connect == app.expanderConnected && !newModel) return;
    if (app.expanderConnected || newModel) {
        for (std::size_t drive = 0; drive < svi605b::DriveCount; ++drive) {
            if (!confirmBlankDisk(app, drive)) return;
        }
        flushDisks(app);
        flushHdd(app);
        flushPrinter(app);
    }
    if (newModel && (want602 || want601)) {
        fitCardExpander(app, want601);
    } else if (newModel) {
        replaceExpander(app, command, true);
    }
    app.expanderConnected = connect;
    restartMachine(app);
}

void setSlotCard(AppState& app, std::size_t slot, int type)
{
    if (app.slotCards[slot] == type || !cardAllowed(app, slot, type)) return;
    if (app.slotCards[slot] == CardSasi) flushHdd(app);
    if (app.slotCards[slot] == CardSvi801) {
        for (std::size_t drive = 0; drive < driveCount(app); ++drive) {
            if (!confirmBlankDisk(app, drive)) return;
        }
        flushDisks(app);
    }
    if (app.slotCards[slot] == CardSvi802) flushPrinter(app);
    auto card = makeCard(app, type);
    if (type != CardEmpty && !card) {
        reportMissingRom(app, type);
        return;
    }
    placeCard(app, slot, std::move(card));
    app.slotCards[slot] = type;
    if (type == CardSvi801) reloadDisks(app);
    restartMachine(app);
}

void selectDisk(AppState& app, std::size_t drive)
{
    if (drive >= driveCount(app) || !confirmBlankDisk(app, drive)) return;
    flushDisks(app);
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    const std::wstring folder = diskFolder().wstring();
    const std::wstring title = fillIn(ui(L"Insert disk in drive %1", L"Diskette in Laufwerk %1 einlegen"), std::wstring(1, driveLetter(app, drive)));
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI disks (*.dsk)\0*.dsk\0All files (*.*)\0*.*\0\0", L"SVI-Disketten (*.dsk)\0*.dsk\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrInitialDir = folder.c_str();
    dialog.lpstrTitle = title.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        if (insertDiskFile(app, drive, std::filesystem::path(fileName.data()))) {
            saveSettings(app);
            updateWindowTitle(app);
        } else {
            MessageBoxW(app.window, ui(L"The file is not an SVI disk image (172032 or 346112 bytes).", L"Die Datei ist kein SVI-Diskettenimage (172032 oder 346112 Byte)."),
                        ui(L"Disk", L"Diskette"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"Disk", L"Diskette"),
                    MB_OK | MB_ICONERROR);
    }
    updateExpanderMenu(app);
    resumeAfterDialog(app);
}

void ejectDisk(AppState& app, std::size_t drive)
{
    if (!confirmBlankDisk(app, drive)) return;
    flushDisks(app);
    driveAt(app, drive).eject();
    app.diskPaths[drive].clear();
    saveSettings(app);
    updateExpanderMenu(app);
}

void toggleWriteProtect(AppState& app, std::size_t drive)
{
    auto& floppy = driveAt(app, drive);
    if (!floppy.hasDisk()) return;
    if (floppy.writeProtected() && !app.diskPaths[drive].empty() && readOnlyFile(app.diskPaths[drive])) {
        MessageBoxW(app.window, ui(L"The disk file is read-only. For a writable copy use System > New disk.", L"Die Datei der Diskette ist schreibgeschuetzt. Fuer eine beschreibbare Kopie: "
                    L"System > Neue Diskette."), ui(L"Disk", L"Diskette"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    floppy.setWriteProtected(!floppy.writeProtected());
    updateExpanderMenu(app);
}

bool attachHddFile(AppState& app, const std::filesystem::path& path)
{
    auto image = readHddFile(path);
    if (image.empty()) return false;
    app.hddPath = path;
    app.hddReadOnly = readOnlyFile(path);
    app.hddWriteFailed = false;
    if (auto* card = sasiCard(app)) card->controller().attachDisk(std::move(image));
    return true;
}

void selectHdd(AppState& app)
{
    flushHdd(app);
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    const std::wstring folder = mediaFolder(L"SVIHDD").wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI hard disks (*.hdd;*.img)\0*.hdd;*.img\0All files (*.*)\0*.*\0\0", L"SVI-Festplatten (*.hdd;*.img)\0*.hdd;*.img\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrInitialDir = folder.c_str();
    dialog.lpstrTitle = ui(L"Choose hard disk image", L"Festplatten-Image waehlen");
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        if (attachHddFile(app, std::filesystem::path(fileName.data()))) {
            saveSettings(app);
            updateWindowTitle(app);
        } else {
            MessageBoxW(app.window, ui(L"The file is not a hard disk image (sectors of 256 bytes, at most 10027008 bytes).", L"Die Datei ist kein Festplatten-Image (Sektoren zu 256 Byte, hoechstens 10027008 Byte)."),
                        ui(L"Hard disk", L"Festplatte"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"Hard disk", L"Festplatte"),
                    MB_OK | MB_ICONERROR);
    }
    updateExpanderMenu(app);
    resumeAfterDialog(app);
}

// "Neue Festplatte": a blank ST-212 (E5h throughout) under a new name; CP/M 2.27 uses it as is.
void createHdd(AppState& app)
{
    flushHdd(app);
    KillTimer(app.window, EmulationTimer);
    std::vector<wchar_t> fileName(32768, L'\0');
    const std::wstring suggestion = ui(L"HardDisk.hdd", L"Festplatte.hdd");
    std::copy(suggestion.begin(), suggestion.end(), fileName.begin());
    const std::wstring folder = mediaFolder(L"SVIHDD").wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI hard disks (*.hdd)\0*.hdd\0\0", L"SVI-Festplatten (*.hdd)\0*.hdd\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrInitialDir = folder.c_str();
    dialog.lpstrDefExt = L"hdd";
    dialog.lpstrTitle = ui(L"Save new hard disk as", L"Neue Festplatte speichern unter");
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        if (writeFileAtomically(path, std::vector<std::uint8_t>(wd1002shd::St212Size, 0xE5)) && attachHddFile(app, path)) {
            saveSettings(app);
            updateWindowTitle(app);
        } else {
            MessageBoxW(app.window, ui(L"The hard disk could not be created.", L"Die Festplatte konnte nicht angelegt werden."), ui(L"Hard disk", L"Festplatte"), MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The save dialog could not be opened.", L"Der Speicherdialog konnte nicht geoeffnet werden."), ui(L"Hard disk", L"Festplatte"), MB_OK | MB_ICONERROR);
    }
    updateExpanderMenu(app);
    resumeAfterDialog(app);
}

void ejectHdd(AppState& app)
{
    flushHdd(app);
    if (auto* card = sasiCard(app)) card->controller().detachDisk();
    app.hddPath.clear();
    app.hddReadOnly = false;
    saveSettings(app);
    updateExpanderMenu(app);
    updateWindowTitle(app);
}

void refreshComMenu(AppState& app)
{
    while (GetMenuItemCount(app.comMenu) > 0) DeleteMenu(app.comMenu, 0, MF_BYPOSITION);
    app.comPorts = svi805::ComPortLink::availablePorts();
    AppendMenuW(app.comMenu, MF_STRING, ComPortNoneCommand, ui(L"&No COM port", L"&kein COM-Port"));
    UINT checked = ComPortNoneCommand;
    for (std::size_t i = 0; i < app.comPorts.size() && ComPortNoneCommand + 1 + i <= ComPortLastCommand; ++i) {
        const auto id = static_cast<UINT>(ComPortNoneCommand + 1 + i);
        AppendMenuW(app.comMenu, MF_STRING, id, app.comPorts[i].c_str());
        if (app.comLink.isOpen() && app.comLink.name() == app.comPorts[i]) checked = id;
    }
    CheckMenuRadioItem(app.comMenu, ComPortNoneCommand, ComPortNoneCommand + static_cast<UINT>(app.comPorts.size()),
                       checked, MF_BYCOMMAND);
}

void selectComPort(AppState& app, UINT command)
{
    app.comLink.close();
    if (command > ComPortNoneCommand && command - ComPortNoneCommand - 1 < app.comPorts.size()) {
        const auto& name = app.comPorts[command - ComPortNoneCommand - 1];
        if (!app.comLink.open(name)) {
            const std::wstring text = fillIn(ui(L"%1 could not be opened (in use or not present).", L"%1 konnte nicht geoeffnet werden (belegt oder nicht vorhanden)."), name);
            MessageBoxW(app.window, text.c_str(), L"RS-232", MB_OK | MB_ICONERROR);
        }
    }
    connectSerialCards(app);
    saveSettings(app);
    updateWindowTitle(app);
}

std::filesystem::path printerFolder(const AppState& app)
{
    if (!app.printerDirectory.empty()) return app.printerDirectory;
    return mediaFolder(L"SVIPRINT");
}

// Printer output goes to a text file; a new file starts after a pause of PrinterIdleMs.
void flushPrinter(AppState& app)
{
    std::vector<printer::Printer*> printers{&app.expander->printer()};
    for (std::size_t slot = 0; slot < slotCount(app); ++slot) {
        if (auto* card = dynamic_cast<lanif01::LanCard*>(slotCard(app, slot))) printers.push_back(&card->printerPort().printer());
        if (auto* card = dynamic_cast<svi802::SVI802*>(slotCard(app, slot))) printers.push_back(&card->port().printer());
    }
    if (std::none_of(printers.begin(), printers.end(), [](const auto* device) { return !device->output().empty(); })) return;
    const auto now = GetTickCount64();
    if (app.printerFile.empty() || now - app.printerLastByte > PrinterIdleMs) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t name[64];
        swprintf(name, 64, L"SVI318_%04u%02u%02u_%02u%02u%02u.txt", time.wYear, time.wMonth, time.wDay, time.wHour,
                 time.wMinute, time.wSecond);
        app.printerFile = printerFolder(app) / name;
    }
    std::ofstream file(app.printerFile, std::ios::binary | std::ios::app);
    for (auto* device : printers) {
        const auto& output = device->output();
        file.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
        device->clearOutput();
    }
    app.printerLastByte = now;
}

void selectPrinterFolder(AppState& app)
{
    KillTimer(app.window, EmulationTimer);
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        IFileOpenDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            DWORD options = 0;
            (void)dialog->GetOptions(&options);
            (void)dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            (void)dialog->SetTitle(ui(L"Choose the folder for printer output", L"Ablageordner fuer Druckdateien waehlen"));
            IShellItem* initial = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(printerFolder(app).c_str(), nullptr, IID_PPV_ARGS(&initial)))) {
                (void)dialog->SetFolder(initial);
                initial->Release();
            }
            if (SUCCEEDED(dialog->Show(app.window))) {
                IShellItem* selected = nullptr;
                if (SUCCEEDED(dialog->GetResult(&selected))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(selected->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                        app.printerDirectory = path;
                        app.printerFile.clear();
                        CoTaskMemFree(path);
                        saveSettings(app);
                    }
                    selected->Release();
                }
            }
            dialog->Release();
        }
        CoUninitialize();
    }
    resumeAfterDialog(app);
}

// Fits the machine for a video standard (TMS9929A for PAL, TMS9918A for NTSC) and puts the BASIC
// ROM, the module, the cassette and the expander back in.
void rebuildMachine(AppState& app, svi318::VideoStandard standard)
{
    const auto tape = app.machine.cassetteImage();
    const bool tapeInserted = app.machine.cassetteState().inserted && !app.machine.cassetteState().recording;
    app.machine = svi318::SVI318(standard);
    (void)app.machine.loadRom(app.romImage);
    if (!app.cartridgePath.empty() &&
        !app.machine.insertCartridge(readWholeFile(app.cartridgePath, svi318::CartridgeMaxSize))) {
        app.cartridgePath.clear();
    }
    if (tapeInserted && !app.machine.loadCassette(tape)) app.cassettePath.clear();
    if (!tapeInserted) app.cassettePath.clear();
    app.machine.connectExpansion(connectedDevice(app));
}

void updateVideoMenu(AppState& app)
{
    const auto ntsc = app.machine.videoStandard() == svi318::VideoStandard::Ntsc;
    CheckMenuRadioItem(GetMenu(app.window), VideoPalCommand, VideoNtscCommand,
                       ntsc ? VideoNtscCommand : VideoPalCommand, MF_BYCOMMAND);
}

// Switching the video standard is like switching on another machine: it restarts.
void selectVideoStandard(AppState& app, svi318::VideoStandard standard)
{
    if (app.machine.videoStandard() == standard || !confirmCassetteDiscard(app)) return;
    releaseAllInput(app);
    rebuildMachine(app, standard);
    app.machine.reset();
    app.fieldPhase = 0;
    app.audioSamplePhase = 0;
    updateVideoMenu(app);
    updateCassetteMenu(app);
    updateCartridgeMenu(app);
    saveSettings(app);
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
    saveSettings(app);
}

// A snapshot holds the plain SVI-318 without expander; loading it disconnects the expander like
// "kein Expander" and switches to the snapshot's video standard. BASIC ROM, module and cassette
// stay in place.
bool loadStateFile(AppState& app, const std::filesystem::path& path)
{
    const auto state = readWholeFile(path, 16 * 1024 * 1024);
    const auto standard = svi318::SVI318::stateVideoStandard(state);
    if (!standard) {
        MessageBoxW(app.window, ui(L"The file is not an SVI-318 state.", L"Die Datei ist kein SVI-318-Zustand."), ui(L"Load state", L"Zustand laden"), MB_OK | MB_ICONERROR);
        return false;
    }
    if (app.expanderConnected) {
        selectExpander(app, ExpanderNoneCommand);
        if (app.expanderConnected) return false;
    }
    releaseAllInput(app);
    if (app.machine.videoStandard() != *standard) {
        rebuildMachine(app, *standard);
        app.machine.reset();
        updateVideoMenu(app);
        updateCassetteMenu(app);
        updateCartridgeMenu(app);
        saveSettings(app);
    }
    if (!app.machine.loadState(state)) {
        MessageBoxW(app.window, ui(L"The state does not fit this emulator (other version).", L"Der Zustand passt nicht zu diesem Emulator (andere Version)."), ui(L"Load state", L"Zustand laden"),
                    MB_OK | MB_ICONERROR);
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
    std::vector<wchar_t> fileName(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"SVI-318 states (*.sst)\0*.sst\0All files (*.*)\0*.*\0\0", L"SVI-318-Zustaende (*.sst)\0*.sst\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = save ? ui(L"Save state", L"Zustand speichern") : ui(L"Load state", L"Zustand laden");
    dialog.lpstrDefExt = L"sst";
    const std::wstring stateFolder = mediaFolder(L"SVISTATE").wstring();
    dialog.lpstrInitialDir = stateFolder.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) {
        const std::filesystem::path path(fileName.data());
        if (!save) {
            (void)loadStateFile(app, path);
        } else if (!writeFileAtomically(path, app.machine.saveState())) {
            MessageBoxW(app.window, ui(L"The state could not be saved.", L"Der Zustand konnte nicht gespeichert werden."), ui(L"Save state", L"Zustand speichern"),
                        MB_OK | MB_ICONERROR);
        }
    } else if (CommDlgExtendedError() != 0) {
        MessageBoxW(app.window, ui(L"The file dialog could not be opened.", L"Der Dateiauswahldialog konnte nicht geoeffnet werden."), ui(L"State", L"Zustand"),
                    MB_OK | MB_ICONERROR);
    }
    resumeAfterDialog(app);
}

void updateVideoRecordMenu(AppState& app)
{
    const auto menu = GetMenu(app.window);
    EnableMenuItem(menu, VideoRecordCommand, MF_BYCOMMAND | (app.videoRecording ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, VideoStopCommand, MF_BYCOMMAND | (app.videoRecording ? MF_ENABLED : MF_GRAYED));
    DrawMenuBar(app.window);
}

// Records picture and sound of every field into an MP4 file (video media folder) until stopped. The
// picture is the window picture, with the SVI-806 panel when it is shown at the start.
void startVideoRecording(AppState& app)
{
    if (app.videoRecording) return;
    KillTimer(app.window, EmulationTimer);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    // The suggested name carries the folder; otherwise Windows opens the folder used last.
    const std::wstring videoFolder = mediaFolder(L"SVIVIDEO").wstring();
    std::vector<wchar_t> fileName(32768, L'\0');
    swprintf(fileName.data(), fileName.size(), L"%ls\\SVI318_%04u%02u%02u_%02u%02u%02u.mp4", videoFolder.c_str(),
             time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = ui(L"MP4 video (*.mp4)\0*.mp4\0All files (*.*)\0*.*\0\0", L"MP4-Video (*.mp4)\0*.mp4\0Alle Dateien (*.*)\0*.*\0\0");
    dialog.lpstrFile = fileName.data();
    dialog.nMaxFile = static_cast<DWORD>(fileName.size());
    dialog.lpstrTitle = ui(L"Record video", L"Video aufnehmen");
    dialog.lpstrDefExt = L"mp4";
    dialog.lpstrInitialDir = videoFolder.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&dialog)) {
        videorec::Settings settings;
        settings.path = fileName.data();
        app.recordPanel = app.panelVisible;
        settings.width = ClientWidth + (app.recordPanel ? PanelWidth : 0);
        settings.height = ClientHeight;
        settings.frameRateNumerator = svi318::VdpMasterClockHz;
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
    resumeAfterDialog(app);
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

void loadSettings(AppState& app)
{
    std::ifstream file(app.settingsPath, std::ios::binary);
    std::string line;
    bool missing = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const auto key = line.substr(0, equals);
        const auto value = line.substr(equals + 1);
        if ((key == "joystick1" || key == "joystick2")) {
            const auto port = static_cast<std::size_t>(key[8] - '1');
            for (int source = 0; source < JoystickSourceCount; ++source) {
                if (value == JoystickSourceNames[static_cast<std::size_t>(source)]) app.joystickSources[port] = source;
            }
            if (app.joystickSources[port] == JoystickController) probeController(app, port);
        } else if (key == "video") {
            if (value == "NTSC") rebuildMachine(app, svi318::VideoStandard::Ntsc);
        } else if (key == "expander") {
            app.expanderConnected = value == "1";
        } else if (key == "model") {
            if (value == "608M" && !factoryRomsPresent(app)) {
                if (!characterRomPresent(app)) noteMissingRom(app, L"svi806.rom (SVI-806)");
                if (!bootRomPresent(app)) noteMissingRom(app, L"svi609.rom (LAN-IF-01)");
            } else if (value == "608M") {
                replaceExpander(app, Expander608mCommand, false);
            }
            if (value == "605BHD") replaceExpander(app, Expander605bHdCommand, false);
            if (value == "602" || value == "601") fitCardExpander(app, value == "601");
        } else if (key.size() == 5 && key.compare(0, 4, "slot") == 0 && key[4] >= '1' && key[4] <= '7') {
            const auto slot = static_cast<std::size_t>(key[4] - '1');
            const int type = std::atoi(value.c_str());
            if (type == CardVideo806 && !characterRomPresent(app)) noteMissingRom(app, L"svi806.rom (SVI-806)");
            if ((type == CardBoot) && !bootRomPresent(app)) noteMissingRom(app, L"svi609.rom (LAN-IF-01)");
            auto card = makeCard(app, type);
            if (type > CardEmpty && type < CardTypeCount && card && cardAllowed(app, slot, type)) {
                placeCard(app, slot, std::move(card));
                app.slotCards[slot] = type;
            }
        } else if (key == "diskA" || key == "diskB") {
            const std::size_t drive = key == "diskA" ? 0 : 1;
            if (drive < driveCount(app) && !insertDiskFile(app, drive, std::filesystem::u8path(value))) missing = true;
        } else if (key == "hdd") {
            if (!attachHddFile(app, std::filesystem::u8path(value))) missing = true;
        } else if (key == "comPort") {
            (void)app.comLink.open(std::filesystem::u8path(value).wstring());
        } else if (key == "printerDirectory") {
            app.printerDirectory = std::filesystem::u8path(value);
        }
    }
    connectSerialCards(app);
    app.machine.connectExpansion(connectedDevice(app));
    if (missing) {
        MessageBoxW(nullptr, ui(L"At least one saved disk or hard disk was not found.", L"Mindestens eine gespeicherte Diskette oder Festplatte wurde nicht gefunden."), L"SVI-318",
                    MB_OK | MB_ICONWARNING);
    }
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
    if (app.panelVisible) {
        info.bmiHeader.biWidth = PanelWidth;
        StretchDIBits(dc, ClientWidth, 0, PanelWidth, ClientHeight, 0, 0, PanelWidth, ClientHeight,
                      app.panelPixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    }
    EndPaint(window, &paintState);
}

// Switches the frontend language: rebuilds the menu bar, refreshes menus and status bar and keeps the
// choice in the settings.
void setLanguage(AppState& app, UiLanguage language)
{
    if (uiLanguage() == language) return;
    uiLanguage() = language;
    const HMENU previous = GetMenu(app.window);
    const HMENU menu = buildMenu(app);
    if (!menu) return;
    SetMenu(app.window, menu);
    if (previous) DestroyMenu(previous);
    updateCassetteMenu(app);
    updateCartridgeMenu(app);
    updateExpanderMenu(app);
    updateVideoMenu(app);
    updateJoystickMenu(app);
    updateWindowTitle(app);
    saveSettings(app);
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
        case NewDiskCommand:
            createDiskCopy(*app);
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
        case VideoPalCommand:
        case VideoNtscCommand:
            selectVideoStandard(*app, LOWORD(wParam) == VideoNtscCommand ? svi318::VideoStandard::Ntsc
                                                                         : svi318::VideoStandard::Pal);
            return 0;
        case ExpanderNoneCommand:
        case Expander605bCommand:
        case Expander605bHdCommand:
        case Expander608mCommand:
        case Expander602Command:
        case Expander601Command:
            selectExpander(*app, LOWORD(wParam));
            return 0;
        case HddInsertCommand:
            if (!app->expanderConnected || !sasiCard(*app)) return 0;
            selectHdd(*app);
            return 0;
        case HddNewCommand:
            if (!app->expanderConnected || !sasiCard(*app)) return 0;
            createHdd(*app);
            return 0;
        case HddEjectCommand:
            if (!app->expanderConnected) return 0;
            ejectHdd(*app);
            return 0;
        case DriveInsertCommand:
        case DriveInsertCommand + 10:
            if (!app->expanderConnected) return 0;
            selectDisk(*app, (LOWORD(wParam) - DriveInsertCommand) / 10);
            return 0;
        case DriveEjectCommand:
        case DriveEjectCommand + 10:
            if (!app->expanderConnected) return 0;
            ejectDisk(*app, (LOWORD(wParam) - DriveEjectCommand) / 10);
            return 0;
        case DriveProtectCommand:
        case DriveProtectCommand + 10:
            if (!app->expanderConnected) return 0;
            toggleWriteProtect(*app, (LOWORD(wParam) - DriveProtectCommand) / 10);
            return 0;
        case PrinterFolderCommand:
            if (!app->expanderConnected) return 0;
            selectPrinterFolder(*app);
            return 0;
        case ResetCommand:
            releaseAllInput(*app);
            app->machine.reset();
            return 0;
        case CassetteOpenCommand:
            selectCassetteImage(*app);
            return 0;
        case CartridgeInsertCommand:
            selectCartridge(*app);
            return 0;
        case CartridgeEjectCommand:
            ejectCartridge(*app);
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
        default:
            if (!app->expanderConnected) break;
            if (LOWORD(wParam) >= SlotCommandBase && LOWORD(wParam) < SlotCommandBase + SlotCommandStride * MaxSlotCount &&
                (LOWORD(wParam) - SlotCommandBase) % SlotCommandStride < CardTypeCount) {
                setSlotCard(*app, (LOWORD(wParam) - SlotCommandBase) / SlotCommandStride,
                            (LOWORD(wParam) - SlotCommandBase) % SlotCommandStride);
                return 0;
            }
            if (LOWORD(wParam) >= ComPortNoneCommand && LOWORD(wParam) <= ComPortLastCommand) {
                selectComPort(*app, LOWORD(wParam));
                return 0;
            }
            break;
        }
        break;
    case WM_INITMENUPOPUP:
        if (app && reinterpret_cast<HMENU>(wParam) == app->comMenu) refreshComMenu(*app);
        break;
    case WM_KEYDOWN:
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
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (app && wParam < app->keyDown.size()) updateInput(*app, wParam, lParam, false);
        return 0;
    case WM_KILLFOCUS:
        if (app) releaseAllInput(*app);
        return 0;
    case WM_TIMER:
        if (app && wParam == EmulationTimer) {
            (void)retryAudio(app->audio);
            const bool advanced = advanceWallClock(*app);
            if (GetTickCount64() - app->titleTick >= TitleRefreshIntervalMs) {
                updateCassetteMenu(*app);
                updateExpanderMenu(*app);
                flushPrinter(*app);
                if (app->videoRecording && !app->recorder.recording()) stopVideoRecording(*app);
            }
            if (GetTickCount64() - app->diskFlushTick >= DiskFlushIntervalMs) {
                app->diskFlushTick = GetTickCount64();
                flushDisks(*app);
                flushHdd(*app);
            }
            if (advanced) {
                updateDisplayPixels(*app, app->machine.video().scanoutFrame());
                if (app->panelVisible) updatePanelPixels(*app);
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
        if (app) {
            for (std::size_t drive = 0; drive < svi605b::DriveCount; ++drive) {
                if (!confirmBlankDisk(*app, drive)) return 0;
            }
            flushDisks(*app);
            flushHdd(*app);
            flushPrinter(*app);
            saveSettings(*app);
            stopVideoRecording(*app);
        }
        break;
    case WM_DESTROY:
        KillTimer(window, EmulationTimer);
        if (app) stopAudio(app->audio);
        if (app && app->timerResolutionRaised) timeEndPeriod(1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool loadRom(AppState& app, const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    const std::istreambuf_iterator<char> romEnd{};
    app.romImage.assign(std::istreambuf_iterator<char>{file}, romEnd);
    return app.romImage.size() == svi318::RomSize && app.machine.loadRom(app.romImage);
}

std::filesystem::path executableDirectory()
{
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

// Menu bar in the frontend language.
HMENU buildMenu(AppState& app)
{
    const HMENU menu = CreateMenu();
    const HMENU systemMenu = CreatePopupMenu();
    const HMENU cassetteMenu = CreatePopupMenu();
    const HMENU cartridgeMenu = CreatePopupMenu();
    app.expanderMenu = CreatePopupMenu();
    app.comMenu = CreatePopupMenu();
    if (!menu || !systemMenu || !cassetteMenu || !cartridgeMenu || !app.expanderMenu || !app.comMenu) {
        if (menu) DestroyMenu(menu);
        if (systemMenu) DestroyMenu(systemMenu);
        if (cassetteMenu) DestroyMenu(cassetteMenu);
        if (cartridgeMenu) DestroyMenu(cartridgeMenu);
        return nullptr;
    }
    AppendMenuW(systemMenu, MF_STRING, ResetCommand, L"&Reset");
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, NewDiskCommand, ui(L"New &disk...", L"Neue &Diskette..."));
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, StateLoadCommand, ui(L"&Load state...", L"Zustand &laden..."));
    AppendMenuW(systemMenu, MF_STRING, StateSaveCommand, ui(L"&Save state...", L"Zustand &speichern..."));
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING | (app.videoRecording ? MF_GRAYED : 0u), VideoRecordCommand,
                ui(L"Record &video...", L"&Video aufnehmen..."));
    AppendMenuW(systemMenu, MF_STRING | (app.videoRecording ? 0u : MF_GRAYED), VideoStopCommand,
                ui(L"S&top video", L"Video &beenden"));
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, VideoPalCommand, L"&PAL (TMS9929A, 50 Hz)");
    AppendMenuW(systemMenu, MF_STRING, VideoNtscCommand, L"&NTSC (TMS9918A, 60 Hz)");
    AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(systemMenu, MF_STRING, LanguageEnglishCommand, L"&English");
    AppendMenuW(systemMenu, MF_STRING, LanguageGermanCommand, L"&Deutsch");
    CheckMenuRadioItem(systemMenu, LanguageEnglishCommand, LanguageGermanCommand,
                       uiLanguage() == UiLanguage::German ? LanguageGermanCommand : LanguageEnglishCommand, MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(systemMenu), L"&System");
    AppendMenuW(cassetteMenu, MF_STRING, CassetteOpenCommand, ui(L"&Open image...", L"Image &oeffnen..."));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteRewindCommand, ui(L"&Rewind", L"&Zurueckspulen"));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteEjectCommand, ui(L"&Eject", L"&Auswerfen"));
    AppendMenuW(cassetteMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(cassetteMenu, MF_STRING, CassetteRecordCommand, ui(L"&New recording", L"Neue &Aufnahme"));
    AppendMenuW(cassetteMenu, MF_STRING | MF_GRAYED, CassetteSaveCommand, ui(L"&Stop recording and save...", L"Aufnahme &beenden und speichern..."));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(cassetteMenu), ui(L"&Cassette", L"&Kassette"));
    AppendMenuW(cartridgeMenu, MF_STRING, CartridgeInsertCommand, ui(L"&Insert module...", L"Modul &einstecken..."));
    AppendMenuW(cartridgeMenu, MF_STRING | MF_GRAYED, CartridgeEjectCommand, ui(L"&Remove module", L"Modul &entfernen"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(cartridgeMenu), ui(L"&Module", L"&Modul"));
    AppendMenuW(app.expanderMenu, MF_STRING, ExpanderNoneCommand, ui(L"&No expander", L"&kein Expander"));
    AppendMenuW(app.expanderMenu, MF_STRING, Expander601Command, ui(L"SVI-60&1 Super Expander (7 slots)", L"SVI-60&1 Super Expander (7 Steckplaetze)"));
    AppendMenuW(app.expanderMenu, MF_STRING, Expander602Command, ui(L"SVI-60&2 Mini Expander (1 slot)", L"SVI-60&2 Mini Expander (1 Steckplatz)"));
    AppendMenuW(app.expanderMenu, MF_STRING, Expander605bCommand, L"SVI-60&5B");
    AppendMenuW(app.expanderMenu, MF_STRING, Expander605bHdCommand, ui(L"SVI-605B with hard dis&k", L"SVI-605B mit Fe&stplatte"));
    AppendMenuW(app.expanderMenu, MF_STRING, Expander608mCommand, ui(L"SVI-60&8M (hard disk)", L"SVI-60&8M (Festplatte)"));
    AppendMenuW(app.expanderMenu, MF_SEPARATOR, 0, nullptr);
    for (UINT drive = 0; drive < svi605b::DriveCount; ++drive) {
        const HMENU driveMenu = CreatePopupMenu();
        app.driveMenus[drive] = driveMenu;
        AppendMenuW(driveMenu, MF_STRING, DriveInsertCommand + 10 * drive, ui(L"&Insert disk...", L"Diskette &einlegen..."));
        AppendMenuW(driveMenu, MF_STRING | MF_GRAYED, DriveEjectCommand + 10 * drive, ui(L"&Eject", L"&Auswerfen"));
        AppendMenuW(driveMenu, MF_STRING | MF_GRAYED, DriveProtectCommand + 10 * drive, ui(L"&Write protection", L"&Schreibschutz"));
        AppendMenuW(app.expanderMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(driveMenu),
                    drive == 0 ? ui(L"Drive &A", L"Laufwerk &A") : ui(L"Drive &B", L"Laufwerk &B"));
    }
    app.hddMenu = CreatePopupMenu();
    AppendMenuW(app.hddMenu, MF_STRING, HddInsertCommand, ui(L"&Choose image...", L"Image &waehlen..."));
    AppendMenuW(app.hddMenu, MF_STRING, HddNewCommand, ui(L"&New hard disk...", L"&Neue Festplatte..."));
    AppendMenuW(app.hddMenu, MF_STRING | MF_GRAYED, HddEjectCommand, ui(L"&Remove", L"&Entfernen"));
    AppendMenuW(app.expanderMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(app.hddMenu), ui(L"&Hard disk", L"&Festplatte"));
    AppendMenuW(app.expanderMenu, MF_SEPARATOR, 0, nullptr);
    for (UINT slot = 0; slot < MaxSlotCount; ++slot) {
        const HMENU slotMenu = CreatePopupMenu();
        app.slotMenus[slot] = slotMenu;
        for (const auto& choice : CardChoices) {
            AppendMenuW(slotMenu, MF_STRING, SlotCommandBase + SlotCommandStride * slot + static_cast<UINT>(choice.type),
                        ui(choice.name, choice.germanName));
        }
        const std::wstring slotTitle = ui(L"Slot &", L"Steckplatz &") + std::to_wstring(slot + 1);
        AppendMenuW(app.expanderMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(slotMenu), slotTitle.c_str());
    }
    AppendMenuW(app.expanderMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(app.expanderMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(app.comMenu), L"&RS-232 (SVI-805)");
    AppendMenuW(app.expanderMenu, MF_STRING, PrinterFolderCommand, ui(L"&Printer folder...", L"&Druckerordner waehlen..."));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(app.expanderMenu), L"&Expander");
    const HMENU joystickMenu = CreatePopupMenu();
    for (UINT port = 0; port < 2; ++port) {
        const HMENU portMenu = CreatePopupMenu();
        const UINT first = JoystickCommandBase + 10 * port;
        AppendMenuW(portMenu, MF_STRING, first + JoystickOff, ui(L"&off", L"&aus"));
        AppendMenuW(portMenu, MF_STRING, first + JoystickKeyboard,
                    port == 0 ? ui(L"&Keyboard (arrows + C)", L"&Tastatur (Pfeile + C)") : ui(L"&Keyboard (W A S D + Z)", L"&Tastatur (W A S D + Y)"));
        AppendMenuW(portMenu, MF_STRING, first + JoystickController,
                    port == 0 ? ui(L"PC &game controller 1", L"PC-&Gamecontroller 1") : ui(L"PC &game controller 2", L"PC-&Gamecontroller 2"));
        AppendMenuW(joystickMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(portMenu),
                    port == 0 ? ui(L"Port &1", L"Anschluss &1") : ui(L"Port &2", L"Anschluss &2"));
    }
    AppendMenuW(joystickMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(joystickMenu, MF_STRING, KeyboardJoystickToggleCommand, ui(L"Keyboard joystick &active	F11", L"Tastatur-Joystick &aktiv	F11"));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(joystickMenu), L"&Joystick");
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
    // svi318.exe [basic.rom] [-state file.sst]
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
    static AppState app;
    createUserFolders();
    app.soundPortTransitions.reserve(16);
    if (!QueryPerformanceFrequency(&app.counterFrequency) ||
        !QueryPerformanceCounter(&app.lastCounter)) {
        MessageBoxW(nullptr, ui(L"The high-resolution system clock could not be initialised.", L"Die hochaufloesende Systemuhr konnte nicht initialisiert werden."),
                    L"SVI-318", MB_OK | MB_ICONERROR);
        return 1;
    }
    const auto appDirectory = executableDirectory();
    loadLanguage(appDirectory / SettingsFileName);
    const std::filesystem::path romPath = !romArgument.empty()
        ? std::filesystem::path(romArgument)
        : findRom(sourceDirectory() / L"SVI318" / L"rom", L"svi111.rom", svi318::RomSize);
    if (romPath.empty() || !loadRom(app, romPath)) {
        std::wstring text = ui(L"The SVI-318 ROM (svi111.rom, 32 KiB) is missing. Place it in the ROM folder and start the emulator again:\n\n",
                               L"Das SVI-318-ROM (svi111.rom, 32 KiB) fehlt. Legen Sie es in den ROM-Ordner und starten Sie den Emulator neu:\n\n");
        text += mediaFolder(L"SVIROM").wstring();
        MessageBoxW(nullptr, text.c_str(), L"SVI-318", MB_OK | MB_ICONERROR);
        return 1;
    }
    app.characterRom = readWholeFile(findRom(componentDirectory() / L"SVI806" / L"rom", L"svi806.rom", svi806::SVI806CharacterRomSize),
                                     svi806::SVI806CharacterRomSize);
    app.bootRom = readWholeFile(findRom(componentDirectory() / L"LANIF01" / L"rom", L"svi609.rom", lanif01::BootRomSize),
                                lanif01::BootRomSize);
    app.settingsPath = appDirectory / SettingsFileName;
    loadSettings(app);
    if (!app.missingRoms.empty()) {
        std::wstring text = ui(L"The settings use parts whose ROM images are missing; they are left out:\n",
                               L"Die Einstellungen nutzen Teile, deren ROMs fehlen; sie bleiben weg:\n");
        for (const auto& file : app.missingRoms) text += L"\n    " + file;
        text += ui(L"\n\nPlace the files in the ROM folder and start the emulator again:\n\n",
                   L"\n\nLegen Sie die Dateien in den ROM-Ordner und starten Sie den Emulator neu:\n\n");
        text += mediaFolder(L"SVIROM").wstring();
        MessageBoxW(nullptr, text.c_str(), L"SVI-318", MB_OK | MB_ICONWARNING);
    }
    app.machine.reset();

    const wchar_t* className = L"SVI318KeyboardWindow";
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
    const HWND window = CreateWindowExW(0, className, WindowTitle,
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
        app.statusHeight = statusRect.bottom - statusRect.top;
    }
    app.windowStyle = style;
    applyLayout(app);
    updateExpanderMenu(app);
    updateVideoMenu(app);
    updateJoystickMenu(app);
    app.speedStart = app.lastCounter;

    if (!startAudio(app.audio)) {
        app.audio.nextRetryTick = GetTickCount64() + AudioRetryIntervalMs;
    }
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
