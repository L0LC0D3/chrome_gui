#include <chrome/controls/core/label.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_line_height = 24;
    }

    std::uint32_t label::flags() const noexcept
    {
        std::uint32_t result = draw::text_ellipsis;

        if (m_style == label_style::heading)
            result |= draw::text_bold;

        if (m_style == label_style::link && hot())
            result |= draw::text_underline;

        if (m_align == label_align::center)
            result |= draw::text_center;
        else if (m_align == label_align::right)
            result |= draw::text_right;

        if (m_wrap)
            result |= draw::text_wrap;

        return result;
    }

    control_size label::preferred_size(void* dc) const noexcept
    {
        if (m_width > 0)
            return {draw::px(m_width), draw::px(k_line_height)};

        // The width does not change with the underline, so it is measured plain. Bold is wider, so it is measured bold.
        const std::uint32_t measure = m_style == label_style::heading ? draw::text_bold : 0;
        return {draw::text_width(dc, m_text, measure), draw::px(k_line_height)};
    }

    control_size label::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        if (!m_wrap)
            return preferred_size(dc);

        const std::int32_t width = m_width > 0 ? draw::px(m_width) : max_width;
        const std::int32_t height = draw::text_height(dc, m_text, width, flags());
        return {width, height > draw::px(k_line_height) ? height : draw::px(k_line_height)};
    }

    void label::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        std::uint32_t colour = colours.text;

        switch (m_style)
        {
        case label_style::body:
            break;
        case label_style::dim:
            colour = colours.text_dim;
            break;
        case label_style::heading:
            colour = blend_colour(colours.text, 0xFFFFFF, 0.2f);
            break;
        case label_style::link:
            colour = hot() ? blend_colour(colours.accent, 0xFFFFFF, 0.3f) : blend_colour(colours.accent, 0xFFFFFF, 0.15f);
            break;
        case label_style::success:
            colour = 0x57AB5A;
            break;
        case label_style::warning:
            colour = 0xD9A441;
            break;
        case label_style::error:
            colour = 0xE5534B;
            break;
        }

        if (!enabled())
            colour = blend_colour(colours.text_dim, colours.panel, 0.45f);

        draw::text(dc, area(), m_text, colour, flags());
    }

    void label::on_click() noexcept
    {
        if (m_style == label_style::link && m_handler != nullptr)
            m_handler(m_user, *this);
    }
}
