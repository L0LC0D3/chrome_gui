#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>

#include <cstdint>

namespace chrome
{
    // What int_slider and float_slider have in common, and it works as Unity's slider does: a track with a thumb, and a
    // number field at the right that shows the value and takes a typed one. A click on the track puts the thumb there and
    // drags it, a click on the thumb itself drags without a jump. The arrows, Page Up and Down, Home and End move it, and
    // Enter edits the number. Use one of the two typed sliders, which carry the value as their own type.
    class slider : public control
    {
    public:
        // A fixed width at 96 dpi, with the value text in it.
        slider& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // The name and the tooltip are UTF-8 and have to outlive the slider.
        slider& set_name(const char* name) noexcept { m_name = name; return *this; }
        slider& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        // A label at the left, UTF-8, which has to outlive the slider. Dragging it sideways changes the value, as dragging
        // across the whole track would: Shift is ten times faster and Ctrl ten times slower. Null for none.
        slider& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The room for the label at 96 dpi, inside the width of the slider.
        slider& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The number field at the right of the track. On by default.
        slider& set_show_value(bool show) noexcept { m_show_value = show; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        // 0 at the minimum and 1 at the maximum.
        [[nodiscard]] double ratio() const noexcept { return m_max > m_min ? (m_value - m_min) / (m_max - m_min) : 0.0; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        // Arrows left and right over the thumb and the label, and while either is dragged.
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        // While the number is being edited its caret blinks.
        [[nodiscard]] bool animating() const noexcept override { return m_editing; }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        slider() noexcept = default;

        // Sets the value from the user's hands: snapped to the step and kept inside the range. true when it changed.
        bool move_to(double value) noexcept;
        // Sets the value from code. Nothing is called.
        void assign(double value) noexcept;
        // The value changed by the user.
        virtual void value_changed() noexcept = 0;
        // Brings the number field in line with the range, the step, the digits and the value. Call after changing any.
        void sync() noexcept;

        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

        // The digits after the point, in the number field.
        std::int32_t m_digits{0};
        double m_value{0.0};
        double m_min{0.0};
        double m_max{1.0};
        // 0 is continuous.
        double m_step{0.0};

    private:
        struct track_box
        {
            std::int32_t left{0};
            std::int32_t width{0};
        };

        [[nodiscard]] track_box track() const noexcept;
        [[nodiscard]] double snap(double value) const noexcept;
        bool drag_to(std::int32_t x) noexcept;
        [[nodiscard]] dock_rect field_area() const noexcept;
        [[nodiscard]] bool in_field(std::int32_t x) const noexcept;
        [[nodiscard]] bool in_label(std::int32_t x) const noexcept;
        [[nodiscard]] std::int32_t label_zone() const noexcept;
        [[nodiscard]] std::int32_t thumb_x() const noexcept;
        void begin_editing() noexcept;
        void end_editing() noexcept;
        // Looks after what the field asked for after a key: to give up the focus.
        void settle() noexcept;
        static void field_changed(void* user, number_field& source) noexcept;

        const char* m_name{nullptr};
        const char* m_label{nullptr};
        std::int32_t m_label_width{90};
        std::int32_t m_width{300};
        bool m_show_value{true};
        bool m_dragging{false};
        // The number is being edited, so the keyboard and the mouse in its box go to the field.
        bool m_editing{false};
        bool m_field_drag{false};
        // The label is being dragged: the value it would have without the step, and where the mouse was.
        bool m_scrubbing{false};
        double m_scrub_value{0.0};
        std::int32_t m_scrub_x{0};
        // Where on the thumb it was taken, so that it does not jump.
        std::int32_t m_grab{0};
        mutable number_field m_field;
    };

    // A slider over whole numbers.
    //
    //   int_slider quality;
    //   quality.set_range(0, 10).set_value(5).set_on_change(&changed, this);
    //   set.add(quality);
    class int_slider final : public slider
    {
    public:
        using change_fn = void (*)(void* user, int_slider& source) noexcept;

        // The value is kept inside the range. Setting it does not call the handler.
        int_slider& set_range(std::int32_t minimum, std::int32_t maximum) noexcept;
        int_slider& set_value(std::int32_t value) noexcept;
        // The distance between the values the thumb stops at, 1 or more.
        int_slider& set_step(std::int32_t step) noexcept;
        int_slider& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        int_slider& set_width(std::int32_t width) noexcept { slider::set_width(width); return *this; }
        int_slider& set_name(const char* name) noexcept { slider::set_name(name); return *this; }
        int_slider& set_label(const char* label) noexcept { slider::set_label(label); return *this; }
        int_slider& set_label_width(std::int32_t width) noexcept { slider::set_label_width(width); return *this; }
        int_slider& set_tooltip(const char* text) noexcept { slider::set_tooltip(text); return *this; }
        int_slider& set_show_value(bool show) noexcept { slider::set_show_value(show); return *this; }

        [[nodiscard]] std::int32_t value() const noexcept { return static_cast<std::int32_t>(m_value); }
        [[nodiscard]] std::int32_t minimum() const noexcept { return static_cast<std::int32_t>(m_min); }
        [[nodiscard]] std::int32_t maximum() const noexcept { return static_cast<std::int32_t>(m_max); }

        int_slider() noexcept { m_max = 100.0; m_step = 1.0; sync(); }

    protected:
        void value_changed() noexcept override;

    private:
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };

    // A slider over fractions.
    //
    //   float_slider volume;
    //   volume.set_range(0.0f, 1.0f).set_value(0.8f).set_decimals(2).set_on_change(&changed, this);
    class float_slider final : public slider
    {
    public:
        using change_fn = void (*)(void* user, float_slider& source) noexcept;

        float_slider& set_range(float minimum, float maximum) noexcept;
        float_slider& set_value(float value) noexcept;
        // The distance between the values the thumb stops at. 0 lets it stop anywhere.
        float_slider& set_step(float step) noexcept;
        // The digits after the point in the text at the right, 0 to 6.
        float_slider& set_decimals(std::int32_t decimals) noexcept;
        float_slider& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        float_slider& set_width(std::int32_t width) noexcept { slider::set_width(width); return *this; }
        float_slider& set_name(const char* name) noexcept { slider::set_name(name); return *this; }
        float_slider& set_label(const char* label) noexcept { slider::set_label(label); return *this; }
        float_slider& set_label_width(std::int32_t width) noexcept { slider::set_label_width(width); return *this; }
        float_slider& set_tooltip(const char* text) noexcept { slider::set_tooltip(text); return *this; }
        float_slider& set_show_value(bool show) noexcept { slider::set_show_value(show); return *this; }

        [[nodiscard]] float value() const noexcept { return static_cast<float>(m_value); }
        [[nodiscard]] float minimum() const noexcept { return static_cast<float>(m_min); }
        [[nodiscard]] float maximum() const noexcept { return static_cast<float>(m_max); }
        [[nodiscard]] std::int32_t decimals() const noexcept { return m_digits; }

        float_slider() noexcept { m_digits = 2; sync(); }

    protected:
        void value_changed() noexcept override;

    private:
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
