#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/layout.hpp>

#include <cstdint>

namespace chrome
{
    enum class split_axis : std::uint8_t
    {
        // The panes side by side, with a bar between them that is dragged left and right.
        horizontal,
        // The panes one above the other, with a bar that is dragged up and down.
        vertical
    };

    // Two panes with a bar between them that is dragged to give one more room and the other less. A pane is a layout
    // container, so what is in it is laid out in the room it has, and clipped to it. The sizes are kept as a share, so the
    // panes keep their proportions when the splitter is resized, and each pane has a least size the bar cannot go past.
    //
    // A double click on the bar puts it back to where it began. With the focus the arrows move it (Shift ten times as far,
    // Ctrl a tenth), and Home and End send it to either end. A splitter can hold another, in a pane.
    //
    // The splitter has to be added to the panel's control_set after the controls in its panes, so that a click on one of
    // them reaches it and only the bar and what is round it reaches the splitter. arrange() puts the panes where they go
    // and has to be called after the splitter's own area was set, outer splitters before inner ones.
    //
    //   splitter split;
    //   split.set_panes(left, right).set_ratio(0.3f).set_first_min(80);
    //   set.add(...controls in the panes...);
    //   set.add(split);
    //   split.arrange(dc);
    class splitter final : public control
    {
    public:
        using change_fn = void (*)(void* user, splitter& source) noexcept;

        splitter(const splitter&) = delete;
        splitter& operator=(const splitter&) = delete;
        splitter() noexcept = default;

        splitter& set_axis(split_axis axis) noexcept { m_axis = axis; return *this; }
        // The containers of the two panes, which have to outlive the splitter. The first is left or above.
        splitter& set_panes(layout_container& first, layout_container& second) noexcept { m_first = &first; m_second = &second; return *this; }
        // The share of the room the first pane has, 0 to 1. The one it was given first is what a double click goes back to.
        splitter& set_ratio(float ratio) noexcept;
        // The least sizes of the panes along the axis at 96 dpi.
        splitter& set_first_min(std::int32_t size) noexcept { m_first_min = size; return *this; }
        splitter& set_second_min(std::int32_t size) noexcept { m_second_min = size; return *this; }
        // The height it asks for in a layout, at 96 dpi. The width is the row's.
        splitter& set_height(std::int32_t height) noexcept { m_height = height; return *this; }
        // How thick the bar is, at 96 dpi.
        splitter& set_bar_width(std::int32_t width) noexcept { m_bar = width; return *this; }
        splitter& set_name(const char* name) noexcept { m_name = name; return *this; }
        splitter& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        splitter& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] float ratio() const noexcept { return m_ratio; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        // The bar, in the coordinates of the controls.
        [[nodiscard]] dock_rect bar() const noexcept;
        // The two panes, as of the last arrange.
        [[nodiscard]] dock_rect first_area() const noexcept;
        [[nodiscard]] dock_rect second_area() const noexcept;

        // Lays the panes out in the area the splitter has now.
        void arrange(void* dc) noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        [[nodiscard]] dock_rect focus_ring_area() const noexcept override { return bar(); }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override;
        void on_release() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        // The room along the axis, and the size the first pane has in it.
        [[nodiscard]] std::int32_t room() const noexcept;
        [[nodiscard]] std::int32_t first_size() const noexcept;
        [[nodiscard]] bool on_bar(std::int32_t x, std::int32_t y) const noexcept;
        // Sets the first pane's size in pixels, kept between the least sizes. true when it changed.
        bool set_first_size(std::int32_t size) noexcept;
        void notify() noexcept;

        split_axis m_axis{split_axis::horizontal};
        layout_container* m_first{nullptr};
        layout_container* m_second{nullptr};
        float m_ratio{0.5f};
        float m_start_ratio{0.5f};
        bool m_ratio_given{false};
        std::int32_t m_first_min{40};
        std::int32_t m_second_min{40};
        std::int32_t m_height{160};
        std::int32_t m_bar{6};
        const char* m_name{nullptr};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
        bool m_dragging{false};
        bool m_over_bar{false};
        // How far into the bar the pointer was when it was taken, so the bar does not jump.
        std::int32_t m_grab{0};
    };
}
