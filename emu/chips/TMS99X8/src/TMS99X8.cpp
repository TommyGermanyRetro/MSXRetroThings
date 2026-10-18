#include "TMS99X8/TMS99X8.hpp"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>

namespace tms99x8 {
namespace {

constexpr std::uint16_t VramMask = 0x3FFF;
constexpr std::uint8_t StatusVBlank = 0x80;
constexpr std::uint8_t StatusOverflow = 0x40;
constexpr std::uint8_t StatusCollision = 0x20;
constexpr std::uint8_t RegisterMasks[8] = {
    0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF
};
constexpr std::uint8_t PaletteBackdrop = 0;
constexpr std::array<std::array<std::uint8_t, 3>, 16> RgbPalette = {{
    {{0, 0, 0}}, {{0, 0, 0}}, {{33, 200, 66}}, {{94, 220, 120}},
    {{84, 85, 237}}, {{125, 118, 252}}, {{212, 82, 77}}, {{66, 235, 245}},
    {{252, 85, 84}}, {{255, 121, 120}}, {{212, 193, 84}}, {{230, 206, 128}},
    {{33, 176, 59}}, {{201, 91, 186}}, {{204, 204, 204}}, {{255, 255, 255}}
}};

constexpr std::array<std::uint16_t, 107> Msx1ScreenOffSlots = {
    4, 12, 20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124,
    132, 140, 148, 156, 164, 172, 180, 188, 196, 204, 220, 236, 252, 268,
    284, 300, 316, 332, 348, 364, 380, 396, 412, 428, 444, 460, 476, 492,
    508, 524, 540, 556, 572, 588, 604, 620, 636, 652, 668, 684, 700, 716,
    732, 748, 764, 780, 796, 812, 828, 844, 860, 876, 892, 908, 924, 940,
    956, 972, 988, 1004, 1020, 1036, 1052, 1068, 1084, 1100, 1116, 1132,
    1148, 1164, 1180, 1196, 1212, 1228, 1236, 1244, 1252, 1260, 1268, 1276,
    1284, 1292, 1300, 1308, 1316, 1324, 1332, 1340, 1348, 1356, 1364
};
constexpr std::array<std::uint16_t, 19> Msx1Graphics12Slots = {
    4, 12, 20, 28, 116, 124, 132, 140, 220, 348, 476, 604, 732, 860, 988,
    1116, 1236, 1244, 1364
};
constexpr std::array<std::uint16_t, 51> Msx1MulticolorSlots = {
    4, 12, 20, 28, 116, 124, 132, 140, 220, 228, 260, 292, 324, 348, 356,
    388, 420, 452, 476, 484, 516, 548, 580, 604, 612, 644, 676, 708, 732,
    740, 772, 804, 836, 860, 868, 900, 932, 964, 988, 996, 1028, 1060, 1092,
    1116, 1124, 1156, 1188, 1220, 1236, 1244, 1364
};
constexpr std::array<std::uint16_t, 91> Msx1TextSlots = {
    4, 12, 20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124,
    132, 140, 148, 156, 164, 172, 180, 188, 196, 204, 212, 220, 228, 244,
    268, 292, 316, 340, 364, 388, 412, 436, 460, 484, 508, 532, 556, 580,
    604, 628, 652, 676, 700, 724, 748, 772, 796, 820, 844, 868, 892, 916,
    940, 964, 988, 1012, 1036, 1060, 1084, 1108, 1132, 1156, 1180, 1196,
    1204, 1212, 1220, 1228, 1236, 1244, 1252, 1260, 1268, 1276, 1284, 1292,
    1300, 1308, 1316, 1324, 1332, 1340, 1348, 1356, 1364
};
constexpr std::array<std::uint16_t, 32> SpriteYFetchDots = {
    37, 39, 41, 43, 45, 47, 49, 51, 63, 71, 79, 95, 103, 111, 127, 135,
    143, 159, 167, 175, 191, 199, 207, 223, 231, 239, 255, 263, 271, 287,
    295, 303
};
constexpr std::uint16_t SpriteDataFetchIntervalDots = 2;
constexpr std::uint16_t GraphicsNameFetchLeadDots = 10;
constexpr std::uint16_t GraphicsColorFetchLeadDots = 6;
constexpr std::uint16_t GraphicsPatternFetchLeadDots = 4;
constexpr std::uint16_t TextNameFetchLeadDots = 8;
constexpr std::uint16_t TextPatternFetchLeadDots = 4;
constexpr unsigned BogusBorder = 8;
constexpr std::uint32_t VerticalRefreshMicroseconds = 4300;
// Delay from a CPU data-port request to the earliest VRAM access: 28 master clocks (1.3 us)
// rather than the 2 us of the data sheet. The SVI-318/328 BASIC ROM fills the Graphics II name table
// with writes exactly 27 CPU cycles apart (OUT (80h),A / INC A / JR NZ at 361Ch), which the
// machine displays without loss.
constexpr std::uint64_t VramSetupDelayMasterTicks = 28;
constexpr std::uint64_t VerticalRefreshMasterTicks =
    (static_cast<std::uint64_t>(VerticalRefreshMicroseconds) * MasterClockHz + 999999) /
    1000000;
}

template<std::size_t Count>
std::uint64_t nextAccessSlot(
    std::uint64_t masterTick, std::uint64_t minimumDelay,
    const std::array<std::uint16_t, Count>& slots) noexcept
{
    const auto earliest = masterTick + minimumDelay;
    const auto lineStart = earliest - earliest % MasterTicksPerScanline;
    const auto offset = earliest % MasterTicksPerScanline;
    for (const auto slot : slots) {
        if (slot >= offset) return lineStart + slot;
    }
    return lineStart + MasterTicksPerScanline + slots.front();
}

struct TMS99X8::Impl {
    struct BackgroundFetch {
        std::uint8_t name = 0;
        std::uint8_t pattern = 0;
        std::uint8_t color = 0;
        bool nameReady = false;
        bool colorReady = false;
    };

