#include <chrome/controls/core/scrollbar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        // The whole bar is easy to hit, and the thumb inside it is thinner until the mouse is on it.
        constexpr std::int32_t k_thickness = 12;
        constexpr std::int32_t k_thumb_thin = 4;
        constexpr std::int32_t k_thumb_wide = 8;
        constexpr std::int32_t k_min_thumb = 28;
    }

    std::int32_t scrollbar::thickness() noexcept
    {
        return draw::px(k_thickness);
    }

    scrollbar& scrollbar::set_range(std::int32_t content, std::int32_t page) noexcept
    {
        m_content = std::max(content, 0);
        m_page = std::max(page, 0);
        m_position = std::clamp(m_position, 0, max_position());
        return *this;
    }

    scrollbar& scrollbar::set_position(std::int32_t position) noexcept
    {
        m_position = std::clamp(position, 0, max_position());
        return *this;
    }

    std::int32_t scrollbar::max_position() const noexcept
    {
        return std::max(m_content - m_page, 0);
    }

    control_size scrollbar::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {thickness(), thickness()};
    }

    scrollbar::thumb_span scrollbar::thumb() const noexcept
    {
        const dock_rect& bar = area();
        const std::int32_t track = m_axis == scroll_axis::vertical ? bar.height : bar.width;
        thumb_span span;

        if (!needed() || track <= 0)
            return span;

        const std::int32_t wanted = static_cast<std::int32_t>(static_cast<std::int64_t>(track) * m_page / m_content);
        span.length = std::min(std::max(wanted, draw::px(k_min_thumb)), track);
        span.travel = track - span.length;
        span.start = max_position() > 0 ? static_cast<std::int32_t>(static_cast<std::int64_t>(span.travel) * m_position / max_position()) : 0;
        return span;
    }

    std::int32_t scrollbar::along(std::int32_t x, std::int32_t y) const noexcept
    {
        return m_axis == scroll_axis::vertical ? y - area().y : x - area().x;
    }

    void scrollbar::paint(void* dc) const noexcept
    {
        if (!needed())
            return;

        const theme_colours& colours = theme();
        const dock_rect& bar = area();
        const bool lit = hot() || m_dragging;
        const thumb_span span = thumb();
        const std::int32_t thin = draw::px(k_thumb_thin);
        const std::int32_t wide = draw::px(k_thumb_wide);
        const std::int32_t width = lit ? wide : thin;
        const std::int32_t across = m_axis == scroll_axis::vertical ? bar.width : bar.height;

        // The track only shows when the mouse is near, as a faint groove.
        if (lit)
        {
            const std::int32_t groove = wide + draw::px(2);
            const std::int32_t off = (across - groove) / 2;
            const dock_rect track = m_axis == scroll_axis::vertical ? dock_rect{bar.x + off, bar.y, groove, bar.height}
                                                                    : dock_rect{bar.x, bar.y + off, bar.width, groove};
            draw::rounded(dc, track, groove / 2, blend_colour(colours.panel, 0x000000, 0.2f), blend_colour(colours.panel, 0x000000, 0.2f));
        }

        const std::int32_t off = (across - width) / 2;
        const dock_rect grip = m_axis == scroll_axis::vertical ? dock_rect{bar.x + off, bar.y + span.start, width, span.length}
                                                               : dock_rect{bar.x + span.start, bar.y + off, span.length, width};
        const float strength = m_dragging ? 0.75f : hot() ? 0.6f : 0.4f;
        const std::uint32_t face = blend_colour(colours.panel, colours.text, strength * 0.55f);
        draw::rounded(dc, grip, width / 2, face, face);
    }

    void scrollbar::moved_to(std::int32_t position) noexcept
    {
        const std::int32_t next = std::clamp(position, 0, max_position());

        if (next == m_position)
            return;

        m_position = next;
        m_changed = true;

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void scrollbar::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        const thumb_span span = thumb();
        const std::int32_t at = along(x, y);

        if (at >= span.start && at < span.start + span.length)
        {
            m_dragging = true;
            m_grab = at - span.start;
            return;
        }

        // A click on the track moves a page toward it.
        moved_to(m_position + (at < span.start ? -m_page : m_page));
    }

    bool scrollbar::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_dragging)
            return false;

        const thumb_span span = thumb();
        m_changed = false;

        if (span.travel > 0)
        {
            const std::int32_t start = std::clamp(along(x, y) - m_grab, 0, span.travel);
            moved_to(static_cast<std::int32_t>(static_cast<std::int64_t>(start) * max_position() / span.travel));
        }

        return m_changed;
    }

    void scrollbar::on_release() noexcept
    {
        m_dragging = false;
    }
}
