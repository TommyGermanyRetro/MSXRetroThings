#include "TMS99X8/TMS99X8.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void setRegister(tms99x8::TMS99X8& vdp, std::uint8_t index, std::uint8_t value)
{
    vdp.write(1, value);
    vdp.write(1, static_cast<std::uint8_t>(0x80 | (index & 7)));
}

// Graphics II: after the sprite Y scan of a line has stopped at Y = 208, its remaining Y cycles
// serve the CPU; with 32 sprites in the table the CPU waits for the next fixed window.
bool freedSpriteSlotPass()
{
    const auto executionDot = [](bool terminated) {
        tms99x8::TMS99X8 vdp;
        setRegister(vdp, 0, 0x02);
        setRegister(vdp, 1, 0xC2);
        setRegister(vdp, 5, 0x36);
        for (unsigned sprite = 0; sprite < 32; ++sprite) {
            vdp.pokeVram(static_cast<std::uint16_t>(0x1B00 + sprite * 4), terminated && sprite == 0 ? 208 : 0xC0);
        }
        while (!(vdp.outputs().scanline == 100 && vdp.outputs().dot == 88)) vdp.advanceMasterTick();
        vdp.write(1, 0x00);
        vdp.write(1, 0x40);
        vdp.write(0, 0x77);
        const auto start = vdp.outputs().vramAddress;
        while (vdp.outputs().vramAddress == start) vdp.advanceMasterTick();
        return vdp.outputs().dot;
    };
    if (executionDot(true) != 95) {
        std::cerr << "With the sprite scan stopped, a Y cycle must serve the CPU (dot 95).\n";
        return false;
    }
    if (executionDot(false) != 119) {
        std::cerr << "During the sprite scan the CPU must wait for the fixed window at dot 119.\n";
        return false;
    }
    return true;
}

bool mixedModesAndLine192Pass()
{
    const auto checkModeFrame = [](tms99x8::TMS99X8& vdp, std::size_t index,
                                   std::uint8_t expected, const char* message) {
        const auto snapshot = vdp.renderFrame();
        const auto line = index / tms99x8::ScreenWidth;
        for (unsigned scanline = 0; scanline < 64 + line + 1; ++scanline) vdp.advanceScanline();
        if (snapshot[index] != expected || vdp.scanoutFrame()[index] != expected) {
            std::cerr << message << '\n';
            return false;
        }
        return true;
    };

    tms99x8::TMS99X8 text1QVdp;
    setRegister(text1QVdp, 0, 0x02);
    setRegister(text1QVdp, 1, 0x50);
    setRegister(text1QVdp, 2, 0x0F);
    setRegister(text1QVdp, 3, 0x00);
    setRegister(text1QVdp, 4, 0x07);
    setRegister(text1QVdp, 7, 0xF1);
    text1QVdp.pokeVram(0x3C00, 0x08);
    text1QVdp.pokeVram(0x2000, 0x00);
    text1QVdp.pokeVram(0x2040, 0x80);
    if (!checkModeFrame(text1QVdp, 6, 0x0F,
                        "Text mode with M3 must not apply the R3 colour mask to patterns.")) {
        return false;
    }

    tms99x8::TMS99X8 multiQVdp;
    setRegister(multiQVdp, 0, 0x02);
    setRegister(multiQVdp, 1, 0x48);
    setRegister(multiQVdp, 2, 0x0F);
    setRegister(multiQVdp, 4, 0x07);
    multiQVdp.pokeVram(0x3C00, 0x01);
    multiQVdp.pokeVram(0x2009, 0xF1);
    multiQVdp.pokeVram(0x2010, 0x00);
    if (!checkModeFrame(multiQVdp, 4 * tms99x8::ScreenWidth, 0x0F,
                        "Multicolor with M3 must add the block row as a byte offset.")) {
        return false;
    }

    const auto setupLine192Sprites = [](tms99x8::TMS99X8& vdp) {
        setRegister(vdp, 1, 0x40);
        setRegister(vdp, 5, 0x06);
        for (unsigned sprite = 0; sprite < 5; ++sprite) {
            const auto descriptor = static_cast<std::uint16_t>(0x0300 + sprite * 4);
            vdp.pokeVram(descriptor, 191);
            vdp.pokeVram(descriptor + 1, 10);
            vdp.pokeVram(descriptor + 2, 0x00);
            vdp.pokeVram(descriptor + 3, 0x0F);
        }
        vdp.pokeVram(0x0314, 208);
        vdp.pokeVram(0x0000, 0x80);
    };

    tms99x8::TMS99X8 line192SnapshotVdp;
    setupLine192Sprites(line192SnapshotVdp);
    (void)line192SnapshotVdp.renderFrame();
    if ((line192SnapshotVdp.read(1) & 0x7F) != 0x64) {
        std::cerr << "Snapshot sprite check must include line 192 for 5S and collision.\n";
        return false;
    }

    tms99x8::TMS99X8 line192ScanoutVdp;
    setupLine192Sprites(line192ScanoutVdp);
    for (unsigned line = 0; line < 64 + 193; ++line) line192ScanoutVdp.advanceScanline();
    if ((line192ScanoutVdp.read(1) & 0x7F) != 0x64) {
        std::cerr << "Timed sprite check must include line 192 for 5S and collision.\n";
        return false;
    }
    return true;
}

