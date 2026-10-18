#include "V9938/V9938.hpp"

#include <cstring>
#include <type_traits>

#include <array>

namespace v9938 {

namespace {

constexpr std::uint8_t ControlRegisterCount = 47;
constexpr std::uint8_t FirstCommandRegister = 32;

// Writable bits per register as given in the V9938 Technical Data Book; R#24-R#31 do not exist.
constexpr std::array<std::uint8_t, ControlRegisterCount> RegisterMasks = {
    0x7E, 0x7B, 0x7F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, // R#0-R#7
    0xFB, 0xBF, 0x07, 0x03, 0xFF, 0xFF, 0x07, 0x0F, // R#8-R#15
    0x0F, 0xBF, 0xFF, 0xFF, 0x3F, 0x3F, 0x3F, 0xFF, // R#16-R#23
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // R#24-R#31
    0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x01, 0xFF, 0x03, // R#32-R#39
    0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x7F, 0xFF,       // R#40-R#46
};

// Palette after reset, appendix 8 of the data book, as 0x0GRB.
constexpr std::array<std::uint16_t, PaletteSize> ResetPalette = {
    0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
    0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777,
};

// Horizontal sync and left erase precede the left border of every line.
constexpr std::uint32_t LeftBorderStartTicks = 100 + 102;
// Vertical sync and top erase precede the top border of every field.
constexpr std::uint32_t TopBorderStartLine = 3 + 13;
constexpr std::uint32_t HorizontalBlankTicksText = 404;
constexpr std::uint32_t HorizontalBlankTicksGraphic = 312;
constexpr std::uint32_t LineFlagTicksText = 87 + 27 + LeftBorderStartTicks;
constexpr std::uint32_t LineFlagTicksGraphic = 59 + 27 + LeftBorderStartTicks;
constexpr std::uint32_t Never = 0xFFFFFFFF;

// Command engine: fixed duration of one step in master clocks until access slots are modelled.
constexpr std::uint32_t PointTicks = 32;
constexpr std::uint32_t PsetTicks = 24 + 48;
constexpr std::uint32_t SearchTicks = 88;
constexpr std::uint32_t LineTicks = 24 + 88;
constexpr std::uint32_t LineMinorStepTicks = 32;
constexpr std::uint32_t LogicalFillTicks = 24 + 72;
constexpr std::uint32_t LogicalCopyTicks = 32 + 24 + 64;
constexpr std::uint32_t ByteFillTicks = 48;
constexpr std::uint32_t ByteCopyTicks = 24 + 64;
constexpr std::uint32_t LineCopyTicks = 24 + 40;
constexpr std::uint32_t RectangleLineEndTicks = 64;

constexpr std::uint8_t CommandPoint = 0x4;
constexpr std::uint8_t CommandPset = 0x5;
constexpr std::uint8_t CommandSearch = 0x6;
constexpr std::uint8_t CommandLine = 0x7;
constexpr std::uint8_t CommandLmmv = 0x8;
constexpr std::uint8_t CommandLmmm = 0x9;
constexpr std::uint8_t CommandLmcm = 0xA;
constexpr std::uint8_t CommandLmmc = 0xB;
constexpr std::uint8_t CommandHmmv = 0xC;
constexpr std::uint8_t CommandHmmm = 0xD;
constexpr std::uint8_t CommandYmmm = 0xE;
constexpr std::uint8_t CommandHmmc = 0xF;

// R#45 argument bits.
constexpr std::uint8_t ArgumentMajor = 0x01;
constexpr std::uint8_t ArgumentEqual = 0x02;
constexpr std::uint8_t ArgumentLeft = 0x04;
constexpr std::uint8_t ArgumentUp = 0x08;
constexpr std::uint8_t ArgumentSourceExpansion = 0x10;
constexpr std::uint8_t ArgumentDestinationExpansion = 0x20;

// Logical operation on the destination byte: colour holds the dot at its position, mask has ones
// at all other dot positions.
std::uint8_t logicalOperation(std::uint8_t operation, std::uint8_t destination, std::uint8_t colour, std::uint8_t mask) noexcept
{
    if ((operation & 0x08) && colour == 0) return destination;
    switch (operation & 0x07) {
    case 0:
        return static_cast<std::uint8_t>((destination & mask) | colour);
    case 1:
        return static_cast<std::uint8_t>(destination & (colour | mask));
    case 2:
        return static_cast<std::uint8_t>(destination | colour);
    case 3:
        return static_cast<std::uint8_t>(destination ^ colour);
    case 4:
        return static_cast<std::uint8_t>((destination & mask) | ~(colour | mask));
    default:
        return destination;
    }
}

// Fixed sprite colours of GRAPHIC 7 as 0x0GRB, data book "Setting the Sprite Colors".
constexpr std::array<std::uint16_t, PaletteSize> Graphic7SpriteColours = {
    0x000, 0x002, 0x030, 0x032, 0x300, 0x302, 0x330, 0x332,
    0x472, 0x007, 0x070, 0x077, 0x700, 0x707, 0x770, 0x777,
};

constexpr std::uint8_t ModeM1 = 0x01;
constexpr std::uint8_t ModeM2 = 0x02;
constexpr std::uint8_t ModeM3 = 0x04;
constexpr std::uint8_t ModeM4 = 0x08;
constexpr std::uint8_t ModeM5 = 0x10;
constexpr std::uint8_t ModeGraphic4 = ModeM4 | ModeM3;
constexpr std::uint8_t ModeGraphic5 = ModeM5;
constexpr std::uint8_t ModeGraphic6 = ModeM5 | ModeM3;
constexpr std::uint8_t ModeGraphic7 = ModeM5 | ModeM4 | ModeM3;

// GRAPHIC 7 colour byte GGGRRRBB as 0x0GRB; the two blue bits are spread over three.
constexpr std::uint16_t graphic7Colour(std::uint8_t value) noexcept
{
    const unsigned blue = value & 0x03;
    return static_cast<std::uint16_t>(((value >> 5) << 8) | (((value >> 2) & 0x07) << 4) | (blue << 1) | (blue >> 1));
}

} // namespace

struct V9938::Impl {
    std::array<std::uint8_t, VramSize> vram{};
    std::array<std::uint8_t, ControlRegisterCount> registers{};
    std::array<std::uint16_t, PaletteSize> palette{};

    std::uint16_t vramPointer = 0;
    std::uint8_t cpuVramData = 0;
    std::uint8_t dataLatch = 0;
    bool registerLatchFull = false;
    bool paletteLatchFull = false;

    bool verticalFlag = false;
    bool lineInterrupt = false;
    bool evenOddField = false;
    bool interlace = false;

    bool pal = false;
    std::uint32_t ticksPerFrame = NtscScanlines * MasterTicksPerScanline;
    std::uint32_t tick = 0;
    std::uint32_t displayStart = 0;
    std::uint32_t displayEnd = 0;
    std::uint32_t lineMatch = Never;
    std::uint32_t frames = 0;

    // S#0 bits 6-0: fifth/ninth sprite flag, collision flag and sprite number.
    std::uint8_t spriteStatus = 0;
    std::uint16_t collisionX = 0;
    std::uint16_t collisionY = 0;

    bool blinkOn = false;
    std::uint8_t blinkCount = 0;

    std::uint32_t nextLineTick = 0;
    std::uint32_t nextRenderTick = Never;
    std::uint16_t renderLine = 0;
    std::uint16_t renderedLines = 0;
    FrameBuffer frameBuffer{};
    BorderRaster border{};

