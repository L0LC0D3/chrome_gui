#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>
#include <chrome/controls/core/text_field.hpp>
#include <chrome/controls/property/color.hpp>
#include <chrome/controls/property/screen_loupe.hpp>

#include <cstdint>

namespace chrome
{
    // The window that opens over a colour swatch, laid out like Unity's: a square to pick the saturation and the value, a
    // bar for the hue beside it, and the new colour over the old one at the right. Under them are sliders with a number
    // field for each channel, in red, green, blue and alpha or in hue, saturation, value and alpha, with tracks that show
    // what the channel would give; a hex field with a pipette beside it; and a row of preset colours. Clicking the old
    // colour puts it back. The pipette is held down and dragged anywhere on the screen, even outside the editor, and takes
    // the colour of the pixel under the pointer when the button is let go, showing it as the new colour on the way, and
    // a loupe beside the pointer shows the pixels around it magnified. Escape ends the pipette and the loupe, and puts the
    // colour back to what it was before the pipette.
    //
    // It is not a control. A swatch opens it, lends it the mouse and the keys while it is open, and draws it in its overlay,
    // so one picker serves every swatch of a panel, and only one can be open at a time.
    //
    //   color_picker picker;                       // one for the panel
    //   swatch.set_picker(&picker);                // for each swatch
    class color_picker
    {
    public:
        // Called after every change the user makes, while they make it.
        using change_fn = void (*)(void* user, color_picker& source) noexcept;

        color_picker() noexcept;
        color_picker(const color_picker&) = delete;
        color_picker& operator=(const color_picker&) = delete;

        // Opens under an area, or over it when there is no room below, inside the bounds. alpha says whether the colour has
        // one to edit. user is whatever opened it, which can ask whether it still owns the picker.
        void open(const dock_rect& anchor, const dock_rect& bounds, const color_rgba& color, bool alpha, change_fn handler, void* user) noexcept;
        void close() noexcept;
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        [[nodiscard]] void* owner() const noexcept { return m_user; }

        [[nodiscard]] const color_rgba& color() const noexcept { return m_color; }
        // Shows another colour, such as when something else changed the one being edited. Does not call the handler.
        void set_color(const color_rgba& color) noexcept;
        // Moves the window when its swatch moved or the bounds did.
        void set_anchor(const dock_rect& anchor, const dock_rect& bounds) noexcept { m_anchor = anchor; m_bounds = bounds; }

        // Where the window is, in the coordinates of the controls.
        [[nodiscard]] dock_rect area() const noexcept;
        [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const noexcept;

        void paint(void* dc) const noexcept;
        // The mouse, in the coordinates of the controls. true when the window needs a repaint.
        [[nodiscard]] bool on_mouse(const panel_mouse& mouse) noexcept;
        // Keys and text for the field being typed in. Escape, or Enter with no field in use, closes the window. true when used.
        [[nodiscard]] bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept;
        void on_char(std::uint32_t code) noexcept;
        // A caret blinks while a field has the focus.
        [[nodiscard]] bool animating() const noexcept { return m_open && m_inner.animating(); }
        bool animate(std::uint64_t now_ms) noexcept;
        // true when something that needs a repaint happened since the last call, such as a blink.
        [[nodiscard]] bool take_dirty() noexcept { return m_inner.take_dirty(); }

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            square,
            hue,
            channel,
            // The pipette is held down and the colour under the pointer follows it.
            pipette,
            // A press inside a number field or the hex field, which belong to it.
            field
        };

        static constexpr std::uint32_t k_channels = 4;
        static constexpr std::uint32_t k_presets = 10;

        // Puts the fields where the window is. Done before anything that needs their places.
        void place() const noexcept;
        // Rectangles of the parts, in pixels, from the origin of the window.
        [[nodiscard]] dock_rect square_rect() const noexcept;
        [[nodiscard]] dock_rect hue_rect() const noexcept;
        [[nodiscard]] dock_rect new_rect() const noexcept;
        [[nodiscard]] dock_rect old_rect() const noexcept;
        [[nodiscard]] dock_rect track_rect(std::uint32_t channel) const noexcept;
        [[nodiscard]] dock_rect hex_rect() const noexcept;
        [[nodiscard]] dock_rect mode_rect(bool hsv) const noexcept;
        [[nodiscard]] dock_rect pipette_rect() const noexcept;
        [[nodiscard]] dock_rect preset_rect(std::uint32_t index) const noexcept;
        [[nodiscard]] std::uint32_t channel_count() const noexcept { return m_alpha ? 4u : 3u; }
        [[nodiscard]] dock_rect part(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const noexcept;
        [[nodiscard]] std::int32_t height96() const noexcept;

        // The channel as a number from 0 to 1, in the mode in use.
        [[nodiscard]] float channel_value(std::uint32_t channel) const noexcept;
        // Sets a channel from a number from 0 to 1, and keeps the other forms of the colour in step.
        void set_channel(std::uint32_t channel, float value) noexcept;
        void set_mode(bool hsv) noexcept;
        void drag_to(std::int32_t x, std::int32_t y) noexcept;
        void apply_hsv() noexcept;
        void apply_rgb() noexcept;
        // Writes the colour into the number fields and the hex field, except the one being typed in.
        void sync_fields() noexcept;
        void changed() noexcept;
        void commit_hex() noexcept;
        // Takes the colour of the screen pixel under the pointer, wherever on the screen that is.
        void pick_screen() noexcept;
        // Ends the pipette and hides the loupe. With restore the colour goes back to what it was when the pipette began.
        void end_pipette(bool restore) noexcept;
        void blur_fields() noexcept;

        static void channel_changed(void* user, number_field& source) noexcept;
        static bool hex_acted(void* user, text_field& source, text_action action) noexcept;
        static std::int32_t hex_filter(void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept;

        bool m_open{false};
        bool m_alpha{true};
        bool m_hsv_mode{false};
        dock_rect m_anchor{};
        dock_rect m_bounds{};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};

        color_rgba m_color{};
        color_rgba m_original{};
        color_hsv m_hsv{};
        // The colour when the pipette began, for Escape to go back to.
        color_rgba m_before_pipette{};
        screen_loupe m_loupe;

        drag_kind m_drag{drag_kind::none};
        std::uint32_t m_drag_channel{0};

        // Placed by place(), which only moves them to the window.
        mutable number_field m_fields[k_channels];
        mutable text_field m_hex;
        mutable control_set m_inner;
    };
}