    struct ScanoutSprite {
        int x = 0;
        std::array<std::uint8_t, 2> patternBytes{};
        std::uint8_t pattern = 0;
        std::uint8_t color = 0;
        std::uint8_t sourceY = 0;
        bool earlyClock = false;
    };

    struct SpriteCandidate {
        std::uint8_t index = 0;
        std::uint8_t sourceY = 0;
    };

    explicit Impl(Model selectedModel)
        : videoModel(selectedModel)
    {
    }

    Model videoModel;
    std::array<std::uint8_t, VramSize> vram{};
    std::array<std::uint8_t, 8> registers{};
    std::array<std::uint8_t, 8> scanlineRegisters{};
    std::array<std::uint8_t, 8> nextScanlineRegisters{};
    FrameBuffer scanout{};
    BackdropRaster backdropColors{};
    std::array<std::uint8_t, ScreenWidth> scanoutBackground{};
    std::array<BackgroundFetch, 40> backgroundFetches{};
    std::array<ScanoutSprite, 4> scanoutSprites{};
    std::array<ScanoutSprite, 4> pendingScanoutSprites{};
    std::array<SpriteCandidate, 5> spriteCandidates{};
    std::uint8_t scanoutSpriteCount = 0;
    std::uint8_t pendingScanoutSpriteCount = 0;
    std::uint8_t spriteYFetchIndex = 0;
    std::uint8_t spriteDataFetchSprite = 0;
    std::uint8_t spriteDataFetchPhase = 0;
    std::uint16_t spriteEvaluationTargetY = 0;
    std::uint16_t spriteTargetScanline = 0;
    std::uint16_t spriteEvaluationScanline = 0xFFFF;
    std::uint8_t scanoutSpriteSize = 8;
    std::uint8_t scanoutMagnification = 1;
    std::uint8_t scanoutPatternBase = 0;
    std::uint8_t spriteDataPatternBase = 0;
    std::uint8_t status = 0;
    std::uint8_t fifthSprite = 31;
    std::uint8_t cpuVramData = 0;
    std::uint8_t pendingVramData = 0;
    std::uint8_t controlFirstByte = 0;
    std::uint16_t address = 0;
    std::uint16_t scanline = 0;
    std::uint16_t dot = 0;
    std::uint8_t masterPhase = 0;
    std::uint64_t masterTicks = 0;
    std::uint64_t totalDots = 0;
    std::uint64_t nextSpriteDataFetchDot = 0;
    std::uint64_t pendingVramTick = 0;
    std::uint64_t pendingVramEarliestTick = 0;
    bool controlLatch = false;
    bool interruptRequest = false;
    bool pendingVramAccess = false;
    bool pendingVramRead = false;
    bool spriteYFetchActive = false;
    bool spriteDataFetchActive = false;
    bool spriteDataReady = false;
    bool spriteYTerminated = false;

    [[nodiscard]] std::uint16_t totalScanlines() const noexcept
    {
        return videoModel == Model::Pal ? 313 : 262;
    }

    [[nodiscard]] std::uint16_t displayStart() const noexcept
    {
        return videoModel == Model::Pal ? 64 : 40;
    }

    [[nodiscard]] std::uint8_t vramRead(std::uint16_t location) const noexcept
    {
        return vram[location & VramMask];
    }

    void finishSpriteEvaluation() noexcept
    {
        spriteYFetchActive = false;
        spriteDataFetchSprite = 0;
        spriteDataFetchPhase = 0;
        if (pendingScanoutSpriteCount == 0) {
            spriteDataFetchActive = false;
            spriteDataReady = true;
            return;
        }
        spriteDataFetchActive = true;
        spriteDataReady = false;
        nextSpriteDataFetchDot = totalDots + SpriteDataFetchIntervalDots;
    }

    void beginSpriteEvaluation(std::uint16_t targetScanline) noexcept
    {
        // The pattern fetches of up to four sprites belong to the horizontal blanking of the
        // previous line; fetches still open when the next evaluation starts are completed first.
        while (spriteDataFetchActive) fetchSpriteData();
        if (spriteDataReady && scanline == spriteTargetScanline) commitSpriteEvaluation();
        spriteYFetchIndex = 0;
        pendingScanoutSpriteCount = 0;
        spriteYTerminated = false;
        nextScanlineRegisters = registers;
        spriteTargetScanline = targetScanline;
        spriteEvaluationScanline = scanline;
        spriteEvaluationTargetY = targetScanline >= displayStart()
            ? static_cast<std::uint16_t>(targetScanline - displayStart())
            : 0;
        if (targetScanline < displayStart() ||
            targetScanline > displayStart() + ScreenHeight ||
            (nextScanlineRegisters[1] & 0x50) != 0x40) {
            finishSpriteEvaluation();
            return;
        }
        spriteYFetchActive = true;
        if ((status & StatusOverflow) == 0) {
            fifthSprite = 31;
            status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
        }
    }

    void fetchSpriteY() noexcept
    {
        const auto index = spriteYFetchIndex;
        const auto attributeBase = static_cast<std::uint16_t>((nextScanlineRegisters[5] & 0x7F) << 7);
        const auto rawY = vramRead(attributeBase + index * 4);
        if (rawY == 208) {
            spriteYTerminated = true;
            if ((status & StatusOverflow) == 0) {
                fifthSprite = index;
                status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
            }
            finishSpriteEvaluation();
            return;
        }

        const unsigned spriteSize = (nextScanlineRegisters[1] & 0x02) ? 16 : 8;
        const unsigned magnification = (nextScanlineRegisters[1] & 0x01) ? 2 : 1;
        const auto spriteLine = static_cast<unsigned>((spriteEvaluationTargetY - 1u - rawY) & 0xFFu);
        if (spriteLine < spriteSize * magnification) {
            if (pendingScanoutSpriteCount == 4) {
                if ((status & (StatusVBlank | StatusOverflow)) == 0) {
                    fifthSprite = index;
                    status = static_cast<std::uint8_t>((status & 0xA0) | StatusOverflow | index);
                } else if ((status & StatusOverflow) == 0) {
                    fifthSprite = index;
                    status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
                }
                finishSpriteEvaluation();
                updateInterrupt();
                return;
            }
            const auto sourceY = static_cast<std::uint8_t>(spriteLine / magnification);
            spriteCandidates[pendingScanoutSpriteCount] = {index, sourceY};
            pendingScanoutSprites[pendingScanoutSpriteCount] = {};
            pendingScanoutSprites[pendingScanoutSpriteCount].sourceY = sourceY;
            ++pendingScanoutSpriteCount;
        }

        if ((status & StatusOverflow) == 0) {
            fifthSprite = index;
            status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
        }
        ++spriteYFetchIndex;
        if (spriteYFetchIndex == SpriteYFetchDots.size()) finishSpriteEvaluation();
        updateInterrupt();
    }

