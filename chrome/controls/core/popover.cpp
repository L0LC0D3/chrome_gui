#include <chrome/controls/core/popover.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_radius = 6;
        constexpr std::int32_t k_arrow_height = 7;
        constexpr std::int32_t k_arrow_half = 7;
        constexpr std::int32_t k_gap = 4;
        // After a click outside has closed it, a click on its anchor within this long is the same click.
        constexpr std::uint64_t k_reopen_guard_ms = 300;

        constexpr std::uint32_t k_key_tab = 0x09;
        constexpr std::uint32_t k_key_escape = 0x1B;
    }

    popover::popover() noexcept
    {
        m_body.set_padding(draw::px(12)).set_gap(draw::px(8));
    }

    popover& popover::attach(control_set& set) noexcept
    {
        m_set = &set;
        const bool added = set.add(*this);
        (void)added;
        return *this;
    }

    layout_slot& popover::add(control& item) noexcept
    {
        const bool added = m_inner.add(item);
        (void)added;
        return m_body.add(item);
    }

    panel_mouse popover::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    control_size popover::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {1, 1};
    }

    control_size popover::content_size() const noexcept
    {
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : (m_fit.width > 0 ? m_fit.width : draw::px(200));
        const std::int32_t height = m_height > 0 ? draw::px(m_height) : (m_fit.height > 0 ? m_fit.height : draw::px(80));
        return {width, height};
    }

    dock_rect popover::place(popover_side& side) const noexcept
    {
        const control_size size = content_size();
        const std::int32_t gap = m_arrow ? draw::px(k_arrow_height) + draw::px(1) : draw::px(k_gap);
        const bool bounded = m_bounds.width > 0 && m_bounds.height > 0;
        const std::int32_t bottom = m_bounds.y + m_bounds.height;
        const std::int32_t right = m_bounds.x + m_bounds.width;
        side = m_side;

        if (side == popover_side::automatic)
        {
            if (!bounded || m_anchor.y + m_anchor.height + gap + size.height <= bottom)
                side = popover_side::below;
            else if (m_anchor.y - gap - size.height >= m_bounds.y)
                side = popover_side::above;
            else
                side = popover_side::below;
        }

        std::int32_t x = m_anchor.x;
        std::int32_t y = m_anchor.y + m_anchor.height + gap;

        switch (side)
        {
        case popover_side::above:
            y = m_anchor.y - gap - size.height;
            break;
        case popover_side::right:
            x = m_anchor.x + m_anchor.width + gap;
            y = m_anchor.y;
            break;
        case popover_side::left:
            x = m_anchor.x - gap - size.width;
            y = m_anchor.y;
            break;
        default:
            break;
        }

        if (bounded)
        {
            x = std::clamp(x, m_bounds.x, std::max(right - size.width, m_bounds.x));
            y = std::clamp(y, m_bounds.y, std::max(bottom - size.height, m_bounds.y));
        }

        return {x, y, size.width, size.height};
    }

    dock_rect popover::frame() const noexcept
    {
        if (!m_open)
            return {};

        popover_side side = popover_side::below;
        return place(side);
    }

    void popover::set_anchor(const dock_rect& anchor, const dock_rect& bounds) noexcept
    {
        m_anchor = anchor;
        m_bounds = bounds;
        set_area({anchor.x, anchor.y, 1, 1});
    }

    void popover::open(const dock_rect& anchor, const dock_rect& bounds) noexcept
    {
        set_anchor(anchor, bounds);

        if (m_open)
            return;

        m_open = true;
        m_closed_by_focus = false;
        m_x = anchor.x;
        m_y = anchor.y;

        if (m_set != nullptr)
        {
            m_return = m_set->focused() != this ? m_set->focused() : nullptr;
            m_set->set_focus(this);
        }

        invalidate();
    }

    void popover::toggle(const dock_rect& anchor, const dock_rect& bounds) noexcept
    {
        if (m_open)
        {
            close();
            return;
        }

        // The press that began this click already closed it by taking the focus, so the click does not open it again.
        if (m_closed_by_focus && GetTickCount64() - m_closed_at < k_reopen_guard_ms)
        {
            m_closed_by_focus = false;
            return;
        }

        open(anchor, bounds);
    }

    void popover::close_now(bool give_back) noexcept
    {
        if (!m_open)
            return;

        m_open = false;
        m_inner.blur();
        m_closed_at = GetTickCount64();
        invalidate();
        control* back = m_return;
        m_return = nullptr;

        if (m_set != nullptr && m_set->focused() == this)
            m_set->set_focus(give_back ? back : nullptr);

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void popover::on_focus(bool gained) noexcept
    {
        if (gained || !m_open || !m_dismissable)
            return;

        // The focus went to something else, such as by a click outside.
        m_closed_by_focus = true;
        close_now(false);
    }

    void popover::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        (void)m_inner.on_mouse(mouse_at(mouse_event::down, x, y));
        invalidate();
    }

    void popover::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        (void)m_inner.on_mouse(mouse_at(mouse_event::double_click, x, y));
        invalidate();
    }

    bool popover::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        (void)m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
        return true;
    }

    bool popover::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        return m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
    }

    void popover::on_release() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::up, m_x, m_y));
        invalidate();
    }

    void popover::on_leave() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
        invalidate();
    }

    void popover::on_char(std::uint32_t code) noexcept
    {
        m_inner.on_char(code);
        invalidate();
    }

    bool popover::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (!m_open)
            return false;

        // The controls first: a field being typed in, a slider, a dropdown that is open, and Tab between them.
        const bool used = m_inner.on_key(key, ctrl, shift, alt);
        invalidate();

        if (used)
            return true;

        if (key == k_key_escape)
        {
            close();
            return true;
        }

        // Tab stays inside while it is open.
        return key == k_key_tab;
    }

    bool popover::animate(std::uint64_t now_ms) noexcept
    {
        if (!m_open)
            return false;

        const bool more = m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return more;
    }

    void popover::paint_overlay(void* dc) const noexcept
    {
        if (!m_open)
            return;

        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;

        // A popover that fits its controls takes what they need, at the width it was given.
        const std::int32_t measure_width = m_width > 0 ? draw::px(m_width) : draw::px(m_max_width);
        control_size fit = m_body.measure(dc, measure_width);

        if (m_width > 0)
            fit.width = draw::px(m_width);

        if (fit.width != m_fit.width || fit.height != m_fit.height)
            m_fit = fit;

        popover_side side = popover_side::below;
        const dock_rect box = place(side);
        (void)m_body.arrange(dc, box);

        const std::uint32_t face = blend_colour(colours.panel, colours.title, 0.8f);
        const std::uint32_t edge = blend_colour(colours.title, black, 0.45f);

        // A soft shadow, as two darker boxes under it.
        draw::rounded(dc, {box.x - 2, box.y + 1, box.width + 4, box.height + 5}, k_radius + 2, blend_colour(colours.panel, black, 0.28f), blend_colour(colours.panel, black, 0.28f));
        draw::rounded(dc, {box.x - 1, box.y + 1, box.width + 2, box.height + 3}, k_radius + 1, blend_colour(colours.panel, black, 0.5f), blend_colour(colours.panel, black, 0.5f));
        draw::rounded(dc, box, k_radius, face, edge);

        if (m_arrow)
        {
            const float height = static_cast<float>(draw::px(k_arrow_height));
            const float half = static_cast<float>(draw::px(k_arrow_half));
            const float u = static_cast<float>(draw::px(1));
            const float inset = static_cast<float>(draw::px(k_radius)) + half;
            float tip_x = 0.0f;
            float tip_y = 0.0f;
            draw::point base_a{};
            draw::point base_b{};
            draw::point tip{};

            if (side == popover_side::below || side == popover_side::above)
            {
                const float centre = static_cast<float>(m_anchor.x) + static_cast<float>(m_anchor.width) * 0.5f;
                tip_x = std::clamp(centre, static_cast<float>(box.x) + inset, static_cast<float>(box.x + box.width) - inset);
                const bool below = side == popover_side::below;
                const float edge_y = below ? static_cast<float>(box.y) : static_cast<float>(box.y + box.height);
                tip_y = below ? edge_y - height : edge_y + height;
                const float base_y = below ? edge_y + u : edge_y - u;
                base_a = {tip_x - half, base_y};
                base_b = {tip_x + half, base_y};
            }
            else
            {
                const float centre = static_cast<float>(m_anchor.y) + static_cast<float>(m_anchor.height) * 0.5f;
                tip_y = std::clamp(centre, static_cast<float>(box.y) + inset, static_cast<float>(box.y + box.height) - inset);
                const bool at_right = side == popover_side::right;
                const float edge_x = at_right ? static_cast<float>(box.x) : static_cast<float>(box.x + box.width);
                tip_x = at_right ? edge_x - height : edge_x + height;
                const float base_x = at_right ? edge_x + u : edge_x - u;
                base_a = {base_x, tip_y - half};
                base_b = {base_x, tip_y + half};
            }

            tip = {tip_x, tip_y};
            const draw::point triangle[3] = {base_a, tip, base_b};
            draw::polygon(dc, triangle, 3, face);
            draw::polyline(dc, triangle, 3, edge, u);
            // The base of the arrow is open, joined to the box, so it is drawn over with the face.
            const draw::point seam[2] = {base_a, base_b};
            draw::polyline(dc, seam, 2, face, 2.0f * u);
        }

        m_inner.paint(dc);
    }
}