bool backdropRasterPass()
{
    tms99x8::TMS99X8 vdp;
    setRegister(vdp, 7, 0x02);
    for (unsigned dot = 0; dot < 10; ++dot) vdp.advanceDot();
    setRegister(vdp, 7, 0xF5);
    for (unsigned dot = 0; dot < 10; ++dot) vdp.advanceDot();
    const auto& raster = vdp.backdropRaster();
    if (raster[9] != 0x02 || raster[10] != 0x05 || raster[19] != 0x05) {
        std::cerr << "Backdrop raster must record register 7 changes dot by dot.\n";
        return false;
    }
    return true;
}

bool fourSpritesWithoutTerminatorPass()
{
    // Four 16x16 sprites share lines 20-35; no attribute entry holds 208, so the Y scan reads
    // all 32 entries before the pattern fetches of the four sprites.
    tms99x8::TMS99X8 vdp;
    setRegister(vdp, 1, 0x42);
    setRegister(vdp, 5, 0x06);
    setRegister(vdp, 6, 0x07);
    setRegister(vdp, 7, 0x01);
    for (unsigned offset = 0; offset < 32; ++offset) {
        vdp.pokeVram(static_cast<std::uint16_t>(0x3800 + offset), 0xFF);
    }
    for (unsigned sprite = 0; sprite < 32; ++sprite) {
        const auto entry = static_cast<std::uint16_t>(0x0300 + sprite * 4);
        vdp.pokeVram(entry, sprite < 4 ? 19 : 0xC0);
        vdp.pokeVram(static_cast<std::uint16_t>(entry + 1), static_cast<std::uint8_t>(sprite * 32));
        vdp.pokeVram(static_cast<std::uint16_t>(entry + 2), 0x00);
        vdp.pokeVram(static_cast<std::uint16_t>(entry + 3), static_cast<std::uint8_t>(2 + sprite % 4));
    }
    constexpr unsigned line = 30;
    for (unsigned scanline = 0; scanline < 64 + line + 1; ++scanline) vdp.advanceScanline();
    const auto& scanout = vdp.scanoutFrame();
    for (unsigned sprite = 0; sprite < 4; ++sprite) {
        if (scanout[line * tms99x8::ScreenWidth + sprite * 32 + 8] != 2 + sprite) {
            std::cerr << "Four sprites on a line without terminator must all be drawn.\n";
            return false;
        }
    }
    return true;
}

bool spriteGeometryAndStatusPass()
{
    const auto setupLargeSprite = [](tms99x8::TMS99X8& vdp, std::uint8_t r1, std::uint8_t y) {
        setRegister(vdp, 1, r1);
        setRegister(vdp, 5, 0x06);
        setRegister(vdp, 6, 0x07);
        vdp.pokeVram(0x0300, y);
        vdp.pokeVram(0x0301, 0);
        vdp.pokeVram(0x0302, 0x00);
        vdp.pokeVram(0x0303, 0x0F);
        vdp.pokeVram(0x0304, 208);
    };

    tms99x8::TMS99X8 quadrantVdp;
    setupLargeSprite(quadrantVdp, 0x42, 0xFF);
    for (unsigned row = 0; row < 8; ++row) {
        quadrantVdp.pokeVram(static_cast<std::uint16_t>(0x3808 + row), 0x80);
    }
    const auto quadrantFrame = quadrantVdp.renderFrame();
    if (quadrantFrame[8 * tms99x8::ScreenWidth] != 0x0F || quadrantFrame[8] != 0x00) {
        std::cerr << "16x16 sprite pattern 1 must be the lower-left quadrant.\n";
        return false;
    }
    for (unsigned line = 0; line < 64 + 9; ++line) quadrantVdp.advanceScanline();
    const auto& quadrantScanout = quadrantVdp.scanoutFrame();
    if (quadrantScanout[8 * tms99x8::ScreenWidth] != 0x0F || quadrantScanout[8] != 0x00) {
        std::cerr << "Timed 16x16 sprite fetch must read the left column, then 16 bytes later.\n";
        return false;
    }

    tms99x8::TMS99X8 wrapVdp;
    setupLargeSprite(wrapVdp, 0x43, 0xE0);
    for (unsigned row = 0; row < 32; ++row) {
        wrapVdp.pokeVram(static_cast<std::uint16_t>(0x3800 + row), 0xFF);
    }
    if (wrapVdp.renderFrame()[0] != 0x0F) {
        std::cerr << "Sprite Y must wrap with 8-bit arithmetic.\n";
        return false;
    }

    tms99x8::TMS99X8 fifthWithFrameFlagVdp;
    setRegister(fifthWithFrameFlagVdp, 1, 0x40);
    setRegister(fifthWithFrameFlagVdp, 5, 0x06);
    fifthWithFrameFlagVdp.pokeVram(0x0300, 208);
    for (unsigned line = 0; line < 257; ++line) fifthWithFrameFlagVdp.advanceScanline();
    for (unsigned sprite = 0; sprite < 5; ++sprite) {
        const auto descriptor = static_cast<std::uint16_t>(0x0300 + sprite * 4);
        fifthWithFrameFlagVdp.pokeVram(descriptor, 0xFF);
        fifthWithFrameFlagVdp.pokeVram(descriptor + 1, static_cast<std::uint8_t>(10 + sprite * 8));
        fifthWithFrameFlagVdp.pokeVram(descriptor + 2, 0x00);
        fifthWithFrameFlagVdp.pokeVram(descriptor + 3, 0x0F);
    }
    fifthWithFrameFlagVdp.pokeVram(0x0314, 208);
    (void)fifthWithFrameFlagVdp.renderFrame();
    if ((fifthWithFrameFlagVdp.read(1) & 0xC0) != 0x80) {
        std::cerr << "Fifth sprite must not set overflow while the frame flag is set.\n";
        return false;
    }
    return true;
}

}

