#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A box showing one choice from a list. Clicking it opens the list over the controls below, and clicking a row chooses
    // it. With the keyboard focus, Up and Down change the choice, Alt+Down, F4, Space and Enter open the list, and
    // while it is open the arrows, Home, End and the paging keys move along it, Enter chooses and Escape closes.
    //
    //   dropdown quality;
    //   quality.set_name("Quality").set_width(160).set_on_change(&picked, this);
    //   quality.add_item("Low").add_item("Medium").add_item("High");
    //   quality.select(1);
    //   set.add(quality);
    class dropdown final : public control
    {
    public:
        static constexpr std::uint32_t k_max_items = 32;
        // The rows the list shows at once. A longer list scrolls with the wheel and the keys.
        static constexpr std::uint32_t k_max_rows = 8;
        static constexpr std::int32_t k_none = -1;

        // Called after the user changed the choice. Choosing from code does not call it.
        using change_fn = void (*)(void* user, dropdown& source) noexcept;

        // Text and names are UTF-8 and have to outlive the dropdown.
        // false when the list is full.
        bool add_item(const char* text) noexcept;
        dropdown& clear_items() noexcept;
        // Chooses a row, or k_none for no choice. It has to be in the list.
        dropdown& select(std::int32_t index) noexcept;
        dropdown& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // A caption to the left of the box. Optional. The width is at 96 dpi, and the box keeps its own width after it.
        dropdown& set_label(const char* label) noexcept { m_label = label; return *this; }
        dropdown& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // Shown in the box while nothing is chosen.
        dropdown& set_placeholder(const char* text) noexcept { m_placeholder = text; return *this; }
        // What it is called, such as in a status line.
        dropdown& set_name(const char* name) noexcept { m_name = name; return *this; }
        dropdown& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        dropdown& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }
        // The area the list has to stay inside, in the coordinates of the controls. It opens upward when it does not fit
        // below. An empty area, the default, means it always opens downward.
        dropdown& set_bounds(const dock_rect& bounds) noexcept { m_bounds = bounds; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] std::int32_t selected() const noexcept { return m_selected; }
        // The chosen text, or null.
        [[nodiscard]] const char* selected_text() const noexcept;
        [[nodiscard]] const char* item(std::uint32_t index) const noexcept { return index < m_count ? m_items[index] : nullptr; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label != nullptr ? m_label : m_placeholder; }
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        void close() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        // The list stays up while it closes. Opening and closing grow and shrink it from the box.
        [[nodiscard]] bool overlay_open() const noexcept override { return m_open || m_pos > 0.0f; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_pos != (m_open ? 1.0f : 0.0f); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_click() noexcept override;
        void on_press(std::int32_t x, std::int32_t y) noexcept override { m_x = x; m_y = y; }
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        // The box itself, which is all of the area but the label.
        [[nodiscard]] dock_rect box() const noexcept;
        // The whole list, at its full height. overlay_area() is the part of it that shows while it opens or closes.
        [[nodiscard]] dock_rect full_area() const noexcept;
        void open() noexcept;
        // Chooses a row, tells the handler if that changed it, and closes the list.
        void choose(std::int32_t index) noexcept;
        // The row at a point of the open list, or k_none.
        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        void show_row(std::int32_t index) noexcept;
        // The highlighted row follows a point, and true when it moved.
        bool point_at(std::int32_t x, std::int32_t y) noexcept;

        const char* m_items[k_max_items]{};
        std::uint32_t m_count{0};
        const char* m_placeholder{nullptr};
        const char* m_name{nullptr};
        const char* m_label{nullptr};
        std::int32_t m_label_width{90};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
        dock_rect m_bounds{};
        std::int32_t m_width{160};
        std::int32_t m_selected{k_none};
        // The row the keys and the mouse are on while the list is open, and the first row that shows.
        std::int32_t m_highlight{k_none};
        std::int32_t m_first{0};
        // The last place the mouse was over the control.
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_open{false};
        // How far the list has opened, 0 to 1, and when it was last moved.
        float m_pos{0.0f};
        std::uint64_t m_stamp{0};
    };
}
