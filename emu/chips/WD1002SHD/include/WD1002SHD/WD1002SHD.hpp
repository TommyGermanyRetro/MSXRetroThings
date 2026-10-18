#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wd1002shd {

// SASI bus control signals in the bit order of the SVI-608M host adapter (port 46h), as the phase
// values polled by the CP/M 2.27 BIOS show: Command 0Bh, Data Out 03h, Data In 13h, Status 1Bh,
// Message In 1Fh.
inline constexpr std::uint8_t SignalBsy = 0x01;
inline constexpr std::uint8_t SignalReq = 0x02;
inline constexpr std::uint8_t SignalMsg = 0x04;
inline constexpr std::uint8_t SignalCd = 0x08;
inline constexpr std::uint8_t SignalIo = 0x10;

// Bus phases (combinations of the signals above).
inline constexpr std::uint8_t PhaseBusFree = 0x00;
inline constexpr std::uint8_t PhaseDataOut = SignalBsy | SignalReq;
inline constexpr std::uint8_t PhaseCommand = SignalBsy | SignalReq | SignalCd;
inline constexpr std::uint8_t PhaseDataIn = SignalBsy | SignalReq | SignalIo;
inline constexpr std::uint8_t PhaseStatus = SignalBsy | SignalReq | SignalCd | SignalIo;
inline constexpr std::uint8_t PhaseMessage = SignalBsy | SignalReq | SignalCd | SignalIo | SignalMsg;

inline constexpr std::size_t SectorSize = 256;
inline constexpr unsigned SectorsPerTrack = 32;

// Seagate ST-212: 306 cylinders, 4 heads, 32 sectors of 256 bytes.
inline constexpr unsigned St212Cylinders = 306;
inline constexpr unsigned St212Heads = 4;
inline constexpr std::size_t St212Size = std::size_t{St212Cylinders} * St212Heads * SectorsPerTrack * SectorSize;

// Error codes of the Request Status command.
inline constexpr std::uint8_t ErrorNone = 0x00;
inline constexpr std::uint8_t ErrorNotReady = 0x04;
inline constexpr std::uint8_t ErrorInvalidCommand = 0x20;
inline constexpr std::uint8_t ErrorInvalidAddress = 0x21;

// Western Digital WD1002-SHD Winchester disk controller as SASI target (ID 0) with one drive on
// LUN 0, whose content is a flat image of 256-byte sectors in logical sector order. Command
// blocks of six bytes; after each command a status byte (bit 1 = error, bits 5-7 LUN) and a
// completion byte 00h. Commands: Test Drive Ready, Restore, Request Status, the format
// commands (data fields 6Ch or the sector buffer), Read, Write, Seek, Set Parameters (8 bytes:
// cylinders, heads, reduced write current, write precompensation, ECC burst), Return Burst Length,
// Format Alternate Track, Read/Write Sector Buffer, the diagnostics and Read/Write Long. A block
// count of 0 means 256 sectors. Power-up parameters are those of the ST-506 (153 cylinders,
// 4 heads); addresses beyond the parameter capacity give error 21h.
class WD1002SHD final {
public:
    WD1002SHD();

    // SASI RST: back to bus free; the drive parameters stay.
    void reset() noexcept;
    // Selection with the ID bits on the data bus; the controller answers to bit 0.
    [[nodiscard]] bool select(std::uint8_t idBits) noexcept;
    [[nodiscard]] std::uint8_t signals() const noexcept;
    // Host-to-target byte in the Command or Data Out phase (with ACK).
    void writeData(std::uint8_t value);
    // Target-to-host byte in the Data In, Status or Message phase (with ACK).
    [[nodiscard]] std::uint8_t readData();

    // Disk image; a shorter image reads E5h beyond its end and grows when written there.
    void attachDisk(std::vector<std::uint8_t> image);
    void detachDisk() noexcept;
    [[nodiscard]] bool hasDisk() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& image() const noexcept;
    [[nodiscard]] bool modified() const noexcept;
    void markSaved() noexcept;
    [[nodiscard]] unsigned cylinders() const noexcept;
    [[nodiscard]] unsigned heads() const noexcept;

private:
    enum class Phase { BusFree, Command, DataIn, DataOut, Status, Message };

    void execute();
    void finishWithError(std::uint8_t error);
    void finish();
    void dataOutComplete();
    [[nodiscard]] std::uint32_t logicalAddress() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint8_t lun() const noexcept;
    void readSectors(std::uint32_t address, std::uint32_t count);
    void writeSectors(std::uint32_t address, const std::uint8_t* data, std::uint32_t count);
    void fillTracks(std::uint32_t firstTrack, std::uint32_t lastTrack);

    Phase phase = Phase::BusFree;
    std::array<std::uint8_t, 6> command{};
    std::size_t commandLength = 0;
    std::vector<std::uint8_t> transfer;
    std::size_t transferPosition = 0;
    std::uint8_t statusByte = 0;
    std::array<std::uint8_t, 2> lastError{};
    std::array<std::uint32_t, 2> lastAddress{};
    std::array<bool, 2> addressValid{};
    std::array<std::uint8_t, SectorSize> sectorBuffer{};
    unsigned parameterCylinders = 153;
    unsigned parameterHeads = 4;
    std::vector<std::uint8_t> disk;
    bool diskPresent = false;
    bool changed = false;
};

} // namespace wd1002shd
