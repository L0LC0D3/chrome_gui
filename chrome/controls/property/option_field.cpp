#include <chrome/controls/property/option_field.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 28;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_row_height = 24;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_arrow = 24;
        constexpr std::int32_t k_inset = 2;
        constexpr std::int32_t k_bar_width = 4;
        constexpr std::int32_t k_label_gap = 8;
        constexpr std::int32_t k_tick_box = 14;

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

    bool option_field::add_option(const char* name, std::uint32_t value) noexcept
    {
        if (m_count >= k_max_options)
            return false;

        m_names[m_count] = name;
        m_values[m_count] = value;
        ++m_count;
        rebuild_text();
        return true;
    }

    void option_field::clear_options() noexcept
    {
        m_count = 0;
        m_highlight = k_none;
        m_first = 0;
        m_open = false;
        m_pos = 0.0f;
        rebuild_text();
    }

    void option_field::set_bits(std::uint32_t value) noexcept
    {
        m_value = value;
        rebuild_text();
        invalidate();
    }

    std::uint32_t option_field::all_bits() const noexcept
    {
        std::uint32_t all = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
            all |= m_values[i];

        return all;
    }

    void option_field::rebuild_text() noexcept
    {
        m_text[0] = '\0';

        if (!m_flags)
        {
            for (std::uint32_t i = 0; i < m_count; ++i)
            {
                if (m_values[i] == m_value)
                {
                    std::snprintf(m_text, k_text_size, "%s", m_names[i]);
                    return;
                }
            }

            std::snprintf(m_text, k_text_size, "%d", static_cast<std::int32_t>(m_value));
            return;
        }

        const std::uint32_t all = all_bits();
        const std::uint32_t set = m_value & all;

        if (set == 0)
        {
            std::snprintf(m_text, k_text_size, "Nothing");
            return;
        }

        if (set == all)
        {
            std::snprintf(m_text, k_text_size, "Everything");
            return;
        }

        std::size_t length = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_values[i] == 0 || (m_value & m_values[i]) != m_values[i])
                continue;

            const std::size_t part = std::strlen(m_names[i]);
            const std::size_t need = part + (length > 0 ? 2 : 0);

            // Too many to fit: the rest is cut, and the box ellipsises what shows.
            if (length + need + 1 >= k_text_size)
                break;

            if (length > 0)
            {
                m_text[length++] = ',';
                m_text[length++] = ' ';
            }

            std::memcpy(m_text + length, m_names[i], part);
            length += part;
            m_text[length] = '\0';
        }
    }

    const char* option_field::row_text(std::int32_t row) const noexcept
    {
        if (m_flags)
        {
            if (row == 0)
                return "Nothing";

            if (row == 1)
                return "Everything";

            row -= 2;
        }

        return m_names[row];
    }

    bool option_field::row_on(std::int32_t row) const noexcept
    {
        if (!m_flags)
            return m_values[row] == m_value;

        const std::uint32_t all = all_bits();

        if (row == 0)
            return (m_value & all) == 0;

        if (row == 1)
            return all != 0 && (m_value & all) == all;

        const std::uint32_t mask = m_values[row - 2];
        return mask != 0 && (m_value & mask) == mask;
    }

    void option_field::close() noexcept
    {
        if (!m_open)
            return;

        m_open = false;
        invalidate();
    }

    void option_field::open() noexcept
    {
        if (m_count == 0)
            return;

        m_open = true;
        m_highlight = k_none;
        m_first = 0;

        // An enum opens on its value, so a long list shows it.
        if (!m_flags)
        {
            for (std::int32_t i = 0; i < row_count(); ++i)
            {
                if (row_on(i))
                {
                    m_highlight = i;
                    break;
                }
            }
        }

        show_row(m_highlight != k_none ? m_highlight : 0);
        invalidate();
    }

    std::int32_t option_field::visible_rows() const noexcept
    {
        return std::min(row_count(), static_cast<std::int32_t>(k_max_rows));
    }

    void option_field::show_row(std::int32_t index) noexcept
    {
        const std::int32_t rows = visible_rows();
        const std::int32_t last_first = std::max(row_count() - rows, 0);

        if (index < m_first)
            m_first = index;
        else if (index >= m_first + rows)
            m_first = index - rows + 1;

        m_first = std::clamp(m_first, 0, last_first);
    }

    control_size option_field::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width) + (m_label != nullptr ? draw::px(m_label_width) + draw::px(k_label_gap) : 0), draw::px(k_height)};
    }

    dock_rect option_field::box() const noexcept
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

    dock_rect option_field::full_area() const noexcept
    {
        const dock_rect box = this->box();
        const std::int32_t height = visible_rows() * draw::px(k_row_height) + 2 * draw::px(k_inset);
        std::int32_t y = box.y + box.height + 1;

        if (m_bounds.height > 0 && y + height > m_bounds.y + m_bounds.height)
        {
            const std::int32_t above = box.y - 1 - height;

            if (above >= m_bounds.y || box.y - m_bounds.y > m_bounds.y + m_bounds.height - y)
                y = above;
        }

        return {box.x, y, box.width, height};
    }

    dock_rect option_field::overlay_area() const noexcept
    {
        const dock_rect full = full_area();

        if (m_pos >= 1.0f)
            return full;

        const std::int32_t shown = std::max(static_cast<std::int32_t>(static_cast<float>(full.height) * m_pos + 0.5f), 1);
        const bool upward = full.y < area().y;
        return {full.x, upward ? full.y + full.height - shown : full.y, full.width, shown};
    }

    bool option_field::animate(std::uint64_t now_ms) noexcept
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

    std::int32_t option_field::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect list = full_area();

        if (!contains(overlay_area(), x, y))
            return k_none;

        const std::int32_t row = (y - list.y - draw::px(k_inset)) / draw::px(k_row_height);

        if (y - list.y < draw::px(k_inset) || row >= visible_rows())
            return k_none;

        return m_first + row;
    }

    bool option_field::point_at(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (!m_open)
            return false;

        const std::int32_t row = row_at(x, y);

        if (row == k_none || row == m_highlight)
            return false;

        m_highlight = row;
        return true;
    }

    bool option_field::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        return point_at(x, y);
    }

    bool option_field::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        return point_at(x, y);
    }

    bool option_field::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t last_first = std::max(row_count() - visible_rows(), 0);
        const std::int32_t first = std::clamp(m_first - notches * 3, 0, last_first);
        const bool moved = first != m_first;
        m_first = first;
        return point_at(m_x, m_y) || moved;
    }

    void option_field::set_from_user(std::uint32_t value) noexcept
    {
        if (value == m_value)
            return;

        set_bits(value);

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void option_field::act(std::int32_t row) noexcept
    {
        if (row < 0 || row >= row_count())
            return;

        if (!m_flags)
        {
            close();
            set_from_user(m_values[row]);
            return;
        }

        // The list stays open, so several can be ticked in a row.
        const std::uint32_t all = all_bits();

        if (row == 0)
            set_from_user(m_value & ~all);
        else if (row == 1)
            set_from_user(m_value | all);
        else
        {
            const std::uint32_t mask = m_values[row - 2];
            set_from_user(row_on(row) ? (m_value & ~mask) : (m_value | mask));
        }

        invalidate();
    }

    void option_field::on_click() noexcept
    {
        if (!m_open)
        {
            open();
            return;
        }

        const std::int32_t row = row_at(m_x, m_y);

        if (row != k_none)
            act(row);
        else
            close();
    }

    void option_field::on_focus(bool gained) noexcept
    {
        if (!gained)
            close();
    }

    bool option_field::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)ctrl;
        (void)shift;
        const std::int32_t count = row_count();

        if (count == 0)
            return false;

        if (!m_open)
        {
            if ((key == k_key_down && alt) || key == k_key_f4 || key == k_key_space || key == k_key_enter)
            {
                open();
                return true;
            }

            if (m_flags)
                return false;

            // An enum steps through its values with the arrows while it is closed.
            std::int32_t current = k_none;

            for (std::int32_t i = 0; i < count; ++i)
            {
                if (row_on(i))
                {
                    current = i;
                    break;
                }
            }

            std::int32_t target = current;

            switch (key)
            {
            case k_key_up: target = current == k_none ? 0 : current - 1; break;
            case k_key_down: target = current == k_none ? 0 : current + 1; break;
            case k_key_home: target = 0; break;
            case k_key_end: target = count - 1; break;
            default: return false;
            }

            target = std::clamp(target, 0, count - 1);
            set_from_user(m_values[target]);
            return true;
        }

        std::int32_t target = m_highlight == k_none ? 0 : m_highlight;

        switch (key)
        {
        case k_key_escape:
            close();
            return true;
        case k_key_tab:
            close();
            return false;
        case k_key_enter:
            if (m_flags)
                close();
            else
                act(m_highlight);

            return true;
        case k_key_space:
            act(m_highlight);
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

    void option_field::paint(void* dc) const noexcept
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
        const dock_rect text_area{box.x + padding, box.y, std::max(box.width - padding - arrow, 0), box.height};
        // Nothing and Everything are not choices of their own, so they are dimmer than names.
        const bool plain = m_flags && (std::strcmp(m_text, "Nothing") == 0 || std::strcmp(m_text, "Everything") == 0);
        draw::text(dc, text_area, m_text, plain ? dim : ink, draw::text_ellipsis);

        const float cx = static_cast<float>(box.x + box.width) - static_cast<float>(arrow) * 0.5f;
        const float cy = static_cast<float>(box.y) + static_cast<float>(box.height) * 0.5f;
        const float half = static_cast<float>(draw::px(4));
        const float lift = m_open ? half * 0.5f : -half * 0.5f;
        const draw::point chevron[3] = {{cx - half, cy + lift}, {cx, cy - lift}, {cx + half, cy + lift}};
        draw::polyline(dc, chevron, 3, dim, 1.5f);
    }

    void option_field::paint_overlay(void* dc) const noexcept
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
        const bool scrolls = row_count() > rows;
        const std::int32_t bar = scrolls ? draw::px(k_bar_width) + inset : 0;
        const std::int32_t tick = draw::px(k_tick_box);

        for (std::int32_t i = 0; i < rows; ++i)
        {
            const std::int32_t index = m_first + i;
            const dock_rect row{list.x + inset, list.y + inset + i * row_height, list.width - 2 * inset - bar, row_height};
            const bool highlighted = index == m_highlight;
            const bool on = row_on(index);
            std::uint32_t ink = colours.text;

            if (highlighted)
            {
                draw::rounded(dc, row, 2, colours.accent, colours.accent);
                ink = white;
            }
            else if (!m_flags && on)
            {
                const std::uint32_t tint = blend_colour(colours.accent, colours.panel, 0.7f);
                draw::rounded(dc, row, 2, tint, tint);
            }

            std::int32_t text_x = row.x + padding;

            // Flags have a box before each name, ticked when the bits are set.
            if (m_flags)
            {
                const dock_rect check{text_x, row.y + (row.height - tick) / 2, tick, tick};
                const std::uint32_t edge = highlighted ? white : colours.text_dim;
                draw::rounded(dc, check, 2, on ? (highlighted ? white : colours.accent) : blend_colour(colours.panel, black, 0.15f), on && !highlighted ? colours.accent : edge);

                if (on)
                {
                    const float x = static_cast<float>(check.x);
                    const float y = static_cast<float>(check.y);
                    const float u = static_cast<float>(tick) / 14.0f;
                    const draw::point mark[3] = {{x + 3.0f * u, y + 7.5f * u}, {x + 6.0f * u, y + 10.5f * u}, {x + 11.0f * u, y + 4.0f * u}};
                    draw::polyline(dc, mark, 3, highlighted ? colours.accent : white, 1.8f);
                }

                text_x += tick + padding;
            }

            const bool special = m_flags && index < 2;
            draw::text(dc, {text_x, row.y, std::max(row.x + row.width - padding - text_x, 0), row.height}, row_text(index),
                special && !highlighted ? colours.text_dim : ink, draw::text_ellipsis);
        }

        if (scrolls)
        {
            const std::int32_t track = list.height - 2 * inset;
            const std::int32_t thumb = std::max(track * rows / row_count(), draw::px(12));
            const std::int32_t span = row_count() - rows;
            const std::int32_t at = span > 0 ? (track - thumb) * m_first / span : 0;
            draw::rounded(dc, {list.x + list.width - inset - draw::px(k_bar_width), list.y + inset + at, draw::px(k_bar_width), thumb}, 2,
                blend_colour(colours.text_dim, black, 0.2f), blend_colour(colours.text_dim, black, 0.2f));
        }

        draw::unclip(dc);
    }
}
