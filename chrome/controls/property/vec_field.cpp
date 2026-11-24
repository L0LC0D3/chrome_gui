#include <chrome/controls/property/vec_field.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        // The room for a letter before its field, and between two components, at 96 dpi.
        constexpr std::int32_t k_letter_width = 16;
        constexpr std::int32_t k_gap = 6;
        constexpr std::int32_t k_min_field = 62;

        constexpr std::uint32_t k_key_tab = 0x09;

        constexpr std::uint32_t k_axis_colours[vec_field::k_max_components] = {0xE06C6C, 0x7CC66B, 0x5B9BE6, 0xB9A58A};
    }

    vec_field::vec_field(std::uint32_t components) noexcept
        : m_count(std::clamp<std::uint32_t>(components, 2, k_max_components))
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            m_fields[i].set_buttons(false).set_decimals(3).set_range(-1.0e9, 1.0e9).set_step(0.1).set_on_change(&vec_field::field_changed, this);
            const bool added = m_inner.add(m_fields[i]);
            (void)added;
        }
    }

    bool vec_field::set_value(std::uint32_t index, double value) noexcept
    {
        if (index >= m_count)
            return false;

        m_fields[index].set_value(value);
        invalidate();
        return true;
    }

    double vec_field::value(std::uint32_t index) const noexcept
    {
        return index < m_count ? m_fields[index].value() : 0.0;
    }

    vec_field& vec_field::set_range(double minimum, double maximum) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_fields[i].set_range(minimum, maximum);

        return *this;
    }

    vec_field& vec_field::set_step(double step) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_fields[i].set_step(step);

        return *this;
    }

    vec_field& vec_field::set_decimals(std::int32_t decimals) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_fields[i].set_decimals(decimals);

        return *this;
    }

    vec_field& vec_field::set_buttons(bool buttons) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_fields[i].set_buttons(buttons);

        return *this;
    }

    vec_field& vec_field::set_read_only(bool read_only) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_fields[i].set_read_only(read_only);

        return *this;
    }

    const char* vec_field::axis_name(std::uint32_t index) const noexcept
    {
        static const char* const k_xyzw[] = {"X", "Y", "Z", "W"};
        static const char* const k_rgba[] = {"R", "G", "B", "A"};

        if (index >= k_max_components)
            return "";

        return m_axes == vec_axes::rgba ? k_rgba[index] : k_xyzw[index];
    }

    std::uint32_t vec_field::axis_colour(std::uint32_t index) noexcept
    {
        return k_axis_colours[std::min(index, k_max_components - 1)];
    }

    void vec_field::place() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            m_fields[i].set_area(field_rect(i));
            m_fields[i].set_enabled(enabled());
        }
    }

    dock_rect vec_field::letter_rect(std::uint32_t index) const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t room = std::max(box.width - label, 0);
        const std::int32_t slot = std::max((room - static_cast<std::int32_t>(m_count - 1) * gap) / static_cast<std::int32_t>(m_count), 0);
        return {box.x + label + static_cast<std::int32_t>(index) * (slot + gap), box.y, std::min(draw::px(k_letter_width), slot), box.height};
    }

    dock_rect vec_field::field_rect(std::uint32_t index) const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t room = std::max(box.width - label, 0);
        const std::int32_t slot = std::max((room - static_cast<std::int32_t>(m_count - 1) * gap) / static_cast<std::int32_t>(m_count), 0);
        const std::int32_t letter = std::min(draw::px(k_letter_width), slot);
        return {box.x + label + static_cast<std::int32_t>(index) * (slot + gap) + letter, box.y, std::max(slot - letter, 0), box.height};
    }

    std::int32_t vec_field::letter_at(std::int32_t x, std::int32_t y) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const dock_rect letter = letter_rect(i);

            if (x >= letter.x && x < letter.x + letter.width && y >= letter.y && y < letter.y + letter.height)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    panel_mouse vec_field::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    std::int32_t vec_field::focused_index() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_inner.focused() == &m_fields[i])
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    void vec_field::notify(std::uint32_t index) noexcept
    {
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this, index);
    }

    void vec_field::field_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<vec_field*>(user);

        for (std::uint32_t i = 0; i < self.m_count; ++i)
        {
            if (&self.m_fields[i] == &source)
            {
                self.notify(i);
                return;
            }
        }
    }

    control_size vec_field::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size vec_field::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        // A field has to hold a number like -3.000 whole, so the fields together are never narrower than that.
        const std::int32_t least = static_cast<std::int32_t>(m_count) * draw::px(k_min_field + k_letter_width) + static_cast<std::int32_t>(m_count - 1) * draw::px(k_gap);
        const std::int32_t fields = std::max(m_width > 0 ? draw::px(m_width) : max_width - label, least);
        return {label + fields, m_fields[0].preferred_size(dc).height};
    }

    mouse_cursor vec_field::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_drag == drag_kind::scrub)
            return mouse_cursor::resize_horizontal;

        return m_drag == drag_kind::none && enabled() && letter_at(x, y) >= 0 ? mouse_cursor::resize_horizontal : mouse_cursor::arrow;
    }

    void vec_field::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        m_drag = drag_kind::none;
        const std::int32_t letter = letter_at(x, y);

        if (letter >= 0)
        {
            m_inner.blur();
            m_drag = drag_kind::scrub;
            m_scrub = letter;
            m_press_x = x;
            m_press_value = value(static_cast<std::uint32_t>(letter));
            invalidate();
            return;
        }

        (void)m_inner.on_mouse(mouse_at(mouse_event::down, x, y));
        m_drag = drag_kind::field;
        invalidate();
    }

    void vec_field::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        place();

        if (letter_at(x, y) >= 0)
        {
            on_press(x, y);
            return;
        }

        m_drag = drag_kind::field;
        (void)m_inner.on_mouse(mouse_at(mouse_event::double_click, x, y));
        invalidate();
    }

    bool vec_field::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;

        if (m_drag == drag_kind::field)
        {
            (void)m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
            return true;
        }

        if (m_drag != drag_kind::scrub || m_scrub < 0 || m_fields[m_scrub].read_only())
            return false;

        // A pixel is a step, ten times that with Shift and a tenth with Ctrl.
        const double scale = m_fields[m_scrub].step() * (shift_down() ? 10.0 : ctrl_down() ? 0.1 : 1.0);
        const double before = m_fields[m_scrub].value();
        m_fields[m_scrub].set_value(m_press_value + static_cast<double>(x - m_press_x) * scale);

        if (m_fields[m_scrub].value() == before)
            return false;

        notify(static_cast<std::uint32_t>(m_scrub));
        return true;
    }

    bool vec_field::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        const std::int32_t letter = letter_at(x, y);
        bool changed = m_inner.on_mouse(mouse_at(mouse_event::move, x, y));

        if (letter != m_hover_letter)
        {
            m_hover_letter = letter;
            changed = true;
        }

        return changed;
    }

    void vec_field::on_release() noexcept
    {
        if (m_drag == drag_kind::field)
            (void)m_inner.on_mouse(mouse_at(mouse_event::up, m_x, m_y));

        m_drag = drag_kind::none;
        m_scrub = -1;
        invalidate();
    }

    void vec_field::on_leave() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
        m_hover_letter = -1;
        invalidate();
    }

    bool vec_field::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        const std::int32_t index = focused_index();

        // Tab moves between the fields, and past the first or the last it goes on to the next control.
        if (key == k_key_tab && !ctrl && !alt && ((!shift && index >= static_cast<std::int32_t>(m_count) - 1) || (shift && index == 0)))
        {
            m_inner.blur();
            invalidate();
            return false;
        }

        const bool used = m_inner.on_key(key, ctrl, shift, alt);
        invalidate();
        return used;
    }

    void vec_field::on_char(std::uint32_t code) noexcept
    {
        m_inner.on_char(code);
        invalidate();
    }

    void vec_field::on_focus(bool gained) noexcept
    {
        // The control has the focus for its fields: the first one takes it, and losing it commits what was typed.
        if (gained && m_inner.focused() == nullptr)
            m_inner.set_focus(&m_fields[0]);
        else if (!gained)
            m_inner.blur();

        invalidate();
    }

    bool vec_field::animate(std::uint64_t now_ms) noexcept
    {
        place();
        const bool more = m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return more;
    }

    void vec_field::paint(void* dc) const noexcept
    {
        place();
        const theme_colours& colours = theme();
        const bool live = enabled();
        const std::uint32_t dim = blend_colour(colours.text_dim, colours.panel, 0.45f);

        if (m_label != nullptr)
            draw::text(dc, {area().x, area().y, draw::px(m_label_width), area().height}, m_label, live ? colours.text : dim, draw::text_ellipsis);

        // Each field has its axis's letter before it, which brightens under the mouse and while it is dragged.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const bool lit = live && (m_scrub == static_cast<std::int32_t>(i) || (m_hover_letter == static_cast<std::int32_t>(i) && hot()));
            std::uint32_t colour = axis_colour(i);

            if (!live)
                colour = blend_colour(colour, colours.panel, 0.6f);
            else if (lit)
                colour = blend_colour(colour, 0xFFFFFF, 0.35f);

            draw::text(dc, letter_rect(i), axis_name(i), colour, draw::text_center | draw::text_bold);
        }

        m_inner.paint(dc);
    }

    vec2_field& vec2_field::set(double x, double y) noexcept
    {
        (void)set_value(0, x);
        (void)set_value(1, y);
        return *this;
    }

    vec3_field& vec3_field::set(double x, double y, double z) noexcept
    {
        (void)set_value(0, x);
        (void)set_value(1, y);
        (void)set_value(2, z);
        return *this;
    }

    vec4_field& vec4_field::set(double x, double y, double z, double w) noexcept
    {
        (void)set_value(0, x);
        (void)set_value(1, y);
        (void)set_value(2, z);
        (void)set_value(3, w);
        return *this;
    }
}
