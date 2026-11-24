#pragma once

#include <chrome/controls/core/button.hpp>
#include <chrome/controls/core/notice_icon.hpp>
#include <chrome/dock.hpp>

#include <cstdint>

namespace chrome
{
    // Called when a button of a dialog was clicked, with its number. Returning false keeps the dialog open, such as for a Copy button
    // that does something and stays. Returning true, or having no handler, closes it with that button as the answer.
    using dialog_click_fn = bool (*)(void* user, std::uint32_t index) noexcept;
    // Paints the custom part of a dialog. dc is a Win32 HDC with the dialog's font selected, and area is where it goes, in pixels.
    using dialog_paint_fn = void (*)(void* user, void* dc, const dock_rect& area) noexcept;

    // What a dialog says and offers. Only the fields that are set show. The text is UTF-8 and has to stay valid while the dialog is open.
    struct dialog_spec
    {
        static constexpr std::uint32_t k_max_buttons = 4;

        const char* title{nullptr};
        // A bold line, and the paragraph under it. Both wrap at the width of the dialog.
        const char* heading{nullptr};
        const char* message{nullptr};
        notice_kind icon{notice_kind::none};

        // The buttons, left to right, right aligned. With none there is one, OK.
        button_style styles[k_max_buttons]{};
        const char* labels[k_max_buttons]{};
        std::uint32_t button_count{0};
        // The button Enter presses when no other has the focus.
        std::uint32_t default_button{0};
        // What Escape and the close box answer: a button, or -1 for no answer.
        std::int32_t cancel_button{-1};

        // The width of the client area at 96 dpi.
        std::int32_t width{440};

        // Room under the text for something of the caller's, painted by paint_fn.
        dialog_paint_fn paint_fn{nullptr};
        void* paint_user{nullptr};
        std::int32_t paint_height{0};

        // A check box under all that, such as "Do not ask again". It starts as *checked and is written back when the dialog closes.
        const char* check_label{nullptr};
        bool* checked{nullptr};

        dialog_click_fn on_click{nullptr};
        void* user{nullptr};

        dialog_spec& set_title(const char* text) noexcept { title = text; return *this; }
        dialog_spec& set_heading(const char* text) noexcept { heading = text; return *this; }
        dialog_spec& set_message(const char* text) noexcept { message = text; return *this; }
        dialog_spec& set_icon(notice_kind kind) noexcept { icon = kind; return *this; }
        dialog_spec& set_width(std::int32_t pixels) noexcept { width = pixels; return *this; }
        // Adds a button. The first four count.
        dialog_spec& add_button(const char* label, button_style style = button_style::standard) noexcept
        {
            if (button_count < k_max_buttons)
            {
                labels[button_count] = label;
                styles[button_count] = style;
                ++button_count;
            }

            return *this;
        }
        dialog_spec& set_default(std::uint32_t index) noexcept { default_button = index; return *this; }
        dialog_spec& set_cancel(std::int32_t index) noexcept { cancel_button = index; return *this; }
        dialog_spec& set_content(dialog_paint_fn painter, void* painter_user, std::int32_t height) noexcept
        {
            paint_fn = painter;
            paint_user = painter_user;
            paint_height = height;
            return *this;
        }
        dialog_spec& set_check(const char* label, bool* value) noexcept { check_label = label; checked = value; return *this; }
        dialog_spec& set_on_click(dialog_click_fn handler, void* handler_user) noexcept { on_click = handler; user = handler_user; return *this; }
    };

    // Shows a dialog in the editor's theme in a window of its own, over the owner (an HWND), which is disabled meanwhile. Does not
    // return until it is closed. The answer is the number of the button that closed it, or cancel_button for Escape and the close box.
    // Enter presses the focused button, or the default one; Tab and the arrows move between them. A dialog opened from inside a dialog
    // is refused with -1.
    //
    //   dialog_spec about;
    //   about.set_title("About").set_heading("My App").set_message("A tool.").add_button("Close", button_style::primary).set_cancel(0);
    //   show_dialog(main_window, about);
    std::int32_t show_dialog(void* owner, const dialog_spec& spec) noexcept;

    // Asks a yes or no question. true when the first button was chosen. The label of the first button should say what it does.
    [[nodiscard]] bool confirm_dialog(void* owner, const char* title, const char* heading, const char* message, const char* yes = "OK",
        const char* no = "Cancel", notice_kind icon = notice_kind::question) noexcept;
}
