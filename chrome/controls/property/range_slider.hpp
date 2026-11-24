#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>

#include <cstdint>

namespace chrome
{
    // A slider with two thumbs for a span of values, a lowest and a highest, such as the distances a light reaches. The
    // span between the thumbs is filled, and a number field sits at each end of the track for typing either value.
    //
    // A click on a thumb drags it, a click on the track beside the span moves the nearer thumb there and drags it, and
    // a click on the filled span drags the whole span along, keeping its width. The thumbs never cross, and stay a
    // least gap apart if one is set. While a thumb is dragged a bubble above it shows its value. With the focus: Space
    // chooses the other thumb, the arrows move the chosen one, Page Up and Down ten steps, Home and End to its limit,
    // Shift and Ctrl make a step ten times larger or a tenth, and Enter types into its number field.
    //
    //   range_slider reach;
    //   reach.set_limits(0.0, 100.0).set_values(10.0, 60.0).set_label("Reach");
    //   reach.set_on_change(&changed, this);
    //   set.add(reach);
    class range_slider final : public control
    {
    public:
        // Called after the user moved a thumb or the span, or typed a value. Setting values from code does not call it.
        using change_fn = void (*)(void* user, range_slider& source) noexcept;

        range_slider() noexcept;
        range_slider(const range_slider&) = delete;
        range_slider& operator=(const range_slider&) = delete;

        // The ends of the track. The values are kept inside them.
        range_slider& set_limits(double minimum, double maximum) noexcept;
        // The lowest and the highest value, kept in order inside the limits and snapped to the step.
        range_slider& set_values(double low, double high) noexcept;
        // The distance between the values a thumb stops at. 0, the default, lets it stop anywhere.
        range_slider& set_step(double step) noexcept;
        // The least distance between the two thumbs. 0 lets them meet.
        range_slider& set_min_gap(double gap) noexcept;
        // The digits after the point in the number fields, 0 to 6.
        range_slider& set_decimals(std::int32_t decimals) noexcept;
        range_slider& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The room for the label at 96 dpi.
        range_slider& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The width of the whole control, with the label, at 96 dpi.
        range_slider& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // The number fields at the ends. On by default.
        range_slider& set_show_values(bool show) noexcept { m_show_values = show; return *this; }
        range_slider& set_name(const char* name) noexcept { m_name = name; return *this; }
        range_slider& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        range_slider& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] double low() const noexcept { return m_low; }
        [[nodiscard]] double high() const noexcept { return m_high; }
        [[nodiscard]] double minimum() const noexcept { return m_min; }
        [[nodiscard]] double maximum() const noexcept { return m_max; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        // The bubble with the value, over the controls near the thumb that is dragged.
        [[nodiscard]] bool overlay_open() const noexcept override { return m_drag == drag_kind::low || m_drag == drag_kind::high; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_inner.animating(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            low,
            high,
            // The whole span, by its filled part.
            span,
            // A press inside a number field, which belongs to it.
            field
        };

        struct track_box
        {
            std::int32_t left{0};
            std::int32_t width{0};
        };

        void place() const noexcept;
        [[nodiscard]] track_box track() const noexcept;
        [[nodiscard]] dock_rect low_field_area() const noexcept;
        [[nodiscard]] dock_rect high_field_area() const noexcept;
        [[nodiscard]] std::int32_t x_of(double value) const noexcept;
        [[nodiscard]] double value_at(std::int32_t x) const noexcept;
        [[nodiscard]] double snap(double value) const noexcept;
        // Moves a thumb to a value, inside the other thumb and the limits. true when it moved.
        bool move_low(double value) noexcept;
        bool move_high(double value) noexcept;
        // Moves both by the same distance, kept inside the limits.
        bool move_span(double low) noexcept;
        void notify() noexcept;
        // Brings the number fields in line with the values and what each may be.
        void sync() noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] bool in_fields(std::int32_t x) const noexcept;
        [[nodiscard]] double unit() const noexcept;
        static void field_changed(void* user, number_field& source) noexcept;

        double m_min{0.0};
        double m_max{1.0};
        double m_low{0.25};
        double m_high{0.75};
        double m_step{0.0};
        double m_gap{0.0};
        std::int32_t m_digits{2};
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_label_width{90};
        std::int32_t m_width{320};
        bool m_show_values{true};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};

        drag_kind m_drag{drag_kind::none};
        // The thumb the keys move: 0 for the low one, 1 for the high one.
        std::int32_t m_active{0};
        std::int32_t m_hover{-1};
        // Where on a thumb it was taken, so it does not jump, and the span's low end where its drag began.
        std::int32_t m_grab{0};
        double m_grab_low{0.0};
        double m_grab_value{0.0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};

        mutable number_field m_fields[2];
        mutable control_set m_inner;
    };
}
