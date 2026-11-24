#include <chrome/controls/core/checkbox_list.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_row_height = 22;
        constexpr std::int32_t k_header_height = 24;
        constexpr std::int32_t k_padding = 6;
        constexpr std::int32_t k_box = 14;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_column_gap = 10;
        constexpr std::int32_t k_radius = 3;
        // What a column needs besides its widest label, for a number at the right.
        constexpr std::int32_t k_number_room = 32;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_down = 0x28;
    }

    std::int32_t checkbox_list::add_row(const char* label, row_kind kind, bool checked) noexcept
    {
        if (m_rows >= k_max_rows)
            return k_none;

        m_row[m_rows] = {label, kind, checked, m_break_next && m_rows > 0, -1};
        m_break_next = false;
        return static_cast<std::int32_t>(m_rows++);
    }

    std::int32_t checkbox_list::add_header(const char* label) noexcept
    {
        return add_row(label, row_kind::header, false);
    }

    std::int32_t checkbox_list::add_item(const char* label, bool checked) noexcept
    {
        return add_row(label, row_kind::item, checked);
    }

    std::int32_t checkbox_list::add_link(const char* label) noexcept
    {
        return add_row(label, row_kind::link, false);
    }

    checkbox_list& checkbox_list::set_checked(std::uint32_t index, bool checked) noexcept
    {
        if (index < m_rows && m_row[index].kind == row_kind::item)
        {
            m_row[index].checked = checked;
            invalidate();
        }

        return *this;
    }

    checkbox_list& checkbox_list::set_item_count(std::uint32_t index, std::int32_t count) noexcept
    {
        if (index < m_rows && m_row[index].count != count)
        {
            m_row[index].count = count;
            invalidate();
        }

        return *this;
    }

    std::uint32_t checkbox_list::columns() const noexcept
    {
        std::uint32_t count = 1;

        for (std::uint32_t i = 0; i < m_rows; ++i)
        {
            if (m_row[i].break_before)
                ++count;
        }

        return count;
    }

    namespace
    {
        [[nodiscard]] std::int32_t height_of_kind(bool header) noexcept
        {
            return header ? draw::px(k_header_height) : draw::px(k_row_height);
        }
    }

    dock_rect checkbox_list::place(std::uint32_t index) const noexcept
    {
        const dock_rect& whole = area();
        const std::int32_t count = static_cast<std::int32_t>(columns());
        const std::int32_t column_gap = draw::px(k_column_gap);
        const std::int32_t column_width = std::max((whole.width - (count - 1) * column_gap) / count, 0);
        std::int32_t column = 0;
        std::int32_t top = 0;

        for (std::uint32_t i = 0; i <= index && i < m_rows; ++i)
        {
            if (m_row[i].break_before)
            {
                ++column;
                top = 0;
            }

            const std::int32_t height = height_of_kind(m_row[i].kind == row_kind::header);

            if (i == index)
                return {whole.x + column * (column_width + column_gap), whole.y + top, column_width, height};

            top += height;
        }

        return {};
    }

    std::int32_t checkbox_list::content_height() const noexcept
    {
        std::int32_t tallest = 0;
        std::int32_t top = 0;

        for (std::uint32_t i = 0; i < m_rows; ++i)
        {
            if (m_row[i].break_before)
                top = 0;

            top += height_of_kind(m_row[i].kind == row_kind::header);
            tallest = std::max(tallest, top);
        }

        return tallest;
    }

    control_size checkbox_list::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size checkbox_list::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        std::int32_t widest = 0;

        for (std::uint32_t i = 0; i < m_rows; ++i)
            widest = std::max(widest, draw::text_width(dc, m_row[i].label != nullptr ? m_row[i].label : ""));

        const std::int32_t count = static_cast<std::int32_t>(columns());
        const std::int32_t column = widest + 2 * draw::px(k_padding) + draw::px(k_box) + draw::px(k_gap) + draw::px(k_number_room);
        const std::int32_t natural = count * column + (count - 1) * draw::px(k_column_gap);
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, natural);
        return {width, content_height()};
    }

    std::int32_t checkbox_list::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        for (std::uint32_t i = 0; i < m_rows; ++i)
        {
            const dock_rect box = place(i);

            if (x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height)
                return static_cast<std::int32_t>(i);
        }

        return k_none;
    }

    std::int32_t checkbox_list::step(std::int32_t from, std::int32_t direction) const noexcept
    {
        for (std::int32_t i = from + direction; i >= 0 && i < static_cast<std::int32_t>(m_rows); i += direction)
        {
            if (m_row[i].kind != row_kind::header)
                return i;
        }

        return k_none;
    }

    void checkbox_list::flip(std::int32_t index) noexcept
    {
        if (index < 0 || index >= static_cast<std::int32_t>(m_rows) || m_row[index].kind == row_kind::header)
            return;

        if (m_row[index].kind == row_kind::item)
            m_row[index].checked = !m_row[index].checked;

        invalidate();

        if (m_handler != nullptr)
            m_handler(m_user, *this, static_cast<std::uint32_t>(index));
    }

    void checkbox_list::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t index = row_at(x, y);

        if (index == k_none || m_row[index].kind == row_kind::header)
            return;

        m_current = index;
        flip(index);
    }

    bool checkbox_list::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t index = row_at(x, y);
        const std::int32_t over = index != k_none && m_row[index].kind != row_kind::header ? index : k_none;

        if (over == m_hover)
            return false;

        m_hover = over;
        return true;
    }

    void checkbox_list::on_focus(bool gained) noexcept
    {
        if (gained && m_current == k_none)
            m_current = step(k_none, 1);

        invalidate();
    }

    bool checkbox_list::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)ctrl;
        (void)shift;

        if (alt || m_rows == 0)
            return false;

        std::int32_t target = k_none;

        switch (key)
        {
        case k_key_up:
            target = step(m_current == k_none ? static_cast<std::int32_t>(m_rows) : m_current, -1);
            break;

        case k_key_down:
            target = step(m_current, 1);
            break;

        case k_key_home:
            target = step(k_none, 1);
            break;

        case k_key_end:
            target = step(static_cast<std::int32_t>(m_rows), -1);
            break;

        case k_key_space:
        case k_key_enter:
            flip(m_current);
            return true;

        default:
            return false;
        }

        if (target != k_none)
            m_current = target;

        invalidate();
        return true;
    }

    void checkbox_list::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool live = enabled();
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t box_size = draw::px(k_box);

        for (std::uint32_t i = 0; i < m_rows; ++i)
        {
            const row& item = m_row[i];
            const dock_rect line = place(i);
            const char* label = item.label != nullptr ? item.label : "";

            if (item.kind == row_kind::header)
            {
                const std::int32_t text_w = draw::text_width(dc, label);
                draw::text(dc, {line.x + padding, line.y + draw::px(4), text_w + 2, line.height - draw::px(4)}, label, colours.text_dim);
                const std::int32_t rule_x = line.x + padding + text_w + draw::px(8);
                draw::fill(dc, {rule_x, line.y + line.height / 2 + draw::px(2), std::max(line.x + line.width - padding - rule_x, 0), 1}, blend_colour(colours.panel, black, 0.3f));
                continue;
            }

            const bool over = hot() && static_cast<std::int32_t>(i) == m_hover && live;

            if (over)
                draw::rounded(dc, line, k_radius, colours.hover, colours.hover);

            if (focus_visible() && static_cast<std::int32_t>(i) == m_current)
                draw::outline(dc, line, k_radius, colours.accent, 1.0f);

            if (item.kind == row_kind::link)
            {
                draw::text(dc, {line.x + padding, line.y, std::max(line.width - 2 * padding, 0), line.height}, label,
                    live ? blend_colour(colours.accent, white, 0.35f) : colours.text_dim, over ? draw::text_underline : 0);
                continue;
            }

            const dock_rect box{line.x + padding, line.y + (line.height - box_size) / 2, box_size, box_size};
            const std::uint32_t face = item.checked && live ? colours.accent : blend_colour(colours.panel, black, 0.28f);
            const std::uint32_t edge = item.checked && live ? blend_colour(colours.accent, black, 0.35f) : blend_colour(colours.text_dim, colours.panel, 0.5f);
            draw::rounded(dc, box, 3, face, edge);

            if (item.checked)
                draw::icon(dc, icon_kind::check, {box.x + draw::px(2), box.y + draw::px(2), box_size - draw::px(4), box_size - draw::px(4)}, white);

            std::int32_t right = line.x + line.width - padding;

            if (item.count >= 0)
            {
                char number[16];
                std::snprintf(number, sizeof(number), "%d", item.count);
                const std::int32_t number_w = draw::text_width(dc, number);
                draw::text(dc, {right - number_w, line.y, number_w + 2, line.height}, number, colours.text_dim);
                right -= number_w + draw::px(k_gap);
            }

            const std::int32_t text_x = box.x + box_size + draw::px(k_gap);
            draw::text(dc, {text_x, line.y, std::max(right - text_x, 0), line.height}, label, live ? colours.text : colours.text_dim, draw::text_ellipsis);
        }
    }
}
