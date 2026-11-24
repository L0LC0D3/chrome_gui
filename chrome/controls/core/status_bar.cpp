#include <chrome/controls/core/status_bar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 22;
        constexpr std::int32_t k_padding = 10;
        constexpr std::int32_t k_dot_room = 14;
        constexpr float k_dot_radius = 3.5f;

        [[nodiscard]] std::uint32_t tone_colour(status_tone tone) noexcept
        {
            switch (tone)
            {
            case status_tone::ok:
                return 0x4CAF6E;
            case status_tone::info:
                return theme().accent;
            case status_tone::warning:
                return 0xE0A030;
            case status_tone::error:
                return 0xE05555;
            case status_tone::none:
                break;
            }

            return theme().text_dim;
        }
    }

    std::uint32_t status_bar::add_segment(std::int32_t min_width, bool flexible) noexcept
    {
        if (m_count >= k_max_segments)
            return k_max_segments - 1;

        segment& next = m_segments[m_count];
        next = segment{};
        next.min_width = min_width;
        next.flexible = flexible;
        return m_count++;
    }

    bool status_bar::set_text(std::uint32_t index, const char* text) noexcept
    {
        if (index >= m_count)
            return false;

        segment& target = m_segments[index];
        const char* wanted = text != nullptr ? text : "";

        if (std::strncmp(target.text, wanted, k_text - 1) == 0)
            return false;

        std::strncpy(target.text, wanted, k_text - 1);
        target.text[k_text - 1] = '\0';
        invalidate();
        return true;
    }

    bool status_bar::set_tone(std::uint32_t index, status_tone tone) noexcept
    {
        if (index >= m_count || m_segments[index].tone == tone)
            return false;

        m_segments[index].tone = tone;
        invalidate();
        return true;
    }

    status_bar& status_bar::set_clickable(std::uint32_t index, bool clickable) noexcept
    {
        if (index < m_count)
            m_segments[index].clickable = clickable;

        return *this;
    }

    control_size status_bar::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {0, draw::px(k_height)};
    }

    void status_bar::layout(void* dc) const noexcept
    {
        const dock_rect& bar = area();
        std::int32_t widths[k_max_segments]{};
        std::int32_t fixed = 0;
        std::uint32_t flexible = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const segment& s = m_segments[i];

            if (s.flexible)
            {
                ++flexible;
                continue;
            }

            const std::int32_t dot = s.tone != status_tone::none ? draw::px(k_dot_room) : 0;
            widths[i] = std::max(draw::px(s.min_width), draw::text_width(dc, s.text) + 2 * draw::px(k_padding) + dot);
            fixed += widths[i];
        }

        // The flexible segments share what the others leave.
        const std::int32_t spare = std::max(bar.width - fixed, 0);

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_segments[i].flexible)
                widths[i] = flexible > 0 ? spare / static_cast<std::int32_t>(flexible) : 0;
        }

        std::int32_t x = bar.x;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            m_rect[i] = {x, bar.y + 1, widths[i], std::max(bar.height - 1, 0)};
            x += widths[i];
        }
    }

    void status_bar::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect& bar = area();
        layout(dc);
        draw::fill(dc, bar, blend_colour(colours.panel, 0x000000, 0.22f));
        draw::fill(dc, {bar.x, bar.y, bar.width, 1}, blend_colour(colours.panel, 0x000000, 0.45f));
        const std::uint32_t rule = blend_colour(colours.panel, 0x000000, 0.4f);

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const segment& s = m_segments[i];
            const dock_rect& box = m_rect[i];

            if (box.width <= 0)
                continue;

            if (s.clickable && m_hot_segment == static_cast<std::int32_t>(i) && hot())
                draw::fill(dc, box, pressed() && m_press_segment == m_hot_segment ? blend_colour(colours.panel, 0x000000, 0.1f) : blend_colour(colours.panel, colours.hover, 0.7f));

            if (i > 0)
                draw::fill(dc, {box.x, box.y + draw::px(4), 1, std::max(box.height - draw::px(8), 0)}, rule);

            std::int32_t text_x = box.x + draw::px(k_padding);

            if (s.tone != status_tone::none)
            {
                const std::uint32_t colour = tone_colour(s.tone);
                draw::disc(dc, static_cast<float>(text_x) + static_cast<float>(draw::px(4)), static_cast<float>(box.y) + static_cast<float>(box.height) / 2.0f,
                    static_cast<float>(draw::px(1)) * k_dot_radius, colour, blend_colour(colour, 0x000000, 0.3f), 1.0f);
                text_x += draw::px(k_dot_room);
            }

            const std::int32_t room = std::max(box.x + box.width - draw::px(k_padding) - text_x, 0);
            draw::text(dc, {text_x, box.y, room, box.height}, s.text, colours.text_dim, draw::text_ellipsis);
        }
    }

    std::int32_t status_bar::segment_at(std::int32_t x, std::int32_t y) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const dock_rect& box = m_rect[i];

            if (x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    void status_bar::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_press_segment = segment_at(x, y);
        invalidate();
    }

    bool status_bar::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t over = segment_at(x, y);

        if (over == m_hot_segment)
            return false;

        m_hot_segment = over;
        return true;
    }

    void status_bar::on_leave() noexcept
    {
        m_hot_segment = -1;
        invalidate();
    }

    void status_bar::on_click() noexcept
    {
        const std::int32_t index = m_press_segment;
        m_press_segment = -1;

        if (index >= 0 && m_segments[index].clickable && m_handler != nullptr)
            m_handler(m_user, *this, static_cast<std::uint32_t>(index));
    }
}
