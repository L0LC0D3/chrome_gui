#include <chrome/controls/core/dropdown.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 28;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_row_height = 24;
        constexpr std::int32_t k_padding = 8;
        // The width of the arrow at the right edge of the box.
        constexpr std::int32_t k_arrow = 24;
        // The space around the rows inside the list.
        constexpr std::int32_t k_inset = 2;
        constexpr std::int32_t k_bar_width = 4;
        constexpr std::int32_t k_label_gap = 8;

        constexpr std::uint32_t k_key_escape = 0x1B;
        constexpr std::uint32_t k_key_tab = 0x09;
        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_f4 = 0x73;

        [[nodiscard]] bool contains(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }
    }

    bool dropdown::add_item(const char* text) noexcept
    {
        if (m_count >= k_max_items)
            return false;

        m_items[m_count++] = text;
        return true;
    }

    dropdown& dropdown::clear_items() noexcept
    {
        m_count = 0;
        m_selected = k_none;
        m_highlight = k_none;
        m_first = 0;
        m_open = false;
        m_pos = 0.0f;
        return *this;
    }

    dropdown& dropdown::select(std::int32_t index) noexcept
    {
        m_selected = index >= 0 && index < static_cast<std::int32_t>(m_count) ? index : k_none;
        return *this;
    }

    const char* dropdown::selected_text() const noexcept
    {
        return m_selected != k_none ? m_items[m_selected] : nullptr;
    }

    void dropdown::close() noexcept
    {
        if (!m_open)
            return;

        m_open = false;
        invalidate();
    }

    void dropdown::open() noexcept
    {
        if (m_count == 0)
            return;

        m_open = true;
        m_highlight = m_selected;
        // A long list opens scrolled far enough to show the chosen row.
        m_first = 0;
        show_row(m_selected != k_none ? m_selected : 0);
        invalidate();
    }

    std::int32_t dropdown::visible_rows() const noexcept
    {
        return std::min(static_cast<std::int32_t>(m_count), static_cast<std::int32_t>(k_max_rows));
    }

    void dropdown::show_row(std::int32_t index) noexcept
    {
        const std::int32_t rows = visible_rows();
        const std::int32_t last_first = std::max(static_cast<std::int32_t>(m_count) - rows, 0);

        if (index < m_first)
            m_first = index;
        else if (index >= m_first + rows)
            m_first = index - rows + 1;

        m_first = std::clamp(m_first, 0, last_first);
    }

    control_size dropdown::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width) + (m_label != nullptr ? draw::px(m_label_width) + draw::px(k_label_gap) : 0), draw::px(k_height)};
    }

    dock_rect dropdown::box() const noexcept
    {
        dock_rect box = area();

        if (m_label != nullptr)
        {
            const std::int32_t taken = std::min(draw::px(m_label_width) + draw::px(k_label_gap), box.width);
            box.x += taken;
            box.width -= taken;
        }

        return box;
    }

    dock_rect dropdown::full_area() const noexcept
    {
        const dock_rect box = this->box();
        const std::int32_t height = visible_rows() * draw::px(k_row_height) + 2 * draw::px(k_inset);
        std::int32_t y = box.y + box.height + 1;

        // Not enough room below, and more above: the list opens upward.
        if (m_bounds.height > 0 && y + height > m_bounds.y + m_bounds.height)
        {
            const std::int32_t above = box.y - 1 - height;

            if (above >= m_bounds.y || box.y - m_bounds.y > m_bounds.y + m_bounds.height - y)
                y = above;
        }

        return {box.x, y, box.width, height};
    }

    dock_rect dropdown::overlay_area() const noexcept
    {
        const dock_rect full = full_area();

        if (m_pos >= 1.0f)
            return full;

        // The list grows out of the box, so the part that shows is the one next to it.
        const std::int32_t shown = std::max(static_cast<std::int32_t>(static_cast<float>(full.height) * m_pos + 0.5f), 1);
        const bool upward = full.y < area().y;
        return {full.x, upward ? full.y + full.height - shown : full.y, full.width, shown};
    }

    bool dropdown::animate(std::uint64_t now_ms) noexcept
    {
        const float target = m_open ? 1.0f : 0.0f;
        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
        m_stamp = now_ms;
        m_pos += (target - m_pos) * (1.0f - std::exp(-dt / 40.0f));
        invalidate();

        if (std::fabs(target - m_pos) < 0.01f)
        {
            m_pos = target;
            m_stamp = 0;
            return false;
        }

        return true;
    }

    std::int32_t dropdown::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect list = full_area();

        if (!contains(overlay_area(), x, y))
            return k_none;

        const std::int32_t row = (y - list.y - draw::px(k_inset)) / draw::px(k_row_height);

        if (y - list.y < draw::px(k_inset) || row >= visible_rows())
            return k_none;

        return m_first + row;
    }

    bool dropdown::point_at(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (!m_open)
            return false;

        const std::int32_t row = row_at(x, y);

        // Off the rows the highlight stays where it was, as the keys left it.
        if (row == k_none || row == m_highlight)
            return false;

        m_highlight = row;
        return true;
    }

    bool dropdown::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        return point_at(x, y);
    }

    bool dropdown::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        return point_at(x, y);
    }

    bool dropdown::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t last_first = std::max(static_cast<std::int32_t>(m_count) - visible_rows(), 0);
        const std::int32_t first = std::clamp(m_first - notches * 3, 0, last_first);
        const bool moved = first != m_first;
        m_first = first;
        // The row under the mouse changed with the scrolling.
        return point_at(m_x, m_y) || moved;
    }

    void dropdown::choose(std::int32_t index) noexcept
    {
        const bool changed = index != m_selected && index >= 0 && index < static_cast<std::int32_t>(m_count);
        close();

        if (!changed)
            return;

        m_selected = index;

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void dropdown::on_click() noexcept
    {
        if (!m_open)
        {
            open();
            return;
        }

        // A click on a row chooses it, and one anywhere else on the box closes the list.
        const std::int32_t row = row_at(m_x, m_y);

        if (row != k_none)
            choose(row);
        else
            close();
    }

    void dropdown::on_focus(bool gained) noexcept
    {
        if (!gained)
            close();
    }

    bool dropdown::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)ctrl;
        (void)shift;
        const std::int32_t count = static_cast<std::int32_t>(m_count);

        if (count == 0)
            return false;

        if (!m_open)
        {
            if ((key == k_key_down && alt) || key == k_key_f4 || key == k_key_space || key == k_key_enter)
            {
                open();
                return true;
            }

            std::int32_t target = m_selected;

            switch (key)
            {
            case k_key_up: target = m_selected == k_none ? 0 : m_selected - 1; break;
            case k_key_down: target = m_selected == k_none ? 0 : m_selected + 1; break;
            case k_key_home: target = 0; break;
            case k_key_end: target = count - 1; break;
            default: return false;
            }

            target = std::clamp(target, 0, count - 1);

            if (target != m_selected)
            {
                m_selected = target;
                invalidate();

                if (m_handler != nullptr)
                    m_handler(m_user, *this);
            }

            return true;
        }

        std::int32_t target = m_highlight == k_none ? 0 : m_highlight;

        switch (key)
        {
        case k_key_escape:
            close();
            return true;
        case k_key_tab:
            // Tab goes on to the next control, and the list closes as the focus leaves.
            close();
            return false;
        case k_key_enter:
        case k_key_space:
            choose(m_highlight);
            return true;
        case k_key_up: target -= 1; break;
        case k_key_down: target += 1; break;
        case k_key_page_up: target -= visible_rows(); break;
        case k_key_page_down: target += visible_rows(); break;
        case k_key_home: target = 0; break;
        case k_key_end: target = count - 1; break;
        default: return false;
        }

        m_highlight = std::clamp(target, 0, count - 1);
        show_row(m_highlight);
        invalidate();
        return true;
    }

    void dropdown::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool hot = this->hot();
        const bool pressed = this->pressed();
        const bool lit = focused() || m_open;
        std::uint32_t face = pressed ? blend_colour(colours.panel, black, 0.18f) : hot ? colours.hover : blend_colour(colours.panel, colours.hover, 0.55f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.5f);
        std::uint32_t ink = colours.text;
        std::uint32_t dim = colours.text_dim;

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            dim = ink;
        }

        const dock_rect box = this->box();

        if (m_label != nullptr)
            draw::text(dc, {area().x, area().y, draw::px(m_label_width), area().height}, m_label, enabled() ? colours.text : dim, draw::text_ellipsis);

        draw::rounded(dc, box, k_radius, face, edge);

        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t arrow = draw::px(k_arrow);
        const char* text = selected_text();
        const dock_rect text_area{box.x + padding, box.y, std::max(box.width - padding - arrow, 0), box.height};

        if (text != nullptr)
            draw::text(dc, text_area, text, ink, draw::text_ellipsis);
        else if (m_placeholder != nullptr)
            draw::text(dc, text_area, m_placeholder, dim, draw::text_ellipsis);

        // The arrow points down, or up while the list is open.
        const float cx = static_cast<float>(box.x + box.width) - static_cast<float>(arrow) * 0.5f;
        const float cy = static_cast<float>(box.y) + static_cast<float>(box.height) * 0.5f;
        const float half = static_cast<float>(draw::px(4));
        const float lift = m_open ? half * 0.5f : -half * 0.5f;
        const draw::point chevron[3] = {{cx - half, cy + lift}, {cx, cy - lift}, {cx + half, cy + lift}};
        draw::polyline(dc, chevron, 3, dim, 1.5f);
    }

    void dropdown::paint_overlay(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const dock_rect list = full_area();
        const dock_rect shown = overlay_area();
        draw::clip(dc, shown);
        draw::rounded(dc, list, k_radius, blend_colour(colours.panel, colours.title, 0.8f), colours.accent);

        const std::int32_t inset = draw::px(k_inset);
        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t rows = visible_rows();
        const bool scrolls = static_cast<std::int32_t>(m_count) > rows;
        const std::int32_t bar = scrolls ? draw::px(k_bar_width) + inset : 0;

        for (std::int32_t i = 0; i < rows; ++i)
        {
            const std::int32_t index = m_first + i;
            const dock_rect row{list.x + inset, list.y + inset + i * row_height, list.width - 2 * inset - bar, row_height};
            const bool highlighted = index == m_highlight;
            std::uint32_t ink = colours.text;

            if (highlighted)
            {
                draw::rounded(dc, row, 2, colours.accent, colours.accent);
                ink = white;
            }
            else if (index == m_selected)
            {
                const std::uint32_t tint = blend_colour(colours.accent, colours.panel, 0.7f);
                draw::rounded(dc, row, 2, tint, tint);
            }

            draw::text(dc, {row.x + padding, row.y, std::max(row.width - 2 * padding, 0), row.height}, m_items[index], ink, draw::text_ellipsis);
        }

        // A list longer than it shows has a thin bar down its right edge, to say where in it the rows are.
        if (scrolls)
        {
            const std::int32_t track = list.height - 2 * inset;
            const std::int32_t thumb = std::max(track * rows / static_cast<std::int32_t>(m_count), draw::px(12));
            const std::int32_t span = static_cast<std::int32_t>(m_count) - rows;
            const std::int32_t at = span > 0 ? (track - thumb) * m_first / span : 0;
            draw::rounded(dc, {list.x + list.width - inset - draw::px(k_bar_width), list.y + inset + at, draw::px(k_bar_width), thumb}, 2,
                blend_colour(colours.text_dim, black, 0.2f), blend_colour(colours.text_dim, black, 0.2f));
        }

        draw::unclip(dc);
    }
}
