#include <chrome/controls/core/number_field.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace chrome
{
    namespace
    {
        // The width of the step buttons at the right edge, at 96 dpi.
        constexpr std::int32_t k_buttons = number_field::k_buttons_width;
        constexpr std::int32_t k_max_decimals = 9;
    }

    number_field::number_field() noexcept
    {
        text_field::set_on_action(&number_field::acted, this);
        text_field::set_filter(&number_field::filter, this);
        text_field::set_width(120);
        reserve_right(k_buttons);
        show();
    }

    void number_field::show() noexcept
    {
        char text[64]{};
        std::snprintf(text, sizeof(text), "%.*f", m_decimals, m_value);
        text_field::set_text(text);
    }

    bool number_field::apply(double value, bool notify) noexcept
    {
        if (!std::isfinite(value))
            value = m_value;

        value = std::clamp(value, m_min, m_max);

        // Round by writing the number out and reading it back, so what is held is what is shown.
        char text[64]{};
        std::snprintf(text, sizeof(text), "%.*f", m_decimals, value);
        value = std::strtod(text, nullptr);

        // The range may cut between two roundable values, so the rounded one is kept inside it too.
        value = std::clamp(value, m_min, m_max);

        if (value == 0.0)
            value = 0.0;

        const bool changed = value != m_value;
        m_value = value;
        show();

        if (changed && notify && m_on_change != nullptr)
            m_on_change(m_on_change_user, *this);

        return changed;
    }

    number_field& number_field::set_value(double value) noexcept
    {
        (void)apply(value, false);
        return *this;
    }

    number_field& number_field::set_range(double minimum, double maximum) noexcept
    {
        m_min = std::min(minimum, maximum);
        m_max = std::max(minimum, maximum);
        (void)apply(m_value, false);
        return *this;
    }

    number_field& number_field::set_decimals(std::int32_t decimals) noexcept
    {
        m_decimals = std::clamp(decimals, 0, k_max_decimals);
        (void)apply(m_value, false);
        return *this;
    }

    bool number_field::commit() noexcept
    {
        if (read_only())
            return false;

        const wchar_t* begin = wide_text();
        const wchar_t* end = begin;
        const double parsed = length() > 0 ? std::wcstod(begin, const_cast<wchar_t**>(&end)) : 0.0;

        // Nothing, or something that is not all a number: the old value comes back.
        if (end == begin || *end != L'\0')
        {
            show();
            return false;
        }

        return apply(parsed, true);
    }

    bool number_field::step_by(std::int32_t direction, double scale) noexcept
    {
        if (read_only() || !enabled())
            return false;

        (void)commit();
        return apply(m_value + static_cast<double>(direction) * m_step * scale, true);
    }

    bool number_field::acted(void* user, text_field& source, text_action action) noexcept
    {
        (void)source;
        auto& self = *static_cast<number_field*>(user);

        if (action == text_action::submitted || action == text_action::blurred)
            (void)self.commit();

        return true;
    }

    std::int32_t number_field::filter(void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept
    {
        (void)source;
        const auto& self = *static_cast<number_field*>(user);
        std::int32_t kept = 0;

        for (std::int32_t i = 0; i < length; ++i)
        {
            wchar_t unit = text[i];

            if (unit == L',')
                unit = L'.';

            const bool digit = unit >= L'0' && unit <= L'9';
            const bool sign = unit == L'-' && self.m_min < 0.0;
            const bool point = unit == L'.' && self.m_decimals > 0;

            if (digit || sign || point)
                text[kept++] = unit;
        }

        return kept;
    }

    namespace
    {
        constexpr std::int32_t k_label_gap = 8;
    }

    number_field& number_field::set_label(const char* label) noexcept
    {
        m_label = label;
        text_field::set_name(label);
        reserve_label();
        return *this;
    }

    number_field& number_field::set_label_width(std::int32_t width) noexcept
    {
        m_label_width = width;
        reserve_label();
        return *this;
    }

    void number_field::reserve_label() noexcept
    {
        reserve_left(m_label != nullptr ? m_label_width + k_label_gap : 0);
    }

    bool number_field::in_label(std::int32_t x) const noexcept
    {
        return m_label != nullptr && x < area().x + draw::px(m_label_width);
    }

    control_size number_field::preferred_size(void* dc) const noexcept
    {
        control_size size = text_field::preferred_size(dc);

        if (m_label != nullptr)
            size.width += draw::px(m_label_width + k_label_gap);

        return size;
    }

    mouse_cursor number_field::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        (void)y;
        return m_scrubbing || (in_label(x) && !read_only()) ? mouse_cursor::resize_horizontal : mouse_cursor::arrow;
    }

    void number_field::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (in_label(x) || in_buttons(x))
            on_press(x, y);
        else
            text_field::on_double_click(x, y);
    }

    bool number_field::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_scrubbing)
            return text_field::on_drag(x, y);

        const double fast = GetKeyState(VK_SHIFT) < 0 ? 10.0 : GetKeyState(VK_CONTROL) < 0 ? 0.1 : 1.0;
        m_scrub_value += static_cast<double>(x - m_scrub_x) * m_step * fast;
        m_scrub_value = std::clamp(m_scrub_value, m_min, m_max);
        m_scrub_x = x;
        return apply(m_scrub_value, true);
    }

    void number_field::on_release() noexcept
    {
        m_scrubbing = false;
        text_field::on_release();
    }

    bool number_field::in_buttons(std::int32_t x) const noexcept
    {
        return m_buttons && x >= area().x + area().width - draw::px(k_buttons);
    }

    void number_field::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        if (in_label(x))
        {
            // Not the text: the value is dragged, from where it is now.
            if (!read_only())
            {
                (void)commit();
                m_scrubbing = true;
                m_scrub_value = m_value;
                m_scrub_x = x;
            }

            return;
        }

        if (!in_buttons(x))
        {
            text_field::on_press(x, y);
            return;
        }

        const double scale = GetKeyState(VK_SHIFT) < 0 ? 10.0 : GetKeyState(VK_CONTROL) < 0 ? 0.1 : 1.0;
        (void)step_by(y < area().y + area().height / 2 ? 1 : -1, scale);
    }

    bool number_field::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (!alt && (key == VK_UP || key == VK_DOWN))
        {
            (void)step_by(key == VK_UP ? 1 : -1, shift ? 10.0 : ctrl ? 0.1 : 1.0);
            return true;
        }

        return text_field::on_key(key, ctrl, shift, alt);
    }

    void number_field::paint(void* dc) const noexcept
    {
        text_field::paint(dc);

        if (m_label != nullptr)
        {
            const theme_colours& look = theme();
            const std::uint32_t ink = !enabled() ? blend_colour(look.text_dim, look.panel, 0.45f) : m_scrubbing ? 0xFFFFFF : look.text;
            draw::text(dc, {area().x, area().y, draw::px(m_label_width), area().height}, m_label, ink, draw::text_ellipsis);
        }

        if (!m_buttons)
            return;

        const theme_colours& colours = theme();
        const dock_rect& box = area();
        const std::int32_t zone = draw::px(k_buttons);
        const std::int32_t left = box.x + box.width - zone;
        const bool live = enabled() && !read_only();
        const std::uint32_t dim = blend_colour(colours.text_dim, colours.panel, live ? 0.0f : 0.55f);
        const std::uint32_t gone = blend_colour(colours.text_dim, colours.panel, 0.65f);

        // A rule between the text and the buttons, and another between the two buttons.
        const std::uint32_t rule = blend_colour(colours.panel, 0x000000, 0.3f);
        draw::fill(dc, {left, box.y + 1, 1, box.height - 2}, rule);
        draw::fill(dc, {left + 1, box.y + box.height / 2, zone - 2, 1}, rule);

        // Each button gets a small triangle, dimmer when the value cannot go that way.
        const float centre = static_cast<float>(left) + static_cast<float>(zone) * 0.5f + 0.5f;
        const float half = static_cast<float>(draw::px(3));
        const float up_y = static_cast<float>(box.y) + static_cast<float>(box.height) * 0.25f;
        const float down_y = static_cast<float>(box.y) + static_cast<float>(box.height) * 0.75f;
        const draw::point up[3] = {{centre - half, up_y + half * 0.5f}, {centre + half, up_y + half * 0.5f}, {centre, up_y - half * 0.5f}};
        const draw::point down[3] = {{centre - half, down_y - half * 0.5f}, {centre + half, down_y - half * 0.5f}, {centre, down_y + half * 0.5f}};

        draw::polygon(dc, up, 3, live && m_value < m_max ? dim : gone);
        draw::polygon(dc, down, 3, live && m_value > m_min ? dim : gone);
    }
}
