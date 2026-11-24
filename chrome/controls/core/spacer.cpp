#include <chrome/controls/core/spacer.hpp>

#include <chrome/controls/core/draw.hpp>

namespace chrome
{
    control_size spacer::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size spacer::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {m_width > 0 ? draw::px(m_width) : max_width, draw::px(m_height)};
    }
}