    // A 16x16 sprite stores its left column in pattern bytes 0-15 and its right column 16 later.
    [[nodiscard]] std::uint16_t spritePatternAddress(const ScanoutSprite& sprite) const noexcept
    {
        return static_cast<std::uint16_t>(((nextScanlineRegisters[6] & 0x07) << 11) +
                                          sprite.pattern * 8 + sprite.sourceY);
    }

    void fetchSpriteData() noexcept
    {
        if (!spriteDataFetchActive) return;
        if (spriteDataFetchSprite >= pendingScanoutSpriteCount) {
            spriteDataFetchActive = false;
            spriteDataReady = true;
            return;
        }

        auto& sprite = pendingScanoutSprites[spriteDataFetchSprite];
        const auto attributeBase = static_cast<std::uint16_t>((nextScanlineRegisters[5] & 0x7F) << 7);
        const auto descriptor = static_cast<std::uint16_t>(
            attributeBase + spriteCandidates[spriteDataFetchSprite].index * 4);
        const auto spriteSize = (nextScanlineRegisters[1] & 0x02) ? 16 : 8;
        switch (spriteDataFetchPhase) {
        case 0:
            sprite.x = vramRead(descriptor + 1);
            break;
        case 1:
            sprite.pattern = vramRead(descriptor + 2);
            if (spriteSize == 16) sprite.pattern &= 0xFC;
            break;
        case 2: {
            const auto colorByte = vramRead(descriptor + 3);
            sprite.earlyClock = (colorByte & 0x80) != 0;
            if (sprite.earlyClock) sprite.x -= 32;
            sprite.color = colorByte & 0x0F;
            break;
        }
        case 3:
            sprite.patternBytes[0] = vramRead(spritePatternAddress(sprite));
            break;
        case 4:
            sprite.patternBytes[1] = vramRead(spritePatternAddress(sprite) + 16);
            break;
        default:
            break;
        }

        if (++spriteDataFetchPhase == 5) {
            spriteDataFetchPhase = 0;
            ++spriteDataFetchSprite;
        }
        nextSpriteDataFetchDot += SpriteDataFetchIntervalDots;
    }

    void commitSpriteEvaluation() noexcept
    {
        scanoutSprites = pendingScanoutSprites;
        scanoutSpriteCount = pendingScanoutSpriteCount;
        scanoutSpriteSize = (nextScanlineRegisters[1] & 0x02) ? 16 : 8;
        scanoutMagnification = (nextScanlineRegisters[1] & 0x01) ? 2 : 1;
        scanoutPatternBase = static_cast<std::uint8_t>(nextScanlineRegisters[6] & 0x07);
        spriteDataReady = false;
    }

    [[nodiscard]] bool inDisplayAccessGrid() const noexcept
    {
        if ((scanlineRegisters[1] & 0x40) == 0) return false;
        const auto frameTicks = static_cast<std::uint64_t>(
            totalScanlines() * MasterTicksPerScanline);
        const auto framePosition = masterTicks % frameTicks;
        const auto displayStartTick = static_cast<std::uint64_t>(
            displayStart() * MasterTicksPerScanline + 202);
        const auto gridStart = displayStartTick - (MasterTicksPerScanline + 202 - 162);
        const auto gridEnd = displayStartTick +
            ScreenHeight * MasterTicksPerScanline - 40;
        return framePosition >= gridStart && framePosition < gridEnd;
    }

    [[nodiscard]] bool inVerticalRefresh() const noexcept
    {
        const auto frameTicks = static_cast<std::uint64_t>(
            totalScanlines() * MasterTicksPerScanline);
        const auto framePosition = masterTicks % frameTicks;
        const auto activeDisplayEnd = static_cast<std::uint64_t>(
            (displayStart() + ScreenHeight) * MasterTicksPerScanline + 202);
        const auto refreshEnd = activeDisplayEnd + VerticalRefreshMasterTicks;
        return framePosition >= activeDisplayEnd || framePosition < refreshEnd - frameTicks;
    }

    [[nodiscard]] std::uint64_t nextCpuVramSlot() const noexcept
    {
        if (inVerticalRefresh()) {
            return masterTicks + VramSetupDelayMasterTicks;
        }
        if (!inDisplayAccessGrid()) {
            return nextAccessSlot(masterTicks, VramSetupDelayMasterTicks, Msx1ScreenOffSlots);
        }
        switch (mode(scanlineRegisters)) {
        case 1:
        case 3:
            return nextAccessSlot(masterTicks, VramSetupDelayMasterTicks, Msx1TextSlots);
        case 4:
            return nextAccessSlot(masterTicks, VramSetupDelayMasterTicks, Msx1MulticolorSlots);
        default:
            return nextAccessSlot(masterTicks, VramSetupDelayMasterTicks, Msx1Graphics12Slots);
        }
    }

    // Graphics I/II give each fourth memory cycle after a character's name, colour and pattern
    // fetches to the CPU and the other three to the sprite Y scan. Once the scan of the line has
    // stopped (Y code 208, or the sprites for the next line found), its remaining Y cycles serve
    // the CPU as well (TI, US patent 4,243,984, "CPU access index" and "stop flag").
    [[nodiscard]] bool freedSpriteSlot() const noexcept
    {
        if (spriteYFetchActive || spriteEvaluationScanline != scanline) return false;
        const auto offset = masterTicks % MasterTicksPerScanline;
        if (offset % MasterTicksPerDot != 0) return false;
        const auto slotDot = static_cast<std::uint16_t>(offset / MasterTicksPerDot);
        if (std::find(SpriteYFetchDots.begin(), SpriteYFetchDots.end(), slotDot) == SpriteYFetchDots.end()) {
            return false;
        }
        const auto currentMode = mode(scanlineRegisters);
        return (currentMode == 0 || currentMode == 2) && inDisplayAccessGrid() && !inVerticalRefresh();
    }

