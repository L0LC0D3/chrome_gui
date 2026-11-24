#pragma once

#include <cstdint>

namespace chrome
{
    // Looks at the text typed so far. Returns true when it is acceptable. Otherwise it returns false and may point why at
    // a sentence that is shown under the box, which has to outlive the call. Called after every change.
    using prompt_check_fn = bool (*)(void* user, const char* text, const char** why) noexcept;

    // Asks for a line of text in a small window of its own, in the editor's theme, and does not return until it is closed.
    // The owner (an HWND) is disabled meanwhile. OK takes the text, in out as UTF-8, when the check accepts it, and Enter
    // does the same. Cancel, Escape and closing the window leave out untouched. initial may be null.
    //
    //   char name[64];
    //   if (prompt_text(main_window, "Save layout", "Name of the layout", "", name, sizeof(name), &check, this))
    //       store(name);
    [[nodiscard]] bool prompt_text(void* owner, const char* title, const char* label, const char* initial, char* out, std::uint32_t capacity,
        prompt_check_fn check, void* user) noexcept;
}
