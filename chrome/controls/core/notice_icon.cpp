#include <chrome/controls/core/notice_icon.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome::notice
{
    std::uint32_t colour(notice_kind kind) noexcept
    {
        switch (kind)
        {
        case notice_kind::success: return 0x3FA66B;
        case notice_kind::warning: return 0xE0A030;
        case notice_kind::error: return 0xE5534B;
        default: return theme().accent;
        }
    }

    void paint_badge(void* dc, notice_kind kind, const dock_rect& box) noexcept
    {
        const std::uint32_t face = colour(kind);
        const float radius = static_cast<float>(std::min(box.width, box.height)) / 2.0f;
        draw::disc(dc, static_cast<float>(box.x) + static_cast<float>(box.width) / 2.0f, static_cast<float>(box.y) + static_cast<float>(box.height) / 2.0f,
            radius, face, face, 0.0f);

        // The mark is white on the colour, a letter or a drawn icon a little inside the disc.
        const std::int32_t inset = box.width / 4;
        const dock_rect mark{box.x + inset, box.y + inset, box.width - 2 * inset, box.height - 2 * inset};

        switch (kind)
        {
        case notice_kind::success:
            draw::icon(dc, icon_kind::check, mark, 0xFFFFFF);
            break;
        case notice_kind::error:
            draw::icon(dc, icon_kind::cross, mark, 0xFFFFFF);
            break;
        case notice_kind::warning:
            draw::text(dc, box, "!", 0xFFFFFF, draw::text_bold | draw::text_center);
            break;
        case notice_kind::question:
            draw::text(dc, box, "?", 0xFFFFFF, draw::text_bold | draw::text_center);
            break;
        default:
            draw::text(dc, box, "i", 0xFFFFFF, draw::text_bold | draw::text_center);
            break;
        }
    }
}
