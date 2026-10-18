#pragma once

#include <cstdint>

namespace svi318 {

struct KeyboardPosition {
    std::uint8_t row;
    std::uint8_t bit;
};

// SVI contact pressed together with a character key.
enum class KeyModifier : std::uint8_t {
    None,
    Shift,
    LeftGraph,
    RightGraph
};

inline constexpr KeyboardPosition SviShift{6, 0};
inline constexpr KeyboardPosition SviLeftGraph{6, 2};
inline constexpr KeyboardPosition SviRightGraph{6, 3};

// Converts a character to the SVI-318 key contact and the modifier it needs
// (character table of openMSX unicodemap.svi).
[[nodiscard]] inline bool mapCharacterToSvi(char16_t character,
                                            KeyboardPosition& position,
                                            KeyModifier& modifier) noexcept
{
    modifier = KeyModifier::None;
    if (character >= u'A' && character <= u'Z') {
        modifier = KeyModifier::Shift;
        character = static_cast<char16_t>(character - u'A' + u'a');
    }
    if (character >= u'a' && character <= u'z') {
        const auto index = static_cast<unsigned>(character - u'a') + 1;
        position = {static_cast<std::uint8_t>(2 + index / 8), static_cast<std::uint8_t>(index % 8)};
        return true;
    }
    if (character >= u'0' && character <= u'9') {
        const auto digit = static_cast<unsigned>(character - u'0');
        position = {static_cast<std::uint8_t>(digit / 8), static_cast<std::uint8_t>(digit % 8)};
        return true;
    }

    struct Entry {
        char16_t character;
        KeyboardPosition position;
        KeyModifier modifier;
    };
    static constexpr Entry entries[] = {
        {u' ', {8, 0}, KeyModifier::None},
        {u'!', {0, 1}, KeyModifier::Shift}, {u'"', {1, 3}, KeyModifier::Shift},
        {u'#', {0, 3}, KeyModifier::Shift}, {u'$', {0, 4}, KeyModifier::Shift},
        {u'%', {0, 5}, KeyModifier::Shift}, {u'&', {0, 7}, KeyModifier::Shift},
        {u'\'', {1, 3}, KeyModifier::None}, {u'(', {1, 1}, KeyModifier::Shift},
        {u')', {0, 0}, KeyModifier::Shift}, {u'*', {1, 0}, KeyModifier::Shift},
        {u'+', {1, 5}, KeyModifier::Shift}, {u',', {1, 4}, KeyModifier::None},
        {u'-', {2, 0}, KeyModifier::None}, {u'.', {1, 6}, KeyModifier::None},
        {u'/', {1, 7}, KeyModifier::None}, {u':', {1, 2}, KeyModifier::None},
        {u';', {1, 2}, KeyModifier::Shift}, {u'<', {1, 4}, KeyModifier::Shift},
        {u'=', {1, 5}, KeyModifier::None}, {u'>', {1, 6}, KeyModifier::Shift},
        {u'?', {1, 7}, KeyModifier::Shift}, {u'@', {0, 2}, KeyModifier::Shift},
        {u'[', {5, 3}, KeyModifier::None}, {u'\\', {5, 4}, KeyModifier::None},
        {u']', {5, 5}, KeyModifier::None}, {u'^', {0, 6}, KeyModifier::Shift},
        {u'_', {2, 0}, KeyModifier::Shift}, {u'`', {5, 3}, KeyModifier::RightGraph},
        {u'{', {5, 3}, KeyModifier::Shift}, {u'|', {5, 3}, KeyModifier::LeftGraph},
        {u'}', {5, 5}, KeyModifier::Shift}, {u'~', {5, 4}, KeyModifier::Shift},
    };
    for (const auto& entry : entries) {
        if (entry.character != character) continue;
        position = entry.position;
        modifier = entry.modifier;
        return true;
    }
    return false;
}

// Converts a key of the German PC layout (scan code set 1) to the SVI-318 contact of the
// character it produces with the current Shift/AltGr state.
[[nodiscard]] inline bool mapGermanScanCodeToSvi(std::uint8_t scanCode,
                                                 bool extended,
                                                 bool pcShift,
                                                 bool pcAltGr,
                                                 KeyboardPosition& position,
                                                 KeyModifier& modifier) noexcept
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
        {0x1B, u'+', u'*', u'~'},
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
        return mapCharacterToSvi(character, position, modifier);
    }
    return false;
}

// Converts a key of the US PC layout (scan code set 1) to the SVI-318 contact of the character it
// produces with the current Shift state. The right Alt key adds no characters on this layout.
[[nodiscard]] inline bool mapUsScanCodeToSvi(std::uint8_t scanCode,
                                             bool extended,
                                             bool pcShift,
                                             bool pcAltGr,
                                             KeyboardPosition& position,
                                             KeyModifier& modifier) noexcept
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
        return mapCharacterToSvi(pcShift ? key.shifted : key.plain, position, modifier);
    }
    return false;
}

} // namespace svi318
