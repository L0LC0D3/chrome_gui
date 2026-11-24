#include <chrome/controls/core/count_toggle.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_padding = 7;
        constexpr std::int32_t k_mark = 12;
        constexpr std::int32_t k_gap = 5;
        constexpr std::int32_t k_radius = 3;

        void number_text(std::uint32_t count, char* out, std::size_t size) noexcept
        {
            if (count > 9999)
                std::snprintf(out, size, "9999+");
            else
                std::snprintf(out, size, "%u", count);
        }
    }

    control_size count_toggle::preferred_size(void* dc) const noexcept
    {
        char number[16];
        number_text(m_count, number, sizeof(number));
        return {2 * draw::px(k_padding) + draw::px(k_mark) + draw::px(k_gap) + draw::text_width(dc, number), draw::px(k_height)};
    }

    void count_toggle::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool live = enabled();
        const bool on = m_checked && live;
        std::uint32_t face = pressed() ? blend_colour(colours.panel, black, 0.2f) : hot() ? colours.hover : colours.panel;
        std::uint32_t edge = face;
        std::uint32_t mark = blend_colour(colours.text_dim, colours.panel, 0.35f);
        std::uint32_t ink = colours.text_dim;

        if (on)
        {
            face = blend_colour(m_colour, colours.panel, hot() ? 0.68f : 0.78f);
            edge = blend_colour(m_colour, colours.panel, 0.4f);
            mark = m_colour;
            ink = colours.text;
        }

        const dock_rect& box = area();
        draw::rounded(dc, box, k_radius, face, edge);

        char number[16];
        number_text(m_count, number, sizeof(number));
        const std::int32_t size = draw::px(k_mark);
        const std::int32_t text_w = draw::text_width(dc, number);
        const std::int32_t content = size + draw::px(k_gap) + text_w;
        const std::int32_t x = box.x + (box.width - content) / 2;
        const float cx = static_cast<float>(x) + static_cast<float>(size) * 0.5f;
        const float cy = static_cast<float>(box.y) + static_cast<float>(box.height) * 0.5f;
        const float r = static_cast<float>(size) * 0.42f;

        if (m_mark == mark_kind::triangle)
        {
            const float h = r + 1.0f;
            const draw::point triangle[3] = {{cx, cy - h}, {cx + h + 0.5f, cy + h - 1.0f}, {cx - h - 0.5f, cy + h - 1.0f}};
            draw::polygon(dc, triangle, 3, mark);
        }
        else
        {
            draw::disc(dc, cx, cy, r + (m_mark == mark_kind::cross_disc ? 1.0f : 0.0f), mark, mark, 0.0f);

            if (m_mark == mark_kind::cross_disc)
            {
                const float s = r * 0.45f;
                const draw::point a[2] = {{cx - s, cy - s}, {cx + s, cy + s}};
                const draw::point b[2] = {{cx + s, cy - s}, {cx - s, cy + s}};
                draw::polyline(dc, a, 2, on ? white : colours.panel, 1.5f);
                draw::polyline(dc, b, 2, on ? white : colours.panel, 1.5f);
            }
        }

        draw::text(dc, {x + size + draw::px(k_gap), box.y, text_w + 2, box.height}, number, ink);
    }

    void count_toggle::on_click() noexcept
    {
        m_checked = !m_checked;

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }
}