    // Command engine registers R#32-R#46 and working counters.
    std::uint16_t commandSx = 0;
    std::uint16_t commandSy = 0;
    std::uint16_t commandDx = 0;
    std::uint16_t commandDy = 0;
    std::uint16_t commandNx = 0;
    std::uint16_t commandNy = 0;
    std::uint8_t commandColour = 0;
    std::uint8_t commandArgument = 0;
    std::uint8_t commandRegister = 0;
    std::uint16_t workSx = 0;
    std::uint16_t workDx = 0;
    std::uint16_t workCount = 0;
    std::uint16_t rowLength = 0;
    std::uint16_t rowsLeft = 0;
    std::uint16_t lineError = 0;
    bool commandExecuting = false;
    bool transferReady = false;
    bool borderDetected = false;
    bool cpuTransferPending = false;
    std::uint32_t commandCountdown = 0;

    struct LineSprite {
        int x = 0;
        // Sprite dots left-aligned, magnification already applied.
        std::uint32_t pattern = 0;
        std::uint8_t colour = 0;
    };

    [[nodiscard]] std::uint8_t mode() const noexcept
    {
        const std::uint8_t r0 = registers[0];
        const std::uint8_t r1 = registers[1];
        return static_cast<std::uint8_t>(((r1 >> 4) & ModeM1) | ((r1 >> 2) & ModeM2) | ((r0 << 1) & (ModeM3 | ModeM4 | ModeM5)));
    }

    [[nodiscard]] bool textMode() const noexcept
    {
        const std::uint8_t m = mode();
        return m == ModeM1 || m == (ModeM1 | ModeM3) || m == (ModeM1 | ModeM4);
    }

    [[nodiscard]] std::uint32_t displayLines() const noexcept { return (registers[9] & 0x80) ? 212 : 192; }

    // R#18 bits 7-4 shift the picture; 0 is the centre, 7 the top and 8 the bottom.
    [[nodiscard]] std::uint32_t verticalAdjust() const noexcept { return ((registers[18] >> 4) ^ 0x07) & 0x0F; }
    [[nodiscard]] std::uint32_t horizontalAdjust() const noexcept { return ((registers[18] & 0x0F) ^ 0x07) & 0x0F; }

    [[nodiscard]] std::uint32_t rightBorderTicks() const noexcept
    {
        const std::uint32_t leftSprites = LeftBorderStartTicks + 56 + horizontalAdjust() * 4 - 7 * 4 + (textMode() ? 9 * 4 : 0);
        return leftSprites + (textMode() ? 960 : 1024);
    }

    void scheduleDisplayStart() noexcept
    {
        const std::uint32_t lineZero = TopBorderStartLine + (pal ? 36 : 9) + ((registers[9] & 0x80) ? 0 : 10) + verticalAdjust();
        displayStart = lineZero * MasterTicksPerScanline + LeftBorderStartTicks;
    }

    void scheduleDisplayEnd() noexcept { displayEnd = displayStart + displayLines() * MasterTicksPerScanline; }

    // The line counter compared with R#19 starts at the first display line and counts R#23 along.
    void scheduleLineMatch() noexcept
    {
        std::uint32_t match = displayStart - LeftBorderStartTicks
            + static_cast<std::uint8_t>(registers[19] - registers[23]) * MasterTicksPerScanline + rightBorderTicks();
        if (match >= ticksPerFrame) {
            match -= ticksPerFrame;
            // The counter is cleared at the top border, so later lines of the previous field never match.
            if (match >= (8 + verticalAdjust()) * MasterTicksPerScanline) match = Never;
        }
        lineMatch = match;
    }

    void startFrame() noexcept
    {
        tick = 0;
        ++frames;
        evenOddField = !evenOddField;
        pal = (registers[9] & 0x02) != 0;
        interlace = (registers[9] & 0x08) != 0;
        ticksPerFrame = (pal ? PalScanlines : NtscScanlines) * MasterTicksPerScanline;
        scheduleDisplayStart();
        scheduleDisplayEnd();
        scheduleLineMatch();
        advanceBlink();
        renderLine = 0;
        nextRenderTick = displayStart;
        nextLineTick = 0;
    }

    // R#13 gives the ON and OFF periods in units of ten fields.
    void advanceBlink() noexcept
    {
        if (blinkCount == 0 || --blinkCount != 0) return;
        blinkOn = !blinkOn;
        blinkCount = static_cast<std::uint8_t>((blinkOn ? registers[13] >> 4 : registers[13] & 0x0F) * 10);
    }

    void restartBlink() noexcept
    {
        const std::uint8_t on = registers[13] >> 4;
        const std::uint8_t off = registers[13] & 0x0F;
        blinkOn = on != 0;
        blinkCount = (on != 0 && off != 0) ? static_cast<std::uint8_t>(on * 10) : 0;
    }

    // ---------------------------------------------------------------------------------------------
    // Rendering
    // ---------------------------------------------------------------------------------------------

    // Table registers hold ones below their lowest address bit; the generated index is ANDed with
    // them, so clearing a base bit mirrors the matching part of the table.
    [[nodiscard]] std::uint8_t tableRead(std::uint32_t base, std::uint32_t index, unsigned indexBits) const noexcept
    {
        const std::uint32_t upper = ~((1u << indexBits) - 1u);
        return vram[physicalAddress(base & (index | upper))];
    }

    // GRAPHIC 6 and 7 interleave both 64 KiB banks: address bit 0 selects the bank.
    [[nodiscard]] bool planar() const noexcept { return (mode() & (ModeM5 | ModeM3)) == (ModeM5 | ModeM3); }

    [[nodiscard]] std::uint32_t physicalAddress(std::uint32_t address) const noexcept
    {
        address &= VramSize - 1;
        return planar() ? (((address & 1u) << 16) | (address >> 1)) : address;
    }

    [[nodiscard]] std::uint32_t nameBase() const noexcept { return (static_cast<std::uint32_t>(registers[2]) << 10) | 0x3FF; }

    [[nodiscard]] std::uint32_t colourBase() const noexcept
    {
        return (static_cast<std::uint32_t>(registers[10]) << 14) | (static_cast<std::uint32_t>(registers[3]) << 6) | 0x3F;
    }

    [[nodiscard]] std::uint32_t patternBase() const noexcept { return (static_cast<std::uint32_t>(registers[4]) << 11) | 0x7FF; }

    [[nodiscard]] std::uint32_t spriteAttributeBase() const noexcept
    {
        return (static_cast<std::uint32_t>(registers[11]) << 15) | (static_cast<std::uint32_t>(registers[5]) << 7) | 0x7F;
    }

    [[nodiscard]] std::uint32_t spritePatternBase() const noexcept { return (static_cast<std::uint32_t>(registers[6]) << 11) | 0x7FF; }

    [[nodiscard]] bool transparentColourZero() const noexcept { return (registers[8] & 0x20) == 0; }

    [[nodiscard]] std::uint16_t backdropColour() const noexcept { return palette[registers[7] & 0x0F]; }

    // GRAPHIC 7 uses R#7 as a GGGRRRBB colour; GRAPHIC 5 uses bits 3-2 for even and 1-0 for odd dots.
    [[nodiscard]] BorderColours borderColours() const noexcept
    {
        switch (mode()) {
        case ModeGraphic7:
            return {graphic7Colour(registers[7]), graphic7Colour(registers[7])};
        case ModeGraphic5:
            return {palette[(registers[7] >> 2) & 0x03], palette[registers[7] & 0x03]};
        default:
            return {backdropColour(), backdropColour()};
        }
    }

