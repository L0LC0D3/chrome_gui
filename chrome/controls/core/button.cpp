#include <chrome/controls/core/button.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_icon_size = 12;
        constexpr std::int32_t k_icon_gap = 6;
        constexpr std::int32_t k_height = 28;
        constexpr std::int32_t k_label_padding = 14;
        constexpr std::int32_t k_radius = 3;
    }

    control_size button::preferred_size(void* dc) const noexcept
    {
        const std::int32_t height = draw::px(k_height);

        // Without a label it is a square around the icon.
        if (m_label == nullptr)
            return {height, height};

        const std::int32_t icon = m_icon != icon_kind::none ? draw::px(k_icon_size) + draw::px(k_icon_gap) : 0;
        return {draw::text_width(dc, m_label) + 2 * draw::px(k_label_padding) + icon, height};
    }

    void button::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool hot = this->hot();
        const bool pressed = this->pressed();
        std::uint32_t face = 0;
        std::uint32_t edge = 0;
        std::uint32_t ink = colours.text;

        switch (m_style)
        {
        case button_style::standard:
            face = pressed ? blend_colour(colours.panel, black, 0.18f) : hot ? colours.hover : blend_colour(colours.panel, colours.hover, 0.55f);
            edge = blend_colour(colours.title, black, 0.5f);
            break;
        case button_style::primary:
            face = pressed ? blend_colour(colours.accent, black, 0.25f) : hot ? blend_colour(colours.accent, white, 0.15f) : colours.accent;
            edge = blend_colour(colours.accent, black, 0.4f);
            ink = white;
            break;
        case button_style::flat:
            face = pressed ? blend_colour(colours.panel, black, 0.2f) : hot ? colours.hover : colours.panel;
            edge = face;
            break;
        }

        // A toggle that is on is lit in the accent colour.
        if (m_toggle && m_checked)
        {
            face = hot ? blend_colour(colours.accent, colours.panel, 0.4f) : blend_colour(colours.accent, colours.panel, 0.55f);
            edge = colours.accent;
            ink = colours.text;
        }

        if (!enabled())
        {
            face = m_style == button_style::flat ? colours.panel : blend_colour(colours.panel, colours.title, 0.5f);
            edge = m_style == button_style::flat ? face : blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
        }

        const dock_rect& outer = area();
        draw::rounded(dc, outer, k_radius, face, edge);

        // Raised buttons catch the light along their top edge, and lose it when pressed.
        if (enabled() && !pressed && m_style != button_style::flat)
        {
            const std::int32_t corner = draw::px(k_radius);
            draw::fill(dc, {outer.x + corner, outer.y + 1, std::max(outer.width - 2 * corner, 0), 1}, blend_colour(face, white, 0.14f));
        }

        // What is in the button, centred: the icon, then the label. A pressed button moves down a pixel.
        const std::int32_t icon_size = draw::px(k_icon_size);
        const std::int32_t between = draw::px(k_icon_gap);
        const std::int32_t label_width = m_label != nullptr ? draw::text_width(dc, m_label) : 0;
        const bool has_icon = m_icon != icon_kind::none;
        const std::int32_t content = (has_icon ? icon_size : 0) + (has_icon && m_label != nullptr ? between : 0) + label_width;
        const std::int32_t shift = pressed ? 1 : 0;
        std::int32_t x = outer.x + (outer.width - content) / 2;

        if (has_icon)
        {
            draw::icon(dc, m_icon, {x, outer.y + (outer.height - icon_size) / 2 + shift, icon_size, icon_size}, ink);
            x += icon_size + (m_label != nullptr ? between : 0);
        }

        if (m_label != nullptr)
            draw::text(dc, {x, outer.y + shift, label_width + 2, outer.height}, m_label, ink);
    }

    void button::on_click() noexcept
    {
        if (m_toggle)
            m_checked = !m_checked;

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }
}