    void requestCpuVramAccess(bool read, std::uint8_t value) noexcept
    {
        if (!read) cpuVramData = value;
        if (pendingVramAccess) {
            pendingVramRead = read;
            pendingVramData = cpuVramData;
            return;
        }
        pendingVramAccess = true;
        pendingVramRead = read;
        pendingVramData = cpuVramData;
        pendingVramEarliestTick = masterTicks + VramSetupDelayMasterTicks;
        pendingVramTick = nextCpuVramSlot();
    }

    void executeCpuVramAccess() noexcept
    {
        if (!pendingVramAccess) return;
        if (pendingVramRead) {
            cpuVramData = vramRead(address);
        } else {
            vram[address & VramMask] = pendingVramData;
        }
        address = (address + 1) & VramMask;
        pendingVramAccess = false;
    }

    void updateInterrupt() noexcept
    {
        interruptRequest = (status & StatusVBlank) != 0 && (registers[1] & 0x20) != 0;
    }

    void writeRegister(std::uint8_t index, std::uint8_t value) noexcept
    {
        index &= 7;
        registers[index] = value & RegisterMasks[index];
        if (index == 1) updateInterrupt();
    }

    [[nodiscard]] std::uint8_t readStatus() noexcept
    {
        const auto result = status;
        status = fifthSprite & 0x1F;
        fifthSprite = 31;
        controlLatch = false;
        updateInterrupt();
        return result;
    }

    void writeControl(std::uint8_t value) noexcept
    {
        if (!controlLatch) {
            controlFirstByte = value;
            address = static_cast<std::uint16_t>((address & 0x3F00) | value);
            controlLatch = true;
            return;
        }

        controlLatch = false;
        address = static_cast<std::uint16_t>((value << 8) | (address & 0x00FF)) & VramMask;
        if (value & 0x80) {
            writeRegister(value & 7, controlFirstByte);
            return;
        }

        address = static_cast<std::uint16_t>(((value & 0x3F) << 8) |
                                             controlFirstByte) & VramMask;
        if ((value & 0x40) == 0) {
            requestCpuVramAccess(true, 0);
        }
    }

    [[nodiscard]] std::uint8_t backgroundColor() const noexcept
    {
        const auto color = registers[7] & 0x0F;
        return color == 0 ? PaletteBackdrop : color;
    }

    [[nodiscard]] std::uint8_t visibleColor(std::uint8_t color) const noexcept
    {
        return color == 0 ? backgroundColor() : color;
    }

    [[nodiscard]] unsigned mode(const std::array<std::uint8_t, 8>& regs) const noexcept
    {
        return (regs[0] & 0x02) |
               ((regs[1] >> 4) & 0x01) |
               ((regs[1] & 0x08) >> 1);
    }

    [[nodiscard]] unsigned mode() const noexcept
    {
        return mode(registers);
    }

    [[nodiscard]] unsigned scanoutMode() const noexcept
    {
        return (scanlineRegisters[0] & 0x02) |
               ((scanlineRegisters[1] >> 4) & 0x01) |
               ((scanlineRegisters[1] & 0x08) >> 1);
    }

    // Text and Multicolor with M3 select the pattern quarter through R4 bits 0-1 only; unlike
    // Graphics II, the colour mask in R3 does not leak into the pattern address.
    [[nodiscard]] static std::uint16_t quarterPatternMask(
        const std::array<std::uint8_t, 8>& regs) noexcept
    {
        return static_cast<std::uint16_t>(((regs[4] & 3) << 8) | 0xFF);
    }

