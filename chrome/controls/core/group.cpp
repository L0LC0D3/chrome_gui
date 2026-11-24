#include <chrome/controls/core/group.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_header_height = 24;
        constexpr std::int32_t k_box_height = 28;
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_arrow = 12;
        constexpr std::int32_t k_arrow_gap = 6;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_right = 0x27;
    }

    group& group::set_expanded(bool expanded, bool animate) noexcept
    {
        m_expanded = expanded || !m_collapsible;

        if (!animate)
            m_pos = m_expanded ? 1.0f : 0.0f;

        return *this;
    }

    control_size group::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size group::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {max_width, draw::px(m_style == group_style::box ? k_box_height : k_header_height)};
    }

    void group::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool hot = this->hot();
        const bool pressed = this->pressed() && m_collapsible;
        const bool live = enabled();
        const dock_rect& bar = area();
        const std::uint32_t ink = live ? colours.text : blend_colour(colours.text_dim, colours.panel, 0.45f);
        const std::uint32_t edge = blend_colour(colours.title, black, 0.5f);
        const std::uint32_t rule = blend_colour(colours.panel, black, 0.3f);

        if (m_style == group_style::box)
        {
            std::uint32_t face = blend_colour(colours.panel, colours.title, 0.9f);

            if (m_collapsible && live)
                face = pressed ? blend_colour(colours.panel, black, 0.18f) : hot ? colours.hover : face;

            const std::uint32_t outline = focused() ? colours.accent : edge;
            draw::rounded(dc, bar, k_radius, face, outline);

            // Open, the bar is only the top of the box, so its lower corners are squared off and a line closes it.
            if (m_expanded)
            {
                const std::int32_t half = bar.height / 2;
                draw::fill(dc, {bar.x + 1, bar.y + half, std::max(bar.width - 2, 0), bar.height - half}, face);
                draw::fill(dc, {bar.x, bar.y + half, 1, bar.height - half}, outline);
                draw::fill(dc, {bar.x + bar.width - 1, bar.y + half, 1, bar.height - half}, outline);
                draw::fill(dc, {bar.x, bar.y + bar.height - 1, bar.width, 1}, outline);
            }
        }
        else
        {
            if (m_collapsible && live && (hot || pressed))
                draw::rounded(dc, bar, 2, pressed ? blend_colour(colours.panel, black, 0.18f) : colours.hover, pressed ? blend_colour(colours.panel, black, 0.18f) : colours.hover);

            if (focused())
                draw::rounded(dc, bar, 2, hot ? colours.hover : colours.panel, colours.accent);

            draw::fill(dc, {bar.x, bar.y + bar.height - 1, bar.width, 1}, rule);
        }

        const std::int32_t padding = draw::px(k_padding);
        std::int32_t x = bar.x + padding;

        if (m_collapsible)
        {
            // The arrow points right when folded and turns to point down as the group opens.
            const std::int32_t size = draw::px(k_arrow);
            const float cx = static_cast<float>(x) + static_cast<float>(size) * 0.5f;
            const float cy = static_cast<float>(bar.y) + static_cast<float>(bar.height) * 0.5f;
            const float reach = static_cast<float>(size) * 0.2f;
            const float turn = m_pos * 1.5707963f;
            const float sine = std::sin(turn);
            const float cosine = std::cos(turn);
            const float local[3][2] = {{-reach * 0.6f, -reach}, {reach * 0.6f, 0.0f}, {-reach * 0.6f, reach}};
            draw::point chevron[3];

            for (int i = 0; i < 3; ++i)
            {
                chevron[i].x = cx + local[i][0] * cosine - local[i][1] * sine;
                chevron[i].y = cy + local[i][0] * sine + local[i][1] * cosine;
            }

            draw::polyline(dc, chevron, 3, live ? colours.text_dim : ink, 1.5f);
            x += size + draw::px(k_arrow_gap);
        }

        if (m_title != nullptr)
            draw::text(dc, {x, bar.y, std::max(bar.x + bar.width - padding - x, 0), bar.height}, m_title, ink, draw::text_bold | draw::text_ellipsis);
    }

    bool group::animate(std::uint64_t now_ms) noexcept
    {
        const float target = m_expanded ? 1.0f : 0.0f;
        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
        m_stamp = now_ms;
        m_pos += (target - m_pos) * (1.0f - std::exp(-dt / 45.0f));
        invalidate();

        if (std::fabs(target - m_pos) < 0.01f)
        {
            m_pos = target;
            m_stamp = 0;
            return false;
        }

        return true;
    }

    void group::toggle(bool expanded) noexcept
    {
        if (!m_collapsible || expanded == m_expanded)
            return;

        m_expanded = expanded;
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void group::on_click() noexcept
    {
        toggle(!m_expanded);
    }

    bool group::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)ctrl;
        (void)shift;
        (void)alt;

        switch (key)
        {
        case k_key_enter:
        case k_key_space: toggle(!m_expanded); return true;
        case k_key_right: toggle(true); return true;
        case k_key_left: toggle(false); return true;
        default: return false;
        }
    }
}