    // Colour 0 shows the backdrop unless R#8 TP makes it an ordinary palette entry.
    [[nodiscard]] std::uint16_t colour(std::uint8_t index) const noexcept
    {
        index &= 0x0F;
        return (index == 0 && transparentColourZero()) ? backdropColour() : palette[index];
    }

    // 0 for no sprites (text modes), 1 for TMS9918 sprites, 2 for the V9938 sprite mode.
    [[nodiscard]] int spriteMode() const noexcept
    {
        if (textMode()) return 0;
        return (mode() & (ModeM4 | ModeM5)) ? 2 : 1;
    }

    static void plot256(std::uint16_t* line, int x, std::uint16_t value) noexcept
    {
        line[2 * x] = value;
        line[2 * x + 1] = value;
    }

    static void drawPattern256(std::uint16_t* line, int x, std::uint8_t pattern, int width, std::uint16_t fg, std::uint16_t bg) noexcept
    {
        for (int bit = 0; bit < width; ++bit) plot256(line, x + bit, (pattern & (0x80 >> bit)) ? fg : bg);
    }

    void fillBackdrop(std::uint16_t* line) const noexcept
    {
        const BorderColours colours = borderColours();
        for (std::size_t x = 0; x < ScreenWidth; x += 2) {
            line[x] = colours.even;
            line[x + 1] = colours.odd;
        }
    }

    // Line number in units of 128 (G4/G5) or 256 (G6/G7) bytes. R#2 masks the line; the odd page bit
    // is cleared on the first field with EO and during the ON period of R#13.
    [[nodiscard]] std::uint32_t bitmapLine(unsigned y) const noexcept
    {
        const bool oddPage = ((registers[9] & 0x04) == 0 || evenOddField) && !blinkOn;
        const std::uint32_t pageBit = oddPage ? 0x100 : 0x000;
        const std::uint32_t upperPage = planar() ? 0x000 : 0x200;
        return (nameBase() >> 7) & (upperPage | pageBit | y);
    }

    void renderGraphic4(std::uint16_t* line, unsigned y) const noexcept
    {
        const std::uint32_t base = bitmapLine(y) * 128;
        for (unsigned index = 0; index < 128; ++index) {
            const std::uint8_t dots = vram[(base + index) & (VramSize - 1)];
            plot256(line, static_cast<int>(index * 2), colour(dots >> 4));
            plot256(line, static_cast<int>(index * 2 + 1), colour(dots));
        }
    }

    void renderGraphic5(std::uint16_t* line, unsigned y) const noexcept
    {
        const std::uint32_t base = bitmapLine(y) * 128;
        const BorderColours backdrop = borderColours();
        const bool transparent = transparentColourZero();
        for (unsigned index = 0; index < 128; ++index) {
            const std::uint8_t dots = vram[(base + index) & (VramSize - 1)];
            for (unsigned dot = 0; dot < 4; ++dot) {
                const unsigned x = index * 4 + dot;
                const std::uint8_t value = (dots >> (6 - dot * 2)) & 0x03;
                if (value == 0 && transparent) {
                    line[x] = (x & 1) ? backdrop.odd : backdrop.even;
                } else {
                    line[x] = palette[value];
                }
            }
        }
    }

    void renderGraphic6(std::uint16_t* line, unsigned y) const noexcept
    {
        const std::uint32_t base = bitmapLine(y) * 256;
        for (unsigned index = 0; index < 256; ++index) {
            const std::uint8_t dots = vram[physicalAddress(base + index)];
            line[index * 2] = colour(dots >> 4);
            line[index * 2 + 1] = colour(dots);
        }
    }

    // GRAPHIC 7 shows the colour byte directly; TP has no effect on the background.
    void renderGraphic7(std::uint16_t* line, unsigned y) const noexcept
    {
        const std::uint32_t base = bitmapLine(y) * 256;
        for (unsigned index = 0; index < 256; ++index) {
            plot256(line, static_cast<int>(index), graphic7Colour(vram[physicalAddress(base + index)]));
        }
    }

    // TEXT 1: 40 x 6 dots, nine dots right of the graphic-mode origin.
    void renderText1(std::uint16_t* line, unsigned displayLine) const noexcept
    {
        const std::uint16_t fg = colour(registers[7] >> 4);
        const std::uint16_t bg = colour(registers[7]);
        const unsigned row = displayLine / 8;
        const unsigned patternLine = (displayLine + registers[23]) & 7;
        for (unsigned column = 0; column < 40; ++column) {
            const std::uint8_t code = tableRead(nameBase(), (row * 40 + column + 0xC00), 12);
            const std::uint8_t pattern = tableRead(patternBase(), code * 8u + patternLine, 11);
            drawPattern256(line, 9 + static_cast<int>(column) * 6, pattern, 6, fg, bg);
        }
    }

    // TEXT 2: 80 x 6 dots on the 512-dot grid, blink attributes switch to the R#12 colours.
    void renderText2(std::uint16_t* line, unsigned displayLine) const noexcept
    {
        const std::uint16_t plainFg = colour(registers[7] >> 4);
        const std::uint16_t plainBg = colour(registers[7]);
        std::uint16_t blinkFg = plainFg;
        std::uint16_t blinkBg = plainBg;
        if (blinkOn) {
            const std::uint8_t fgIndex = registers[12] >> 4;
            blinkBg = palette[registers[12] & 0x0F];
            blinkFg = fgIndex ? palette[fgIndex] : blinkBg;
        }
        const unsigned row = displayLine / 8;
        const unsigned patternLine = (displayLine + registers[23]) & 7;
        for (unsigned column = 0; column < 80; ++column) {
            const std::uint8_t code = tableRead(nameBase(), row * 80 + column, 12);
            const std::uint8_t pattern = tableRead(patternBase(), code * 8u + patternLine, 11);
            const std::uint8_t attributes = tableRead(colourBase(), row * 10 + column / 8, 9);
            const bool blink = (attributes & (0x80 >> (column & 7))) != 0;
            const std::uint16_t fg = blink ? blinkFg : plainFg;
            const std::uint16_t bg = blink ? blinkBg : plainBg;
            for (int bit = 0; bit < 6; ++bit) {
                line[18 + column * 6 + static_cast<unsigned>(bit)] = (pattern & (0x80 >> bit)) ? fg : bg;
            }
        }
    }

    void renderGraphic1(std::uint16_t* line, unsigned y) const noexcept
    {
        for (unsigned column = 0; column < 32; ++column) {
            const std::uint8_t code = tableRead(nameBase(), (y / 8) * 32 + column, 10);
            const std::uint8_t pattern = tableRead(patternBase(), code * 8u + (y & 7), 11);
            const std::uint8_t colours = tableRead(colourBase(), code / 8u, 6);
            drawPattern256(line, static_cast<int>(column) * 8, pattern, 8, colour(colours >> 4), colour(colours));
        }
    }

    // GRAPHIC 2 and 3: the screen is split into thirds with their own pattern and colour tables.
    void renderGraphic2(std::uint16_t* line, unsigned y) const noexcept
    {
        const unsigned third = (y / 64) * 0x800;
        for (unsigned column = 0; column < 32; ++column) {
            const std::uint8_t code = tableRead(nameBase(), (y / 8) * 32 + column, 10);
            const unsigned index = third + code * 8u + (y & 7);
            const std::uint8_t pattern = tableRead(patternBase(), index, 13);
            const std::uint8_t colours = tableRead(colourBase(), index, 13);
            drawPattern256(line, static_cast<int>(column) * 8, pattern, 8, colour(colours >> 4), colour(colours));
        }
    }