    [[nodiscard]] std::uint8_t renderBackgroundPixel(
        unsigned x, unsigned y, const std::array<std::uint8_t, 8>& lineRegisters) const noexcept
    {
        const auto backdropColor = lineRegisters[7] & 0x0F;
        const auto backdrop = backdropColor == 0 ? PaletteBackdrop : backdropColor;
        const auto foregroundColor = [&](std::uint8_t color) {
            return color == 0 ? backdrop : color;
        };
        if ((lineRegisters[1] & 0x40) == 0) return backdrop;

        const auto nameBase = static_cast<std::uint16_t>((lineRegisters[2] & 0x0F) << 10);
        const auto patternBase = static_cast<std::uint16_t>((lineRegisters[4] & 0x07) << 11);
        const auto row = y >> 3;
        const auto rowInCharacter = y & 7;

        switch (mode(lineRegisters)) {
        case 0: {
            const auto column = x >> 3;
            const auto character = vramRead(nameBase + row * 32 + column);
            const auto pattern = vramRead(patternBase + character * 8 + rowInCharacter);
            const auto color = vramRead((lineRegisters[3] << 6) + (character >> 3));
            return foregroundColor((pattern & (0x80 >> (x & 7)))
                ? color >> 4 : color & 0x0F);
        }
        case 1: {
            if (x < 6 || x >= 246) return backdrop;
            const auto textX = x - 6;
            const auto character = vramRead(nameBase + row * 40 + textX / 6);
            const auto pattern = vramRead(patternBase + character * 8 + rowInCharacter);
            return foregroundColor((pattern & (0x80 >> (textX % 6)))
                ? lineRegisters[7] >> 4 : backdrop);
        }
        case 3: {
            if (x < 6 || x >= 246) return backdrop;
            const auto textX = x - 6;
            const auto name = vramRead(nameBase + row * 40 + textX / 6);
            const auto character = static_cast<std::uint16_t>(
                (name + ((y >> 6) << 8)) & quarterPatternMask(lineRegisters));
            const auto patternTableBase = static_cast<std::uint16_t>((lineRegisters[4] & 0x04) << 11);
            const auto pattern = vramRead(patternTableBase + character * 8 + rowInCharacter);
            return foregroundColor((pattern & (0x80 >> (textX % 6)))
                ? lineRegisters[7] >> 4 : backdrop);
        }
        case 5:
        case 7: {
            if (x < BogusBorder || x >= ScreenWidth - BogusBorder) return backdrop;
            const auto bogusX = (x - BogusBorder) % 6;
            return foregroundColor(bogusX < 4 ? lineRegisters[7] >> 4 : backdrop);
        }
        case 2: {
            const auto column = x >> 3;
            const auto colorBase = static_cast<std::uint16_t>((lineRegisters[3] & 0x80) << 6);
            const auto colorMask = static_cast<std::uint16_t>(((lineRegisters[3] & 0x7F) << 3) | 7);
            const auto patternMask = static_cast<std::uint16_t>(((lineRegisters[4] & 3) << 8) |
                                                                (colorMask & 0xFF));
            const auto patternTableBase = static_cast<std::uint16_t>((lineRegisters[4] & 0x04) << 11);
            const auto character = static_cast<std::uint16_t>(
                vramRead(nameBase + row * 32 + column) + ((y >> 6) << 8));
            const auto pattern = vramRead(patternTableBase +
                ((character & patternMask) << 3) + rowInCharacter);
            const auto color = vramRead(colorBase + ((character & colorMask) << 3) + rowInCharacter);
            return foregroundColor((pattern & (0x80 >> (x & 7)))
                ? color >> 4 : color & 0x0F);
        }
        case 4: {
            const auto column = x >> 3;
            const auto character = vramRead(nameBase + row * 32 + column);
            const auto pattern = vramRead(patternBase + character * 8 + ((y >> 2) & 7));
            return foregroundColor((x & 4) == 0 ? pattern >> 4 : pattern & 0x0F);
        }
        case 6: {
            const auto column = x >> 3;
            const auto name = vramRead(nameBase + row * 32 + column);
            const auto patternTableBase = static_cast<std::uint16_t>((lineRegisters[4] & 0x04) << 11);
            const auto colorPattern = vramRead(patternTableBase +
                (((name + ((y >> 6) << 8)) & quarterPatternMask(lineRegisters)) << 3) +
                ((y >> 2) & 7));
            return foregroundColor((x & 4) == 0
                ? colorPattern >> 4 : colorPattern & 0x0F);
        }
        default:
            return backdrop;
        }
    }

    void fetchBackgroundName(unsigned group, unsigned y) noexcept
    {
        const auto currentMode = mode(scanlineRegisters);
        auto& fetch = backgroundFetches[group];
        fetch = {};
        if ((scanlineRegisters[1] & 0x40) == 0 || currentMode == 5 || currentMode == 7) {
            return;
        }
        const auto nameBase = static_cast<std::uint16_t>((scanlineRegisters[2] & 0x0F) << 10);
        const auto row = y >> 3;
        if (currentMode == 1 || currentMode == 3) {
            fetch.name = vramRead(nameBase + row * 40 + group);
        } else {
            fetch.name = vramRead(nameBase + row * 32 + group);
        }
        fetch.nameReady = true;
    }

    void fetchBackgroundColor(unsigned group, unsigned y) noexcept
    {
        const auto currentMode = mode(scanlineRegisters);
        if (!backgroundFetches[group].nameReady) return;
        const auto rowInCharacter = y & 7;
        const auto name = backgroundFetches[group].name;
        if (currentMode == 0) {
            backgroundFetches[group].color = vramRead(
                (scanlineRegisters[3] << 6) + (name >> 3));
            backgroundFetches[group].colorReady = true;
        } else if (currentMode == 2) {
            const auto colorBase = static_cast<std::uint16_t>(
                (scanlineRegisters[3] & 0x80) << 6);
            const auto colorMask = static_cast<std::uint16_t>(
                ((scanlineRegisters[3] & 0x7F) << 3) | 7);
            const auto character = static_cast<std::uint16_t>(name + ((y >> 6) << 8));
            backgroundFetches[group].color = vramRead(colorBase +
                ((character & colorMask) << 3) + rowInCharacter);
            backgroundFetches[group].colorReady = true;
        }
    }

