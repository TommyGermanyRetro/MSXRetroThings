#include "SVI805/SVI805.hpp"

#include "SVI3X8/ExpansionPort.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>

namespace svi805 {

namespace {

constexpr std::uint32_t PollTicks = svi3x8::CpuClockHz / 1000;
constexpr std::size_t ReceiveChunk = 256;

bool sameSettings(const ins8250::LineSettings& a, const ins8250::LineSettings& b) noexcept
{
    return a.baud == b.baud && a.dataBits == b.dataBits && a.parity == b.parity && a.stopBits == b.stopBits;
}

} // namespace

SVI805::SVI805(SerialLink* link)
    : chip(svi3x8::CpuClockHz, SVI805CrystalHz), remote(link)
{
}

void SVI805::setLink(SerialLink* link) noexcept
{
    remote = link;
    linkFresh = true;
}

SerialLink* SVI805::link() const noexcept
{
    return remote;
}

const ins8250::INS8250& SVI805::uart() const noexcept
{
    return chip;
}

void SVI805::reset()
{
    chip.reset();
    incoming.clear();
    outgoing.clear();
    linkFresh = true;
}

svi3x8::ExpansionMemoryCycle SVI805::readMemory(std::uint16_t, std::uint8_t) const
{
    return {};
}

svi3x8::ExpansionMemoryCycle SVI805::writeMemory(std::uint16_t, std::uint8_t, std::uint8_t)
{
    return {};
}

std::uint8_t SVI805::readIo(std::uint8_t port)
{
    return port >= 0x28 && port <= 0x2F ? chip.read(port & 7) : static_cast<std::uint8_t>(0xFF);
}

void SVI805::writeIo(std::uint8_t port, std::uint8_t value)
{
    if (port >= 0x28 && port <= 0x2F) chip.write(port & 7, value);
}

void SVI805::tick()
{
    chip.tick();
    std::uint8_t sent = 0;
    if (chip.takeTransmitted(sent)) outgoing.push_back(sent);
    if (!incoming.empty() && chip.readyToReceive() && !chip.loopback()) {
        chip.receive(incoming.front());
        incoming.pop_front();
    }
    if (--pollCountdown == 0) {
        pollCountdown = PollTicks;
        poll();
    }
}

void SVI805::poll()
{
    if (!remote) {
        outgoing.clear();
        chip.setModemInputs({});
        return;
    }
    const auto settings = chip.lineSettings();
    if (linkFresh || !sameSettings(settings, appliedSettings)) {
        remote->configure(settings);
        appliedSettings = settings;
    }
    const auto outputs = chip.modemOutputs();
    if (linkFresh || outputs.dtr != appliedOutputs.dtr || outputs.rts != appliedOutputs.rts) {
        remote->setModemOutputs(outputs.dtr, outputs.rts);
        appliedOutputs = outputs;
    }
    if (linkFresh || chip.breakActive() != appliedBreak) {
        appliedBreak = chip.breakActive();
        remote->setBreak(appliedBreak);
    }
    linkFresh = false;
    auto inputs = remote->modemInputs();
    inputs.ri = false;
    chip.setModemInputs(inputs);
    if (!outgoing.empty()) {
        remote->send(outgoing.data(), outgoing.size());
        outgoing.clear();
    }
    if (incoming.size() < 4 * ReceiveChunk) {
        std::array<std::uint8_t, ReceiveChunk> chunk{};
        const auto count = remote->receive(chunk.data(), chunk.size());
        incoming.insert(incoming.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    }
}

bool SVI805::interruptRequest() const
{
    return chip.interruptRequest();
}

ComPortLink::~ComPortLink()
{
    close();
}

bool ComPortLink::open(const std::wstring& name)
{
    close();
    const std::wstring path = L"\\\\.\\" + name;
    const auto port = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (port == INVALID_HANDLE_VALUE) return false;
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = 50;
    SetCommTimeouts(port, &timeouts);
    SetupComm(port, 4096, 4096);
    handle = port;
    portName = name;
    return true;
}

void ComPortLink::close() noexcept
{
    if (handle) CloseHandle(static_cast<HANDLE>(handle));
    handle = nullptr;
    portName.clear();
}

bool ComPortLink::isOpen() const noexcept
{
    return handle != nullptr;
}

const std::wstring& ComPortLink::name() const noexcept
{
    return portName;
}

std::vector<std::wstring> ComPortLink::availablePorts()
{
    std::vector<std::wstring> ports;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return ports;
    }
    for (DWORD index = 0;; ++index) {
        wchar_t valueName[256];
        wchar_t data[64];
        DWORD nameLength = 256;
        DWORD dataSize = sizeof(data);
        DWORD type = 0;
        const auto result = RegEnumValueW(key, index, valueName, &nameLength, nullptr, &type,
                                          reinterpret_cast<BYTE*>(data), &dataSize);
        if (result == ERROR_NO_MORE_ITEMS) break;
        if (result != ERROR_SUCCESS || type != REG_SZ) continue;
        data[std::min<std::size_t>(dataSize / sizeof(wchar_t), 63)] = L'\0';
        ports.emplace_back(data);
    }
    RegCloseKey(key);
    const auto number = [](const std::wstring& port) {
        unsigned value = 0;
        for (const auto c : port) {
            if (std::iswdigit(c)) value = value * 10 + static_cast<unsigned>(c - L'0');
        }
        return value;
    };
    std::sort(ports.begin(), ports.end(), [&](const auto& a, const auto& b) { return number(a) < number(b); });
    return ports;
}

void ComPortLink::configure(const ins8250::LineSettings& settings)
{
    if (!handle || settings.baud == 0) return;
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(static_cast<HANDLE>(handle), &dcb)) return;
    dcb.BaudRate = settings.baud;
    dcb.ByteSize = static_cast<BYTE>(settings.dataBits);
    switch (settings.parity) {
    case 'O': dcb.Parity = ODDPARITY; break;
    case 'E': dcb.Parity = EVENPARITY; break;
    case 'M': dcb.Parity = MARKPARITY; break;
    case 'S': dcb.Parity = SPACEPARITY; break;
    default: dcb.Parity = NOPARITY; break;
    }
    dcb.fParity = settings.parity != 'N';
    dcb.StopBits = settings.stopBits == 1 ? ONESTOPBIT : (settings.dataBits == 5 ? ONE5STOPBITS : TWOSTOPBITS);
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    SetCommState(static_cast<HANDLE>(handle), &dcb);
}

