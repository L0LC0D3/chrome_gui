#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>

#include <cstdint>

namespace chrome
{
    // Where the dial points at zero degrees.
    enum class angle_zero : std::uint8_t
    {
        east,
        north,
        west,
        south
    };

    // An angle in degrees as a round dial with a needle, and a number field beside it for typing the angle. The needle
    // follows the mouse while the dial is held down, from anywhere in the panel, and Shift snaps it to steps of 15 degrees
    // (or to the snap step, if one is set). The arc from zero to the needle is filled, and a tick marks each 45 degrees.
    // A double click puts the angle back to its default.
    //
    // The angle wraps round, into -180 to 180 or into 0 to 360. With limits it stops at them instead, as a hinge does,
    // and the dial shows the arc it can reach. With the focus, the arrows turn it a degree (Shift 15, Ctrl a tenth),
    // Page Up and Down 15, Home puts it back to its default and Enter types into the number field.
    //
    //   angle_dial yaw;
    //   yaw.set_label("Yaw").set_zero(angle_zero::north).set_clockwise(true).set_on_change(&turned, this);
    //   yaw.set_angle(90.0);
    //   set.add(yaw);
    class angle_dial final : public control
    {
    public:
        // Called after the user turned the dial or typed an angle. Setting the angle from code does not call it.
        using change_fn = void (*)(void* user, angle_dial& source) noexcept;

        angle_dial() noexcept;
        angle_dial(const angle_dial&) = delete;
        angle_dial& operator=(const angle_dial&) = delete;

        // Wrapped or kept inside the limits, and snapped to the step.
        angle_dial& set_angle(double degrees) noexcept;
        [[nodiscard]] double angle() const noexcept { return m_angle; }
        [[nodiscard]] double radians() const noexcept;

        // true for -180 to 180, the default, false for 0 to 360. Without limits only.
        angle_dial& set_signed(bool signed_range) noexcept;
        // The angle stops at these instead of wrapping. They are degrees, and minimum has to be below maximum.
        angle_dial& set_limits(double minimum, double maximum) noexcept;
        angle_dial& clear_limits() noexcept;
        [[nodiscard]] bool limited() const noexcept { return m_limited; }
        angle_dial& set_zero(angle_zero zero) noexcept { m_zero = zero; invalidate(); return *this; }
        // Whether the angle grows clockwise on the screen. On by default, like a compass.
        angle_dial& set_clockwise(bool clockwise) noexcept { m_clockwise = clockwise; invalidate(); return *this; }
        // Every angle is rounded to a multiple of this. 0, the default, leaves it free.
        angle_dial& set_step(double degrees) noexcept { m_step = degrees > 0.0 ? degrees : 0.0; return *this; }
        // What a double click and Home put the angle back to.
        angle_dial& set_default_angle(double degrees) noexcept { m_default = degrees; return *this; }
        angle_dial& set_label(const char* label) noexcept { m_label = label; return *this; }
        angle_dial& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The number field beside the dial. On by default.
        angle_dial& set_show_value(bool show) noexcept { m_show_value = show; return *this; }
        angle_dial& set_name(const char* name) noexcept { m_name = name; return *this; }
        angle_dial& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        angle_dial& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
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
            dial,
            // A press inside the number field, which belongs to it.
            field
        };

        void place() const noexcept;
        [[nodiscard]] dock_rect dial_area() const noexcept;
        [[nodiscard]] dock_rect field_area() const noexcept;
        // The angle brought inside the range: wrapped, or kept between the limits.
        [[nodiscard]] double normalise(double degrees) const noexcept;
        // The direction of an angle on the screen, in radians, with y down.
        [[nodiscard]] double screen_radians(double degrees) const noexcept;
        // The angle the dial would have if it pointed at a screen point.
        [[nodiscard]] double angle_at(std::int32_t x, std::int32_t y) const noexcept;
        // Sets the angle from the user's hands. true when it changed.
        bool turn_to(double degrees, bool snap) noexcept;
        void notify() noexcept;
        void sync() noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;
        static void field_changed(void* user, number_field& source) noexcept;

        double m_angle{0.0};
        double m_default{0.0};
        double m_step{0.0};
        double m_limit_min{-90.0};
        double m_limit_max{90.0};
        bool m_limited{false};
        bool m_signed{true};
        bool m_clockwise{true};
        angle_zero m_zero{angle_zero::east};
        bool m_show_value{true};
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_label_width{90};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};

        drag_kind m_drag{drag_kind::none};
        std::int32_t m_x{0};
        std::int32_t m_y{0};

        mutable number_field m_field;
        mutable control_set m_inner;
    };
}
