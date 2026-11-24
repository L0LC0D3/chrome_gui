#include <chrome/controls/core/progress_bar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_track = 8;
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_percent_width = 44;
        constexpr std::int32_t k_gap = 8;
        // One run of the indeterminate segment, in milliseconds, and its length as a share of the track.
        constexpr std::uint64_t k_period = 1500;
        constexpr float k_segment = 0.35f;
    }

    progress_bar& progress_bar::set_value(float value) noexcept
    {
        m_value = std::clamp(value, 0.0f, 1.0f);
        m_shown = m_value;
        invalidate();
        return *this;
    }

    progress_bar& progress_bar::ease_to(float value) noexcept
    {
        m_value = std::clamp(value, 0.0f, 1.0f);
        invalidate();
        return *this;
    }

    control_size progress_bar::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size progress_bar::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {m_width > 0 ? draw::px(m_width) : max_width, draw::px(k_height)};
    }

    bool progress_bar::animate(std::uint64_t now_ms) noexcept
    {
        bool more = false;

        if (m_indeterminate && enabled())
        {
            m_phase = static_cast<float>(now_ms % k_period) / static_cast<float>(k_period);
            more = true;
        }

        if (m_shown != m_value)
        {
            // The same exponential ease as a sliding switch, so the bar settles instead of stopping.
            const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
            m_shown += (m_value - m_shown) * (1.0f - std::exp(-dt / 90.0f));

            if (std::fabs(m_value - m_shown) < 0.002f)
                m_shown = m_value;

            more = more || m_shown != m_value;
        }

        m_stamp = m_shown != m_value ? now_ms : 0;
        invalidate();
        return more;
    }

    void progress_bar::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect& box = area();
        const bool on = enabled();
        const std::uint32_t ink = on ? colours.text : colours.text_dim;
        const std::uint32_t track_colour = blend_colour(colours.panel, colours.text_dim, 0.25f);
        const std::uint32_t fill = on ? colours.accent : blend_colour(colours.text_dim, colours.panel, 0.3f);

        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) + draw::px(k_gap) : 0;
        const std::int32_t percent = m_show_percent && !m_indeterminate ? draw::px(k_percent_width) + draw::px(k_gap) : 0;

        if (m_label != nullptr)
            draw::text(dc, {box.x, box.y, draw::px(m_label_width), box.height}, m_label, ink, draw::text_ellipsis);

        const std::int32_t height = draw::px(k_track);
        const dock_rect bar{box.x + label, box.y + (box.height - height) / 2, std::max(box.width - label - percent, 1), height};
        draw::rounded(dc, bar, k_radius, track_colour, track_colour);

        if (m_indeterminate)
        {
            // The segment enters at the left edge and leaves at the right, and is cut by the track.
            const std::int32_t length = static_cast<std::int32_t>(static_cast<float>(bar.width) * k_segment);
            const std::int32_t at = bar.x - length + static_cast<std::int32_t>(m_phase * static_cast<float>(bar.width + length));
            // Only the part inside the track is drawn, as its own rounded bar, so both ends stay round.
            const std::int32_t from = std::max(at, bar.x);
            const std::int32_t to = std::min(at + length, bar.x + bar.width);

            if (to - from >= bar.height)
                draw::rounded(dc, {from, bar.y, to - from, bar.height}, k_radius, fill, fill);
            else if (to > from)
            {
                draw::clip(dc, {from, bar.y, to - from, bar.height});
                draw::rounded(dc, {to - bar.height, bar.y, bar.height, bar.height}, k_radius, fill, fill);
                draw::unclip(dc);
            }

            return;
        }

        if (m_shown > 0.0f)
        {
            // Never narrower than the height, so a bar that has just begun is still a round dot.
            const std::int32_t width = std::max(static_cast<std::int32_t>(std::lround(static_cast<float>(bar.width) * m_shown)), bar.height);
            draw::clip(dc, bar);
            draw::rounded(dc, {bar.x, bar.y, width, bar.height}, k_radius, fill, fill);
            draw::unclip(dc);
        }

        if (percent > 0)
        {
            char text[8]{};
            std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(std::lround(m_value * 100.0f)));
            draw::text(dc, {bar.x + bar.width + draw::px(k_gap), box.y, draw::px(k_percent_width), box.height}, text, ink, draw::text_right);
        }
    }
}