void ComPortLink::setModemOutputs(bool dtr, bool rts)
{
    if (!handle) return;
    EscapeCommFunction(static_cast<HANDLE>(handle), dtr ? SETDTR : CLRDTR);
    EscapeCommFunction(static_cast<HANDLE>(handle), rts ? SETRTS : CLRRTS);
}

void ComPortLink::setBreak(bool active)
{
    if (!handle) return;
    if (active) SetCommBreak(static_cast<HANDLE>(handle));
    else ClearCommBreak(static_cast<HANDLE>(handle));
}

ins8250::ModemInputs ComPortLink::modemInputs()
{
    DWORD status = 0;
    if (!handle || !GetCommModemStatus(static_cast<HANDLE>(handle), &status)) return {};
    return {(status & MS_CTS_ON) != 0, (status & MS_DSR_ON) != 0, (status & MS_RING_ON) != 0, (status & MS_RLSD_ON) != 0};
}

void ComPortLink::send(const std::uint8_t* bytes, std::size_t count)
{
    if (!handle || count == 0) return;
    DWORD written = 0;
    WriteFile(static_cast<HANDLE>(handle), bytes, static_cast<DWORD>(count), &written, nullptr);
}

std::size_t ComPortLink::receive(std::uint8_t* buffer, std::size_t capacity)
{
    if (!handle || capacity == 0) return 0;
    DWORD read = 0;
    if (!ReadFile(static_cast<HANDLE>(handle), buffer, static_cast<DWORD>(capacity), &read, nullptr)) return 0;
    return read;
}

} // namespace svi805
