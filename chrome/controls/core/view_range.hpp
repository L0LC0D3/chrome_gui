#pragma once

#include <algorithm>
#include <cstdint>

namespace chrome
{
    // A band of a scrolled panel's content, in content coordinates, from top up to bottom.
    struct view_range
    {
        std::int32_t top{0};
        std::int32_t bottom{0};

        [[nodiscard]] constexpr bool empty() const noexcept { return bottom <= top; }
        [[nodiscard]] constexpr bool covers(const view_range& other) const noexcept { return other.top >= top && other.bottom <= bottom; }
        // True when a span from y of the given height touches the band.
        [[nodiscard]] constexpr bool touches(std::int32_t y, std::int32_t height) const noexcept { return y < bottom && y + height > top; }
    };

    // What a scroll view keeps painted: the part that shows with a page more above and below, inside the content. Scrolling
    // within it only copies, and the controls outside it are not drawn at all.
    [[nodiscard]] constexpr view_range scroll_paint_range(std::int32_t offset, std::int32_t view_height, std::int32_t content_height) noexcept
    {
        const std::int32_t page = std::max(view_height, 0);
        return {std::max(offset - page, 0), std::min(offset + view_height + page, std::max(content_height, 0))};
    }
}
