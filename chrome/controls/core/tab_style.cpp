#include <chrome/controls/core/tab_style.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome::tab_style
{
    namespace
    {
        constexpr std::int32_t k_height = 30;
        constexpr std::int32_t k_top = 3;
        constexpr std::int32_t k_pad = 10;
        constexpr std::int32_t k_icon = 14;
        constexpr std::int32_t k_gap = 6;
        constexpr std::int32_t k_slot = 16;
        constexpr std::int32_t k_slot_pad = 6;
        constexpr std::int32_t k_min_width = 72;
        constexpr std::int32_t k_max_width = 200;
        constexpr std::int32_t k_add_width = 28;
        constexpr std::int32_t k_radius = 4;
    }

    std::int32_t height() noexcept { return draw::px(k_height); }
    std::int32_t top() noexcept { return draw::px(k_top); }
    std::int32_t least_width() noexcept { return draw::px(k_min_width); }
    std::int32_t most_width() noexcept { return draw::px(k_max_width); }
    std::int32_t add_width() noexcept { return draw::px(k_add_width); }

    std::int32_t natural_width(std::int32_t title_width, bool has_icon, bool slot) noexcept
    {
        std::int32_t width = draw::px(k_pad) + title_width + (slot ? draw::px(k_gap) + draw::px(k_slot) + draw::px(k_slot_pad) : draw::px(k_pad));

        if (has_icon)
            width += draw::px(k_icon) + draw::px(k_gap);

        return width;
    }

    dock_rect close_rect(const dock_rect& tab) noexcept
    {
        const std::int32_t size = draw::px(k_slot);
        return {tab.x + tab.width - draw::px(k_slot_pad) - size, tab.y + (tab.height - size) / 2, size, size};
    }

    std::uint32_t bar_colour() noexcept
    {
        return blend_colour(theme().panel, 0x000000, 0.24f);
    }

    std::uint32_t line_colour() noexcept
    {
        return blend_colour(theme().panel, 0x000000, 0.45f);
    }

    void paint_strip(void* dc, const dock_rect& area) noexcept
    {
        draw::fill(dc, area, bar_colour());
        draw::fill(dc, {area.x, area.y + area.height - 1, area.width, 1}, line_colour());
    }

    void paint_tab(void* dc, const dock_rect& whole, const tab_look& look) noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t bar = bar_colour();
        dock_rect tab = whole;

        if (!look.chosen && !look.ghost)
            tab.height -= 1;

        std::uint32_t face = bar;

        if (look.ghost)
            face = blend_colour(colours.hint, bar, 0.55f);
        else if (look.chosen)
            face = colours.panel;
        else if (look.hot && look.enabled)
            face = blend_colour(bar, colours.hover, 0.8f);

        if (face != bar)
        {
            // The face reaches below the tab by its radius, so that only the top corners show round. What falls outside the window is
            // cut by it, and a tab that stops short of the strip's line gets the line drawn over the excess. This is not done with a
            // clip, because a clip inside the clip of the strip took the tabs drawn after it with it.
            draw::rounded(dc, {tab.x, tab.y, tab.width, tab.height + draw::px(k_radius) + 2}, k_radius, face, face);

            if (tab.height != whole.height)
                draw::fill(dc, {tab.x, tab.y + tab.height, tab.width, whole.height - tab.height}, line_colour());
        }

        if (look.chosen || look.ghost)
        {
            const std::uint32_t accent = look.line != 0 ? look.line : colours.accent;
            const std::uint32_t line = look.ghost ? colours.hint : look.lifted ? colours.text : accent;
            draw::fill(dc, {tab.x + draw::px(k_radius), tab.y, tab.width - 2 * draw::px(k_radius), draw::px(2)}, line);
        }

        const bool bright = look.chosen || look.hot || look.ghost;
        const std::uint32_t ink = !look.enabled ? blend_colour(colours.text_dim, colours.panel, 0.4f) : bright ? colours.text : colours.text_dim;
        std::int32_t x = tab.x + draw::px(k_pad);
        const std::int32_t middle = tab.y + tab.height / 2;

        if (look.icon != icon_kind::none)
        {
            const std::int32_t size = draw::px(k_icon);
            draw::icon(dc, look.icon, {x, middle - size / 2, size, size}, look.chosen ? colours.accent : ink);
            x += size + draw::px(k_gap);
        }

        const dock_rect slot = close_rect(tab);
        const std::int32_t right = look.slot ? slot.x - draw::px(k_gap) / 2 : tab.x + tab.width - draw::px(k_pad) / 2;
        draw::text(dc, {x, tab.y, std::max(right - x, 0), tab.height}, look.title, ink, draw::text_ellipsis);

        if (!look.slot || look.ghost)
            return;

        if (look.close_shown && look.enabled)
        {
            if (look.close_hot)
                draw::rounded(dc, slot, 3, colours.hover, blend_colour(face, colours.text_dim, 0.5f));

            const std::int32_t size = draw::px(10);
            draw::icon(dc, icon_kind::cross, {slot.x + (slot.width - size) / 2, slot.y + (slot.height - size) / 2, size, size},
                look.close_hot ? colours.text : colours.text_dim);
        }
        else if (look.modified)
        {
            const float cx = static_cast<float>(slot.x) + static_cast<float>(slot.width) * 0.5f;
            const float cy = static_cast<float>(slot.y) + static_cast<float>(slot.height) * 0.5f;
            draw::disc(dc, cx, cy, static_cast<float>(draw::px(4)) * 0.75f, ink, ink, 0.0f);
        }
    }
}