    void fetchBackgroundPattern(unsigned group, unsigned y) noexcept
    {
        const auto currentMode = mode(scanlineRegisters);
        auto& fetch = backgroundFetches[group];
        const auto backdropColor = scanlineRegisters[7] & 0x0F;
        const auto backdrop = backdropColor == 0 ? PaletteBackdrop : backdropColor;
        const auto visibleColor = [backdrop](std::uint8_t color) {
            return color == 0 ? backdrop : color;
        };
        const auto x = (currentMode == 1 || currentMode == 3) ? 6 + group * 6
            : (currentMode == 5 || currentMode == 7) ? BogusBorder + group * 6
            : group * 8;
        if ((scanlineRegisters[1] & 0x40) == 0) return;
        if (currentMode == 5 || currentMode == 7) {
            const auto foreground = visibleColor(scanlineRegisters[7] >> 4);
            for (unsigned bit = 0; bit < 6; ++bit) {
                scanoutBackground[x + bit] = bit < 4 ? foreground : backdrop;
            }
            return;
        }
        if (!fetch.nameReady) return;

        const auto rowInCharacter = y & 7;
        const auto patternBase = static_cast<std::uint16_t>((scanlineRegisters[4] & 0x07) << 11);
        std::uint8_t pattern = 0;
        if (currentMode == 0) {
            if (!fetch.colorReady) return;
            pattern = vramRead(patternBase + fetch.name * 8 + rowInCharacter);
            const auto foreground = visibleColor(fetch.color >> 4);
            const auto background = visibleColor(fetch.color & 0x0F);
            for (unsigned bit = 0; bit < 8; ++bit) {
                scanoutBackground[x + bit] = (pattern & (0x80 >> bit))
                    ? foreground : background;
            }
            return;
        }
        if (currentMode == 1 || currentMode == 3) {
            auto character = static_cast<std::uint16_t>(fetch.name);
            auto tableBase = patternBase;
            if (currentMode == 3) {
                character = static_cast<std::uint16_t>(
                    (character + ((y >> 6) << 8)) & quarterPatternMask(scanlineRegisters));
                tableBase = static_cast<std::uint16_t>((scanlineRegisters[4] & 0x04) << 11);
            }
            pattern = vramRead(tableBase + character * 8 + rowInCharacter);
            const auto foreground = visibleColor(scanlineRegisters[7] >> 4);
            for (unsigned bit = 0; bit < 6; ++bit) {
                scanoutBackground[x + bit] = (pattern & (0x80 >> bit))
                    ? foreground : backdrop;
            }
            return;
        }
        if (currentMode == 2) {
            if (!fetch.colorReady) return;
            const auto colorMask = static_cast<std::uint16_t>(
                ((scanlineRegisters[3] & 0x7F) << 3) | 7);
            const auto patternMask = static_cast<std::uint16_t>(
                ((scanlineRegisters[4] & 3) << 8) | (colorMask & 0xFF));
            const auto patternTableBase = static_cast<std::uint16_t>(
                (scanlineRegisters[4] & 0x04) << 11);
            const auto character = static_cast<std::uint16_t>(fetch.name + ((y >> 6) << 8));
            pattern = vramRead(patternTableBase +
                ((character & patternMask) << 3) + rowInCharacter);
            const auto foreground = visibleColor(fetch.color >> 4);
            const auto background = visibleColor(fetch.color & 0x0F);
            for (unsigned bit = 0; bit < 8; ++bit) {
                scanoutBackground[x + bit] = (pattern & (0x80 >> bit))
                    ? foreground : background;
            }
            return;
        }
        if (currentMode == 4) {
            pattern = vramRead(patternBase + fetch.name * 8 + ((y >> 2) & 7));
        } else if (currentMode == 6) {
            const auto patternTableBase = static_cast<std::uint16_t>(
                (scanlineRegisters[4] & 0x04) << 11);
            pattern = vramRead(patternTableBase +
                (((fetch.name + ((y >> 6) << 8)) & quarterPatternMask(scanlineRegisters)) << 3) +
                ((y >> 2) & 7));
        }
        const auto foreground = visibleColor(pattern >> 4);
        const auto background = visibleColor(pattern & 0x0F);
        std::fill_n(scanoutBackground.data() + x, 4, foreground);
        std::fill_n(scanoutBackground.data() + x + 4, 4, background);
    }

    void advanceBackgroundFetches(unsigned y) noexcept
    {
        const auto currentMode = mode(scanlineRegisters);
        const bool textMode = currentMode == 1 || currentMode == 3 ||
                              currentMode == 5 || currentMode == 7;
        if (textMode) {
            const auto firstTextOutputDot = HorizontalDisplayStart + 6;
            const auto firstTextNameDot = firstTextOutputDot - TextNameFetchLeadDots;
            const auto firstTextPatternDot = firstTextOutputDot - TextPatternFetchLeadDots;
            if (dot >= firstTextNameDot &&
                (dot - firstTextNameDot) % 6 == 0) {
                const auto group = static_cast<unsigned>((dot - firstTextNameDot) / 6);
                if (group < 40) fetchBackgroundName(group, y);
            }
            if (dot >= firstTextPatternDot &&
                (dot - firstTextPatternDot) % 6 == 0) {
                const auto group = static_cast<unsigned>((dot - firstTextPatternDot) / 6);
                if (group < 40) fetchBackgroundPattern(group, y);
            }
            return;
        }

        const auto firstGraphicsNameDot =
            HorizontalDisplayStart - GraphicsNameFetchLeadDots;
        const auto firstGraphicsColorDot =
            HorizontalDisplayStart - GraphicsColorFetchLeadDots;
        const auto firstGraphicsPatternDot =
            HorizontalDisplayStart - GraphicsPatternFetchLeadDots;
        if (dot >= firstGraphicsNameDot &&
            (dot - firstGraphicsNameDot) % 8 == 0) {
            const auto group = static_cast<unsigned>((dot - firstGraphicsNameDot) / 8);
            if (group < 32) fetchBackgroundName(group, y);
        }
        if (dot >= firstGraphicsColorDot &&
            (dot - firstGraphicsColorDot) % 8 == 0) {
            const auto group = static_cast<unsigned>((dot - firstGraphicsColorDot) / 8);
            if (group < 32) fetchBackgroundColor(group, y);
        }
        if (dot >= firstGraphicsPatternDot &&
            (dot - firstGraphicsPatternDot) % 8 == 0) {
            const auto group = static_cast<unsigned>((dot - firstGraphicsPatternDot) / 8);
            if (group < 32) fetchBackgroundPattern(group, y);
        }
    }

    void renderBackgroundLine(FrameBuffer& frame, unsigned y) const noexcept
    {
        auto* pixels = frame.data() + y * ScreenWidth;
        for (unsigned x = 0; x < ScreenWidth; ++x) {
            pixels[x] = renderBackgroundPixel(x, y, registers);
        }
    }

