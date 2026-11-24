#pragma once

#include <chrome/controls/core/notice_icon.hpp>

#include <cstdint>

namespace chrome::toast
{
    // Called when the action link of a toast was clicked.
    using action_fn = void (*)(void* user) noexcept;

    // What a toast says. The text is copied, so it need not outlive the call.
    struct options
    {
        notice_kind kind{notice_kind::info};
        // An optional bold line above the text.
        const char* title{nullptr};
        const char* text{nullptr};
        // How long it stays, in milliseconds, while the pointer is not on it. 0 keeps it until it is closed.
        std::uint32_t duration{4000};
        // An optional link under the text, and what it does. A click on it also closes the toast.
        const char* action{nullptr};
        action_fn on_action{nullptr};
        void* user{nullptr};
    };

    // A short message that slides up in the lower right corner of the main window, stays a few seconds, and fades away. It does not
    // take the focus and does not block anything, so it can come from anywhere in the editor, also while a dialog is open. Hovering
    // it holds the clock, a click closes it, and up to five show at once, the newest at the bottom.
    //
    //   toast::install(main_window);
    //   toast::success("Layout saved");
    //   toast::options o;
    //   o.kind = notice_kind::error;
    //   o.title = "Build failed";
    //   o.text = "2 shaders did not compile.";
    //   o.action = "Show log";
    //   o.on_action = &show_log;
    //   toast::show(o);
    //
    // All of it is for the thread that runs the window messages.

    // Sets the window the toasts stack in the corner of. Call once, after the window exists.
    void install(void* main_window) noexcept;
    void show(const options& what) noexcept;
    void info(const char* text) noexcept;
    void success(const char* text) noexcept;
    void warning(const char* text) noexcept;
    void error(const char* text) noexcept;
    // Takes every toast away at once.
    void clear() noexcept;
}
