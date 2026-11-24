#include <chrome/controls/property/color_swatch.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_radius = 3;
        // Of the box's width, the part that shows the colour solid. The rest shows it at its alpha.
        constexpr float k_solid_part = 0.7f;
        constexpr std::int32_t k_alpha_bar = 3;
        constexpr std::int32_t k_cell = 6;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
    }

    color_swatch& color_swatch::set_color(const color_rgba& color) noexcept
    {
        m_color = color;

        if (!m_alpha)
            m_color.a = 1.0f;

        if (is_open())
            m_picker->set_color(m_color);

        invalidate();
        return *this;
    }

    color_swatch& color_swatch::set_alpha(bool alpha) noexcept
    {
        m_alpha = alpha;

        if (!alpha)
            m_color.a = 1.0f;

        invalidate();
        return *this;
    }

    bool color_swatch::is_open() const noexcept
    {
        return m_picker != nullptr && m_picker->is_open() && m_picker->owner() == this;
    }

    void color_swatch::open() noexcept
    {
        if (m_picker == nullptr || !enabled())
            return;

        m_picker->open(box(), m_bounds, m_color, m_alpha, &color_swatch::picked, this);
        invalidate();
    }

    void color_swatch::close() noexcept
    {
        if (is_open())
        {
            m_picker->close();
            invalidate();
        }
    }

    void color_swatch::picked(void* user, color_picker& source) noexcept
    {
        auto& self = *static_cast<color_swatch*>(user);
        self.m_color = source.color();
        self.invalidate();

        if (self.m_handler != nullptr)
            self.m_handler(self.m_user, self);
    }

    dock_rect color_swatch::box() const noexcept
    {
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        return {area().x + label, area().y, std::max(area().width - label, 0), area().height};
    }

    panel_mouse color_swatch::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    control_size color_swatch::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size color_swatch::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width - label, draw::px(40));
        return {label + width, draw::px(k_height)};
    }

    dock_rect color_swatch::overlay_area() const noexcept
    {
        if (!is_open())
            return {};

        m_picker->set_anchor(box(), m_bounds);
        return m_picker->area();
    }

    void color_swatch::paint_overlay(void* dc) const noexcept
    {
        if (is_open())
        {
            m_picker->set_anchor(box(), m_bounds);
            m_picker->paint(dc);
        }
    }

    bool color_swatch::animate(std::uint64_t now_ms) noexcept
    {
        if (!is_open())
            return false;

        const bool more = m_picker->animate(now_ms);

        if (m_picker->take_dirty())
            invalidate();

        return more;
    }

    void color_swatch::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool live = enabled();
        const bool lit = focused() || is_open();

        if (m_label != nullptr)
            draw::text(dc, {area().x, area().y, draw::px(m_label_width), area().height}, m_label,
                live ? colours.text : blend_colour(colours.text_dim, colours.panel, 0.45f), draw::text_ellipsis);

        const dock_rect frame = box();
        std::uint32_t edge = lit ? colours.accent : hot() && live ? blend_colour(colours.text_dim, colours.panel, 0.2f) : blend_colour(colours.title, black, 0.5f);

        if (!live)
            edge = blend_colour(colours.panel, black, 0.2f);

        draw::rounded(dc, frame, k_radius, blend_colour(colours.panel, black, 0.3f), edge);

        // Inside the frame: the colour solid at the left, at its alpha over squares at the right, and the bar of the alpha.
        const std::int32_t line = draw::px(1);
        const dock_rect inside{frame.x + line, frame.y + line, std::max(frame.width - 2 * line, 0), std::max(frame.height - 2 * line, 0)};
        const std::int32_t bar = m_alpha ? std::min(draw::px(k_alpha_bar), inside.height / 2) : 0;
        const dock_rect colour_area{inside.x, inside.y, inside.width, inside.height - bar};
        color_rgba shown = m_color;

        // A swatch that is off shows its colour faded, so it reads as not there to touch.
        if (!live)
        {
            const color_rgba ground = from_rgb24(colours.panel);
            shown = {shown.r + (ground.r - shown.r) * 0.55f, shown.g + (ground.g - shown.g) * 0.55f, shown.b + (ground.b - shown.b) * 0.55f, shown.a};
        }

        const std::uint32_t solid = to_rgb24(shown);

        if (m_alpha)
        {
            const std::int32_t solid_width = static_cast<std::int32_t>(static_cast<float>(colour_area.width) * k_solid_part);
            draw::fill(dc, {colour_area.x, colour_area.y, solid_width, colour_area.height}, solid);
            paint_checker(dc, {colour_area.x + solid_width, colour_area.y, colour_area.width - solid_width, colour_area.height}, draw::px(k_cell), &shown);

            // The alpha as a bar, white for what there is and black for what is not.
            const std::int32_t white_width = static_cast<std::int32_t>(static_cast<float>(inside.width) * std::clamp(m_color.a, 0.0f, 1.0f) + 0.5f);
            draw::fill(dc, {inside.x, inside.y + inside.height - bar, white_width, bar}, live ? 0xFFFFFF : 0x909090);
            draw::fill(dc, {inside.x + white_width, inside.y + inside.height - bar, inside.width - white_width, bar}, black);
        }
        else
        {
            draw::fill(dc, colour_area, solid);
        }
    }

    void color_swatch::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        m_in_picker = false;

        if (is_open() && m_picker->contains(x, y))
        {
            m_in_picker = true;

            if (m_picker->on_mouse(mouse_at(mouse_event::down, x, y)))
                invalidate();
        }
    }

    void color_swatch::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (is_open() && m_picker->contains(x, y))
        {
            m_x = x;
            m_y = y;
            m_in_picker = true;

            if (m_picker->on_mouse(mouse_at(mouse_event::double_click, x, y)))
                invalidate();

            return;
        }

        on_press(x, y);
    }

    bool color_swatch::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (!m_in_picker || !is_open())
            return false;

        (void)m_picker->on_mouse(mouse_at(mouse_event::move, x, y));
        return true;
    }

    bool color_swatch::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (!is_open())
            return false;

        return m_picker->on_mouse(mouse_at(mouse_event::move, x, y));
    }

    void color_swatch::on_release() noexcept
    {
        if (m_in_picker && is_open())
        {
            (void)m_picker->on_mouse(mouse_at(mouse_event::up, m_x, m_y));
            invalidate();
        }
    }

    void color_swatch::on_click() noexcept
    {
        // A press and a release inside the picker were the picker's, and not a click on the box.
        if (m_in_picker)
        {
            m_in_picker = false;
            return;
        }

        if (is_open())
            close();
        else
            open();
    }

    void color_swatch::on_leave() noexcept
    {
        if (is_open())
            (void)m_picker->on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
    }

    bool color_swatch::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        (void)notches;

        // The wheel over the picker is not for the panel behind it.
        return is_open();
    }

    bool color_swatch::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (is_open())
        {
            const bool used = m_picker->on_key(key, ctrl, shift, alt);
            invalidate();
            return used;
        }

        if ((key == k_key_enter || key == k_key_space) && !ctrl && !alt)
        {
            open();
            return true;
        }

        return false;
    }

    void color_swatch::on_char(std::uint32_t code) noexcept
    {
        if (is_open())
        {
            m_picker->on_char(code);
            invalidate();
        }
    }

    void color_swatch::on_focus(bool gained) noexcept
    {
        // Another control took the keyboard, so the picker, which is this one's, goes away.
        if (!gained)
            close();
    }
}