    // A null pixel row checks status and collisions only, as for the undisplayed line 192.
    void renderSpritesLine(std::uint8_t* pixels, unsigned y) noexcept
    {
        if ((status & StatusOverflow) == 0) fifthSprite = 31;
        if ((registers[1] & 0x50) != 0x40) {
            if ((status & StatusOverflow) == 0) {
                status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
            }
            return;
        }

        std::array<bool, ScreenWidth> occupied{};
        std::array<bool, ScreenWidth> drawn{};
        const auto attributeBase = static_cast<std::uint16_t>((registers[5] & 0x7F) << 7);
        const auto patternBase = static_cast<std::uint16_t>((registers[6] & 0x07) << 11);
        const unsigned spriteSize = (registers[1] & 0x02) ? 16 : 8;
        const unsigned magnification = (registers[1] & 0x01) ? 2 : 1;
        const unsigned displayedHeight = spriteSize * magnification;
        unsigned spritesOnLine = 0;
        std::uint8_t lastEvaluatedSprite = 31;

        for (unsigned sprite = 0; sprite < 32; ++sprite) {
            const auto descriptor = attributeBase + sprite * 4;
            lastEvaluatedSprite = static_cast<std::uint8_t>(sprite & 0x1F);
            const auto rawY = vramRead(descriptor);
            if (rawY == 208) break;

            const auto spriteLine = (y - 1u - rawY) & 0xFFu;
            if (spriteLine >= displayedHeight) continue;

            ++spritesOnLine;
            if (spritesOnLine > 4) {
                if ((status & (StatusVBlank | StatusOverflow)) == 0) {
                    status = static_cast<std::uint8_t>((status & 0xA0) | StatusOverflow |
                                                       (sprite & 0x1F));
                    fifthSprite = static_cast<std::uint8_t>(sprite & 0x1F);
                }
                break;
            }

            auto x = static_cast<int>(vramRead(descriptor + 1));
            const auto spritePattern = vramRead(descriptor + 2);
            const auto spriteColor = vramRead(descriptor + 3);
            if (spriteColor & 0x80) x -= 32;
            const auto color = spriteColor & 0x0F;

            const auto sourceY = spriteLine / magnification;
            const auto alignedPattern = spriteSize == 16
                ? static_cast<std::uint8_t>(spritePattern & 0xFC) : spritePattern;
            for (unsigned sourceX = 0; sourceX < spriteSize; ++sourceX) {
                const auto patternAddress = patternBase + alignedPattern * 8 + sourceY +
                                            (sourceX / 8) * 16;
                const auto pattern = vramRead(patternAddress);
                if ((pattern & (0x80 >> (sourceX & 7))) == 0) continue;

                for (unsigned scaleX = 0; scaleX < magnification; ++scaleX) {
                    const auto screenX = x + static_cast<int>(sourceX * magnification + scaleX);
                    if (screenX < 0 || screenX >= static_cast<int>(ScreenWidth)) continue;
                    const auto pixel = static_cast<unsigned>(screenX);
                    if (occupied[pixel]) status |= StatusCollision;
                    occupied[pixel] = true;
                    if (pixels && color != 0 && !drawn[pixel]) {
                        pixels[pixel] = color;
                        drawn[pixel] = true;
                    }
                }
            }
        }
        if ((status & StatusOverflow) == 0) {
            fifthSprite = lastEvaluatedSprite;
            status = static_cast<std::uint8_t>((status & 0xE0) | fifthSprite);
        }
    }

    [[nodiscard]] std::uint8_t renderScanoutSpritePixel(unsigned x) noexcept
    {
        std::uint8_t firstColor = 0;
        unsigned opaqueSprites = 0;
        bool hasVisibleColor = false;
        for (std::uint8_t index = 0; index < scanoutSpriteCount; ++index) {
            const auto& sprite = scanoutSprites[index];
            const auto deltaX = static_cast<int>(x) - sprite.x;
            const unsigned displayedWidth = scanoutSpriteSize * scanoutMagnification;
            if (deltaX < 0 || deltaX >= static_cast<int>(displayedWidth)) {
                continue;
            }

            const auto sourceX = static_cast<unsigned>(deltaX) / scanoutMagnification;
            const auto patternByte = sprite.patternBytes[sourceX / 8];
            if ((patternByte & (0x80 >> (sourceX & 7))) == 0) continue;

            ++opaqueSprites;
            if (sprite.color != 0 && !hasVisibleColor) {
                firstColor = sprite.color;
                hasVisibleColor = true;
            }
        }
        if (opaqueSprites > 1) status |= StatusCollision;
        return firstColor;
    }

