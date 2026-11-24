#include <chrome/controls/core/radio.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_circle = 16;
        constexpr std::int32_t k_dot = 8;
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_gap = 8;
    }

    bool radio_group::add(radio& item) noexcept
    {
        if (m_count >= k_max_radios)
            return false;

        m_items[m_count++] = &item;
        item.set_group(this);
        return true;
    }

    void radio_group::select(radio& item, bool animate) noexcept
    {
        bool member = false;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            member = member || m_items[i] == &item;
            m_items[i]->set_selected(false, animate);
        }

        if (!member)
        {
            m_selected = nullptr;
            return;
        }

        item.set_selected(true, animate);
        m_selected = &item;
    }

    void radio_group::select_index(std::uint32_t index) noexcept
    {
        if (index < m_count)
            select(*m_items[index]);
    }

    void radio_group::clear() noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_items[i]->set_selected(false, false);

        m_selected = nullptr;
    }

    std::uint32_t radio_group::selected_index() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i] == m_selected)
                return i;
        }

        return k_none;
    }

    void radio_group::choose(radio& item) noexcept
    {
        if (m_selected == &item)
            return;

        select(item, true);

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    control_size radio::preferred_size(void* dc) const noexcept
    {
        const std::int32_t label = m_label != nullptr ? draw::px(k_gap) + draw::text_width(dc, m_label) : 0;
        return {draw::px(k_circle) + label, draw::px(k_height)};
    }

    void radio::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool hot = this->hot();
        const bool pressed = this->pressed();
        const float t = m_pos;

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
        const std::int32_t size = draw::px(k_circle);
        const std::int32_t label_width = m_label != nullptr ? draw::text_width(dc, m_label) : 0;
        const std::int32_t circle_x = m_label_left && m_label != nullptr ? whole.x + whole.width - size : whole.x;
        const dock_rect circle{circle_x, whole.y + (whole.height - size) / 2, size, size};
        draw::ellipse(dc, circle, face, edge);

        // The dot grows from the middle, so a new choice fills in and the old one empties.
        const std::int32_t dot = static_cast<std::int32_t>(std::lround(static_cast<float>(draw::px(k_dot)) * t));

        if (dot > 0)
            draw::ellipse(dc, {circle.x + (size - dot) / 2, circle.y + (size - dot) / 2, dot, dot}, ink, ink);

        if (m_label != nullptr)
        {
            const std::int32_t label_x = m_label_left ? whole.x : circle.x + size + draw::px(k_gap);
            draw::text(dc, {label_x, whole.y, label_width + 2, whole.height}, m_label, label_colour);
        }
    }

    bool radio::animate(std::uint64_t now_ms) noexcept
    {
        const float target = m_selected ? 1.0f : 0.0f;
        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
        m_stamp = now_ms;
        m_pos += (target - m_pos) * (1.0f - std::exp(-dt / 45.0f));
        invalidate();

        if (std::fabs(target - m_pos) < 0.01f)
        {
            m_pos = target;
            m_stamp = 0;
            return false;
        }

        return true;
    }

    void radio::on_click() noexcept
    {
        if (m_group != nullptr)
        {
            m_group->choose(*this);
            return;
        }

        // On its own it only turns on.
        m_selected = true;
    }
}
