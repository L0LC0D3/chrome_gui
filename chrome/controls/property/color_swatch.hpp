#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/property/color.hpp>
#include <chrome/controls/property/color_picker.hpp>

#include <cstdint>

namespace chrome
{
    // A box that shows a colour, as Unity's colour field does: the colour solid on the left, the same colour at its alpha
    // over squares on the right, and under both a thin bar that is white for the alpha and black for what is missing. A
    // click, Enter or Space opens a colour picker under it, which changes the colour as the user works, and a click anywhere
    // else, or Escape, closes it. Without alpha the box is one solid colour.
    //
    // The picker is shared: one color_picker serves all the swatches of a panel, and the panel gives each swatch its address.
    //
    //   color_picker picker;
    //   color_swatch albedo;
    //   albedo.set_label("Albedo").set_picker(&picker).set_on_change(&painted, this);
    //   albedo.set_color({0.8f, 0.4f, 0.1f, 1.0f});
    //   set.add(albedo);
    class color_swatch final : public control
    {
    public:
        // Called after the user changed the colour in the picker, as they change it.
        using change_fn = void (*)(void* user, color_swatch& source) noexcept;

        // Shows another colour, such as when the data changed. Does not call the handler. An open picker follows it.
        color_swatch& set_color(const color_rgba& color) noexcept;
        [[nodiscard]] const color_rgba& color() const noexcept { return m_color; }
        // Whether the colour has an alpha, which the box shows and the picker edits. On by default.
        color_swatch& set_alpha(bool alpha) noexcept;
        [[nodiscard]] bool has_alpha() const noexcept { return m_alpha; }

        // The picker this swatch opens, not owned. Without one a click does nothing.
        color_swatch& set_picker(color_picker* picker) noexcept { m_picker = picker; return *this; }
        // The area the picker has to stay inside, in the coordinates of the controls. It opens upward when it does not fit
        // below. An empty area, the default, means it always opens downward.
        color_swatch& set_bounds(const dock_rect& bounds) noexcept { m_bounds = bounds; return *this; }

        // A caption to the left of the box, UTF-8, which has to outlive the control. Optional.
        color_swatch& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The room for the caption at 96 dpi.
        color_swatch& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The width of the box at 96 dpi, without the caption. 0 fills the row.
        color_swatch& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // What it is called, such as in a status line. The label is used when this is not set.
        color_swatch& set_name(const char* name) noexcept { m_name = name; return *this; }
        color_swatch& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        color_swatch& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        // The picker is open on this swatch.
        [[nodiscard]] bool is_open() const noexcept;
        void open() noexcept;
        void close() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        // The picker stays over the controls while it is open.
        [[nodiscard]] bool overlay_open() const noexcept override { return is_open(); }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return is_open() && m_picker->animating(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_click() noexcept override;
        void on_leave() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        // The box itself, which is all of the area but the caption.
        [[nodiscard]] dock_rect box() const noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;
        static void picked(void* user, color_picker& source) noexcept;

        color_rgba m_color{};
        bool m_alpha{true};
        color_picker* m_picker{nullptr};
        dock_rect m_bounds{};
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_label_width{90};
        std::int32_t m_width{160};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};

        // A press that began in the picker, which the picker keeps until the button comes up.
        bool m_in_picker{false};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
    };
}
