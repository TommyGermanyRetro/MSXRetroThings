#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fd1793 {

// One rotation of a 5.25" disk at 300 rpm.
inline constexpr std::uint32_t RevolutionUs = 200000;

// ID field of a sector: cylinder, side, sector number and size code (length 128 << sizeCode).
struct SectorId {
    std::uint8_t track = 0;
    std::uint8_t side = 0;
    std::uint8_t sector = 0;
    std::uint8_t sizeCode = 0;
};

// An ID field as it passes under the head: positionUs is its distance from the index hole.
struct TrackSector {
    SectorId id;
    std::uint32_t positionUs = 0;
};

// A sector as WRITE TRACK lays it down: ID field followed by its data field.
struct FormattedSector {
    SectorId id;
    std::vector<std::uint8_t> data;
};

// Floppy drive as the controller sees it through the drive cable. Sector access goes to the
// cylinder under the head; side and density are the controller's side select and DDEN outputs.
class Drive {
public:
    virtual ~Drive() = default;

    [[nodiscard]] virtual bool ready() const = 0;
    [[nodiscard]] virtual bool writeProtected() const = 0;
    [[nodiscard]] virtual bool track0() const = 0;
    [[nodiscard]] virtual bool index() const = 0;
    // Microseconds since the index hole passed, 0 to RevolutionUs - 1.
    [[nodiscard]] virtual std::uint32_t rotationUs() const = 0;
    virtual void step(bool towardsCenter) = 0;

    // Sectors readable on the current cylinder with this side and density, in rotation order.
    [[nodiscard]] virtual std::vector<TrackSector> sectors(std::uint8_t side, bool doubleDensity) const = 0;
    // index selects an entry of sectors(); data receives 128 << sizeCode bytes.
    [[nodiscard]] virtual bool readSector(std::uint8_t side, bool doubleDensity, std::size_t index,
                                          std::vector<std::uint8_t>& data) const = 0;
    [[nodiscard]] virtual bool writeSector(std::uint8_t side, bool doubleDensity, std::size_t index,
                                           const std::vector<std::uint8_t>& data) = 0;
    // Replaces the current track on this side with the sectors written by WRITE TRACK.
    [[nodiscard]] virtual bool formatTrack(std::uint8_t side, bool doubleDensity,
                                           const std::vector<FormattedSector>& sectors) = 0;
};

// Status register bits.
inline constexpr std::uint8_t StatusBusy = 0x01;
inline constexpr std::uint8_t StatusIndex = 0x02;        // type I
inline constexpr std::uint8_t StatusDataRequest = 0x02;  // types II and III
inline constexpr std::uint8_t StatusTrack0 = 0x04;       // type I
inline constexpr std::uint8_t StatusLostData = 0x04;     // types II and III
inline constexpr std::uint8_t StatusCrcError = 0x08;
inline constexpr std::uint8_t StatusSeekError = 0x10;    // type I
inline constexpr std::uint8_t StatusNotFound = 0x10;     // types II and III
inline constexpr std::uint8_t StatusHeadLoaded = 0x20;   // type I
inline constexpr std::uint8_t StatusWriteProtect = 0x40;
inline constexpr std::uint8_t StatusNotReady = 0x80;

// Western Digital / Fujitsu FD1793 floppy disk controller (true data bus) at 1 MHz for 5.25"
// drives: step rates 6, 12, 20 and 30 ms, 30 ms head settling, 32 us per byte in MFM and 64 us
// in FM. Registers 0-3: command/status, track, sector, data. The host clocks it with tick() at
// hostClockHz; the side select and DDEN outputs and the drive come from the surrounding board.
class FD1793 final {
public:
    explicit FD1793(std::uint32_t hostClockHz);

    // Master reset: clears the registers and runs a RESTORE as the chip does after /MR.
    void reset();
    [[nodiscard]] std::uint8_t read(std::uint8_t reg);
    void write(std::uint8_t reg, std::uint8_t value);
    void tick();

    // The selected drive, or nullptr when no drive is selected.
    void setDrive(Drive* drive) noexcept;
    void setSide(std::uint8_t side) noexcept;
    void setDoubleDensity(bool doubleDensity) noexcept;

    [[nodiscard]] bool interruptRequest() const noexcept;
    [[nodiscard]] bool dataRequest() const noexcept;
    // Register values without the side effects of read().
    [[nodiscard]] std::uint8_t peekStatus() const noexcept;
    [[nodiscard]] std::uint8_t trackRegister() const noexcept;
    [[nodiscard]] std::uint8_t sectorRegister() const noexcept;

private:
    enum class Phase {
        Idle,
        Step,
        StepDone,
        VerifySettle,
        Verify,
        HeadSettle,
        Search,
        ReadData,
        WriteStart,
        WriteData,
        ReadAddress,
        WriteTrackStart,
        TrackWaitIndex,
        ReadTrackData,
        WriteTrackData,
        Finish,
    };

    void startCommand(std::uint8_t command);
    void forceInterrupt(std::uint8_t command);
    void advance();
    void nextStep();
    void finish(std::uint8_t errors);
    void searchSector();
    void startTrackTransfer();
    void finishWriteTrack();
    [[nodiscard]] std::vector<std::uint8_t> buildTrackImage() const;
    [[nodiscard]] bool driveReady() const noexcept;
    [[nodiscard]] std::uint64_t ticksForUs(std::uint64_t microseconds) const noexcept;
    [[nodiscard]] std::uint64_t byteTicks() const noexcept;
    [[nodiscard]] std::uint32_t untilPositionUs(std::uint32_t positionUs) const noexcept;
    void presentByte(std::uint8_t value);

    std::uint32_t clockHz;
    Drive* drive = nullptr;
    std::uint8_t sideSelect = 0;
    bool mfm = false;

    std::uint8_t command = 0;
    std::uint8_t status = 0;
    std::uint8_t track = 0;
    std::uint8_t sector = 0;
    std::uint8_t data = 0;
    bool typeOne = true;
    bool busy = false;
    bool intrq = false;
    bool drq = false;
    bool headLoaded = false;
    bool stepInward = true;
    bool immediateInterrupt = false;
    std::uint8_t interruptConditions = 0;
    bool lastIndex = false;
    bool lastReady = false;
    unsigned idleRevolutions = 0;

    Phase phase = Phase::Idle;
    std::uint64_t waitTicks = 0;
    unsigned stepsLeft = 0;
    std::uint8_t errors = 0;
    std::vector<std::uint8_t> buffer;
    std::size_t bufferPosition = 0;
    std::size_t sectorIndex = 0;
    std::uint16_t indexPulsesLeft = 0;
};

// CRC-16/CCITT (polynomial 1021h, start FFFFh) as the controller computes it over address marks,
// ID fields and data fields.
[[nodiscard]] std::uint16_t crc16(const std::uint8_t* bytes, std::size_t count, std::uint16_t crc = 0xFFFF) noexcept;

} // namespace fd1793
