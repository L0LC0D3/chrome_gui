#include <chrome/controls/core/splitter.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        // The bar can be taken a little outside its drawn thickness, so it is easy to hit.
        constexpr std::int32_t k_slop = 2;
        constexpr std::int32_t k_key_step = 16;

        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
    }

    splitter& splitter::set_ratio(float ratio) noexcept
    {
        m_ratio = std::clamp(ratio, 0.0f, 1.0f);

        if (!m_ratio_given)
        {
            m_start_ratio = m_ratio;
            m_ratio_given = true;
        }

        invalidate();
        return *this;
    }

    std::int32_t splitter::room() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t total = m_axis == split_axis::horizontal ? box.width : box.height;
        return std::max(total - draw::px(m_bar), 0);
    }

    std::int32_t splitter::first_size() const noexcept
    {
        const std::int32_t total = room();
        const std::int32_t low = draw::px(m_first_min);
        const std::int32_t high = total - draw::px(m_second_min);
        const std::int32_t wanted = static_cast<std::int32_t>(std::lround(static_cast<double>(m_ratio) * static_cast<double>(total)));

        // With no room for both least sizes, the share decides.
        if (high < low)
            return wanted;

        return std::clamp(wanted, low, high);
    }

    dock_rect splitter::first_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t size = first_size();
        return m_axis == split_axis::horizontal ? dock_rect{box.x, box.y, size, box.height} : dock_rect{box.x, box.y, box.width, size};
    }

    dock_rect splitter::bar() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t size = first_size();
        const std::int32_t thick = draw::px(m_bar);
        return m_axis == split_axis::horizontal ? dock_rect{box.x + size, box.y, thick, box.height} : dock_rect{box.x, box.y + size, box.width, thick};
    }

    dock_rect splitter::second_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t size = first_size();
        const std::int32_t thick = draw::px(m_bar);
        const std::int32_t rest = std::max(room() - size, 0);
        return m_axis == split_axis::horizontal ? dock_rect{box.x + size + thick, box.y, rest, box.height} : dock_rect{box.x, box.y + size + thick, box.width, rest};
    }

    void splitter::arrange(void* dc) noexcept
    {
        if (m_first != nullptr)
            (void)m_first->arrange_clipped(dc, first_area());

        if (m_second != nullptr)
            (void)m_second->arrange_clipped(dc, second_area());
    }

    control_size splitter::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, draw::px(320));
    }

    control_size splitter::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {max_width, draw::px(m_height)};
    }

    bool splitter::on_bar(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect b = bar();
        const std::int32_t slop = draw::px(k_slop);
        return x >= b.x - slop && x < b.x + b.width + slop && y >= b.y - slop && y < b.y + b.height + slop;
    }

    mouse_cursor splitter::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (!m_dragging && !on_bar(x, y))
            return mouse_cursor::arrow;

        return m_axis == split_axis::horizontal ? mouse_cursor::resize_horizontal : mouse_cursor::resize_vertical;
    }

    bool splitter::set_first_size(std::int32_t size) noexcept
    {
        const std::int32_t total = room();

        if (total <= 0)
            return false;

        const std::int32_t low = draw::px(m_first_min);
        const std::int32_t high = total - draw::px(m_second_min);
        size = high < low ? std::clamp(size, 0, total) : std::clamp(size, low, high);
        const float ratio = static_cast<float>(size) / static_cast<float>(total);

        if (size == first_size())
            return false;

        m_ratio = ratio;
        notify();
        return true;
    }

    void splitter::notify() noexcept
    {
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void splitter::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        if (!on_bar(x, y))
            return;

        const dock_rect b = bar();
        m_dragging = true;
        m_grab = m_axis == split_axis::horizontal ? x - b.x : y - b.y;
        invalidate();
    }

    void splitter::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (!on_bar(x, y))
            return;

        m_dragging = false;
        const float before = m_ratio;
        m_ratio = m_start_ratio;

        if (before != m_ratio)
            notify();
    }

    bool splitter::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_dragging)
            return false;

        const dock_rect& box = area();
        const std::int32_t at = m_axis == split_axis::horizontal ? x - box.x : y - box.y;
        (void)set_first_size(at - m_grab);
        return true;
    }

    bool splitter::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const bool over = on_bar(x, y);

        if (over == m_over_bar)
            return false;

        m_over_bar = over;
        return true;
    }

    void splitter::on_leave() noexcept
    {
        m_over_bar = false;
        invalidate();
    }

    void splitter::on_release() noexcept
    {
        m_dragging = false;
        invalidate();
    }

    bool splitter::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt)
            return false;

        const bool horizontal = m_axis == split_axis::horizontal;
        const std::int32_t step = draw::px(k_key_step) * (shift ? 10 : 1) / (ctrl ? 10 : 1);
        const std::int32_t now = first_size();

        switch (key)
        {
        case k_key_left:
            if (!horizontal)
                return false;

            (void)set_first_size(now - step);
            return true;
        case k_key_right:
            if (!horizontal)
                return false;

            (void)set_first_size(now + step);
            return true;
        case k_key_up:
            if (horizontal)
                return false;

            (void)set_first_size(now - step);
            return true;
        case k_key_down:
            if (horizontal)
                return false;

            (void)set_first_size(now + step);
            return true;
        case k_key_home:
            (void)set_first_size(0);
            return true;
        case k_key_end:
            (void)set_first_size(room());
            return true;
        default:
            return false;
        }
    }

    void splitter::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool live = enabled();
        const bool lit = live && (m_dragging || m_over_bar);
        const dock_rect b = bar();

        // The edge round both panes. Only the edge, since the controls in the panes are drawn first and have to stay visible.
        draw::outline(dc, area(), 3, blend_colour(colours.title, black, 0.35f), 1.0f);

        std::uint32_t face = blend_colour(colours.panel, black, 0.22f);

        if (lit)
            face = m_dragging ? colours.accent : blend_colour(colours.panel, colours.accent, 0.55f);

        if (!live)
            face = blend_colour(colours.panel, colours.title, 0.4f);

        draw::fill(dc, b, face);

        // Three small dots along the middle of the bar, to say it can be taken.
        const std::uint32_t dot = m_dragging ? 0xFFFFFF : blend_colour(colours.text_dim, colours.panel, lit ? 0.0f : 0.35f);
        const std::int32_t size = std::max(draw::px(2), 2);
        const std::int32_t spacing = size * 3;

        for (std::int32_t i = -1; i <= 1; ++i)
        {
            const std::int32_t cx = b.x + b.width / 2;
            const std::int32_t cy = b.y + b.height / 2;

            if (m_axis == split_axis::horizontal)
                draw::fill(dc, {cx - size / 2, cy + i * spacing - size / 2, size, size}, dot);
            else
                draw::fill(dc, {cx + i * spacing - size / 2, cy - size / 2, size, size}, dot);
        }
    }
}
