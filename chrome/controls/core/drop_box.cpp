#include <chrome/controls/core/drop_box.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_icon = 18;
        constexpr std::int32_t k_pad = 12;
        constexpr std::uint32_t k_refused = 0xE5534B;

        [[nodiscard]] const char* effect_name(drop_effect effect) noexcept
        {
            return effect == drop_effect::copy ? "Copy" : effect == drop_effect::move ? "Move" : effect == drop_effect::link ? "Link" : "";
        }
    }

    bool drop_box::add_kind(std::uint32_t tag) noexcept
    {
        if (m_kind_count >= k_max_kinds)
            return false;

        m_kinds[m_kind_count++] = tag;
        return true;
    }

    control_size drop_box::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width), draw::px(m_height)};
    }

    drop_effect drop_box::judge(const drag_event& event) noexcept
    {
        if (event.payload == nullptr)
            return drop_effect::none;

        if (m_kind_count > 0)
        {
            bool known = false;

            for (std::uint32_t i = 0; i < m_kind_count; ++i)
                known = known || m_kinds[i] == event.payload->kind;

            if (!known)
                return drop_effect::none;
        }

        if (m_accept != nullptr && !m_accept(m_accept_user, *this, event))
            return drop_effect::none;

        // What the keys ask for when the zone answers with it, else the first the zone and the source both have.
        const auto both = [&](drop_effect effect) {
            const std::uint8_t bit = effect == drop_effect::copy ? effects::copy : effect == drop_effect::move ? effects::move : effects::link;
            return event.payload->allows(effect) && (m_effects & bit) != 0;
        };

        if (event.wanted != drop_effect::none && both(event.wanted))
            return event.wanted;

        for (const drop_effect effect : {drop_effect::move, drop_effect::copy, drop_effect::link})
        {
            if (both(effect))
                return effect;
        }

        return drop_effect::none;
    }

    drop_effect drop_box::on_drag_over(const drag_event& event) noexcept
    {
        const drop_effect effect = judge(event);

        if (!m_over || effect != m_effect)
            invalidate();

        m_over = true;
        m_effect = effect;
        return effect;
    }

    void drop_box::on_drag_leave() noexcept
    {
        m_over = false;
        m_effect = drop_effect::none;
        invalidate();
    }

    drop_effect drop_box::on_drop(const drag_event& event) noexcept
    {
        const drop_effect effect = judge(event);
        m_over = false;
        m_effect = drop_effect::none;
        invalidate();

        if (effect != drop_effect::none && m_drop != nullptr)
            m_drop(m_drop_user, *this, event, effect);

        return effect;
    }

    void drop_box::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool live = enabled();
        const dock_rect box = area();
        std::uint32_t face = blend_colour(colours.panel, black, 0.14f);
        std::uint32_t edge = blend_colour(colours.text_dim, colours.panel, 0.55f);
        std::uint32_t ink = live ? colours.text_dim : blend_colour(colours.text_dim, colours.panel, 0.45f);

        if (m_over && m_effect != drop_effect::none)
        {
            face = blend_colour(colours.accent, colours.panel, 0.8f);
            edge = colours.accent;
            ink = colours.text;
        }
        else if (m_over)
        {
            face = blend_colour(k_refused, colours.panel, 0.85f);
            edge = k_refused;
        }
        else if (m_text != nullptr)
        {
            ink = colours.text;
        }

        draw::rounded(dc, box, k_radius, face, edge);

        // A dashed line inside the edge says "put something here" while the zone waits, as a solid edge says "this holds a thing".
        if (!m_over && m_text == nullptr)
        {
            const std::int32_t step = draw::px(8);
            const std::int32_t dash = draw::px(4);
            const std::int32_t in = draw::px(3);
            const std::uint32_t line = blend_colour(colours.text_dim, face, 0.55f);

            for (std::int32_t x = box.x + in + draw::px(4); x + dash < box.x + box.width - in; x += step)
            {
                draw::fill(dc, {x, box.y + in, dash, 1}, line);
                draw::fill(dc, {x, box.y + box.height - in - 1, dash, 1}, line);
            }

            for (std::int32_t y = box.y + in + draw::px(4); y + dash < box.y + box.height - in; y += step)
            {
                draw::fill(dc, {box.x + in, y, 1, dash}, line);
                draw::fill(dc, {box.x + box.width - in - 1, y, 1, dash}, line);
            }
        }

        const char* shown = m_text != nullptr ? m_text : m_prompt;

        if (m_over && m_effect != drop_effect::none)
            shown = effect_name(m_effect);
        else if (m_over)
            shown = "Not here";

        std::int32_t x = box.x + draw::px(k_pad);

        if (m_icon != icon_kind::none)
        {
            const std::int32_t size = draw::px(k_icon);
            draw::icon(dc, m_icon, {x, box.y + (box.height - size) / 2, size, size}, m_over && m_effect != drop_effect::none ? colours.accent : ink);
            x += size + draw::px(8);
        }

        draw::text(dc, {x, box.y, std::max(box.x + box.width - draw::px(k_pad) - x, 0), box.height}, shown != nullptr ? shown : "", ink,
            draw::text_ellipsis | (m_icon == icon_kind::none ? draw::text_center : 0));
    }
}
