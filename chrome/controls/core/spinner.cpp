#include <chrome/controls/core/spinner.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_min_height = 24;
        constexpr std::int32_t k_gap = 8;
        // One turn in milliseconds, how much of the ring the arc covers in degrees, and the line width at 96 dpi.
        constexpr std::uint64_t k_period = 900;
        constexpr float k_sweep = 100.0f;
        constexpr float k_line = 2.5f;
        constexpr float k_pi = 3.14159265358979f;
        constexpr std::uint32_t k_ring_points = 40;
        constexpr std::uint32_t k_arc_points = 16;
    }

    control_size spinner::preferred_size(void* dc) const noexcept
    {
        const std::int32_t size = draw::px(m_size);
        const std::int32_t text = m_label != nullptr ? draw::px(k_gap) + draw::text_width(dc, m_label) : 0;
        return {size + text, std::max(size + draw::px(4), draw::px(k_min_height))};
    }

    bool spinner::animate(std::uint64_t now_ms) noexcept
    {
        if (!animating())
            return false;

        m_angle = 360.0f * static_cast<float>(now_ms % k_period) / static_cast<float>(k_period);
        invalidate();
        return true;
    }

    void spinner::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect& box = area();
        const bool on = enabled();
        const std::int32_t size = draw::px(m_size);
        const std::int32_t text = m_label != nullptr ? draw::text_width(dc, m_label) : 0;
        const std::int32_t gap = m_label != nullptr ? draw::px(k_gap) : 0;
        const std::int32_t ring_x = m_label_left ? box.x + text + gap : box.x;
        const std::int32_t text_x = m_label_left ? box.x : box.x + size + gap;

        if (m_label != nullptr)
            draw::text(dc, {text_x, box.y, text + 2, box.height}, m_label, on ? colours.text : colours.text_dim, draw::text_ellipsis);

        // The line is centred on the circle, so the circle sits half a line inside the box.
        const float line = k_line * static_cast<float>(draw::px(100)) / 100.0f;
        const float radius = std::max(static_cast<float>(size) / 2.0f - line / 2.0f, 1.0f);
        const float cx = static_cast<float>(ring_x) + static_cast<float>(size) / 2.0f;
        const float cy = static_cast<float>(box.y) + static_cast<float>(box.height) / 2.0f;

        draw::point points[k_ring_points];

        const auto arc = [&](float from, float sweep, std::uint32_t count, std::uint32_t colour) {
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const float angle = (from + sweep * static_cast<float>(i) / static_cast<float>(count - 1)) * k_pi / 180.0f;
                points[i] = {cx + radius * std::cos(angle), cy + radius * std::sin(angle)};
            }

            draw::polyline(dc, points, count, colour, line);
        };

        arc(0.0f, 360.0f, k_ring_points, blend_colour(colours.panel, colours.text_dim, on ? 0.55f : 0.4f));

        if (animating())
            arc(m_angle - 90.0f, k_sweep, k_arc_points, colours.accent);
    }
}