    // MULTICOLOR: every pattern byte holds two 4 x 4 blocks.
    void renderMulticolour(std::uint16_t* line, unsigned y) const noexcept
    {
        for (unsigned column = 0; column < 32; ++column) {
            const std::uint8_t code = tableRead(nameBase(), (y / 8) * 32 + column, 10);
            const std::uint8_t colours = tableRead(patternBase(), code * 8u + ((y / 4) & 7), 11);
            const int x = static_cast<int>(column) * 8;
            for (int dot = 0; dot < 4; ++dot) {
                plot256(line, x + dot, colour(colours >> 4));
                plot256(line, x + 4 + dot, colour(colours));
            }
        }
    }

    [[nodiscard]] static std::uint32_t magnify(std::uint16_t pattern) noexcept
    {
        std::uint32_t result = 0;
        for (int bit = 15; bit >= 0; --bit) {
            result <<= 2;
            if (pattern & (1u << bit)) result |= 3;
        }
        return result;
    }

    // Collects the sprites of one display line and updates S#0 and the collision registers.
    int collectSprites(unsigned displayLine, int mode, std::array<LineSprite, 8>& sprites) noexcept
    {
        const int size = (registers[1] & 0x02) ? 16 : 8;
        const bool magnified = (registers[1] & 0x01) != 0;
        const unsigned magSize = static_cast<unsigned>(size) << (magnified ? 1 : 0);
        const int limit = mode == 1 ? 4 : 8;
        const std::uint8_t terminator = mode == 1 ? 208 : 216;
        const unsigned attributeOffset = mode == 1 ? 0 : 0x200;
        const unsigned attributeBits = mode == 1 ? 7 : 10;
        // Sprites are evaluated one line ahead of the line they appear on.
        const unsigned checkLine = (displayLine + registers[23] - 1) & 0xFF;

        int count = 0;
        int overflow = -1;
        int sprite = 0;
        for (; sprite < 32; ++sprite) {
            const unsigned attribute = attributeOffset + static_cast<unsigned>(sprite) * 4;
            const std::uint8_t y = tableRead(spriteAttributeBase(), attribute, attributeBits);
            if (y == terminator) break;
            const unsigned spriteLine = (checkLine - y) & 0xFF;
            if (spriteLine >= magSize) continue;
            if (count == limit) {
                overflow = sprite;
                break;
            }
            const unsigned patternLine = spriteLine >> (magnified ? 1 : 0);
            const std::uint8_t x = tableRead(spriteAttributeBase(), attribute + 1, attributeBits);
            std::uint8_t name = tableRead(spriteAttributeBase(), attribute + 2, attributeBits);
            if (size == 16) name &= 0xFC;
            const std::uint8_t colourByte = mode == 1
                ? tableRead(spriteAttributeBase(), attribute + 3, attributeBits)
                : tableRead(spriteAttributeBase(), static_cast<unsigned>(sprite) * 16 + patternLine, 10);
            const unsigned patternIndex = name * 8u + patternLine;
            std::uint16_t dots = static_cast<std::uint16_t>(tableRead(spritePatternBase(), patternIndex, 11) << 8);
            if (size == 16) dots = static_cast<std::uint16_t>(dots | tableRead(spritePatternBase(), patternIndex + 16, 11));

            LineSprite& entry = sprites[static_cast<std::size_t>(count++)];
            entry.x = (colourByte & 0x80) ? x - 32 : x;
            entry.pattern = magnified ? magnify(dots) : static_cast<std::uint32_t>(dots) << 16;
            entry.colour = colourByte;
        }

        // The overflow sprite number is only latched while F and 5S are clear.
        if (overflow >= 0 && !verticalFlag && !(spriteStatus & 0x40)) {
            spriteStatus = static_cast<std::uint8_t>(0x40 | (spriteStatus & 0x20) | overflow);
        } else if (!(spriteStatus & 0x40)) {
            spriteStatus = static_cast<std::uint8_t>((spriteStatus & 0x20) | (sprite < 31 ? sprite : 31));
        }

        if (!(spriteStatus & 0x20)) detectCollision(sprites, count, mode, displayLine);
        return count;
    }

    void detectCollision(const std::array<LineSprite, 8>& sprites, int count, int mode, unsigned displayLine) noexcept
    {
        // On the V9938 colour-0 sprites only collide when TP makes colour 0 visible.
        const auto canCollide = [&](const LineSprite& sprite) {
            if (transparentColourZero() && (sprite.colour & 0x0F) == 0) return false;
            return mode == 1 || (sprite.colour & 0x60) == 0;
        };
        int firstX = 256;
        for (int i = 0; i < count; ++i) {
            if (!canCollide(sprites[static_cast<std::size_t>(i)])) continue;
            for (int j = i + 1; j < count; ++j) {
                if (!canCollide(sprites[static_cast<std::size_t>(j)])) continue;
                const LineSprite& a = sprites[static_cast<std::size_t>(i)];
                const LineSprite& b = sprites[static_cast<std::size_t>(j)];
                for (int dot = 0; dot < 32; ++dot) {
                    if (!(a.pattern & (0x80000000u >> dot))) continue;
                    const int x = a.x + dot;
                    const int other = x - b.x;
                    if (x < 0 || x > 255 || other < 0 || other > 31) continue;
                    if ((b.pattern & (0x80000000u >> other)) && x < firstX) firstX = x;
                }
            }
        }
        if (firstX < 256) {
            spriteStatus |= 0x20;
            collisionX = static_cast<std::uint16_t>(firstX + 12);
            collisionY = static_cast<std::uint16_t>(displayLine + 7);
        }
    }

    void drawSprites(std::uint16_t* line, const std::array<LineSprite, 8>& sprites, int count, int mode) const noexcept
    {
        const bool transparent = transparentColourZero();
        int first = 0;
        if (mode == 2) {
            // A CC sprite only shows behind a preceding sprite without CC.
            while (first < count && (sprites[static_cast<std::size_t>(first)].colour & 0x40)) ++first;
        }
        for (int i = count - 1; i >= first; --i) {
            const LineSprite& sprite = sprites[static_cast<std::size_t>(i)];
            const std::uint8_t base = sprite.colour & 0x0F;
            if (base == 0 && transparent) continue;
            for (int dot = 0; dot < 32; ++dot) {
                if (!(sprite.pattern & (0x80000000u >> dot))) continue;
                const int x = sprite.x + dot;
                if (x < 0 || x > 255) continue;
                std::uint8_t value = base;
                if (mode == 2) {
                    for (int j = i + 1; j < count; ++j) {
                        const LineSprite& merged = sprites[static_cast<std::size_t>(j)];
                        if (!(merged.colour & 0x40)) break;
                        const int other = x - merged.x;
                        if (other >= 0 && other < 32 && (merged.pattern & (0x80000000u >> other))) value |= merged.colour & 0x0F;
                    }
                }
                plotSprite(line, x, value);
            }
        }
    }

    void plotSprite(std::uint16_t* line, int x, std::uint8_t value) const noexcept
    {
        switch (mode()) {
        case ModeGraphic5:
            // Hardware tiling: bits 3-2 colour the even dot, bits 1-0 the odd dot.
            line[2 * x] = palette[(value >> 2) & 0x03];
            line[2 * x + 1] = palette[value & 0x03];
            break;
        case ModeGraphic7:
            plot256(line, x, Graphic7SpriteColours[value & 0x0F]);
            break;
        default:
            plot256(line, x, palette[value & 0x0F]);
            break;
        }
    }

