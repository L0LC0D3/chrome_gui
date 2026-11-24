#include <chrome/controls/data/gradient_editor.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        // Sizes at 96 dpi.
        constexpr std::int32_t k_margin = 9;
        constexpr std::int32_t k_marker_row = 18;
        constexpr std::int32_t k_bar_height = 28;
        constexpr std::int32_t k_marker_half = 6;
        constexpr std::int32_t k_marker_tip = 5;
        constexpr std::int32_t k_cell = 6;
        constexpr std::int32_t k_column = 2;
        constexpr std::int32_t k_remove_distance = 30;

        constexpr std::uint32_t k_key_back = 0x08;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_delete = 0x2E;

        [[nodiscard]] bool inside(const dock_rect& rect, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
        }
    }

    gradient_editor& gradient_editor::set_gradient(gradient* data) noexcept
    {
        m_data = data;
        m_kind = stop_kind::none;
        m_index = 0;
        m_hover = {};
        invalidate();
        return *this;
    }

    gradient_editor& gradient_editor::select(stop_kind kind, std::uint32_t index) noexcept
    {
        if (m_data == nullptr || (kind == stop_kind::color && index >= m_data->color_count()) || (kind == stop_kind::alpha && index >= m_data->alpha_count()))
            kind = stop_kind::none;

        m_kind = kind;
        m_index = kind == stop_kind::none ? 0 : index;
        invalidate();
        return *this;
    }

    gradient_editor& gradient_editor::clear_selection() noexcept
    {
        return select(stop_kind::none, 0);
    }

    void gradient_editor::set_selected(stop_kind kind, std::uint32_t index) noexcept
    {
        if (kind != m_kind || index != m_index)
        {
            select(kind, index);
            notify_select();
        }
    }

    float gradient_editor::selected_position() const noexcept
    {
        if (m_data == nullptr)
            return 0.0f;

        if (m_kind == stop_kind::color)
            return m_data->color_at(m_index).position;

        return m_kind == stop_kind::alpha ? m_data->alpha_at(m_index).position : 0.0f;
    }

    float gradient_editor::selected_alpha() const noexcept
    {
        return m_data != nullptr && m_kind == stop_kind::alpha ? m_data->alpha_at(m_index).alpha : 1.0f;
    }

    color_rgba gradient_editor::selected_color() const noexcept
    {
        return m_data != nullptr && m_kind == stop_kind::color ? m_data->color_at(m_index).color : color_rgba{};
    }

    void gradient_editor::set_selected_position(float position) noexcept
    {
        move_selected_to(position);
        notify_change(true);
    }

    void gradient_editor::set_selected_alpha(float alpha) noexcept
    {
        if (m_data != nullptr && m_kind == stop_kind::alpha)
        {
            m_data->set_alpha(m_index, alpha);
            invalidate();
            notify_change(true);
        }
    }

    void gradient_editor::set_selected_color(const color_rgba& color) noexcept
    {
        if (m_data != nullptr && m_kind == stop_kind::color)
        {
            m_data->set_color(m_index, color);
            invalidate();
            notify_change(true);
        }
    }

    void gradient_editor::remove_selected() noexcept
    {
        if (m_data == nullptr || m_kind == stop_kind::none)
            return;

        const bool removed = m_kind == stop_kind::color ? m_data->remove_color(m_index) : m_data->remove_alpha(m_index);

        if (!removed)
            return;

        m_kind = stop_kind::none;
        m_index = 0;
        m_hover = {};
        invalidate();
        notify_select();
        notify_change(true);
    }

    void gradient_editor::move_selected_to(float position) noexcept
    {
        if (m_data == nullptr)
            return;

        if (m_kind == stop_kind::color)
            m_index = m_data->move_color(m_index, position);
        else if (m_kind == stop_kind::alpha)
            m_index = m_data->move_alpha(m_index, position);

        invalidate();
    }

    void gradient_editor::notify_change(bool final) noexcept
    {
        if (m_change != nullptr)
            m_change(m_change_user, *this, final);
    }

    void gradient_editor::notify_select() noexcept
    {
        if (m_select != nullptr)
            m_select(m_select_user, *this);
    }

    dock_rect gradient_editor::bar() const noexcept
    {
        const std::int32_t margin = draw::px(k_margin);
        return {area().x + margin, area().y + draw::px(k_marker_row), std::max(area().width - 2 * margin, 1), draw::px(k_bar_height)};
    }

    float gradient_editor::x_of(float position) const noexcept
    {
        const dock_rect box = bar();
        return static_cast<float>(box.x) + position * static_cast<float>(box.width);
    }

    float gradient_editor::position_at(std::int32_t x) const noexcept
    {
        const dock_rect box = bar();
        return std::clamp(static_cast<float>(x - box.x) / static_cast<float>(box.width), 0.0f, 1.0f);
    }

    gradient_editor::hit gradient_editor::hit_test(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_data == nullptr)
            return {};

        const dock_rect box = bar();
        const std::int32_t half = draw::px(k_marker_half) + 1;
        const bool below = y >= box.y + box.height && y < area().y + area().height;
        const bool above = y >= area().y && y < box.y;
        const auto near = [&](float position) { return std::abs(static_cast<float>(x) - x_of(position)) <= static_cast<float>(half); };

        if (!above && !below)
            return {};

        // The selected stop is drawn last, so it is taken first; then later stops, which are drawn over earlier ones.
        if (below)
        {
            if (m_kind == stop_kind::color && near(m_data->color_at(m_index).position))
                return {stop_kind::color, m_index};

            for (std::uint32_t i = m_data->color_count(); i > 0; --i)
            {
                if (near(m_data->color_at(i - 1).position))
                    return {stop_kind::color, i - 1};
            }
        }
        else
        {
            if (m_kind == stop_kind::alpha && near(m_data->alpha_at(m_index).position))
                return {stop_kind::alpha, m_index};

            for (std::uint32_t i = m_data->alpha_count(); i > 0; --i)
            {
                if (near(m_data->alpha_at(i - 1).position))
                    return {stop_kind::alpha, i - 1};
            }
        }

        return {};
    }

    control_size gradient_editor::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size gradient_editor::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(120));
        return {width, 2 * draw::px(k_marker_row) + draw::px(k_bar_height)};
    }

    void gradient_editor::paint_bar(void* dc, const dock_rect& box) const noexcept
    {
        const std::int32_t cell = draw::px(k_cell);
        const std::int32_t step = std::max(draw::px(k_column), 1);

        // Without any alpha the whole bar is plain columns of colour; with it, each column is cut at the squares so the colour shows
        // through them as it would on a real ground.
        bool see_through = false;

        for (std::uint32_t i = 0; i < m_data->alpha_count(); ++i)
            see_through = see_through || m_data->alpha_at(i).alpha < 0.999f;

        constexpr std::uint32_t light = 0xB4B4B4;
        constexpr std::uint32_t dark = 0x808080;

        for (std::int32_t x = 0; x < box.width;)
        {
            std::int32_t width = std::min(step, box.width - x);

            if (see_through)
                width = std::min(width, cell - x % cell);

            const color_rgba c = m_data->evaluate((static_cast<float>(x) + static_cast<float>(width) * 0.5f) / static_cast<float>(box.width));

            if (!see_through)
            {
                draw::fill(dc, {box.x + x, box.y, width, box.height}, to_rgb24(c));
            }
            else
            {
                const std::int32_t column = x / cell;

                for (std::int32_t y = 0, row = 0; y < box.height; y += cell, ++row)
                {
                    const color_rgba ground = from_rgb24(((row + column) & 1) != 0 ? dark : light);
                    const float a = std::clamp(c.a, 0.0f, 1.0f);
                    const color_rgba shown{ground.r + (c.r - ground.r) * a, ground.g + (c.g - ground.g) * a, ground.b + (c.b - ground.b) * a, 1.0f};
                    draw::fill(dc, {box.x + x, box.y + y, width, std::min(cell, box.height - y)}, to_rgb24(shown));
                }
            }

            x += width;
        }
    }

    void gradient_editor::paint_marker(void* dc, stop_kind kind, std::uint32_t index, bool selected, bool hovered, bool fading) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect box = bar();
        const bool is_color = kind == stop_kind::color;
        const float position = is_color ? m_data->color_at(index).position : m_data->alpha_at(index).position;
        const float cx = std::round(x_of(position)) + 0.5f;
        const float half = static_cast<float>(draw::px(k_marker_half));
        const float tip = static_cast<float>(draw::px(k_marker_tip));
        const float length = static_cast<float>(draw::px(k_marker_row)) - 3.0f;
        // The tip touches the bar; the body goes away from it, down for a colour stop and up for an alpha stop.
        const float base = static_cast<float>(is_color ? box.y + box.height : box.y);
        const float sign = is_color ? 1.0f : -1.0f;

        std::uint32_t fill_colour = 0;

        if (is_color)
            fill_colour = to_rgb24(m_data->color_at(index).color);
        else
        {
            const std::int32_t grey = to_byte(m_data->alpha_at(index).alpha);
            fill_colour = static_cast<std::uint32_t>((grey << 16) | (grey << 8) | grey);
        }

        std::uint32_t edge = selected ? colours.accent : hovered ? 0xFFFFFF : 0xD8D8D8;

        if (fading)
        {
            fill_colour = blend_colour(fill_colour, colours.panel, 0.6f);
            edge = blend_colour(edge, colours.panel, 0.6f);
        }

        const dock_rect body{static_cast<std::int32_t>(cx - half), static_cast<std::int32_t>(sign > 0.0f ? base + tip : base - length), static_cast<std::int32_t>(half * 2.0f),
            static_cast<std::int32_t>(length - tip)};
        draw::shadow(dc, body, 2, 3, 1, 0.45f);

        const draw::point outline_points[5] = {{cx, base}, {cx + half, base + sign * tip}, {cx + half, base + sign * length}, {cx - half, base + sign * length},
            {cx - half, base + sign * tip}};
        draw::polygon(dc, outline_points, 5, edge);

        // The colour sits in the body, inside a frame of the edge colour.
        const std::int32_t line = std::max(draw::px(2), 2);
        const dock_rect face{body.x + line, body.y + line, std::max(body.width - 2 * line, 1), std::max(body.height - 2 * line, 1)};
        draw::fill(dc, face, fill_colour);

        if (selected)
            draw::polyline(dc, outline_points, 5, 0xFFFFFF, 1.0f);
    }

    void gradient_editor::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const dock_rect box = bar();

        if (m_data == nullptr)
        {
            draw::rounded(dc, box, 3, blend_colour(colours.panel, black, 0.3f), blend_colour(colours.title, black, 0.5f));
            return;
        }

        const std::int32_t line = draw::px(1);
        const std::uint32_t edge = focused() ? colours.accent : blend_colour(colours.title, black, 0.5f);
        draw::shadow(dc, box, 3, 4, 2, 0.4f);
        draw::rounded(dc, box, 3, blend_colour(colours.panel, black, 0.3f), edge);

        const dock_rect inner{box.x + line, box.y + line, std::max(box.width - 2 * line, 0), std::max(box.height - 2 * line, 0)};
        draw::clip(dc, inner);
        paint_bar(dc, inner);
        // A faint light line along the top and a darker one along the bottom give the bar a little relief.
        draw::fill(dc, {inner.x, inner.y, inner.width, line}, blend_colour(0xFFFFFF, colours.panel, 0.72f));
        draw::fill(dc, {inner.x, inner.y + inner.height - line, inner.width, line}, blend_colour(black, colours.panel, 0.7f));
        draw::unclip(dc);

        const bool fading = m_dragging && m_removing;

        // Markers that are not selected first, then the selected one, so it is on top where stops overlap.
        for (std::uint32_t i = 0; i < m_data->alpha_count(); ++i)
        {
            if (!(m_kind == stop_kind::alpha && m_index == i))
                paint_marker(dc, stop_kind::alpha, i, false, m_hover == hit{stop_kind::alpha, i}, false);
        }

        for (std::uint32_t i = 0; i < m_data->color_count(); ++i)
        {
            if (!(m_kind == stop_kind::color && m_index == i))
                paint_marker(dc, stop_kind::color, i, false, m_hover == hit{stop_kind::color, i}, false);
        }

        if (m_kind != stop_kind::none)
            paint_marker(dc, m_kind, m_index, true, false, fading);
    }

    void gradient_editor::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_dragging = false;
        m_removing = false;
        m_changed = false;

        if (m_data == nullptr)
            return;

        hit target = hit_test(x, y);

        if (target.kind == stop_kind::none)
        {
            // Empty space beside the bar, within its length: a new stop takes the value the gradient has there.
            const dock_rect box = bar();
            const bool below = y >= box.y + box.height && y < area().y + area().height;
            const bool above = y >= area().y && y < box.y;

            if (x < box.x - draw::px(k_marker_half) || x > box.x + box.width + draw::px(k_marker_half) || (!below && !above))
                return;

            const float position = position_at(x);
            std::int32_t added = -1;

            if (below)
                added = m_data->add_color(position, m_data->color_at_position(position));
            else
                added = m_data->add_alpha(position, m_data->alpha_at_position(position));

            if (added < 0)
                return;

            target = {below ? stop_kind::color : stop_kind::alpha, static_cast<std::uint32_t>(added)};
            m_changed = true;
        }

        set_selected(target.kind, target.index);
        m_grab = x - static_cast<std::int32_t>(std::round(x_of(selected_position())));
        m_dragging = true;
        invalidate();

        if (m_changed)
            notify_change(false);
    }

    bool gradient_editor::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_dragging || m_data == nullptr || m_kind == stop_kind::none)
            return false;

        const dock_rect box = bar();
        const std::int32_t row = draw::px(k_marker_row);
        const std::int32_t centre = m_kind == stop_kind::color ? box.y + box.height + row / 2 : box.y - row / 2;
        const std::uint32_t count = m_kind == stop_kind::color ? m_data->color_count() : m_data->alpha_count();
        const bool far = std::abs(y - centre) > draw::px(k_remove_distance) + row / 2 && count > gradient::k_min_stops;
        m_removing = far;

        const float position = position_at(x - m_grab);

        if (std::fabs(position - selected_position()) > 1.0e-5f)
        {
            move_selected_to(position);
            m_changed = true;
            notify_change(false);
        }

        invalidate();
        return true;
    }

    bool gradient_editor::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const hit now = hit_test(x, y);

        if (now == m_hover)
            return false;

        m_hover = now;
        return true;
    }

    void gradient_editor::on_release() noexcept
    {
        const bool removing = m_dragging && m_removing;
        m_dragging = false;
        m_removing = false;

        if (removing)
        {
            remove_selected();
            m_changed = false;
            return;
        }

        if (m_changed)
        {
            m_changed = false;
            notify_change(true);
        }

        invalidate();
    }

    void gradient_editor::on_leave() noexcept
    {
        if (m_hover.kind != stop_kind::none)
        {
            m_hover = {};
            invalidate();
        }
    }

    bool gradient_editor::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_data == nullptr)
            return false;

        if (key == k_key_delete || key == k_key_back)
        {
            if (m_kind == stop_kind::none)
                return false;

            remove_selected();
            return true;
        }

        if (m_kind == stop_kind::none)
            return false;

        const float amount = ctrl ? 0.001f : shift ? 0.1f : 0.01f;

        if (key == k_key_left || key == k_key_right)
        {
            set_selected_position(selected_position() + (key == k_key_left ? -amount : amount));
            return true;
        }

        if ((key == k_key_up || key == k_key_down) && m_kind == stop_kind::alpha)
        {
            const float step = shift ? 0.01f : 0.05f;
            set_selected_alpha(selected_alpha() + (key == k_key_up ? step : -step));
            return true;
        }

        return false;
    }
}
