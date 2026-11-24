#include <chrome/controls/core/checkbox.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_box = 16;
        constexpr std::int32_t k_switch_width = 32;
        constexpr std::int32_t k_switch_height = 16;
        constexpr std::int32_t k_knob = 10;
        // The control is taller than the box, so there is room to hit it.
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_radius = 3;
    }

    control_size checkbox::preferred_size(void* dc) const noexcept
    {
        const std::int32_t box = m_style == checkbox_style::switch_pill ? draw::px(k_switch_width) : draw::px(k_box);
        const std::int32_t label = m_label != nullptr ? draw::px(k_gap) + draw::text_width(dc, m_label) : 0;
        return {box + label, draw::px(k_height)};
    }

    void checkbox::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool hot = this->hot();
        const bool pressed = this->pressed();
        const bool on = m_state != check_state::unchecked;
        const bool is_switch = m_style == checkbox_style::switch_pill;

        // How far on it is: a box is on or off, a switch is wherever its knob is.
        const float t = is_switch ? m_pos : (on ? 1.0f : 0.0f);
        const std::uint32_t off_face = blend_colour(colours.panel, black, pressed ? 0.4f : 0.28f);
        const std::uint32_t off_edge = blend_colour(colours.text_dim, colours.panel, hot ? 0.2f : 0.5f);
        const std::uint32_t on_face = pressed ? blend_colour(colours.accent, black, 0.25f) : hot ? blend_colour(colours.accent, white, 0.15f) : colours.accent;
        const std::uint32_t on_edge = blend_colour(colours.accent, black, 0.35f);
        std::uint32_t face = blend_colour(off_face, on_face, t);
        std::uint32_t edge = blend_colour(off_edge, on_edge, t);
        std::uint32_t ink = white;
        std::uint32_t label_colour = colours.text;

        if (!enabled())
        {
            face = blend_colour(blend_colour(colours.panel, colours.title, 0.4f), blend_colour(colours.accent, colours.panel, 0.6f), t);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(white, colours.panel, 0.45f);
            label_colour = blend_colour(colours.text_dim, colours.panel, 0.45f);
        }

        const dock_rect& whole = area();
        const std::int32_t box_width = is_switch ? draw::px(k_switch_width) : draw::px(k_box);
        const std::int32_t box_height = is_switch ? draw::px(k_switch_height) : draw::px(k_box);
        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t label_width = m_label != nullptr ? draw::text_width(dc, m_label) : 0;
        const std::int32_t box_x = m_label_left && m_label != nullptr ? whole.x + whole.width - box_width : whole.x;
        const dock_rect box{box_x, whole.y + (whole.height - box_height) / 2, box_width, box_height};

        if (is_switch)
        {
            // A pill, and a knob at the end that says which way it is.
            draw::rounded(dc, box, k_switch_height / 2, face, edge);
            const std::int32_t knob = draw::px(k_knob);
            const std::int32_t inset = (box.height - knob) / 2;
            const std::int32_t travel = box.width - knob - 2 * inset;
            const std::int32_t knob_x = box.x + inset + static_cast<std::int32_t>(static_cast<float>(travel) * t + 0.5f);
            const std::uint32_t knob_face = !enabled() ? ink : blend_colour(blend_colour(colours.text, colours.panel, hot ? 0.1f : 0.25f), white, t);
            draw::ellipse(dc, {knob_x, box.y + inset, knob, knob}, knob_face, knob_face);
        }
        else
        {
            draw::rounded(dc, box, k_radius, face, edge);
            const std::int32_t mark = draw::px(12);
            const dock_rect mark_box{box.x + (box.width - mark) / 2, box.y + (box.height - mark) / 2, mark, mark};

            if (m_state == check_state::checked)
                draw::icon(dc, icon_kind::check, mark_box, ink);
            else if (m_state == check_state::mixed)
                draw::fill(dc, {mark_box.x + draw::px(1), mark_box.y + mark / 2 - 1, mark - 2 * draw::px(1), std::max(draw::px(2), 2)}, ink);
        }

        if (m_label != nullptr)
        {
            const std::int32_t label_x = m_label_left ? whole.x : box.x + box.width + gap;
            draw::text(dc, {label_x, whole.y, label_width + 2, whole.height}, m_label, label_colour);
        }
    }

    void checkbox::on_click() noexcept
    {
        switch (m_state)
        {
        case check_state::unchecked:
            m_state = check_state::checked;
            break;
        case check_state::checked:
            m_state = m_tristate ? check_state::mixed : check_state::unchecked;
            break;
        case check_state::mixed:
            // Only a tri-state box is clicked into mixed, and it goes round to off. A box that was set to mixed
            // and cannot be, gets ticked.
            m_state = m_tristate ? check_state::unchecked : check_state::checked;
            break;
        }

        // A switch slides there from where it is. Any other box jumps.
        if (m_style != checkbox_style::switch_pill)
            m_pos = position();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    bool checkbox::animate(std::uint64_t now_ms) noexcept
    {
        const float target = position();

        if (m_style != checkbox_style::switch_pill)
        {
            m_pos = target;
            return false;
        }

        // An exponential ease: a fast start that settles, and it can turn back in the middle when clicked again.
        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
        m_stamp = now_ms;
        const float k = 1.0f - std::exp(-dt / 45.0f);
        m_pos += (target - m_pos) * k;
        invalidate();

        if (std::fabs(target - m_pos) < 0.01f)
        {
            m_pos = target;
            m_stamp = 0;
            return false;
        }

        return true;
    }
}