    void renderDisplayLine(unsigned displayLine) noexcept
    {
        std::uint16_t* line = &frameBuffer[displayLine * ScreenWidth];
        fillBackdrop(line);
        if (!(registers[1] & 0x40)) return;
        const unsigned y = (displayLine + registers[23]) & 0xFF;
        switch (mode()) {
        case ModeM1:
        case ModeM1 | ModeM3:
            renderText1(line, displayLine);
            break;
        case ModeM1 | ModeM4:
            renderText2(line, displayLine);
            break;
        case 0:
            renderGraphic1(line, y);
            break;
        case ModeM2:
            renderMulticolour(line, y);
            break;
        case ModeM3:
        case ModeM4:
            renderGraphic2(line, y);
            break;
        case ModeGraphic4:
            renderGraphic4(line, y);
            break;
        case ModeGraphic5:
            renderGraphic5(line, y);
            break;
        case ModeGraphic6:
            renderGraphic6(line, y);
            break;
        case ModeGraphic7:
            renderGraphic7(line, y);
            break;
        default:
            break;
        }

        const int sprites = spriteMode();
        if (sprites == 0 || (registers[8] & 0x02)) return;
        std::array<LineSprite, 8> visible{};
        const int count = collectSprites(displayLine, sprites, visible);
        drawSprites(line, visible, count, sprites);
    }

    [[nodiscard]] std::uint32_t vramAddress() const noexcept
    {
        return (static_cast<std::uint32_t>(registers[14]) << 14) | vramPointer;
    }

    // The VG 8235 has no expansion RAM: with MXC set reads return FFh and writes are lost.
    [[nodiscard]] bool expansionRamSelected() const noexcept { return (registers[45] & 0x40) != 0; }

    void advanceVramPointer() noexcept
    {
        vramPointer = static_cast<std::uint16_t>((vramPointer + 1) & 0x3FFF);
        // In TEXT 1, MULTICOLOR, GRAPHIC 1 and GRAPHIC 2 R#14 is not incremented.
        if (vramPointer == 0 && (mode() & (ModeM4 | ModeM5)) != 0) {
            registers[14] = static_cast<std::uint8_t>((registers[14] + 1) & 0x07);
        }
    }

    void readVramAhead() noexcept
    {
        cpuVramData = expansionRamSelected() ? 0xFF : vram[physicalAddress(vramAddress())];
        advanceVramPointer();
    }

    void writeVram(std::uint8_t value) noexcept
    {
        cpuVramData = value;
        if (!expansionRamSelected()) vram[physicalAddress(vramAddress())] = value;
        advanceVramPointer();
    }

    [[nodiscard]] bool horizontalRetrace() const noexcept
    {
        const std::uint32_t sinceRightBorder = (tick + MasterTicksPerScanline - rightBorderTicks() % MasterTicksPerScanline) % MasterTicksPerScanline;
        return sinceRightBorder < (textMode() ? HorizontalBlankTicksText : HorizontalBlankTicksGraphic);
    }

    [[nodiscard]] bool verticalRetrace() const noexcept
    {
        return tick + MasterTicksPerScanline < displayStart || tick >= displayEnd;
    }

    [[nodiscard]] bool lineFlag() const noexcept
    {
        if (registers[0] & 0x10) return lineInterrupt;
        // With IE1 off FH only shows the line match until the next left border.
        if (lineMatch == Never) return false;
        std::uint32_t since = tick + ticksPerFrame - lineMatch;
        if (since >= ticksPerFrame) since -= ticksPerFrame;
        return since < (textMode() ? LineFlagTicksText : LineFlagTicksGraphic);
    }


    // ---------------------------------------------------------------------------------------------
    // Command engine
    // ---------------------------------------------------------------------------------------------

    // Commands work in GRAPHIC 4 to 7 only: 0 = G4, 1 = G5, 2 = G6, 3 = G7, -1 = other modes.
    [[nodiscard]] int commandMode() const noexcept
    {
        switch (mode()) {
        case ModeGraphic4:
            return 0;
        case ModeGraphic5:
            return 1;
        case ModeGraphic6:
            return 2;
        case ModeGraphic7:
            return 3;
        default:
            return -1;
        }
    }

    [[nodiscard]] unsigned pixelsPerLine() const noexcept { return (commandMode() == 1 || commandMode() == 2) ? 512 : 256; }

    // log2 of the dots per byte.
    [[nodiscard]] unsigned pixelShift() const noexcept
    {
        static constexpr std::array<unsigned, 4> Shifts = {1, 2, 1, 0};
        return Shifts[static_cast<std::size_t>(commandMode())];
    }

    [[nodiscard]] std::uint8_t colourMask() const noexcept
    {
        static constexpr std::array<std::uint8_t, 4> Masks = {0x0F, 0x03, 0x0F, 0xFF};
        return Masks[static_cast<std::size_t>(commandMode())];
    }

    // Physical VRAM address of a dot; pages follow each other in Y.
    [[nodiscard]] std::uint32_t dotAddress(unsigned x, unsigned y) const noexcept
    {
        switch (commandMode()) {
        case 0:
            return ((y & 1023u) << 7) | ((x & 255u) >> 1);
        case 1:
            return ((y & 1023u) << 7) | ((x & 511u) >> 2);
        case 2:
            return ((x & 2u) << 15) | ((y & 511u) << 7) | ((x & 511u) >> 2);
        default:
            return ((x & 1u) << 16) | ((y & 511u) << 7) | ((x & 255u) >> 1);
        }
    }

    // Bit position of a dot inside its byte.
    [[nodiscard]] unsigned dotShift(unsigned x) const noexcept
    {
        switch (commandMode()) {
        case 1:
            return ((~x) & 3u) << 1;
        case 3:
            return 0;
        default:
            return ((~x) & 1u) << 2;
        }
    }

    // The VG 8235 has no expansion RAM: MXS reads FFh, MXD writes are lost.
    [[nodiscard]] std::uint8_t commandRead(std::uint32_t address) const noexcept
    {
        return (commandArgument & ArgumentSourceExpansion) ? 0xFF : vram[address & (VramSize - 1)];
    }

    [[nodiscard]] std::uint8_t point(unsigned x, unsigned y) const noexcept
    {
        if (commandArgument & ArgumentSourceExpansion) return 0xFF;
        return static_cast<std::uint8_t>((vram[dotAddress(x, y)] >> dotShift(x)) & colourMask());
    }

    void commandWrite(std::uint32_t address, std::uint8_t value) noexcept
    {
        if (!(commandArgument & ArgumentDestinationExpansion)) vram[address & (VramSize - 1)] = value;
    }

    void pset(unsigned x, unsigned y, std::uint8_t colour) noexcept
    {
        if (commandArgument & ArgumentDestinationExpansion) return;
        const std::uint32_t address = dotAddress(x, y);
        const unsigned shift = dotShift(x);
        const std::uint8_t dot = static_cast<std::uint8_t>((colour & colourMask()) << shift);
        const std::uint8_t mask = commandMode() == 3 ? 0x00 : static_cast<std::uint8_t>(~(colourMask() << shift));
        vram[address] = logicalOperation(commandRegister & 0x0F, vram[address], dot, mask);
    }

    // Dots (or bytes, with shift > 0) that fit on the line from x in the chosen direction; 0 means
    // a whole line.
    [[nodiscard]] unsigned clipRow(unsigned x, unsigned count, unsigned shift) const noexcept
    {
        const unsigned width = pixelsPerLine() >> shift;
        x >>= shift;
        if (x >= width) return 1;
        count >>= shift;
        if (count == 0) count = width;
        return (commandArgument & ArgumentLeft) ? std::min(count, x + 1) : std::min(count, width - x);
    }

