#include <chrome/controls/property/range_slider.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 26;
        constexpr std::int32_t k_track = 4;
        constexpr std::int32_t k_thumb = 14;
        constexpr std::int32_t k_field_width = 56;
        constexpr std::int32_t k_field_height = 20;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_bubble_width = 54;
        constexpr std::int32_t k_bubble_height = 20;

        // Which part of the track a point is on: 0 the low thumb, 1 the high one, 2 the span between them, -1 neither. When
        // the thumbs are on top of each other the side of the point decides, except at an end of the limits, where only one
        // of them can move away.
        [[nodiscard]] std::int32_t part_for(std::int32_t x, std::int32_t low_x, std::int32_t high_x, std::int32_t reach, bool low_stuck, bool high_stuck) noexcept
        {
            const bool near_low = std::abs(x - low_x) <= reach;
            const bool near_high = std::abs(x - high_x) <= reach;

            if (near_low && near_high)
            {
                if (low_stuck)
                    return 1;

                if (high_stuck)
                    return 0;

                if (low_x == high_x)
                    return x < low_x ? 0 : 1;

                return std::abs(x - low_x) <= std::abs(x - high_x) ? 0 : 1;
            }

            if (near_low)
                return 0;

            if (near_high)
                return 1;

            return x > low_x && x < high_x ? 2 : -1;
        }
    }

    range_slider::range_slider() noexcept
    {
        for (number_field& field : m_fields)
        {
            field.set_buttons(false).set_on_change(&range_slider::field_changed, this);
            const bool added = m_inner.add(field);
            (void)added;
        }

        sync();
    }

    double range_slider::unit() const noexcept
    {
        return m_step > 0.0 ? m_step : std::max((m_max - m_min) / 100.0, 1.0e-9);
    }

    double range_slider::snap(double value) const noexcept
    {
        if (m_step > 0.0)
            value = m_min + std::round((value - m_min) / m_step) * m_step;

        return std::clamp(value, m_min, m_max);
    }

    void range_slider::sync() noexcept
    {
        const double gap = std::min(m_gap, m_max - m_min);
        (void)m_fields[0].set_range(m_min, std::max(m_high - gap, m_min)).set_decimals(m_digits).set_step(unit());
        (void)m_fields[1].set_range(std::min(m_low + gap, m_max), m_max).set_decimals(m_digits).set_step(unit());
        (void)m_fields[0].set_value(m_low);
        (void)m_fields[1].set_value(m_high);
    }

    range_slider& range_slider::set_limits(double minimum, double maximum) noexcept
    {
        m_min = std::min(minimum, maximum);
        m_max = std::max(minimum, maximum);

        if (m_max <= m_min)
            m_max = m_min + 1.0;

        m_low = std::clamp(m_low, m_min, m_max);
        m_high = std::clamp(m_high, m_low, m_max);
        sync();
        invalidate();
        return *this;
    }

    range_slider& range_slider::set_values(double low, double high) noexcept
    {
        if (low > high)
            std::swap(low, high);

        m_low = snap(low);
        m_high = std::max(snap(high), m_low);
        sync();
        invalidate();
        return *this;
    }

    range_slider& range_slider::set_step(double step) noexcept
    {
        m_step = step > 0.0 ? step : 0.0;
        return set_values(m_low, m_high);
    }

    range_slider& range_slider::set_min_gap(double gap) noexcept
    {
        m_gap = std::max(gap, 0.0);
        sync();
        return *this;
    }

    range_slider& range_slider::set_decimals(std::int32_t decimals) noexcept
    {
        m_digits = std::clamp(decimals, 0, 6);
        sync();
        invalidate();
        return *this;
    }

    control_size range_slider::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width), draw::px(k_height)};
    }

    range_slider::track_box range_slider::track() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) + draw::px(k_gap) : 0;
        const std::int32_t fields = m_show_values ? draw::px(k_field_width) + draw::px(k_gap) : 0;
        const std::int32_t half = draw::px(k_thumb) / 2;
        return {box.x + label + fields + half, std::max(box.width - label - 2 * fields - 2 * half, 1)};
    }

    dock_rect range_slider::low_field_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) + draw::px(k_gap) : 0;
        const std::int32_t height = std::min(draw::px(k_field_height), box.height);
        return {box.x + label, box.y + (box.height - height) / 2, draw::px(k_field_width), height};
    }

    dock_rect range_slider::high_field_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t height = std::min(draw::px(k_field_height), box.height);
        return {box.x + box.width - draw::px(k_field_width), box.y + (box.height - height) / 2, draw::px(k_field_width), height};
    }

    bool range_slider::in_fields(std::int32_t x) const noexcept
    {
        const dock_rect low = low_field_area();
        const dock_rect high = high_field_area();
        const std::int32_t half_gap = draw::px(k_gap) / 2;
        return m_show_values && (x < low.x + low.width + half_gap || x >= high.x - half_gap);
    }

    void range_slider::place() const noexcept
    {
        m_fields[0].set_area(low_field_area());
        m_fields[1].set_area(high_field_area());
        m_fields[0].set_visible(m_show_values);
        m_fields[1].set_visible(m_show_values);
        m_fields[0].set_enabled(enabled());
        m_fields[1].set_enabled(enabled());
    }

    std::int32_t range_slider::x_of(double value) const noexcept
    {
        const track_box box = track();
        const double ratio = m_max > m_min ? (value - m_min) / (m_max - m_min) : 0.0;
        return box.left + static_cast<std::int32_t>(static_cast<double>(box.width) * ratio + 0.5);
    }

    double range_slider::value_at(std::int32_t x) const noexcept
    {
        const track_box box = track();
        const double ratio = std::clamp(static_cast<double>(x - box.left) / static_cast<double>(box.width), 0.0, 1.0);
        return m_min + ratio * (m_max - m_min);
    }

    bool range_slider::move_low(double value) noexcept
    {
        value = std::clamp(snap(value), m_min, std::max(m_high - m_gap, m_min));

        if (value == m_low)
            return false;

        m_low = value;
        return true;
    }

    bool range_slider::move_high(double value) noexcept
    {
        value = std::clamp(snap(value), std::min(m_low + m_gap, m_max), m_max);

        if (value == m_high)
            return false;

        m_high = value;
        return true;
    }

    bool range_slider::move_span(double low) noexcept
    {
        const double span = m_grab_value;
        low = std::clamp(snap(low), m_min, std::max(m_max - span, m_min));

        if (low == m_low)
            return false;

        m_low = low;
        m_high = std::min(low + span, m_max);
        return true;
    }

    void range_slider::notify() noexcept
    {
        sync();
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void range_slider::field_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<range_slider*>(user);
        const bool moved = &source == &self.m_fields[0] ? self.move_low(source.value()) : self.move_high(source.value());

        if (moved)
            self.notify();
        else
            self.sync();
    }

    panel_mouse range_slider::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    mouse_cursor range_slider::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        (void)y;

        if (m_drag == drag_kind::low || m_drag == drag_kind::high || m_drag == drag_kind::span)
            return mouse_cursor::resize_horizontal;

        if (!enabled() || m_drag == drag_kind::field || in_fields(x))
            return mouse_cursor::arrow;

        const std::int32_t reach = draw::px(k_thumb) / 2 + draw::px(2);
        const std::int32_t part = part_for(x, x_of(m_low), x_of(m_high), reach, m_low >= m_max, m_high <= m_min);
        return part >= 0 ? mouse_cursor::resize_horizontal : mouse_cursor::arrow;
    }

    void range_slider::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        m_drag = drag_kind::none;

        if (in_fields(x))
        {
            (void)m_inner.on_mouse(mouse_at(mouse_event::down, x, y));
            m_drag = drag_kind::field;
            invalidate();
            return;
        }

        m_inner.blur();
        const std::int32_t low_x = x_of(m_low);
        const std::int32_t high_x = x_of(m_high);
        const std::int32_t reach = draw::px(k_thumb) / 2 + draw::px(2);
        std::int32_t part = part_for(x, low_x, high_x, reach, m_low >= m_max, m_high <= m_min);

        if (part == 2)
        {
            m_drag = drag_kind::span;
            m_grab = x;
            m_grab_low = m_low;
            m_grab_value = m_high - m_low;
            invalidate();
            return;
        }

        // Not on a thumb: the nearer one jumps to the point, and is then dragged from there.
        if (part < 0)
        {
            part = x < low_x ? 0 : 1;
            m_grab = 0;
        }
        else
        {
            // Taking a thumb itself does not move it.
            m_grab = x - (part == 0 ? low_x : high_x);
        }

        m_active = part;
        m_drag = part == 0 ? drag_kind::low : drag_kind::high;

        if (part == 0 ? move_low(value_at(x - m_grab)) : move_high(value_at(x - m_grab)))
            notify();

        invalidate();
    }

    void range_slider::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        place();

        if (!in_fields(x))
        {
            on_press(x, y);
            return;
        }

        m_drag = drag_kind::field;
        (void)m_inner.on_mouse(mouse_at(mouse_event::double_click, x, y));
        invalidate();
    }

    bool range_slider::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;

        if (m_drag == drag_kind::field)
        {
            (void)m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
            return true;
        }

        bool moved = false;

        if (m_drag == drag_kind::low)
            moved = move_low(value_at(x - m_grab));
        else if (m_drag == drag_kind::high)
            moved = move_high(value_at(x - m_grab));
        else if (m_drag == drag_kind::span)
            moved = move_span(m_grab_low + static_cast<double>(x - m_grab) / static_cast<double>(track().width) * (m_max - m_min));
        else
            return false;

        if (moved)
            notify();

        // The bubble follows the thumb even when it stopped at a limit.
        return true;
    }

    bool range_slider::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        bool changed = m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
        std::int32_t hover = -1;

        if (enabled() && !in_fields(x))
        {
            const std::int32_t reach = draw::px(k_thumb) / 2 + draw::px(2);
            hover = part_for(x, x_of(m_low), x_of(m_high), reach, m_low >= m_max, m_high <= m_min);
        }

        if (hover != m_hover)
        {
            m_hover = hover;
            changed = true;
        }

        return changed;
    }

    void range_slider::on_release() noexcept
    {
        if (m_drag == drag_kind::field)
            (void)m_inner.on_mouse(mouse_at(mouse_event::up, m_x, m_y));

        m_drag = drag_kind::none;
        invalidate();
    }

    void range_slider::on_leave() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
        m_hover = -1;
        invalidate();
    }

    void range_slider::on_focus(bool gained) noexcept
    {
        if (!gained)
        {
            m_inner.blur();
            m_drag = drag_kind::none;
        }

        invalidate();
    }

    void range_slider::on_char(std::uint32_t code) noexcept
    {
        m_inner.on_char(code);
        invalidate();
    }

    bool range_slider::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
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

        const double step = unit() * (shift ? 10.0 : ctrl ? 0.1 : 1.0);
        const bool low = m_active == 0;
        const double now = low ? m_low : m_high;
        bool moved = false;

        const auto move = [&](double value) { moved = low ? move_low(value) : move_high(value); };

        switch (key)
        {
        case VK_SPACE:
            m_active ^= 1;
            invalidate();
            return true;
        case VK_RETURN:
        case VK_F2:
            m_inner.set_focus(&m_fields[m_active]);
            invalidate();
            return true;
        case VK_LEFT:
        case VK_DOWN:
            move(now - step);
            break;
        case VK_RIGHT:
        case VK_UP:
            move(now + step);
            break;
        case VK_NEXT:
            move(now - 10.0 * unit());
            break;
        case VK_PRIOR:
            move(now + 10.0 * unit());
            break;
        case VK_HOME:
            move(m_min);
            break;
        case VK_END:
            move(m_max);
            break;
        default:
            return false;
        }

        if (moved)
            notify();

        return true;
    }

    bool range_slider::animate(std::uint64_t now_ms) noexcept
    {
        place();
        const bool more = m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return more;
    }

    dock_rect range_slider::overlay_area() const noexcept
    {
        const std::int32_t width = draw::px(k_bubble_width);
        const std::int32_t height = draw::px(k_bubble_height);
        const std::int32_t at = x_of(m_drag == drag_kind::low ? m_low : m_high);
        const dock_rect& box = area();
        const std::int32_t x = std::clamp(at - width / 2, box.x, std::max(box.x + box.width - width, box.x));
        return {x, box.y - height - draw::px(1), width, height};
    }

    void range_slider::paint_overlay(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect bubble = overlay_area();
        char text[32];
        std::snprintf(text, sizeof(text), "%.*f", m_digits, m_drag == drag_kind::low ? m_low : m_high);
        draw::rounded(dc, bubble, 4, colours.accent, blend_colour(colours.accent, 0x000000, 0.3f));
        draw::text(dc, bubble, text, 0xFFFFFF, draw::text_center | draw::text_bold);

        // A small point down to the thumb.
        const float cx = static_cast<float>(x_of(m_drag == drag_kind::low ? m_low : m_high));
        const float y = static_cast<float>(bubble.y + bubble.height);
        const float u = static_cast<float>(draw::px(1));
        const draw::point point[3] = {{cx - 4.0f * u, y - u}, {cx, y + 3.0f * u}, {cx + 4.0f * u, y - u}};
        draw::polyline(dc, point, 3, colours.accent, 2.0f * u);
    }

    void range_slider::paint(void* dc) const noexcept
    {
        place();
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool live = enabled();
        const bool lit = focused() && m_inner.focused() == nullptr;
        const bool dragging = m_drag == drag_kind::low || m_drag == drag_kind::high || m_drag == drag_kind::span;

        std::uint32_t groove = blend_colour(colours.panel, black, 0.35f);
        std::uint32_t fill = m_drag == drag_kind::span ? blend_colour(colours.accent, white, 0.2f) : m_hover == 2 && hot() ? blend_colour(colours.accent, white, 0.1f) : colours.accent;
        std::uint32_t ink = colours.text;
        std::uint32_t tick = blend_colour(colours.text_dim, colours.panel, 0.55f);

        if (!live)
        {
            groove = blend_colour(colours.panel, colours.title, 0.4f);
            fill = blend_colour(colours.accent, colours.panel, 0.6f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            tick = groove;
        }

        const dock_rect& box = area();
        const track_box line = track();
        const std::int32_t thick = std::max(draw::px(k_track), 2);
        const std::int32_t thumb = draw::px(k_thumb);
        const std::int32_t mid = box.y + box.height / 2;
        const std::int32_t low_x = x_of(m_low);
        const std::int32_t high_x = x_of(m_high);

        if (m_label != nullptr)
            draw::text(dc, {box.x, box.y, draw::px(m_label_width), box.height}, m_label, ink, draw::text_ellipsis);

        draw::rounded(dc, {line.left, mid - thick / 2, line.width, thick}, thick / 2, groove, groove);

        // Marks at each quarter of the track, under it, so the thumbs can be read against something.
        for (std::int32_t i = 0; i <= 4; ++i)
        {
            const std::int32_t x = line.left + line.width * i / 4;
            draw::fill(dc, {x, mid + thumb / 2 + draw::px(1), 1, draw::px(i % 2 == 0 ? 3 : 2)}, tick);
        }

        if (high_x > low_x)
            draw::rounded(dc, {low_x, mid - thick / 2, high_x - low_x, thick}, thick / 2, fill, fill);

        // The thumbs. The one the keys move shows it with the focus, and the one under the mouse is lighter.
        for (std::int32_t i = 0; i < 2; ++i)
        {
            const std::int32_t x = i == 0 ? low_x : high_x;
            const bool taken = (i == 0 && m_drag == drag_kind::low) || (i == 1 && m_drag == drag_kind::high);
            const bool over = live && m_hover == i && hot();
            std::uint32_t knob = taken ? white : blend_colour(colours.text, white, over ? 0.5f : 0.0f);
            std::uint32_t edge = lit && m_active == i ? colours.accent : blend_colour(colours.text_dim, colours.panel, 0.4f);

            if (!live)
            {
                knob = blend_colour(colours.text_dim, colours.panel, 0.45f);
                edge = blend_colour(colours.panel, black, 0.2f);
            }

            const std::int32_t size = taken ? thumb + draw::px(2) : thumb;
            draw::ellipse(dc, {x - size / 2, mid - size / 2, size, size}, knob, edge);

            if (lit && m_active == i && live && !dragging)
                draw::ellipse(dc, {x - thumb / 4, mid - thumb / 4, thumb / 2, thumb / 2}, colours.accent, colours.accent);
        }

        if (m_show_values)
            m_inner.paint(dc);
    }
}
