#pragma once

#include <cstdint>

namespace chrome::clipboard
{
    // Puts UTF-16 text on the system clipboard. false when the clipboard could not be opened.
    bool set(const wchar_t* text, std::int32_t length) noexcept;
    // Reads the text on the clipboard into out, which holds capacity wide characters, and cuts it if it is longer.
    // false when there is no text. length is the number of characters written, without the terminator.
    bool get(wchar_t* out, std::int32_t capacity, std::int32_t& length) noexcept;
}
