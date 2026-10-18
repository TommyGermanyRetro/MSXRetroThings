#pragma once

#include <cstdint>
#include <string>

namespace vg8235 {

enum class UiLanguage : std::uint8_t {
    English,
    German
};

// Language of the frontend's menus, status bar and messages; English unless the user chooses German.
inline UiLanguage& uiLanguage() noexcept
{
    static UiLanguage language = UiLanguage::English;
    return language;
}

// The text in the chosen language.
inline const wchar_t* ui(const wchar_t* english, const wchar_t* german) noexcept
{
    return uiLanguage() == UiLanguage::German ? german : english;
}

// The text with "%1" replaced by value.
inline std::wstring fillIn(std::wstring text, const std::wstring& value)
{
    const auto position = text.find(L"%1");
    if (position != std::wstring::npos) text.replace(position, 2, value);
    return text;
}

} // namespace vg8235
