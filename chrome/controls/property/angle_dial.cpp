#include <chrome/controls/property/angle_dial.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr double k_pi = 3.14159265358979323846;
        constexpr std::int32_t k_dial = 40;
        constexpr std::int32_t k_height = 48;
        constexpr std::int32_t k_gap = 10;
        constexpr std::int32_t k_field_width = 64;
        constexpr std::int32_t k_field_height = 20;
        // The room after the field for the degree sign.
        constexpr std::int32_t k_unit_width = 14;
        constexpr double k_shift_step = 15.0;

        [[nodiscard]] double to_radians(double degrees) noexcept { return degrees * k_pi / 180.0; }
    }

    angle_dial::angle_dial() noexcept
    {
        m_field.set_buttons(false).set_decimals(1).set_range(-1.0e6, 1.0e6).set_step(1.0).set_on_change(&angle_dial::field_changed, this);
        const bool added = m_inner.add(m_field);
        (void)added;
        sync();
    }

    double angle_dial::radians() const noexcept
    {
        return to_radians(m_angle);
    }

    double angle_dial::normalise(double degrees) const noexcept
    {
        if (m_limited)
        {
            // The turn that lands nearest the middle of the arc, so going past the far side does not jump to the near one.
            const double middle = (m_limit_min + m_limit_max) * 0.5;
            degrees -= 360.0 * std::round((degrees - middle) / 360.0);
            return std::clamp(degrees, m_limit_min, m_limit_max);
        }

        return m_signed ? degrees - 360.0 * std::floor((degrees + 180.0) / 360.0) : degrees - 360.0 * std::floor(degrees / 360.0);
    }

    angle_dial& angle_dial::set_angle(double degrees) noexcept
    {
        if (m_step > 0.0)
            degrees = std::round(degrees / m_step) * m_step;

        m_angle = normalise(degrees);
        sync();
        invalidate();
        return *this;
    }

    angle_dial& angle_dial::set_signed(bool signed_range) noexcept
    {
        m_signed = signed_range;
        return set_angle(m_angle);
    }

    angle_dial& angle_dial::set_limits(double minimum, double maximum) noexcept
    {
        m_limit_min = std::min(minimum, maximum);
        m_limit_max = std::max(minimum, maximum);
        m_limited = m_limit_max > m_limit_min;
        return set_angle(m_angle);
    }

    angle_dial& angle_dial::clear_limits() noexcept
    {
        m_limited = false;
        return set_angle(m_angle);
    }

    void angle_dial::sync() noexcept
    {
        (void)m_field.set_value(m_angle);
    }

    double angle_dial::screen_radians(double degrees) const noexcept
    {
        double zero = 0.0;

        switch (m_zero)
        {
        case angle_zero::east: zero = 0.0; break;
        case angle_zero::north: zero = -k_pi * 0.5; break;
        case angle_zero::west: zero = k_pi; break;
        case angle_zero::south: zero = k_pi * 0.5; break;
        }

        // On the screen y grows downward, so a turn that looks clockwise has a growing angle.
        return zero + (m_clockwise ? 1.0 : -1.0) * to_radians(degrees);
    }

    dock_rect angle_dial::dial_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) + draw::px(k_gap) : 0;
        const std::int32_t size = std::min(draw::px(k_dial), box.height);
        return {box.x + label, box.y + (box.height - size) / 2, size, size};
    }

    dock_rect angle_dial::field_area() const noexcept
    {
        const dock_rect dial = dial_area();
        const dock_rect& box = area();
        const std::int32_t height = std::min(draw::px(k_field_height), box.height);
        return {dial.x + dial.width + draw::px(k_gap), box.y + (box.height - height) / 2, draw::px(k_field_width), height};
    }

    void angle_dial::place() const noexcept
    {
        m_field.set_area(field_area());
        m_field.set_visible(m_show_value);
        m_field.set_enabled(enabled());
    }

    double angle_dial::angle_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect dial = dial_area();
        const double dx = static_cast<double>(x) - (static_cast<double>(dial.x) + static_cast<double>(dial.width) * 0.5);
        const double dy = static_cast<double>(y) - (static_cast<double>(dial.y) + static_cast<double>(dial.height) * 0.5);

        // Right on the centre there is no direction to take, so the angle stays.
        if (std::fabs(dx) < 1.0 && std::fabs(dy) < 1.0)
            return m_angle;

        const double screen = std::atan2(dy, dx);
        const double zero = screen_radians(0.0);
        const double turn = (m_clockwise ? 1.0 : -1.0) * (screen - zero);
        return turn * 180.0 / k_pi;
    }

    bool angle_dial::turn_to(double degrees, bool snap) noexcept
    {
        const double step = m_step > 0.0 ? m_step : snap ? k_shift_step : 0.0;

        if (step > 0.0)
            degrees = std::round(degrees / step) * step;

        degrees = normalise(degrees);

        if (degrees == m_angle)
            return false;

        m_angle = degrees;
        notify();
        return true;
    }

    void angle_dial::notify() noexcept
    {
        sync();
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void angle_dial::field_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<angle_dial*>(user);

        if (!self.turn_to(source.value(), false))
            self.sync();
    }

    panel_mouse angle_dial::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    control_size angle_dial::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) + draw::px(k_gap) : 0;
        const std::int32_t value = m_show_value ? draw::px(k_gap) + draw::px(k_field_width) + draw::px(k_unit_width) : 0;
        return {label + draw::px(k_dial) + value, draw::px(k_height)};
    }

    void angle_dial::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        const dock_rect field = field_area();

        if (m_show_value && x >= field.x - draw::px(k_gap) / 2)
        {
            (void)m_inner.on_mouse(mouse_at(mouse_event::down, x, y));
            m_drag = drag_kind::field;
            invalidate();
            return;
        }

        m_inner.blur();
        const dock_rect dial = dial_area();

        // Only the dial itself, and a little round it, turns it. The label is not a handle.
        if (x < dial.x - draw::px(4) || x > dial.x + dial.width + draw::px(4))
            return;

        m_drag = drag_kind::dial;
        (void)turn_to(angle_at(x, y), shift_down());
        invalidate();
    }

    void angle_dial::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        const dock_rect field = field_area();

        if (m_show_value && x >= field.x - draw::px(k_gap) / 2)
        {
            m_drag = drag_kind::field;
            (void)m_inner.on_mouse(mouse_at(mouse_event::double_click, x, y));
            invalidate();
            return;
        }

        (void)turn_to(m_default, false);
        m_drag = drag_kind::none;
    }

    bool angle_dial::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;

        if (m_drag == drag_kind::field)
        {
            (void)m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
            return true;
        }

        if (m_drag != drag_kind::dial)
            return false;

        return turn_to(angle_at(x, y), shift_down());
    }

    bool angle_dial::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        return m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
    }

    void angle_dial::on_release() noexcept
    {
        if (m_drag == drag_kind::field)
            (void)m_inner.on_mouse(mouse_at(mouse_event::up, m_x, m_y));

        m_drag = drag_kind::none;
        invalidate();
    }

    void angle_dial::on_leave() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
        invalidate();
    }

    void angle_dial::on_focus(bool gained) noexcept
    {
        if (!gained)
        {
            m_inner.blur();
            m_drag = drag_kind::none;
        }

        invalidate();
    }

    void angle_dial::on_char(std::uint32_t code) noexcept
    {
        m_inner.on_char(code);
        invalidate();
    }

    bool angle_dial::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        place();

        if (m_inner.focused() != nullptr)
        {
            const bool used = m_inner.on_key(key, ctrl, shift, alt);
            invalidate();
            return used;
        }

        if (alt)
            return false;

        const double step = m_step > 0.0 ? m_step : 1.0;
        const double amount = step * (shift ? k_shift_step : ctrl ? 0.1 : 1.0);

        switch (key)
        {
        case VK_RETURN:
        case VK_F2:
            m_inner.set_focus(&m_field);
            invalidate();
            return true;
        case VK_LEFT:
        case VK_DOWN:
            (void)turn_to(m_angle - amount, false);
            return true;
        case VK_RIGHT:
        case VK_UP:
            (void)turn_to(m_angle + amount, false);
            return true;
        case VK_NEXT:
            (void)turn_to(m_angle - k_shift_step, false);
            return true;
        case VK_PRIOR:
            (void)turn_to(m_angle + k_shift_step, false);
            return true;
        case VK_HOME:
            (void)turn_to(m_default, false);
            return true;
        default:
            return false;
        }
    }

    bool angle_dial::animate(std::uint64_t now_ms) noexcept
    {
        place();
        const bool more = m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return more;
    }

    void angle_dial::paint(void* dc) const noexcept
    {
        place();
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool live = enabled();
        const bool lit = focused() && m_inner.focused() == nullptr;
        const bool dragging = m_drag == drag_kind::dial;

        std::uint32_t face = blend_colour(colours.panel, black, hot() && live ? 0.15f : 0.28f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.text_dim, colours.panel, 0.4f);
        std::uint32_t tick = blend_colour(colours.text_dim, colours.panel, 0.35f);
        std::uint32_t arc = colours.accent;
        std::uint32_t needle = dragging ? white : colours.text;
        std::uint32_t ink = colours.text;

        if (!live)
        {
            face = blend_colour(colours.panel, colours.title, 0.4f);
            edge = blend_colour(colours.panel, black, 0.2f);
            tick = blend_colour(colours.text_dim, colours.panel, 0.7f);
            arc = blend_colour(colours.accent, colours.panel, 0.6f);
            needle = blend_colour(colours.text_dim, colours.panel, 0.45f);
            ink = needle;
        }

        const dock_rect& box = area();

        if (m_label != nullptr)
            draw::text(dc, {box.x, box.y, draw::px(m_label_width), box.height}, m_label, ink, draw::text_ellipsis);

        const dock_rect dial = dial_area();
        const float u = static_cast<float>(draw::px(1));
        const float cx = static_cast<float>(dial.x) + static_cast<float>(dial.width) * 0.5f;
        const float cy = static_cast<float>(dial.y) + static_cast<float>(dial.height) * 0.5f;
        const float radius = static_cast<float>(dial.width) * 0.5f;
        // Drawn at four times the size and shrunk, which smooths the small round parts far better than drawing them as they are.
        const std::int32_t margin = draw::px(2);
        draw::layer art(dc, {dial.x - margin, dial.y - margin, dial.width + 2 * margin, dial.height + 2 * margin});
        art.disc(cx, cy, radius - 0.5f * u, face, edge, 1.5f * u);

        const auto point_at = [&](double degrees, float r) {
            const double a = screen_radians(degrees);
            return draw::point{cx + static_cast<float>(std::cos(a)) * r, cy + static_cast<float>(std::sin(a)) * r};
        };

        // A sweep of the dial along an arc, from one angle to another.
        const auto sweep = [&](double from, double to, float r, std::uint32_t colour, float width) {
            const double span = to - from;
            const std::uint32_t count = static_cast<std::uint32_t>(std::clamp(std::fabs(span) / 6.0, 1.0, 60.0)) + 1;
            draw::point points[64];

            for (std::uint32_t i = 0; i < count; ++i)
                points[i] = point_at(from + span * static_cast<double>(i) / static_cast<double>(count - 1), r);

            art.polyline(points, count, colour, width);
        };

        // With limits, the arc the needle can reach is drawn as a track.
        if (m_limited)
            sweep(m_limit_min, m_limit_max, radius - 4.5f * u, blend_colour(colours.text_dim, colours.panel, 0.55f), 2.0f * u);

        // A tick at each 45 degrees, longer at the four sides, and the one at zero lit.
        for (std::int32_t i = 0; i < 8; ++i)
        {
            const double degrees = i * 45.0;
            const float length = i % 2 == 0 ? 5.0f : 3.0f;
            const draw::point ends[2] = {point_at(degrees, radius - 1.5f * u), point_at(degrees, radius - (1.5f + length) * u)};
            art.polyline(ends, 2, i == 0 ? blend_colour(tick, white, 0.45f) : tick, i == 0 ? 2.0f * u : 1.5f * u);
        }

        // From zero to the needle.
        if (std::fabs(m_angle) > 0.01)
            sweep(0.0, m_angle, radius - 8.5f * u, arc, 3.0f * u);

        const draw::point tip = point_at(m_angle, radius - 6.0f * u);
        const draw::point shaft[2] = {{cx, cy}, tip};
        art.polyline(shaft, 2, needle, 2.0f * u);
        const float knob = (dragging ? 4.5f : 3.75f) * u;
        art.disc(tip.x, tip.y, knob, dragging ? white : arc, dragging ? arc : blend_colour(arc, black, 0.3f), 1.5f * u);
        art.disc(cx, cy, 2.75f * u, needle, needle, 0.0f);
        art.flush();

        if (m_show_value)
        {
            m_inner.paint(dc);
            const dock_rect field = field_area();
            draw::text(dc, {field.x + field.width + draw::px(3), field.y, draw::px(k_unit_width), field.height}, "\xC2\xB0", colours.text_dim);
        }
    }
}