    void advanceDotState() noexcept
    {
        if (spriteDataFetchActive && totalDots >= nextSpriteDataFetchDot) {
            fetchSpriteData();
        }
        if (spriteDataReady && scanline == spriteTargetScanline) {
            commitSpriteEvaluation();
        }
        if (dot == 2) {
            beginSpriteEvaluation(static_cast<std::uint16_t>(
                (scanline + 1) % totalScanlines()));
        }
        if (spriteYFetchActive && spriteYFetchIndex < SpriteYFetchDots.size() &&
            dot == SpriteYFetchDots[spriteYFetchIndex]) {
            fetchSpriteY();
        }

        if (scanline >= displayStart() &&
            scanline < displayStart() + ScreenHeight) {
            const auto y = static_cast<unsigned>(scanline - displayStart());
            advanceBackgroundFetches(y);
        }

        if (dot >= HorizontalDisplayStart &&
            dot < HorizontalDisplayStart + ScreenWidth &&
            scanline >= displayStart() &&
            scanline < displayStart() + ScreenHeight) {
            const auto y = static_cast<unsigned>(scanline - displayStart());
            const auto x = static_cast<unsigned>(dot - HorizontalDisplayStart);
            const auto index = y * ScreenWidth + x;
            auto color = scanoutBackground[x];
            const auto spriteColor = renderScanoutSpritePixel(x);
            if (spriteColor != 0) color = spriteColor;
            scanout[index] = color;
        } else if (dot >= HorizontalDisplayStart &&
                   dot < HorizontalDisplayStart + ScreenWidth &&
                   scanline == displayStart() + ScreenHeight) {
            // Line 192 is not displayed, but its sprites are still checked for collisions.
            (void)renderScanoutSpritePixel(static_cast<unsigned>(dot - HorizontalDisplayStart));
        }

        backdropColors[static_cast<std::size_t>(scanline) * DotsPerScanline + dot] =
            registers[7] & 0x0F;

        if (++dot == DotsPerScanline) {
            dot = 0;
            scanline = (scanline + 1) % totalScanlines();
            scanlineRegisters = registers;
            backgroundFetches = {};
            const auto backdropColor = scanlineRegisters[7] & 0x0F;
            scanoutBackground.fill(backdropColor == 0 ? PaletteBackdrop : backdropColor);
            if (scanline == displayStart() + ScreenHeight + 1) {
                status |= StatusVBlank;
            } else if (scanline == 0) {
                status &= static_cast<std::uint8_t>(~StatusVBlank);
            }
        }
        updateInterrupt();
        ++totalDots;
    }
};

TMS99X8::TMS99X8(Model model)
    : impl(std::make_unique<Impl>(model))
{
    reset();
}

TMS99X8::~TMS99X8() = default;
TMS99X8::TMS99X8(TMS99X8&&) noexcept = default;
TMS99X8& TMS99X8::operator=(TMS99X8&&) noexcept = default;

void TMS99X8::reset()
{
    impl->vram.fill(0);
    impl->registers.fill(0);
    impl->scanlineRegisters.fill(0);
    impl->scanout.fill(0);
    impl->backdropColors.fill(0);
    impl->scanoutBackground.fill(0);
    impl->scanoutSprites = {};
    impl->pendingScanoutSprites = {};
    impl->spriteCandidates = {};
    impl->scanoutSpriteCount = 0;
    impl->pendingScanoutSpriteCount = 0;
    impl->spriteYFetchIndex = 0;
    impl->spriteDataFetchSprite = 0;
    impl->spriteDataFetchPhase = 0;
    impl->spriteEvaluationTargetY = 0;
    impl->spriteTargetScanline = 0;
    impl->status = 0;
    impl->fifthSprite = 31;
    impl->cpuVramData = 0;
    impl->pendingVramData = 0;
    impl->controlFirstByte = 0;
    impl->address = 0;
    impl->scanline = 0;
    impl->dot = 0;
    impl->masterPhase = 0;
    impl->masterTicks = 0;
    impl->totalDots = 0;
    impl->nextSpriteDataFetchDot = 0;
    impl->pendingVramTick = 0;
    impl->pendingVramEarliestTick = 0;
    impl->spriteEvaluationScanline = 0xFFFF;
    impl->controlLatch = false;
    impl->interruptRequest = false;
    impl->pendingVramAccess = false;
    impl->pendingVramRead = false;
    impl->spriteYFetchActive = false;
    impl->spriteDataFetchActive = false;
    impl->spriteDataReady = false;
    impl->spriteYTerminated = false;
}

std::uint8_t TMS99X8::read(std::uint8_t port)
{
    if (port & 1) return impl->readStatus();

    if (impl->pendingVramAccess && impl->pendingVramRead &&
        impl->pendingVramTick <= impl->masterTicks + 9) {
        impl->executeCpuVramAccess();
    }
    const auto value = impl->cpuVramData;
    impl->requestCpuVramAccess(true, 0);
    impl->controlLatch = false;
    return value;
}

void TMS99X8::write(std::uint8_t port, std::uint8_t value)
{
    if (port & 1) {
        impl->writeControl(value);
        return;
    }

    impl->requestCpuVramAccess(false, value);
    impl->controlLatch = false;
}

void TMS99X8::advanceMasterTick() noexcept
{
    ++impl->masterTicks;
    if (impl->pendingVramAccess &&
        (impl->pendingVramTick <= impl->masterTicks ||
         (impl->pendingVramEarliestTick <= impl->masterTicks && impl->freedSpriteSlot()))) {
        impl->executeCpuVramAccess();
    }
    if (++impl->masterPhase == MasterTicksPerDot) {
        impl->masterPhase = 0;
        impl->advanceDotState();
    }
}

void TMS99X8::advanceDot() noexcept
{
    for (std::uint8_t tick = 0; tick < MasterTicksPerDot; ++tick) {
        advanceMasterTick();
    }
}

void TMS99X8::advanceScanline() noexcept
{
    const auto currentScanline = impl->scanline;
    do {
        advanceDot();
    } while (impl->scanline == currentScanline);
}

FrameBuffer TMS99X8::renderFrame()
{
    FrameBuffer frame{};
    for (unsigned y = 0; y < ScreenHeight; ++y) {
        impl->renderBackgroundLine(frame, y);
        impl->renderSpritesLine(frame.data() + y * ScreenWidth, y);
    }
    impl->renderSpritesLine(nullptr, ScreenHeight);
    return frame;
}

const BackdropRaster& TMS99X8::backdropRaster() const noexcept
{
    return impl->backdropColors;
}

const FrameBuffer& TMS99X8::scanoutFrame() const noexcept
{
    return impl->scanout;
}

YpbprFrame TMS99X8::renderYpbprFrame()
{
    const auto indexed = renderFrame();
    YpbprFrame frame{};
    for (std::size_t i = 0; i < indexed.size(); ++i) {
        const auto& rgb = RgbPalette[indexed[i] & 0x0F];
        const double red = rgb[0] / 255.0;
        const double green = rgb[1] / 255.0;
        const double blue = rgb[2] / 255.0;
        const double y = 0.299 * red + 0.587 * green + 0.114 * blue;
        const double pb = (blue - y) * 0.564;
        const double pr = (red - y) * 0.713;
        frame[i].y = static_cast<std::uint8_t>(y * 255.0 + 0.5);
        frame[i].pb = static_cast<std::int8_t>(pb * 127.0);
        frame[i].pr = static_cast<std::int8_t>(pr * 127.0);
    }
    return frame;
}

Outputs TMS99X8::outputs() const noexcept
{
    return {impl->interruptRequest, impl->status, impl->address, impl->scanline, impl->dot,
            impl->totalScanlines(), impl->displayStart()};
}

Model TMS99X8::model() const noexcept
{
    return impl->videoModel;
}

std::uint32_t TMS99X8::fieldRateHz() const noexcept
{
    return impl->videoModel == Model::Pal ? 50 : 60;
}

std::uint8_t TMS99X8::registerValue(std::uint8_t index) const noexcept
{
    return impl->registers[index & 7];
}

std::uint8_t TMS99X8::peekVram(std::uint16_t address) const noexcept
{
    return impl->vram[address & VramMask];
}

void TMS99X8::pokeVram(std::uint16_t address, std::uint8_t value) noexcept
{
    impl->vram[address & VramMask] = value;
}

std::vector<std::uint8_t> TMS99X8::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the VDP state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool TMS99X8::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    auto candidate = std::make_unique<Impl>(impl->videoModel);
    std::memcpy(candidate.get(), state.data(), sizeof(Impl));
    if (candidate->videoModel != impl->videoModel) return false;
    impl = std::move(candidate);
    return true;
}

} // namespace tms99x8
