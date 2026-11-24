#include <chrome/controls/core/slider.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_track = 4;
        constexpr std::int32_t k_thumb = 14;
        // The room for the value at the right, and the gap before it, at 96 dpi.
        constexpr std::int32_t k_value_width = 60;
        constexpr std::int32_t k_field_height = 20;
        constexpr std::int32_t k_value_gap = 8;
    }

    control_size slider::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width), draw::px(k_height)};
    }

    slider::track_box slider::track() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t reserved = m_show_value ? draw::px(k_value_width) + draw::px(k_value_gap) : 0;
        const std::int32_t half = draw::px(k_thumb) / 2;
        const std::int32_t label = label_zone();
        return {box.x + label + half, std::max(box.width - label - reserved - 2 * half, 1)};
    }

    double slider::snap(double value) const noexcept
    {
        if (m_step > 0.0)
            value = m_min + std::round((value - m_min) / m_step) * m_step;

        return std::clamp(value, m_min, m_max);
    }

    bool slider::move_to(double value) noexcept
    {
        value = snap(value);

        if (value == m_value)
            return false;

        m_value = value;
        sync();
        invalidate();
        value_changed();
        return true;
    }

    void slider::assign(double value) noexcept
    {
        m_value = snap(value);
        sync();
        invalidate();
    }

    void slider::sync() noexcept
    {
        const double unit = m_step > 0.0 ? m_step : std::max((m_max - m_min) / 100.0, 1.0e-9);
        (void)m_field.set_range(m_min, m_max).set_decimals(m_digits).set_step(unit).set_buttons(false).set_on_change(&slider::field_changed, this);
        (void)m_field.set_value(m_value);
    }

    void slider::field_changed(void* user, number_field& source) noexcept
    {
        // What was typed is snapped like anything else, and the field then shows what was kept.
        auto& self = *static_cast<slider*>(user);
        (void)self.move_to(source.value());
        self.sync();
    }

    dock_rect slider::field_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t width = draw::px(k_value_width);
        const std::int32_t height = std::min(draw::px(k_field_height), box.height);
        return {box.x + box.width - width, box.y + (box.height - height) / 2, width, height};
    }

    std::int32_t slider::label_zone() const noexcept
    {
        return m_label != nullptr ? draw::px(m_label_width) + draw::px(k_value_gap) : 0;
    }

    bool slider::in_label(std::int32_t x) const noexcept
    {
        return m_label != nullptr && x < area().x + draw::px(m_label_width);
    }

    bool slider::in_field(std::int32_t x) const noexcept
    {
        return m_show_value && x >= field_area().x;
    }

    std::int32_t slider::thumb_x() const noexcept
    {
        const track_box box = track();
        return box.left + static_cast<std::int32_t>(static_cast<double>(box.width) * ratio() + 0.5);
    }

    void slider::begin_editing() noexcept
    {
        if (m_editing || !m_show_value)
            return;

        m_editing = true;
        m_field.set_area(field_area());
        m_field.m_focused = true;
        m_field.on_focus(true);
        invalidate();
    }

    void slider::end_editing() noexcept
    {
        if (!m_editing)
            return;

        m_editing = false;
        m_field_drag = false;
        m_field.m_focused = false;
        m_field.on_focus(false);
        sync();
        invalidate();
    }

    void slider::settle() noexcept
    {
        if (m_field.m_blur)
        {
            m_field.m_blur = false;
            end_editing();
        }
    }

    mouse_cursor slider::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_dragging || m_scrubbing)
            return mouse_cursor::resize_horizontal;

        if (m_editing || in_field(x))
            return mouse_cursor::arrow;

        const std::int32_t reach = draw::px(k_thumb) / 2;
        const bool on_thumb = std::abs(x - thumb_x()) <= reach && std::abs(y - (area().y + area().height / 2)) <= reach;
        return in_label(x) || on_thumb ? mouse_cursor::resize_horizontal : mouse_cursor::arrow;
    }

    bool slider::animate(std::uint64_t now_ms) noexcept
    {
        if (!m_editing)
            return false;

        (void)m_field.animate(now_ms);

        if (m_field.m_dirty)
        {
            m_field.m_dirty = false;
            invalidate();
        }

        return true;
    }

    bool slider::drag_to(std::int32_t x) noexcept
    {
        const track_box box = track();
        const double ratio = std::clamp(static_cast<double>(x - m_grab - box.left) / static_cast<double>(box.width), 0.0, 1.0);
        return move_to(m_min + ratio * (m_max - m_min));
    }

    void slider::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        if (in_field(x))
        {
            begin_editing();
            m_field_drag = true;
            m_field.on_press(x, y);
            return;
        }

        end_editing();

        if (in_label(x))
        {
            m_scrubbing = true;
            m_scrub_value = m_value;
            m_scrub_x = x;
            return;
        }

        m_dragging = true;
        // Taking the thumb itself does not move it. Anywhere else on the track the thumb jumps there.
        const std::int32_t offset = x - thumb_x();
        m_grab = std::abs(offset) <= draw::px(k_thumb) / 2 ? offset : 0;
        (void)drag_to(x);
    }

    void slider::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (!in_field(x))
        {
            on_press(x, y);
            return;
        }

        begin_editing();
        m_field_drag = false;
        m_field.on_double_click(x, y);
    }

    bool slider::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_field_drag)
            return m_field.on_drag(x, y);

        if (m_scrubbing)
        {
            // The same distance as across the track, so the label and the thumb agree.
            const double fast = GetKeyState(VK_SHIFT) < 0 ? 10.0 : GetKeyState(VK_CONTROL) < 0 ? 0.1 : 1.0;
            const double per_pixel = (m_max - m_min) / static_cast<double>(track().width) * fast;
            m_scrub_value = std::clamp(m_scrub_value + static_cast<double>(x - m_scrub_x) * per_pixel, m_min, m_max);
            m_scrub_x = x;
            return move_to(m_scrub_value);
        }

        return m_dragging && drag_to(x);
    }

    void slider::on_release() noexcept
    {
        if (m_field_drag)
            m_field.on_release();

        m_field_drag = false;
        m_dragging = false;
        m_scrubbing = false;
        invalidate();
    }

    void slider::on_char(std::uint32_t code) noexcept
    {
        if (m_editing)
            m_field.on_char(code);
    }

    void slider::on_focus(bool gained) noexcept
    {
        m_dragging = false;
        m_scrubbing = false;

        if (!gained)
            end_editing();

        invalidate();
    }

    bool slider::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (m_editing)
        {
            const bool used = m_field.on_key(key, ctrl, shift, alt);
            settle();
            return used;
        }

        if (alt)
            return false;

        // A slider without a step moves by a hundredth of its range.
        const double unit = m_step > 0.0 ? m_step : (m_max - m_min) / 100.0;

        switch (key)
        {
        case VK_RETURN:
        case VK_F2:
            begin_editing();
            return true;
        case VK_LEFT:
        case VK_DOWN:
            (void)move_to(m_value - unit);
            return true;
        case VK_RIGHT:
        case VK_UP:
            (void)move_to(m_value + unit);
            return true;
        case VK_NEXT:
            (void)move_to(m_value - 10.0 * unit);
            return true;
        case VK_PRIOR:
            (void)move_to(m_value + 10.0 * unit);
            return true;
        case VK_HOME:
            (void)move_to(m_min);
            return true;
        case VK_END:
            (void)move_to(m_max);
            return true;
        default:
            return false;
        }
    }

    void slider::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool hot = this->hot();
        const bool lit = focused();
        const bool live = enabled();

        std::uint32_t groove = blend_colour(colours.panel, black, 0.35f);
        std::uint32_t fill = m_dragging ? blend_colour(colours.accent, black, 0.2f) : colours.accent;
        std::uint32_t knob = m_dragging ? white : blend_colour(colours.text, white, hot ? 0.5f : 0.0f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.text_dim, colours.panel, 0.4f);
        std::uint32_t ink = colours.text;

        if (!live)
        {
            groove = blend_colour(colours.panel, colours.title, 0.4f);
            fill = blend_colour(colours.accent, colours.panel, 0.6f);
            knob = blend_colour(colours.text_dim, colours.panel, 0.45f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = knob;
        }

        const dock_rect& box = area();
        const track_box line = track();
        const std::int32_t thick = std::max(draw::px(k_track), 2);
        const std::int32_t thumb = draw::px(k_thumb);
        const std::int32_t mid = box.y + box.height / 2;
        const std::int32_t reach = static_cast<std::int32_t>(static_cast<double>(line.width) * ratio() + 0.5);

        if (m_label != nullptr)
        {
            const std::uint32_t label_ink = m_scrubbing || (hot && !live) ? ink : live ? blend_colour(colours.text, white, m_scrubbing ? 0.4f : 0.0f) : ink;
            draw::text(dc, {box.x, box.y, draw::px(m_label_width), box.height}, m_label, label_ink, draw::text_ellipsis);
        }

        draw::rounded(dc, {line.left, mid - thick / 2, line.width, thick}, thick / 2, groove, groove);

        if (reach > 0)
            draw::rounded(dc, {line.left, mid - thick / 2, std::max(reach, thick), thick}, thick / 2, fill, fill);

        draw::ellipse(dc, {line.left + reach - thumb / 2, mid - thumb / 2, thumb, thumb}, knob, edge);

        if (m_show_value)
        {
            m_field.set_enabled(live);
            m_field.set_area(field_area());
            m_field.paint(dc);
        }
    }

    int_slider& int_slider::set_range(std::int32_t minimum, std::int32_t maximum) noexcept
    {
        m_min = std::min(minimum, maximum);
        m_max = std::max(minimum, maximum);
        assign(m_value);
        return *this;
    }

    int_slider& int_slider::set_value(std::int32_t value) noexcept
    {
        assign(static_cast<double>(value));
        return *this;
    }

    int_slider& int_slider::set_step(std::int32_t step) noexcept
    {
        m_step = static_cast<double>(std::max(step, 1));
        assign(m_value);
        return *this;
    }

    void int_slider::value_changed() noexcept
    {
        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    float_slider& float_slider::set_range(float minimum, float maximum) noexcept
    {
        m_min = static_cast<double>(std::min(minimum, maximum));
        m_max = static_cast<double>(std::max(minimum, maximum));
        assign(m_value);
        return *this;
    }

    float_slider& float_slider::set_value(float value) noexcept
    {
        assign(static_cast<double>(value));
        return *this;
    }

    float_slider& float_slider::set_step(float step) noexcept
    {
        m_step = step > 0.0f ? static_cast<double>(step) : 0.0;
        assign(m_value);
        return *this;
    }

    float_slider& float_slider::set_decimals(std::int32_t decimals) noexcept
    {
        m_digits = std::clamp(decimals, 0, 6);
        sync();
        invalidate();
        return *this;
    }

    void float_slider::value_changed() noexcept
    {
        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }
}
