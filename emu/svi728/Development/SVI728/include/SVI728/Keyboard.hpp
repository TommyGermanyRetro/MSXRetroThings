#pragma once

#include <cstdint>

namespace svi728 {

struct KeyboardPosition {
    std::uint8_t row;
    std::uint8_t bit;
};

// Converts the character produced by the active PC layout to the equivalent
// US-labeled MSX key contact and whether the MSX Shift contact is also needed.
[[nodiscard]] inline bool mapCharacterToMsx(char16_t character,
                                             KeyboardPosition& position,
                                             bool& shiftRequired) noexcept
{
    shiftRequired = false;
    if (character >= u'a' && character <= u'z') {
        character = static_cast<char16_t>(character - u'a' + u'A');
    } else if (character >= u'A' && character <= u'Z') {
        shiftRequired = true;
    }

    if (character >= u'A' && character <= u'Z') {
        static constexpr std::uint8_t rows[26] = {
            2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
            4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5
        };
        static constexpr std::uint8_t bits[26] = {
            6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2,
            3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7
        };
        const auto index = static_cast<unsigned>(character - u'A');
        position = {rows[index], bits[index]};
        return true;
    }

    if (character >= u'0' && character <= u'9') {
        const auto digit = static_cast<unsigned>(character - u'0');
        if (digit == 0) position = {0, 0};
        else if (digit <= 7) position = {0, static_cast<std::uint8_t>(digit)};
        else position = {1, static_cast<std::uint8_t>(digit - 8)};
        return true;
    }

    switch (character) {
    case u'!': position = {0, 1}; shiftRequired = true; return true;
    case u'"': position = {2, 0}; shiftRequired = true; return true;
    case u'#': position = {0, 3}; shiftRequired = true; return true;
    case u'$': position = {0, 4}; shiftRequired = true; return true;
    case u'%': position = {0, 5}; shiftRequired = true; return true;
    case u'&': position = {0, 7}; shiftRequired = true; return true;
    case u'\'': position = {2, 0}; return true;
    case u'(': position = {1, 1}; shiftRequired = true; return true;
    case u')': position = {0, 0}; shiftRequired = true; return true;
    case u'*': position = {1, 0}; shiftRequired = true; return true;
    case u'+': position = {1, 3}; shiftRequired = true; return true;
    case u',': position = {2, 2}; return true;
    case u'-': position = {1, 2}; return true;
    case u'.': position = {2, 3}; return true;
    case u'/': position = {2, 4}; return true;
    case u':': position = {1, 7}; shiftRequired = true; return true;
    case u';': position = {1, 7}; return true;
    case u'<': position = {2, 2}; shiftRequired = true; return true;
    case u'=': position = {1, 3}; return true;
    case u'>': position = {2, 3}; shiftRequired = true; return true;
    case u'?': position = {2, 4}; shiftRequired = true; return true;
    case u'@': position = {0, 2}; shiftRequired = true; return true;
    case u'[': position = {1, 5}; return true;
    case u'\\': position = {1, 4}; return true;
    case u']': position = {1, 6}; return true;
    case u'^': position = {0, 6}; shiftRequired = true; return true;
    case u'_': position = {1, 2}; shiftRequired = true; return true;
    case u'`': position = {2, 1}; return true;
    case u'{': position = {1, 5}; shiftRequired = true; return true;
    case u'|': position = {1, 4}; shiftRequired = true; return true;
    case u'}': position = {1, 6}; shiftRequired = true; return true;
    case u'~': position = {2, 1}; shiftRequired = true; return true;
    default: return false;
    }
}

[[nodiscard]] inline bool mapGermanScanCodeToMsx(std::uint8_t scanCode,
                                                  bool extended,
                                                  bool pcShift,
                                                  bool pcAltGr,
                                                  KeyboardPosition& position,
                                                  bool& msxShiftRequired) noexcept
{
    if (extended) return false;

    struct GermanKeyMapping {
        std::uint8_t scanCode;
        char16_t plain;
        char16_t shifted;
        char16_t altGr;
    };
    static constexpr GermanKeyMapping keys[] = {
        {0x02, u'1', u'!', 0}, {0x03, u'2', u'"', 0},
        {0x04, u'3', 0, 0}, {0x05, u'4', u'$', 0},
        {0x06, u'5', u'%', 0}, {0x07, u'6', u'&', 0},
        {0x08, u'7', u'/', u'{'}, {0x09, u'8', u'(', u'['},
        {0x0A, u'9', u')', u']'}, {0x0B, u'0', u'=', u'}'},
        {0x0C, 0, u'?', u'\\'},
        {0x10, u'q', u'Q', u'@'}, {0x11, u'w', u'W', 0},
        {0x12, u'e', u'E', 0}, {0x13, u'r', u'R', 0},
        {0x14, u't', u'T', 0}, {0x15, u'z', u'Z', 0},
        {0x16, u'u', u'U', 0}, {0x17, u'i', u'I', 0},
        {0x18, u'o', u'O', 0}, {0x19, u'p', u'P', 0},
        {0x1B, u'+', u'*', 0},
        {0x1E, u'a', u'A', 0}, {0x1F, u's', u'S', 0},
        {0x20, u'd', u'D', 0}, {0x21, u'f', u'F', 0},
        {0x22, u'g', u'G', 0}, {0x23, u'h', u'H', 0},
        {0x24, u'j', u'J', 0}, {0x25, u'k', u'K', 0},
        {0x26, u'l', u'L', 0}, {0x29, u'^', 0, 0},
        {0x2B, u'#', u'\'', 0},
        {0x2C, u'y', u'Y', 0}, {0x2D, u'x', u'X', 0},
        {0x2E, u'c', u'C', 0}, {0x2F, u'v', u'V', 0},
        {0x30, u'b', u'B', 0}, {0x31, u'n', u'N', 0},
        {0x32, u'm', u'M', 0}, {0x33, u',', u';', 0},
        {0x34, u'.', u':', 0}, {0x35, u'-', u'_', 0},
        {0x56, u'<', u'>', u'|'}
    };

    for (const auto& key : keys) {
        if (key.scanCode != scanCode) continue;
        const char16_t character = pcAltGr ? key.altGr : (pcShift ? key.shifted : key.plain);
        if (character == 0) return false;
        return mapCharacterToMsx(character, position, msxShiftRequired);
    }
    return false;
}

// Converts a key of the US PC layout (scan code set 1) to the MSX contact of the character it produces
// with the current Shift state. The right Alt key adds no characters on this layout.
[[nodiscard]] inline bool mapUsScanCodeToMsx(std::uint8_t scanCode,
                                             bool extended,
                                             bool pcShift,
                                             bool pcAltGr,
                                             KeyboardPosition& position,
                                             bool& msxShiftRequired) noexcept
{
    if (extended || pcAltGr) return false;

    struct UsKeyMapping {
        std::uint8_t scanCode;
        char16_t plain;
        char16_t shifted;
    };
    static constexpr UsKeyMapping keys[] = {
        {0x02, u'1', u'!'}, {0x03, u'2', u'@'},
        {0x04, u'3', u'#'}, {0x05, u'4', u'$'},
        {0x06, u'5', u'%'}, {0x07, u'6', u'^'},
        {0x08, u'7', u'&'}, {0x09, u'8', u'*'},
        {0x0A, u'9', u'('}, {0x0B, u'0', u')'},
        {0x0C, u'-', u'_'}, {0x0D, u'=', u'+'},
        {0x10, u'q', u'Q'}, {0x11, u'w', u'W'},
        {0x12, u'e', u'E'}, {0x13, u'r', u'R'},
        {0x14, u't', u'T'}, {0x15, u'y', u'Y'},
        {0x16, u'u', u'U'}, {0x17, u'i', u'I'},
        {0x18, u'o', u'O'}, {0x19, u'p', u'P'},
        {0x1A, u'[', u'{'}, {0x1B, u']', u'}'},
        {0x1E, u'a', u'A'}, {0x1F, u's', u'S'},
        {0x20, u'd', u'D'}, {0x21, u'f', u'F'},
        {0x22, u'g', u'G'}, {0x23, u'h', u'H'},
        {0x24, u'j', u'J'}, {0x25, u'k', u'K'},
        {0x26, u'l', u'L'}, {0x27, u';', u':'},
        {0x28, u'\'', u'"'}, {0x29, u'`', u'~'},
        {0x2B, u'\\', u'|'},
        {0x2C, u'z', u'Z'}, {0x2D, u'x', u'X'},
        {0x2E, u'c', u'C'}, {0x2F, u'v', u'V'},
        {0x30, u'b', u'B'}, {0x31, u'n', u'N'},
        {0x32, u'm', u'M'}, {0x33, u',', u'<'},
        {0x34, u'.', u'>'}, {0x35, u'/', u'?'},
        {0x56, u'\\', u'|'}
    };
    for (const auto& key : keys) {
        if (key.scanCode != scanCode) continue;
        return mapCharacterToMsx(pcShift ? key.shifted : key.plain, position, msxShiftRequired);
    }
    return false;
}

} // namespace svi728