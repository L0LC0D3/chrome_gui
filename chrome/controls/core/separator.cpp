#include <chrome/controls/core/separator.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_plain_height = 12;
        constexpr std::int32_t k_label_height = 24;
        constexpr std::int32_t k_vertical_width = 9;
        constexpr std::int32_t k_vertical_height = 28;
        // The room around the caption, and the line left on the short side of it.
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_stub = 12;
    }

    control_size separator::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size separator::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;

        if (m_vertical)
            return {draw::px(k_vertical_width), m_length > 0 ? draw::px(m_length) : draw::px(k_vertical_height)};

        return {max_width, draw::px(m_label != nullptr ? k_label_height : k_plain_height)};
    }

    void separator::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t line = blend_colour(colours.panel, colours.text_dim, 0.35f);
        const std::uint32_t ink = enabled() ? colours.text_dim : blend_colour(colours.text_dim, colours.panel, 0.45f);
        const dock_rect& box = area();

        if (m_vertical)
        {
            draw::fill(dc, {box.x + box.width / 2, box.y + draw::px(2), 1, std::max(box.height - 2 * draw::px(2), 0)}, line);
            return;
        }

        const std::int32_t mid = box.y + box.height / 2;

        if (m_label == nullptr)
        {
            draw::fill(dc, {box.x, mid, box.width, 1}, line);
            return;
        }

        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t stub = draw::px(k_stub);
        // The caption gets what is left once both lines keep their stub, and ends in dots when it is longer.
        const std::int32_t room = std::max(box.width - 2 * gap - 2 * stub, 0);
        const std::int32_t text = std::min(draw::text_width(dc, m_label), room);
        const std::int32_t whole = text + 2 * gap;
        std::int32_t left = box.x + stub;

        switch (m_align)
        {
        case separator_align::left: break;
        case separator_align::center: left = box.x + (box.width - whole) / 2; break;
        case separator_align::right: left = box.x + box.width - stub - whole; break;
        }

        const std::int32_t right = left + whole;
        draw::fill(dc, {box.x, mid, std::max(left - box.x, 0), 1}, line);
        draw::fill(dc, {right, mid, std::max(box.x + box.width - right, 0), 1}, line);
        draw::text(dc, {left + gap, box.y, text + 2, box.height}, m_label, ink, draw::text_ellipsis);
    }
}