    [[nodiscard]] unsigned clipRow2(unsigned sx, unsigned dx, unsigned count, unsigned shift) const noexcept
    {
        const unsigned width = pixelsPerLine() >> shift;
        sx >>= shift;
        dx >>= shift;
        if (sx >= width || dx >= width) return 1;
        count >>= shift;
        if (count == 0) count = width;
        return (commandArgument & ArgumentLeft) ? std::min(count, std::min(sx, dx) + 1) : std::min(count, width - std::max(sx, dx));
    }

    // Rows up to the top of VRAM when moving up; 0 means 1024 rows.
    [[nodiscard]] unsigned clipRows(unsigned y, unsigned count) const noexcept
    {
        if (count == 0) count = 1024;
        return (commandArgument & ArgumentUp) ? std::min(count, y + 1) : count;
    }

    [[nodiscard]] int stepX(unsigned shift) const noexcept
    {
        const int dots = 1 << shift;
        return (commandArgument & ArgumentLeft) ? -dots : dots;
    }

    [[nodiscard]] int stepY() const noexcept { return (commandArgument & ArgumentUp) ? -1 : 1; }

    [[nodiscard]] std::uint8_t commandCode() const noexcept { return commandRegister >> 4; }

    void finishCommand() noexcept
    {
        commandExecuting = false;
        commandRegister &= 0x0F;
        commandCountdown = 0;
    }

    void startCommand() noexcept
    {
        const std::uint8_t code = commandCode();
        if (commandMode() < 0 || code < CommandPoint) {
            finishCommand();
            return;
        }
        commandExecuting = true;
        commandCountdown = code == CommandPoint ? PointTicks : code == CommandPset ? PsetTicks : 1;
        workSx = commandSx;
        workDx = commandDx;
        const bool dotCommand = code == CommandLmmv || code == CommandLmmm || code == CommandLmcm || code == CommandLmmc;
        const unsigned shift = dotCommand ? 0 : pixelShift();
        switch (code) {
        case CommandSearch:
            workSx = commandSx;
            break;
        case CommandLine:
            lineError = static_cast<std::uint16_t>((commandNx - 1) >> 1);
            workCount = 0;
            break;
        case CommandLmmv:
        case CommandLmmc:
        case CommandHmmv:
        case CommandHmmc:
            rowLength = static_cast<std::uint16_t>(clipRow(commandDx, commandNx, shift));
            rowsLeft = static_cast<std::uint16_t>(clipRows(commandDy, commandNy));
            workCount = rowLength;
            break;
        case CommandLmcm:
            rowLength = static_cast<std::uint16_t>(clipRow(commandSx, commandNx, 0));
            rowsLeft = static_cast<std::uint16_t>(clipRows(commandSy, commandNy));
            workCount = rowLength;
            break;
        case CommandLmmm:
        case CommandHmmm:
            rowLength = static_cast<std::uint16_t>(clipRow2(commandSx, commandDx, commandNx, shift));
            rowsLeft = static_cast<std::uint16_t>(std::min(clipRows(commandSy, commandNy), clipRows(commandDy, commandNy)));
            workCount = rowLength;
            break;
        case CommandYmmm:
            rowLength = static_cast<std::uint16_t>(clipRow(commandDx, 0, pixelShift()));
            rowsLeft = static_cast<std::uint16_t>(std::min(clipRows(commandSy, commandNy), clipRows(commandDy, commandNy)));
            workCount = rowLength;
            break;
        default:
            break;
        }
        // CPU transfers run when the CPU supplies (R#44) or fetches (S#7) a value.
        if (code == CommandLmcm || code == CommandLmmc || code == CommandHmmc) {
            transferReady = true;
            commandCountdown = 0;
            if (code == CommandLmcm) cpuTransferPending = true;
            if (cpuTransferPending) transferStep();
        }
    }

    // Moves to the next dot or byte of a rectangle; returns the extra clocks of a line change.
    std::uint32_t advanceRectangle(int dx, bool source, bool destination) noexcept
    {
        if (source) workSx = static_cast<std::uint16_t>(workSx + dx);
        if (destination) workDx = static_cast<std::uint16_t>(workDx + dx);
        if (--workCount != 0) return 0;
        const int dy = stepY();
        if (source) commandSy = static_cast<std::uint16_t>((commandSy + dy) & 1023);
        if (destination) commandDy = static_cast<std::uint16_t>((commandDy + dy) & 1023);
        commandNy = static_cast<std::uint16_t>((commandNy - 1) & 1023);
        workSx = commandSx;
        workDx = commandDx;
        workCount = rowLength;
        if (--rowsLeft == 0) finishCommand();
        return RectangleLineEndTicks;
    }

    void transferStep() noexcept
    {
        cpuTransferPending = false;
        if (commandMode() < 0) {
            finishCommand();
            return;
        }
        switch (commandCode()) {
        case CommandHmmc:
            commandWrite(dotAddress(workDx, commandDy), commandColour);
            (void)advanceRectangle(stepX(pixelShift()), false, true);
            break;
        case CommandLmmc:
            pset(workDx, commandDy, commandColour);
            (void)advanceRectangle(stepX(0), false, true);
            break;
        case CommandLmcm:
            commandColour = point(workSx, commandSy);
            (void)advanceRectangle(stepX(0), true, false);
            break;
        default:
            break;
        }
    }

    // Executes one step of a command that runs on its own; returns the clocks until the next step.
    std::uint32_t commandStep() noexcept
    {
        if (commandMode() < 0) {
            finishCommand();
            return 0;
        }
        switch (commandCode()) {
        case CommandPoint:
            commandColour = point(commandSx, commandSy);
            finishCommand();
            return 0;
        case CommandPset:
            pset(commandDx, commandDy, commandColour);
            finishCommand();
            return 0;
        case CommandSearch:
            return searchStep();
        case CommandLine:
            return lineStep();
        case CommandLmmv:
            pset(workDx, commandDy, commandColour);
            return LogicalFillTicks + advanceRectangle(stepX(0), false, true);
        case CommandLmmm: {
            const std::uint8_t dot = point(workSx, commandSy);
            pset(workDx, commandDy, dot);
            return LogicalCopyTicks + advanceRectangle(stepX(0), true, true);
        }
        case CommandHmmv:
            commandWrite(dotAddress(workDx, commandDy), commandColour);
            return ByteFillTicks + advanceRectangle(stepX(pixelShift()), false, true);
        case CommandHmmm:
            commandWrite(dotAddress(workDx, commandDy), commandRead(dotAddress(workSx, commandSy)));
            return ByteCopyTicks + advanceRectangle(stepX(pixelShift()), true, true);
        case CommandYmmm:
            commandWrite(dotAddress(workDx, commandDy), commandRead(dotAddress(workDx, commandSy)));
            (void)advanceRectangle(stepX(pixelShift()), true, true);
            return LineCopyTicks;
        default:
            finishCommand();
            return 0;
        }
    }

    // SRCH with EQ = 0 stops on the border colour, with EQ = 1 on any other colour.
    std::uint32_t searchStep() noexcept
    {
        const std::uint8_t borderColour = commandColour & colourMask();
        const bool found = (point(workSx, commandSy) == borderColour) != ((commandArgument & ArgumentEqual) != 0);
        if (found) {
            borderDetected = true;
            finishCommand();
            return 0;
        }
        workSx = static_cast<std::uint16_t>(workSx + stepX(0));
        if (workSx & pixelsPerLine()) {
            finishCommand();
            return 0;
        }
        return SearchTicks;
    }

