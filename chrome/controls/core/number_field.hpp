#pragma once

#include <chrome/controls/core/text_field.hpp>

#include <cstdint>

namespace chrome
{
    // A text field that holds a number. It takes digits, a minus sign and a decimal point, and the value is decided when
    // Enter is pressed or the field loses the focus: the text is read, kept inside the range, and written back in the
    // field's own format. Text that is not a number goes back to the value it had. Escape goes back as well.
    //
    // Up and Down, and the two small buttons at the right edge, step the value. Shift steps ten times as far, Ctrl a tenth.
    //
    //   number_field scale;
    //   scale.set_range(0.0, 10.0).set_step(0.1).set_decimals(1).set_value(1.0).set_on_change(&changed, this);
    //   set.add(scale);
    class number_field final : public text_field
    {
        friend class slider;

    public:
        static constexpr std::int32_t k_buttons_width = 18;

        // Called after the value changed, by typing and committing, by a step, or by set_value from code when notify is true.
        using change_fn = void (*)(void* user, number_field& source) noexcept;

        number_field() noexcept;

        // The value is clamped to the range and rounded to the decimals. Setting it does not call the handler.
        number_field& set_value(double value) noexcept;
        number_field& set_range(double minimum, double maximum) noexcept;
        number_field& set_step(double step) noexcept { m_step = step > 0.0 ? step : 1.0; return *this; }
        // How many digits follow the point, 0 to 9. 0 makes it a whole number field.
        number_field& set_decimals(std::int32_t decimals) noexcept;
        number_field& set_on_change(change_fn handler, void* user) noexcept { m_on_change = handler; m_on_change_user = user; return *this; }
        // The two step buttons at the right edge. On by default. Up and Down still step.
        number_field& set_buttons(bool buttons) noexcept { m_buttons = buttons; reserve_right(buttons ? k_buttons_width : 0); return *this; }

        // A label at the left, UTF-8, which has to outlive the field. It is also the field's name. Dragging it sideways
        // changes the value by one step for each pixel: Shift ten times as much, Ctrl a tenth. Null for none.
        number_field& set_label(const char* label) noexcept;
        // The room for the label at 96 dpi, outside the width of the field.
        number_field& set_label_width(std::int32_t width) noexcept;
        number_field& set_placeholder(const char* text) noexcept { text_field::set_placeholder(text); return *this; }
        number_field& set_name(const char* name) noexcept { text_field::set_name(name); return *this; }
        number_field& set_tooltip(const char* text) noexcept { text_field::set_tooltip(text); return *this; }
        number_field& set_read_only(bool read_only) noexcept { text_field::set_read_only(read_only); return *this; }
        number_field& set_width(std::int32_t width) noexcept { text_field::set_width(width); return *this; }

        [[nodiscard]] double value() const noexcept { return m_value; }
        [[nodiscard]] double minimum() const noexcept { return m_min; }
        [[nodiscard]] double maximum() const noexcept { return m_max; }
        [[nodiscard]] double step() const noexcept { return m_step; }
        [[nodiscard]] std::int32_t decimals() const noexcept { return m_decimals; }

        // What the keys and buttons do, for code to do as well. direction is 1 or -1, and scale multiplies the step. false
        // when the value did not move: read-only, or already at the end of the range.
        bool step_by(std::int32_t direction, double scale = 1.0) noexcept;
        // Reads the text now, as Enter does. true when the value changed.
        bool commit() noexcept;

        // The handler and the filter of the text belong to the number field.
        number_field& set_on_action(action_fn, void*) = delete;
        number_field& set_filter(filter_fn, void*) = delete;
        number_field& set_text(const char*) = delete;
        number_field& set_password(bool) = delete;
        number_field& set_max_length(std::int32_t) = delete;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        // Arrows left and right over the label, and while it is dragged.
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        void paint(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        // Clamps and rounds the number, shows it, and calls the handler when it differs from the old value.
        bool apply(double value, bool notify) noexcept;
        void show() noexcept;
        [[nodiscard]] bool in_buttons(std::int32_t x) const noexcept;
        [[nodiscard]] bool in_label(std::int32_t x) const noexcept;
        void reserve_label() noexcept;

        static bool acted(void* user, text_field& source, text_action action) noexcept;
        static std::int32_t filter(void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept;

        double m_value{0.0};
        double m_min{-1.0e9};
        double m_max{1.0e9};
        double m_step{1.0};
        std::int32_t m_decimals{0};
        bool m_buttons{true};
        const char* m_label{nullptr};
        std::int32_t m_label_width{90};
        bool m_scrubbing{false};
        double m_scrub_value{0.0};
        std::int32_t m_scrub_x{0};
        change_fn m_on_change{nullptr};
        void* m_on_change_user{nullptr};
    };
}