int main()
{
    tms99x8::TMS99X8 vdp;
    if (vdp.model() != tms99x8::Model::Pal ||
        vdp.fieldRateHz() != 50 || vdp.outputs().scanlinesPerFrame != 313 ||
        vdp.outputs().displayStartScanline != 64) {
        std::cerr << "TMS9929A did not select its PAL timing.\n";
        return EXIT_FAILURE;
    }

    setRegister(vdp, 1, 0x20);
    for (unsigned line = 0; line < 257; ++line) vdp.advanceScanline();
    if (!vdp.outputs().interruptRequest || (vdp.read(1) & 0x80) == 0) {
        std::cerr << "TMS9929A PAL VBlank did not assert at the expected line.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 delayedWriteVdp;
    delayedWriteVdp.write(1, 0x00);
    delayedWriteVdp.write(1, 0x40);
    delayedWriteVdp.write(0, 0xA5);
    if (delayedWriteVdp.peekVram(0) != 0x00 ||
        delayedWriteVdp.outputs().vramAddress != 0) {
        std::cerr << "CPU VRAM writes must wait for an access slot.\n";
        return EXIT_FAILURE;
    }
    for (unsigned tick = 0; tick < 27; ++tick) delayedWriteVdp.advanceMasterTick();
    if (delayedWriteVdp.peekVram(0) != 0x00) {
        std::cerr << "CPU VRAM writes must not execute before their slot.\n";
        return EXIT_FAILURE;
    }
    delayedWriteVdp.advanceMasterTick();
    if (delayedWriteVdp.peekVram(0) != 0xA5 ||
        delayedWriteVdp.outputs().vramAddress != 1) {
        std::cerr << "VRAM access must commit after the setup delay of 28 master clocks.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 delayedReadVdp;
    delayedReadVdp.pokeVram(0, 0x5A);
    delayedReadVdp.write(1, 0x00);
    delayedReadVdp.write(1, 0x00);
    for (unsigned tick = 0; tick < 28; ++tick) delayedReadVdp.advanceMasterTick();
    if (delayedReadVdp.outputs().vramAddress != 1 || delayedReadVdp.read(0) != 0x5A) {
        std::cerr << "VRAM read-ahead must complete at its scheduled access slot.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 overwrittenRequestVdp;
    overwrittenRequestVdp.write(1, 0x00);
    overwrittenRequestVdp.write(1, 0x40);
    overwrittenRequestVdp.write(0, 0x11);
    overwrittenRequestVdp.advanceMasterTick();
    overwrittenRequestVdp.write(0, 0x22);
    for (unsigned tick = 0; tick < 28; ++tick) overwrittenRequestVdp.advanceMasterTick();
    if (overwrittenRequestVdp.peekVram(0) != 0x22 ||
        overwrittenRequestVdp.outputs().vramAddress != 1) {
        std::cerr << "A newer data-port request must replace the pending TMS99x8 request.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 refreshWindowVdp;
    constexpr auto activeDisplayEnd =
        (64u + tms99x8::ScreenHeight) * tms99x8::MasterTicksPerScanline + 202u;
    for (unsigned tick = 0; tick < activeDisplayEnd; ++tick) refreshWindowVdp.advanceMasterTick();
    refreshWindowVdp.write(1, 0x00);
    refreshWindowVdp.write(1, 0x40);
    refreshWindowVdp.write(0, 0x73);
    for (unsigned tick = 0; tick < 27; ++tick) refreshWindowVdp.advanceMasterTick();
    if (refreshWindowVdp.peekVram(0) != 0x00) {
        std::cerr << "Vertical-refresh VRAM access must retain the VDP setup delay.\n";
        return EXIT_FAILURE;
    }
    refreshWindowVdp.advanceMasterTick();
    if (refreshWindowVdp.peekVram(0) != 0x73 ||
        refreshWindowVdp.outputs().vramAddress != 1) {
        std::cerr << "Vertical refresh must grant VRAM access without an access-window wait.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 graphicsSlotVdp;
    setRegister(graphicsSlotVdp, 1, 0x40);
    constexpr auto slotGridStart = 64 * tms99x8::MasterTicksPerScanline + 202 -
                                   (tms99x8::MasterTicksPerScanline + 202 - 162);
    for (unsigned tick = 0; tick < slotGridStart; ++tick) {
        graphicsSlotVdp.advanceMasterTick();
    }
    graphicsSlotVdp.write(1, 0x00);
    graphicsSlotVdp.write(1, 0x40);
    graphicsSlotVdp.write(0, 0x6B);
    for (unsigned tick = 0; tick < 57; ++tick) graphicsSlotVdp.advanceMasterTick();
    if (graphicsSlotVdp.peekVram(0) != 0x00) {
        std::cerr << "Graphics mode must not use display-off VRAM slots.\n";
        return EXIT_FAILURE;
    }
    graphicsSlotVdp.advanceMasterTick();
    if (graphicsSlotVdp.peekVram(0) != 0x6B) {
        std::cerr << "Graphics I access must execute at its mode-specific slot.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 lineLatchVdp;
    setRegister(lineLatchVdp, 7, 0x01);
    constexpr auto firstVisibleLineStart =
        64u * tms99x8::MasterTicksPerScanline;
    setRegister(lineLatchVdp, 7, 0x02);
    for (unsigned tick = 0; tick < firstVisibleLineStart +
         (tms99x8::HorizontalDisplayStart + 1) * 4u; ++tick) {
        lineLatchVdp.advanceMasterTick();
    }
    if (lineLatchVdp.scanoutFrame()[0] != 0x02) {
        std::cerr << "VDP registers written before a raster line must be latched for it.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 lateLineLatchVdp;
    setRegister(lateLineLatchVdp, 7, 0x01);
    for (unsigned tick = 0; tick < firstVisibleLineStart + 37u * 4u; ++tick) {
        lateLineLatchVdp.advanceMasterTick();
    }
    setRegister(lateLineLatchVdp, 7, 0x02);
    for (unsigned tick = 0; tick < 4; ++tick) lateLineLatchVdp.advanceMasterTick();
    if (lateLineLatchVdp.scanoutFrame()[0] != 0x01) {
        std::cerr << "VDP writes after the line-process point must not alter emitted/current-line pixels.\n";
        return EXIT_FAILURE;
    }
    while (lateLineLatchVdp.outputs().scanline == 64) lateLineLatchVdp.advanceMasterTick();
    for (unsigned tick = 0; tick < 36u * 4u + 8u; ++tick) {
        lateLineLatchVdp.advanceMasterTick();
    }
    if (lateLineLatchVdp.scanoutFrame()[tms99x8::ScreenWidth] != 0x02) {
        std::cerr << "A post-latch register write must become visible on the next raster line.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 spriteAttributeFetchVdp;
    setRegister(spriteAttributeFetchVdp, 1, 0x40);
    setRegister(spriteAttributeFetchVdp, 2, 0x0F);
    setRegister(spriteAttributeFetchVdp, 3, 0x40);
    setRegister(spriteAttributeFetchVdp, 4, 0x03);
    setRegister(spriteAttributeFetchVdp, 5, 0x06);
    setRegister(spriteAttributeFetchVdp, 6, 0x04);
    setRegister(spriteAttributeFetchVdp, 7, 0x01);
    spriteAttributeFetchVdp.pokeVram(0x0300, 0x00);
    spriteAttributeFetchVdp.pokeVram(0x0301, 50);
    spriteAttributeFetchVdp.pokeVram(0x0302, 0x00);
    spriteAttributeFetchVdp.pokeVram(0x0303, 0x0F);
    spriteAttributeFetchVdp.pokeVram(0x0304, 208);
    spriteAttributeFetchVdp.pokeVram(0x3C00, 0x00);
    spriteAttributeFetchVdp.pokeVram(0x1800, 0x00);
    for (unsigned row = 0; row < 8; ++row) {
        spriteAttributeFetchVdp.pokeVram(static_cast<std::uint16_t>(0x2000 + row), 0xFF);
    }
    const auto spriteAttributeSnapshot = spriteAttributeFetchVdp.renderFrame();
    if (spriteAttributeSnapshot[tms99x8::ScreenWidth + 50] != 0x0F ||
        spriteAttributeSnapshot[tms99x8::ScreenWidth + 60] != 0x01) {
        std::cerr << "Sprite attribute test fixture does not describe one 8-pixel sprite.\n";
        return EXIT_FAILURE;
    }
    for (unsigned line = 0; line < 64; ++line) spriteAttributeFetchVdp.advanceScanline();
    for (unsigned dot = 0; dot < 218; ++dot) spriteAttributeFetchVdp.advanceDot();
    spriteAttributeFetchVdp.pokeVram(0x0301, 60);
    while (spriteAttributeFetchVdp.outputs().scanline == 64) {
        spriteAttributeFetchVdp.advanceDot();
    }
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart + 60; ++dot) {
        spriteAttributeFetchVdp.advanceDot();
    }
    const auto fetchedSpriteLine = spriteAttributeFetchVdp.scanoutFrame().data() +
        tms99x8::ScreenWidth;
    if (fetchedSpriteLine[50] != 0x0F || fetchedSpriteLine[60] != 0x01) {
        std::cerr << "Sprite attributes must be fetched before the following display line.\n";
        return EXIT_FAILURE;
    }
    while (spriteAttributeFetchVdp.outputs().scanline == 65 &&
           spriteAttributeFetchVdp.outputs().dot < 217) {
        spriteAttributeFetchVdp.advanceDot();
    }
    spriteAttributeFetchVdp.advanceDot();
    while (spriteAttributeFetchVdp.outputs().scanline == 65) {
        spriteAttributeFetchVdp.advanceDot();
    }
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart + 60; ++dot) {
        spriteAttributeFetchVdp.advanceDot();
    }
    const auto nextFetchedSpriteLine = spriteAttributeFetchVdp.scanoutFrame().data() +
        2 * tms99x8::ScreenWidth;
    if (nextFetchedSpriteLine[50] != 0x01 || nextFetchedSpriteLine[60] != 0x0F) {
        std::cerr << "The next pre-display fetch must apply the updated sprite attributes.\n";
        return EXIT_FAILURE;
    }

    setRegister(vdp, 1, 0x40);
    setRegister(vdp, 2, 0x0E);
    setRegister(vdp, 3, 0x80);
    setRegister(vdp, 7, 0xF1);
    vdp.pokeVram(0x3800, 0x00);
    vdp.pokeVram(0x0000, 0x80);
    vdp.pokeVram(0x2000, 0x21);
    const auto analog = vdp.renderYpbprFrame();
    if (analog[0].y <= analog[1].y) {
        std::cerr << "TMS9929A YPbPr conversion failed.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 spriteVdp;
    setRegister(spriteVdp, 1, 0x50);
    setRegister(spriteVdp, 5, 0x06);
    spriteVdp.pokeVram(0x0300, 0x00);
    spriteVdp.pokeVram(0x0301, 10);
    spriteVdp.pokeVram(0x0302, 0x00);
    spriteVdp.pokeVram(0x0303, 0x0F);
    spriteVdp.pokeVram(0x0304, 208);
    spriteVdp.pokeVram(0x0000, 0xFF);
    const auto textFrame = spriteVdp.renderFrame();
    if (textFrame[tms99x8::ScreenWidth + 10] != 0) {
        std::cerr << "Sprites must not be rendered in text mode.\n";
        return EXIT_FAILURE;
    }
    setRegister(spriteVdp, 1, 0x40);
    const auto graphicsFrame = spriteVdp.renderFrame();
    if (graphicsFrame[tms99x8::ScreenWidth + 10] != 0x0F) {
        std::cerr << "Sprites must be rendered in Graphics I mode.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 textMarginVdp;
    setRegister(textMarginVdp, 1, 0x50);
    setRegister(textMarginVdp, 2, 0x0E);
    setRegister(textMarginVdp, 7, 0xF1);
    textMarginVdp.pokeVram(0x3800, 0x00);
    textMarginVdp.pokeVram(0x0000, 0x80);
    const auto textMarginFrame = textMarginVdp.renderFrame();
    if (textMarginFrame[5] != 0x01 || textMarginFrame[6] != 0x0F ||
        textMarginFrame[240] != 0x0F || textMarginFrame[246] != 0x01) {
        std::cerr << "Text mode must have 6-pixel left and 10-pixel right margins.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 mixedTextModeVdp;
    setRegister(mixedTextModeVdp, 0, 0x02);
    setRegister(mixedTextModeVdp, 1, 0x70);
    setRegister(mixedTextModeVdp, 5, 0x06);
    mixedTextModeVdp.pokeVram(0x0300, 0x00);
    mixedTextModeVdp.pokeVram(0x0301, 10);
    mixedTextModeVdp.pokeVram(0x0302, 0x00);
    mixedTextModeVdp.pokeVram(0x0303, 0x0F);
    mixedTextModeVdp.pokeVram(0x0304, 208);
    mixedTextModeVdp.pokeVram(0x0000, 0xFF);
    if (mixedTextModeVdp.renderFrame()[tms99x8::ScreenWidth + 10] != 0) {
        std::cerr << "Sprites must be disabled whenever the text-mode bit is set.\n";
        return EXIT_FAILURE;
    }

    for (unsigned mode = 0; mode < 8; ++mode) {
        tms99x8::TMS99X8 modeVdp;
        const auto register0 = static_cast<std::uint8_t>((mode & 2) ? 0x02 : 0x00);
        const auto register1 = static_cast<std::uint8_t>(0x40 |
            ((mode & 1) ? 0x10 : 0x00) | ((mode & 4) ? 0x08 : 0x00));
        setRegister(modeVdp, 0, register0);
        setRegister(modeVdp, 1, register1);
        setRegister(modeVdp, 2, 0x0F);
        setRegister(modeVdp, 4, 0x04);
        setRegister(modeVdp, 7, 0xF1);
        modeVdp.pokeVram(0x3C00, 0x00);
        modeVdp.pokeVram(0x2000, 0xF1);
        const auto snapshot = modeVdp.renderFrame();
        for (unsigned line = 0; line < 64; ++line) modeVdp.advanceScanline();
        for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart +
             tms99x8::ScreenWidth; ++dot) {
            modeVdp.advanceDot();
        }
        const auto& scanout = modeVdp.scanoutFrame();
        for (unsigned x = 0; x < tms99x8::ScreenWidth; ++x) {
            if (scanout[x] != snapshot[x]) {
                std::cerr << "Snapshot and dot scanout disagree in VDP mode " << mode
                          << " at x=" << x << ": " << unsigned(snapshot[x]) << " vs "
                          << unsigned(scanout[x]) << ".\n";
                return EXIT_FAILURE;
            }
        }
    }

    for (const auto mode : {5u, 7u}) {
        tms99x8::TMS99X8 bogusModeVdp;
        setRegister(bogusModeVdp, 0, static_cast<std::uint8_t>((mode & 2) ? 0x02 : 0x00));
        setRegister(bogusModeVdp, 1, static_cast<std::uint8_t>(0x58 | 0x40));
        setRegister(bogusModeVdp, 7, 0xF1);
        const auto frame = bogusModeVdp.renderFrame();
        if (frame[7] != 0x01 || frame[8] != 0x0F || frame[11] != 0x0F ||
            frame[12] != 0x01 || frame[13] != 0x01 || frame[14] != 0x0F ||
            frame[245] != 0x0F || frame[246] != 0x01 || frame[248] != 0x01) {
            std::cerr << "Bogus VDP modes must emit 8 border pixels, 40 4:2 groups, 8 border pixels.\n";
            return EXIT_FAILURE;
        }
    }

    if (!mixedModesAndLine192Pass()) return EXIT_FAILURE;
    if (!freedSpriteSlotPass()) return EXIT_FAILURE;

    tms99x8::TMS99X8 bankedVdp;
    setRegister(bankedVdp, 0, 0x02);
    setRegister(bankedVdp, 1, 0x40);
    setRegister(bankedVdp, 2, 0x0F);
    setRegister(bankedVdp, 3, 0x7F);
    setRegister(bankedVdp, 4, 0x07);
    setRegister(bankedVdp, 7, 0x01);
    bankedVdp.pokeVram(0x3C00, 0x00);
    bankedVdp.pokeVram(0x1800, 0x00);
    bankedVdp.pokeVram(0x0000, 0xF1);
    bankedVdp.pokeVram(0x2000, 0xF0);
    bankedVdp.pokeVram(0x3800, 0x00);
    const auto bankedFrame = bankedVdp.renderFrame();
    if (bankedFrame[0] != 0x0F || bankedFrame[4] != 0x01) {
        std::cerr << "Graphics II pattern base/mask bank decoding failed.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 spriteBankVdp;
    setRegister(spriteBankVdp, 1, 0x40);
    setRegister(spriteBankVdp, 5, 0x06);
    setRegister(spriteBankVdp, 6, 0x07);
    spriteBankVdp.pokeVram(0x0300, 0x00);
    spriteBankVdp.pokeVram(0x0301, 10);
    spriteBankVdp.pokeVram(0x0302, 0x00);
    spriteBankVdp.pokeVram(0x0303, 0x0F);
    spriteBankVdp.pokeVram(0x0304, 208);
    spriteBankVdp.pokeVram(0x1800, 0x00);
    spriteBankVdp.pokeVram(0x2000, 0x00);
    spriteBankVdp.pokeVram(0x3800, 0x80);
    const auto spriteBankFrame = spriteBankVdp.renderFrame();
    if (spriteBankFrame[tms99x8::ScreenWidth + 10] != 0x0F) {
        std::cerr << "Sprite pattern base must use register 6 bits 0-2.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 transparentSpriteCollisionVdp;
    setRegister(transparentSpriteCollisionVdp, 1, 0x40);
    setRegister(transparentSpriteCollisionVdp, 5, 0x06);
    transparentSpriteCollisionVdp.pokeVram(0x0300, 0x00);
    transparentSpriteCollisionVdp.pokeVram(0x0301, 10);
    transparentSpriteCollisionVdp.pokeVram(0x0302, 0x00);
    transparentSpriteCollisionVdp.pokeVram(0x0303, 0x00);
    transparentSpriteCollisionVdp.pokeVram(0x0304, 0x00);
    transparentSpriteCollisionVdp.pokeVram(0x0305, 10);
    transparentSpriteCollisionVdp.pokeVram(0x0306, 0x01);
    transparentSpriteCollisionVdp.pokeVram(0x0307, 0x0F);
    transparentSpriteCollisionVdp.pokeVram(0x0308, 208);
    transparentSpriteCollisionVdp.pokeVram(0x0000, 0x80);
    transparentSpriteCollisionVdp.pokeVram(0x0008, 0x80);
    (void)transparentSpriteCollisionVdp.renderFrame();
    if ((transparentSpriteCollisionVdp.read(1) & 0x20) == 0) {
        std::cerr << "Transparent sprite pattern pixels must participate in collisions.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 spriteOverflowVdp;
    setRegister(spriteOverflowVdp, 1, 0x40);
    setRegister(spriteOverflowVdp, 5, 0x06);
    for (unsigned sprite = 0; sprite < 5; ++sprite) {
        const auto descriptor = static_cast<std::uint16_t>(0x0300 + sprite * 4);
        spriteOverflowVdp.pokeVram(descriptor, 0xFF);
        spriteOverflowVdp.pokeVram(descriptor + 1, static_cast<std::uint8_t>(10 + sprite * 8));
        spriteOverflowVdp.pokeVram(descriptor + 2, 0x00);
        spriteOverflowVdp.pokeVram(descriptor + 3, 0x0F);
    }
    spriteOverflowVdp.pokeVram(0x0314, 208);
    for (unsigned line = 0; line < 65; ++line) spriteOverflowVdp.advanceScanline();
    spriteOverflowVdp.pokeVram(0x0300, 208);
    spriteOverflowVdp.advanceScanline();
    const auto spriteStatus = spriteOverflowVdp.read(1);
    if ((spriteStatus & 0x5F) != 0x44) {
        std::cerr << "Fifth sprite must set overflow and report its index in status.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 rasterVdp;
    setRegister(rasterVdp, 1, 0x40);
    setRegister(rasterVdp, 2, 0x0F);
    setRegister(rasterVdp, 3, 0x04);
    setRegister(rasterVdp, 7, 0x01);
    rasterVdp.pokeVram(0x3C00, 0x00);
    rasterVdp.pokeVram(0x3C01, 0x01);
    rasterVdp.pokeVram(0x0000, 0xFF);
    rasterVdp.pokeVram(0x0008, 0xFF);
    rasterVdp.pokeVram(0x0100, 0xF1);
    for (unsigned line = 0; line < 64; ++line) rasterVdp.advanceScanline();
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart; ++dot) {
        rasterVdp.advanceDot();
    }
    if (rasterVdp.scanoutFrame()[0] != 0x0F) {
        std::cerr << "Raster scanout did not draw the first pixel after horizontal blanking.\n";
        return EXIT_FAILURE;
    }
    rasterVdp.pokeVram(0x0000, 0x00);
    rasterVdp.pokeVram(0x0008, 0x00);
    while (rasterVdp.outputs().scanline == 64) rasterVdp.advanceDot();
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart; ++dot) {
        rasterVdp.advanceDot();
    }
    if (rasterVdp.scanoutFrame()[0] != 0x0F || rasterVdp.scanoutFrame()[1] != 0x0F ||
        rasterVdp.scanoutFrame()[8] != 0x01 ||
        rasterVdp.scanoutFrame()[tms99x8::ScreenWidth] != 0x01) {
        std::cerr << "Raster fetch must preserve a fetched character and sample the next one later.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 characterFetchVdp;
    setRegister(characterFetchVdp, 1, 0x40);
    setRegister(characterFetchVdp, 2, 0x0F);
    setRegister(characterFetchVdp, 3, 0x04);
    setRegister(characterFetchVdp, 7, 0x01);
    characterFetchVdp.pokeVram(0x3C00, 0x00);
    characterFetchVdp.pokeVram(0x3C01, 0x01);
    characterFetchVdp.pokeVram(0x0000, 0xFF);
    characterFetchVdp.pokeVram(0x0008, 0xFF);
    characterFetchVdp.pokeVram(0x0100, 0xF1);
    for (unsigned line = 0; line < 64; ++line) characterFetchVdp.advanceScanline();
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart; ++dot) {
        characterFetchVdp.advanceDot();
    }
    characterFetchVdp.pokeVram(0x0000, 0x00);
    characterFetchVdp.pokeVram(0x0008, 0x00);
    characterFetchVdp.advanceDot();
    while (characterFetchVdp.outputs().dot !=
           tms99x8::HorizontalDisplayStart + 8) {
        characterFetchVdp.advanceDot();
    }
    for (unsigned dot = 0; dot < 8; ++dot) characterFetchVdp.advanceDot();
    const auto& fetchedCharacters = characterFetchVdp.scanoutFrame();
    if (fetchedCharacters[0] != 0x0F || fetchedCharacters[7] != 0x0F ||
        fetchedCharacters[8] != 0x01 || fetchedCharacters[15] != 0x01) {
        std::cerr << "Character fetch must latch only the group already being emitted.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 stagedFetchVdp;
    setRegister(stagedFetchVdp, 1, 0x40);
    setRegister(stagedFetchVdp, 2, 0x0F);
    setRegister(stagedFetchVdp, 3, 0x04);
    setRegister(stagedFetchVdp, 7, 0x01);
    stagedFetchVdp.pokeVram(0x3C00, 0x00);
    stagedFetchVdp.pokeVram(0x0100, 0x12);
    stagedFetchVdp.pokeVram(0x0000, 0x00);
    for (unsigned line = 0; line < 64; ++line) stagedFetchVdp.advanceScanline();
    for (unsigned tick = 0; tick < 27u * 4u; ++tick) stagedFetchVdp.advanceMasterTick();
    stagedFetchVdp.advanceDot();
    stagedFetchVdp.pokeVram(0x0000, 0xFF);
    for (unsigned tick = 0; tick < 3u * 4u; ++tick) stagedFetchVdp.advanceMasterTick();
    stagedFetchVdp.advanceDot();
    stagedFetchVdp.pokeVram(0x0100, 0xF2);
    for (unsigned tick = 0; tick < 6u * 4u; ++tick) stagedFetchVdp.advanceMasterTick();
    for (unsigned tick = 0; tick < 7u * 4u; ++tick) stagedFetchVdp.advanceMasterTick();
    const auto& stagedFetchFrame = stagedFetchVdp.scanoutFrame();
    if (stagedFetchFrame[0] != 0x01 || stagedFetchFrame[7] != 0x01) {
        std::cerr << "Name, color and pattern bytes must latch at their separate fetch phases.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 rasterModeVdp;
    setRegister(rasterModeVdp, 1, 0x40);
    setRegister(rasterModeVdp, 2, 0x0F);
    setRegister(rasterModeVdp, 3, 0x04);
    setRegister(rasterModeVdp, 7, 0xF1);
    rasterModeVdp.pokeVram(0x3C00, 0x00);
    rasterModeVdp.pokeVram(0x0000, 0xFF);
    rasterModeVdp.pokeVram(0x0001, 0xFF);
    rasterModeVdp.pokeVram(0x0100, 0xF1);
    for (unsigned line = 0; line < 64; ++line) rasterModeVdp.advanceScanline();
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart; ++dot) {
        rasterModeVdp.advanceDot();
    }
    setRegister(rasterModeVdp, 1, 0x50);
    rasterModeVdp.advanceDot();
    const auto currentLineRemainder = rasterModeVdp.scanoutFrame()[1];
    while (rasterModeVdp.outputs().scanline == 64) rasterModeVdp.advanceDot();
    for (unsigned dot = 0; dot <= tms99x8::HorizontalDisplayStart; ++dot) {
        rasterModeVdp.advanceDot();
    }
    if (currentLineRemainder != 0x0F ||
        rasterModeVdp.scanoutFrame()[tms99x8::ScreenWidth] != 0x01) {
        std::cerr << "VDP mode-register changes must take effect at the next raster line.\n";
        return EXIT_FAILURE;
    }

    tms99x8::TMS99X8 rasterSpriteVdp;
    setRegister(rasterSpriteVdp, 1, 0x40);
    setRegister(rasterSpriteVdp, 3, 0x04);
    setRegister(rasterSpriteVdp, 5, 0x06);
    setRegister(rasterSpriteVdp, 6, 0x04);
    setRegister(rasterSpriteVdp, 7, 0x01);
    rasterSpriteVdp.pokeVram(0x0300, 0x00);
    rasterSpriteVdp.pokeVram(0x0301, 50);
    rasterSpriteVdp.pokeVram(0x0302, 0x00);
    rasterSpriteVdp.pokeVram(0x0303, 0x0F);
    rasterSpriteVdp.pokeVram(0x0304, 208);
    rasterSpriteVdp.pokeVram(0x0100, 0x11);
    for (unsigned row = 0; row < 8; ++row) rasterSpriteVdp.pokeVram(0x2000 + row, 0x00);
    for (unsigned line = 0; line < 64; ++line) rasterSpriteVdp.advanceScanline();
    for (unsigned dot = 0; dot < 52; ++dot) rasterSpriteVdp.advanceDot();
    for (unsigned row = 0; row < 8; ++row) rasterSpriteVdp.pokeVram(0x2000 + row, 0xFF);
    while (rasterSpriteVdp.outputs().scanline == 64) rasterSpriteVdp.advanceDot();
    for (unsigned dot = 0; dot < tms99x8::HorizontalDisplayStart + 50; ++dot) {
        rasterSpriteVdp.advanceDot();
    }
    rasterSpriteVdp.advanceDot();
    if (rasterSpriteVdp.scanoutFrame()[tms99x8::ScreenWidth + 50] != 0x01) {
        std::cerr << "Sprite pattern changed after its fetch must not affect the current line.\n";
        return EXIT_FAILURE;
    }
    while (rasterSpriteVdp.outputs().scanline == 65) rasterSpriteVdp.advanceDot();
    for (unsigned dot = 0; dot < tms99x8::HorizontalDisplayStart + 50; ++dot) {
        rasterSpriteVdp.advanceDot();
    }
    rasterSpriteVdp.advanceDot();
    const auto nextSpritePixel = rasterSpriteVdp.scanoutFrame()[
        2 * tms99x8::ScreenWidth + 50];
    if (nextSpritePixel != 0x0F) {
        std::cerr << "Updated sprite pattern must be used after the next timed fetch.\n";
        return EXIT_FAILURE;
    }
    if (!spriteGeometryAndStatusPass()) return EXIT_FAILURE;
    if (!fourSpritesWithoutTerminatorPass()) return EXIT_FAILURE;
    if (!backdropRasterPass()) return EXIT_FAILURE;

    std::cout << "TMS99X8 family library passed.\n";
}