    // LINE: NX dots along the major axis (MAJ), NY along the minor axis.
    std::uint32_t lineStep() noexcept
    {
        pset(workDx, commandDy, commandColour);
        const int dx = stepX(0);
        const int dy = stepY();
        std::uint32_t ticks = LineTicks;
        const auto moveY = [&]() {
            commandDy = static_cast<std::uint16_t>((commandDy + dy) & 1023);
            return dy < 0 && commandDy == 1023;
        };
        if (!(commandArgument & ArgumentMajor)) {
            workDx = static_cast<std::uint16_t>(workDx + dx);
            if (workCount++ == commandNx || (workDx & pixelsPerLine())) {
                finishCommand();
                return 0;
            }
            if (lineError < commandNy) {
                lineError = static_cast<std::uint16_t>(lineError + commandNx);
                ticks += LineMinorStepTicks;
                if (moveY()) {
                    finishCommand();
                    return 0;
                }
            }
            lineError = static_cast<std::uint16_t>((lineError - commandNy) & 1023);
        } else {
            if (moveY()) {
                finishCommand();
                return 0;
            }
            if (lineError < commandNy) {
                lineError = static_cast<std::uint16_t>(lineError + commandNx);
                workDx = static_cast<std::uint16_t>(workDx + dx);
                ticks += LineMinorStepTicks;
            }
            lineError = static_cast<std::uint16_t>((lineError - commandNy) & 1023);
            if (workCount++ == commandNx || (workDx & pixelsPerLine())) {
                finishCommand();
                return 0;
            }
        }
        return ticks;
    }

    void writeCommandRegister(std::uint8_t index, std::uint8_t value) noexcept
    {
        switch (index) {
        case 32:
            commandSx = static_cast<std::uint16_t>((commandSx & 0x100) | value);
            break;
        case 33:
            commandSx = static_cast<std::uint16_t>((commandSx & 0x0FF) | ((value & 0x01) << 8));
            break;
        case 34:
            commandSy = static_cast<std::uint16_t>((commandSy & 0x300) | value);
            break;
        case 35:
            commandSy = static_cast<std::uint16_t>((commandSy & 0x0FF) | ((value & 0x03) << 8));
            break;
        case 36:
            commandDx = static_cast<std::uint16_t>((commandDx & 0x100) | value);
            break;
        case 37:
            commandDx = static_cast<std::uint16_t>((commandDx & 0x0FF) | ((value & 0x01) << 8));
            break;
        case 38:
            commandDy = static_cast<std::uint16_t>((commandDy & 0x300) | value);
            break;
        case 39:
            commandDy = static_cast<std::uint16_t>((commandDy & 0x0FF) | ((value & 0x03) << 8));
            break;
        case 40:
            commandNx = static_cast<std::uint16_t>((commandNx & 0x100) | value);
            break;
        case 41:
            commandNx = static_cast<std::uint16_t>((commandNx & 0x0FF) | ((value & 0x01) << 8));
            break;
        case 42:
            commandNy = static_cast<std::uint16_t>((commandNy & 0x300) | value);
            break;
        case 43:
            commandNy = static_cast<std::uint16_t>((commandNy & 0x0FF) | ((value & 0x03) << 8));
            break;
        case 44:
            commandColour = value;
            if (!commandExecuting) transferReady = false;
            cpuTransferPending = true;
            if (commandExecuting && (commandCode() == CommandHmmc || commandCode() == CommandLmmc)) transferStep();
            break;
        case 45:
            commandArgument = value;
            break;
        case 46:
            commandRegister = value;
            startCommand();
            break;
        default:
            break;
        }
    }

    // S#7 hands the colour to the CPU; during LMCM this requests the next dot.
    std::uint8_t readCommandColour() noexcept
    {
        const std::uint8_t value = commandColour;
        if (!commandExecuting) transferReady = false;
        cpuTransferPending = true;
        if (commandExecuting && commandCode() == CommandLmcm) transferStep();
        return value;
    }

    [[nodiscard]] std::uint8_t commandRegisterValue(std::uint8_t index) const noexcept
    {
        switch (index) {
        case 32:
            return static_cast<std::uint8_t>(commandSx);
        case 33:
            return static_cast<std::uint8_t>(commandSx >> 8);
        case 34:
            return static_cast<std::uint8_t>(commandSy);
        case 35:
            return static_cast<std::uint8_t>(commandSy >> 8);
        case 36:
            return static_cast<std::uint8_t>(commandDx);
        case 37:
            return static_cast<std::uint8_t>(commandDx >> 8);
        case 38:
            return static_cast<std::uint8_t>(commandDy);
        case 39:
            return static_cast<std::uint8_t>(commandDy >> 8);
        case 40:
            return static_cast<std::uint8_t>(commandNx);
        case 41:
            return static_cast<std::uint8_t>(commandNx >> 8);
        case 42:
            return static_cast<std::uint8_t>(commandNy);
        case 43:
            return static_cast<std::uint8_t>(commandNy >> 8);
        case 44:
            return commandColour;
        case 45:
            return commandArgument;
        default:
            return commandRegister;
        }
    }

    [[nodiscard]] std::uint8_t commandStatus() const noexcept
    {
        return static_cast<std::uint8_t>((transferReady ? 0x80 : 0x00) | (borderDetected ? 0x10 : 0x00) | (commandExecuting ? 0x01 : 0x00));
    }

    [[nodiscard]] std::uint8_t status(std::uint8_t index) const noexcept
    {
        switch (index) {
        case 0:
            return static_cast<std::uint8_t>((verticalFlag ? 0x80 : 0x00) | spriteStatus);
        case 1:
            return lineFlag() ? 0x01 : 0x00;
        case 2:
            return static_cast<std::uint8_t>(0x0C | (verticalRetrace() ? 0x40 : 0x00) | (horizontalRetrace() ? 0x20 : 0x00)
                                             | (evenOddField ? 0x02 : 0x00) | commandStatus());
        case 3:
            return static_cast<std::uint8_t>(collisionX);
        case 4:
            return static_cast<std::uint8_t>(0xFE | (collisionX >> 8));
        case 5:
            return static_cast<std::uint8_t>(collisionY);
        case 6:
            return static_cast<std::uint8_t>(0xFC | (collisionY >> 8));
        case 7:
            return commandColour;
        case 8:
            return static_cast<std::uint8_t>(workSx);
        case 9:
            return static_cast<std::uint8_t>(0xFE | ((workSx >> 8) & 0x01));
        default:
            return 0xFF;
        }
    }

    std::uint8_t readStatus()
    {
        const std::uint8_t index = registers[15];
        if (index == 7) return readCommandColour();
        const std::uint8_t value = status(index);
        if (index == 9) borderDetected = false;
        if (index == 0) {
            verticalFlag = false;
            spriteStatus &= 0x1F;
        }
        if (index == 5) {
            collisionX = 0;
            collisionY = 0;
        }
        if (index == 1 && (registers[0] & 0x10)) lineInterrupt = false;
        return value;
    }

    void writeRegister(std::uint8_t index, std::uint8_t value) noexcept
    {
        if (index >= ControlRegisterCount || RegisterMasks[index] == 0) return;
        value &= RegisterMasks[index];
        const std::uint8_t change = static_cast<std::uint8_t>(registers[index] ^ value);
        registers[index] = value;
        // Writing R#13 restarts blinking even when the value is unchanged.
        if (index == 13) restartBlink();
        if (index >= FirstCommandRegister) {
            writeCommandRegister(index, value);
            return;
        }

        switch (index) {
        case 0:
            if ((change & 0x10) && !(value & 0x10)) lineInterrupt = false;
            scheduleLineMatch();
            break;
        case 1:
            scheduleLineMatch();
            break;
        case 9:
            if (change & 0x80) {
                // Before the first display line the new height still moves the display start.
                if (tick < displayStart) {
                    scheduleDisplayStart();
                    nextRenderTick = displayStart;
                }
                scheduleDisplayEnd();
                scheduleLineMatch();
            }
            break;
        case 16:
            paletteLatchFull = false;
            break;
        case 18:
        case 19:
        case 23:
            scheduleLineMatch();
            break;
        default:
            break;
        }
    }

    void writeControl(std::uint8_t value) noexcept
    {
        if (!registerLatchFull) {
            dataLatch = value;
            registerLatchFull = true;
            return;
        }
        registerLatchFull = false;
        if (value & 0x80) {
            if (!(value & 0x40)) writeRegister(value & 0x3F, dataLatch);
            return;
        }
        vramPointer = static_cast<std::uint16_t>(((value << 8) | dataLatch) & 0x3FFF);
        if (!(value & 0x40)) readVramAhead();
    }

    void writePalette(std::uint8_t value) noexcept
    {
        if (!paletteLatchFull) {
            dataLatch = value;
            paletteLatchFull = true;
            return;
        }
        paletteLatchFull = false;
        const std::uint8_t index = registers[16];
        palette[index] = static_cast<std::uint16_t>(((value << 8) | dataLatch) & 0x777);
        registers[16] = static_cast<std::uint8_t>((index + 1) & 0x0F);
    }

    void writeIndirect(std::uint8_t value) noexcept
    {
        dataLatch = value;
        const std::uint8_t pointer = registers[17];
        const std::uint8_t index = pointer & 0x3F;
        // R#17 itself cannot be written indirectly.
        if (index != 17) writeRegister(index, value);
        if (!(pointer & 0x80)) registers[17] = static_cast<std::uint8_t>((index + 1) & 0x3F);
    }
};

V9938::V9938()
    : impl(std::make_unique<Impl>())
{
    reset();
}

V9938::~V9938() = default;
V9938::V9938(V9938&&) noexcept = default;
V9938& V9938::operator=(V9938&&) noexcept = default;

void V9938::reset()
{
    Impl& state = *impl;
    state.vram.fill(0);
    state.registers.fill(0);
    // Colour burst registers are preset at power on.
    state.registers[21] = 0x3F;
    state.registers[22] = 0x05;
    state.palette = ResetPalette;
    state.vramPointer = 0;
    state.cpuVramData = 0;
    state.dataLatch = 0;
    state.registerLatchFull = false;
    state.paletteLatchFull = false;
    state.verticalFlag = false;
    state.lineInterrupt = false;
    state.evenOddField = true;
    state.spriteStatus = 0;
    state.collisionX = 0;
    state.collisionY = 0;
    state.blinkOn = false;
    state.blinkCount = 0;
    state.frames = 0;
    state.renderedLines = 0;
    state.frameBuffer.fill(0);
    state.commandSx = state.commandSy = state.commandDx = state.commandDy = 0;
    state.commandNx = state.commandNy = 0;
    state.commandColour = state.commandArgument = state.commandRegister = 0;
    state.workSx = state.workDx = state.workCount = 0;
    state.commandExecuting = state.transferReady = state.borderDetected = state.cpuTransferPending = false;
    state.commandCountdown = 0;
    state.border.fill({});
    state.startFrame();
}

std::uint8_t V9938::read(std::uint8_t port)
{
    Impl& state = *impl;
    state.registerLatchFull = false;
    switch (port & 3) {
    case 0: {
        const std::uint8_t value = state.cpuVramData;
        state.readVramAhead();
        return value;
    }
    case 1:
        return state.readStatus();
    default:
        return 0xFF;
    }
}

void V9938::write(std::uint8_t port, std::uint8_t value)
{
    Impl& state = *impl;
    switch (port & 3) {
    case 0:
        state.registerLatchFull = false;
        state.writeVram(value);
        break;
    case 1:
        state.writeControl(value);
        break;
    case 2:
        state.writePalette(value);
        break;
    default:
        state.writeIndirect(value);
        break;
    }
}

void V9938::advanceMasterTick() noexcept
{
    Impl& state = *impl;
    if (++state.tick >= state.ticksPerFrame) state.startFrame();
    if (state.commandCountdown != 0 && --state.commandCountdown == 0) {
        state.commandCountdown = state.commandStep();
    }
    if (state.tick == state.nextLineTick) {
        state.border[state.tick / MasterTicksPerScanline] = state.borderColours();
        state.nextLineTick += MasterTicksPerScanline;
    }
    if (state.tick == state.nextRenderTick) {
        state.renderDisplayLine(state.renderLine++);
        state.renderedLines = state.renderLine;
        state.nextRenderTick = state.renderLine < state.displayLines() ? state.nextRenderTick + MasterTicksPerScanline : Never;
    }
    if (state.tick == state.displayEnd) state.verticalFlag = true;
    if (state.tick == state.lineMatch && (state.registers[0] & 0x10)) state.lineInterrupt = true;
}

Outputs V9938::outputs() const noexcept
{
    const Impl& state = *impl;
    Outputs result;
    result.interruptRequest = (state.verticalFlag && (state.registers[1] & 0x20)) || state.lineInterrupt;
    return result;
}

std::uint8_t V9938::peekVram(std::uint32_t address) const noexcept
{
    return impl->vram[address % VramSize];
}

void V9938::pokeVram(std::uint32_t address, std::uint8_t value) noexcept
{
    impl->vram[address % VramSize] = value;
}

std::uint8_t V9938::controlRegister(std::uint8_t index) const noexcept
{
    if (index >= ControlRegisterCount) return 0;
    return index >= FirstCommandRegister ? impl->commandRegisterValue(index) : impl->registers[index];
}

std::uint8_t V9938::peekStatus(std::uint8_t index) const noexcept
{
    return impl->status(index);
}

std::uint16_t V9938::paletteEntry(std::uint8_t index) const noexcept
{
    return impl->palette[index % PaletteSize];
}

bool V9938::palTiming() const noexcept
{
    return impl->pal;
}

std::uint16_t V9938::linesPerFrame() const noexcept
{
    return impl->pal ? PalScanlines : NtscScanlines;
}

std::uint32_t V9938::frameTick() const noexcept
{
    return impl->tick;
}

std::uint32_t V9938::displayStartTick() const noexcept
{
    return impl->displayStart;
}

std::uint32_t V9938::frameCount() const noexcept
{
    return impl->frames;
}

bool V9938::interlaced() const noexcept
{
    return impl->interlace;
}

bool V9938::secondField() const noexcept
{
    return impl->evenOddField;
}

const FrameBuffer& V9938::frame() const noexcept
{
    return impl->frameBuffer;
}

std::uint16_t V9938::frameDisplayLines() const noexcept
{
    return impl->renderedLines;
}

const BorderRaster& V9938::borderRaster() const noexcept
{
    return impl->border;
}

std::vector<std::uint8_t> V9938::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the VDP state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool V9938::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    std::memcpy(impl.get(), state.data(), sizeof(Impl));
    return true;
}

} // namespace v9938
