#include <chrome/controls/core/breadcrumb_bar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 28;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_edge_pad = 4;
        constexpr std::int32_t k_seg_pad = 7;
        constexpr std::int32_t k_seg_inset = 3;
        constexpr std::int32_t k_arrow_width = 16;
        constexpr std::int32_t k_fold_width = 24;
        constexpr std::int32_t k_min_seg = 40;
        constexpr std::int32_t k_max_seg = 220;
        constexpr std::int32_t k_icon = 14;
        constexpr std::int32_t k_icon_gap = 5;
        constexpr std::int32_t k_empty_width = 320;
        // The list under an arrow.
        constexpr std::int32_t k_row_height = 24;
        constexpr std::int32_t k_list_width = 220;
        constexpr std::int32_t k_list_inset = 2;
        constexpr std::int32_t k_list_pad = 8;
        constexpr std::int32_t k_bar_width = 4;

        constexpr std::uint32_t k_key_tab = 0x09;
        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_escape = 0x1B;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_f2 = 0x71;
        constexpr std::uint32_t k_key_f4 = 0x73;
        constexpr std::uint32_t k_key_l = 0x4C;

        // The three dots that stand for the folded segments.
        constexpr char k_fold_text[] = "\xE2\x80\xA6";

        [[nodiscard]] bool contains(const dock_rect& box, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
        }
    }

    breadcrumb_bar& breadcrumb_bar::set_path(const char* path) noexcept
    {
        m_count = 0;
        close();
        m_stale = true;

        if (path != nullptr)
        {
            char current[k_title_length];
            std::uint32_t length = 0;

            for (const char* c = path;; ++c)
            {
                const bool end = *c == '\0';

                if (end || *c == '/' || *c == '\\')
                {
                    if (length > 0 && m_count < k_max_segments)
                    {
                        current[length] = '\0';
                        std::snprintf(m_titles[m_count++], k_title_length, "%s", current);
                    }

                    length = 0;

                    if (end)
                        break;

                    continue;
                }

                if (length + 1 < static_cast<std::uint32_t>(k_title_length))
                    current[length++] = *c;
            }
        }

        invalidate();
        return *this;
    }

    bool breadcrumb_bar::add_segment(const char* title) noexcept
    {
        if (m_count >= k_max_segments)
            return false;

        std::snprintf(m_titles[m_count++], k_title_length, "%s", title != nullptr ? title : "");
        m_stale = true;
        invalidate();
        return true;
    }

    breadcrumb_bar& breadcrumb_bar::truncate(std::int32_t count) noexcept
    {
        if (count >= 0 && count < m_count)
        {
            m_count = count;
            m_stale = true;
            m_hot = {};
            m_press = {};
            close();
            invalidate();
        }

        return *this;
    }

    breadcrumb_bar& breadcrumb_bar::clear() noexcept
    {
        return truncate(0);
    }

    std::uint32_t breadcrumb_bar::path(char* out, std::uint32_t capacity, std::int32_t count) const noexcept
    {
        if (capacity == 0)
            return 0;

        const std::int32_t last = count < 0 || count > m_count ? m_count : count;
        std::uint32_t length = 0;
        out[0] = '\0';

        for (std::int32_t i = 0; i < last; ++i)
        {
            const int written = std::snprintf(out + length, capacity - length, "%s%s", i > 0 ? "/" : "", m_titles[i]);

            if (written < 0 || static_cast<std::uint32_t>(written) >= capacity - length)
                return static_cast<std::uint32_t>(std::strlen(out));

            length += static_cast<std::uint32_t>(written);
        }

        return length;
    }

    void breadcrumb_bar::close() noexcept
    {
        if (!m_open)
            return;

        m_open = false;
        invalidate();
    }

    void breadcrumb_bar::measure(void* dc) const noexcept
    {
        const dock_rect& box = area();

        if (!m_stale && m_measured_width == box.width)
            return;

        m_stale = false;
        m_measured_width = box.width;
        m_first_shown = 0;

        if (m_count == 0)
            return;

        const std::int32_t arrow = draw::px(k_arrow_width);
        const std::int32_t fold = draw::px(k_fold_width) + arrow;
        const std::int32_t avail = std::max(box.width - 2 * draw::px(k_edge_pad), 0);
        const bool last_arrow = m_children != nullptr;
        std::int32_t wide[k_max_segments]{};

        for (std::int32_t i = 0; i < m_count; ++i)
        {
            std::int32_t w = 2 * draw::px(k_seg_pad) + draw::text_width(dc, m_titles[i], i == m_count - 1 ? draw::text_bold : 0);

            if (i == 0 && m_root_icon != icon_kind::none)
                w += draw::px(k_icon) + draw::px(k_icon_gap);

            wide[i] = std::clamp(w, draw::px(k_min_seg), draw::px(k_max_seg));
        }

        // The cost of showing the segments from one on, with the fold in front when some are left out.
        const auto cost = [&](std::int32_t from) {
            std::int32_t total = from > 0 ? fold : 0;

            for (std::int32_t i = from; i < m_count; ++i)
                total += wide[i] + (i < m_count - 1 || last_arrow ? arrow : 0);

            return total;
        };

        std::int32_t from = 0;

        while (from < m_count - 1 && cost(from) > avail)
            ++from;

        m_first_shown = from;

        // Even the last segment alone is too wide: it gives up the rest.
        if (cost(from) > avail)
        {
            const std::int32_t others = cost(from) - wide[from];
            wide[from] = std::max(avail - others, draw::px(k_min_seg));
        }

        std::int32_t x = box.x + draw::px(k_edge_pad);

        if (from > 0)
        {
            m_fold_x = x;
            x += fold;
        }

        for (std::int32_t i = from; i < m_count; ++i)
        {
            m_seg_x[i] = x;
            m_seg_w[i] = wide[i];
            x += wide[i];

            if (i < m_count - 1 || last_arrow)
            {
                m_arrow_x[i] = x;
                x += arrow;
            }
            else
            {
                m_arrow_x[i] = -1;
            }
        }
    }

    dock_rect breadcrumb_bar::segment_rect(std::int32_t segment) const noexcept
    {
        const dock_rect& box = area();
        return {m_seg_x[segment], box.y + draw::px(k_seg_inset), m_seg_w[segment], box.height - 2 * draw::px(k_seg_inset)};
    }

    dock_rect breadcrumb_bar::arrow_rect(std::int32_t segment) const noexcept
    {
        const dock_rect& box = area();
        return {m_arrow_x[segment], box.y + draw::px(k_seg_inset), draw::px(k_arrow_width), box.height - 2 * draw::px(k_seg_inset)};
    }

    dock_rect breadcrumb_bar::fold_rect() const noexcept
    {
        const dock_rect& box = area();
        return {m_fold_x, box.y + draw::px(k_seg_inset), draw::px(k_fold_width), box.height - 2 * draw::px(k_seg_inset)};
    }

    breadcrumb_bar::part breadcrumb_bar::find(std::int32_t x, std::int32_t y) const noexcept
    {
        part result;

        if (!contains(area(), x, y) || m_count == 0)
            return result;

        if (m_first_shown > 0 && contains(fold_rect(), x, y))
            return {part_kind::fold, -1};

        for (std::int32_t i = m_first_shown; i < m_count; ++i)
        {
            if (contains(segment_rect(i), x, y))
                return {part_kind::segment, i};

            if (m_arrow_x[i] >= 0 && m_children != nullptr && contains(arrow_rect(i), x, y))
                return {part_kind::arrow, i};
        }

        return result;
    }

    control_size breadcrumb_bar::preferred_size(void* dc) const noexcept
    {
        std::int32_t natural = draw::px(k_empty_width);

        if (m_count > 0)
        {
            natural = 2 * draw::px(k_edge_pad);

            for (std::int32_t i = 0; i < m_count; ++i)
            {
                std::int32_t w = 2 * draw::px(k_seg_pad) + draw::text_width(dc, m_titles[i], i == m_count - 1 ? draw::text_bold : 0);

                if (i == 0 && m_root_icon != icon_kind::none)
                    w += draw::px(k_icon) + draw::px(k_icon_gap);

                natural += std::clamp(w, draw::px(k_min_seg), draw::px(k_max_seg)) + (i < m_count - 1 || m_children != nullptr ? draw::px(k_arrow_width) : 0);
            }
        }

        return {m_width > 0 ? draw::px(m_width) : natural, draw::px(k_height)};
    }

    void breadcrumb_bar::paint(void* dc) const noexcept
    {
        measure(dc);
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool live = enabled();
        const dock_rect& box = area();
        const std::uint32_t face = live ? blend_colour(colours.panel, black, 0.22f) : blend_colour(colours.panel, colours.title, 0.5f);
        const std::uint32_t edge = !live ? blend_colour(colours.panel, black, 0.2f) : focused() || m_open ? colours.accent : blend_colour(colours.title, black, 0.5f);
        const std::uint32_t dim = live ? colours.text_dim : blend_colour(colours.text_dim, colours.panel, 0.45f);
        const std::uint32_t ink = live ? colours.text : dim;
        const std::uint32_t lift = blend_colour(face, colours.hover, 1.0f);

        draw::rounded(dc, box, k_radius, face, edge);

        if (m_count == 0)
            return;

        draw::clip(dc, {box.x + 1, box.y + 1, box.width - 2, box.height - 2});
        const float half = static_cast<float>(draw::px(4)) * 0.8f;

        // An arrow between segments points right, and down while its list is open.
        const auto chevron = [&](const dock_rect& cell, bool down, std::uint32_t colour) {
            const float cx = static_cast<float>(cell.x) + static_cast<float>(cell.width) * 0.5f;
            const float cy = static_cast<float>(cell.y) + static_cast<float>(cell.height) * 0.5f;

            if (down)
            {
                const draw::point points[3] = {{cx - half, cy - half * 0.5f}, {cx, cy + half * 0.5f}, {cx + half, cy - half * 0.5f}};
                draw::polyline(dc, points, 3, colour, 1.5f);
            }
            else
            {
                const draw::point points[3] = {{cx - half * 0.5f, cy - half}, {cx + half * 0.5f, cy}, {cx - half * 0.5f, cy + half}};
                draw::polyline(dc, points, 3, colour, 1.5f);
            }
        };

        if (m_first_shown > 0)
        {
            const dock_rect fold = fold_rect();
            const bool hot = live && m_hot.kind == part_kind::fold;
            const bool opened = m_open && m_open_for < 0;

            if (hot || opened)
                draw::rounded(dc, fold, k_radius, lift, lift);

            draw::text(dc, fold, k_fold_text, hot || opened ? ink : dim, draw::text_center);
            chevron({fold.x + fold.width, fold.y, draw::px(k_arrow_width), fold.height}, false, dim);
        }

        for (std::int32_t i = m_first_shown; i < m_count; ++i)
        {
            const dock_rect seg = segment_rect(i);
            const bool last = i == m_count - 1;
            const bool hot = live && m_hot.kind == part_kind::segment && m_hot.index == i;

            if (hot)
                draw::rounded(dc, seg, k_radius, pressed() && m_press.kind == part_kind::segment && m_press.index == i ? blend_colour(face, black, 0.2f) : lift, lift);

            std::int32_t x = seg.x + draw::px(k_seg_pad);

            if (i == 0 && m_root_icon != icon_kind::none)
            {
                const std::int32_t size = draw::px(k_icon);
                draw::icon(dc, m_root_icon, {x, seg.y + (seg.height - size) / 2, size, size}, dim);
                x += size + draw::px(k_icon_gap);
            }

            draw::text(dc, {x, seg.y, std::max(seg.x + seg.width - draw::px(k_seg_pad) - x, 0), seg.height}, m_titles[i],
                last || hot ? ink : dim, draw::text_ellipsis | (last ? draw::text_bold : 0));

            if (m_arrow_x[i] >= 0)
            {
                const dock_rect arrow = arrow_rect(i);
                const bool arrow_hot = live && m_hot.kind == part_kind::arrow && m_hot.index == i;
                const bool opened = m_open && m_open_for == i;

                if (arrow_hot || opened)
                    draw::rounded(dc, arrow, k_radius, lift, lift);

                chevron(arrow, opened, arrow_hot || opened ? ink : dim);
            }
        }

        draw::unclip(dc);
    }

    void breadcrumb_bar::raise(crumb_event what, std::int32_t segment, std::int32_t child) noexcept
    {
        if (m_handler != nullptr)
            m_handler(m_user, *this, what, segment, child);
    }

    std::int32_t breadcrumb_bar::visible_rows() const noexcept
    {
        return std::min(static_cast<std::int32_t>(m_name_count), k_max_rows);
    }

    void breadcrumb_bar::show_row(std::int32_t index) noexcept
    {
        const std::int32_t rows = visible_rows();
        const std::int32_t last_first = std::max(static_cast<std::int32_t>(m_name_count) - rows, 0);

        if (index < m_first)
            m_first = index;
        else if (index >= m_first + rows)
            m_first = index - rows + 1;

        m_first = std::clamp(m_first, 0, last_first);
    }

    void breadcrumb_bar::open_list(std::int32_t segment) noexcept
    {
        m_name_count = 0;

        if (segment < 0)
        {
            for (std::int32_t i = 0; i < m_first_shown && m_name_count < k_max_names; ++i)
                std::snprintf(m_names[m_name_count++], k_title_length, "%s", m_titles[i]);
        }
        else if (m_children != nullptr && segment < m_count)
        {
            const char* names[k_max_names]{};
            const std::uint32_t found = std::min(m_children(m_children_user, *this, segment, names, k_max_names), k_max_names);

            for (std::uint32_t i = 0; i < found; ++i)
                std::snprintf(m_names[m_name_count++], k_title_length, "%s", names[i] != nullptr ? names[i] : "");
        }

        if (m_name_count == 0)
            return;

        m_open = true;
        m_open_for = segment;
        m_highlight = -1;
        m_first = 0;
        invalidate();
    }

    std::int32_t breadcrumb_bar::anchor_x() const noexcept
    {
        return m_open_for < 0 ? m_fold_x : m_arrow_x[m_open_for] >= 0 ? m_arrow_x[m_open_for] - draw::px(k_arrow_width) / 2 : m_seg_x[m_open_for];
    }

    dock_rect breadcrumb_bar::full_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t width = draw::px(k_list_width);
        const std::int32_t height = visible_rows() * draw::px(k_row_height) + 2 * draw::px(k_list_inset);
        std::int32_t x = anchor_x();
        std::int32_t y = box.y + box.height + 1;

        if (m_bounds.width > 0)
            x = std::clamp(x, m_bounds.x, std::max(m_bounds.x + m_bounds.width - width, m_bounds.x));

        // Not enough room below, and more above: the list opens upward.
        if (m_bounds.height > 0 && y + height > m_bounds.y + m_bounds.height)
        {
            const std::int32_t above = box.y - 1 - height;

            if (above >= m_bounds.y || box.y - m_bounds.y > m_bounds.y + m_bounds.height - y)
                y = above;
        }

        return {x, y, width, height};
    }

    dock_rect breadcrumb_bar::overlay_area() const noexcept
    {
        return m_open ? full_area() : dock_rect{};
    }

    std::int32_t breadcrumb_bar::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (!m_open)
            return -1;

        const dock_rect list = full_area();

        if (!contains(list, x, y) || y - list.y < draw::px(k_list_inset))
            return -1;

        const std::int32_t row = (y - list.y - draw::px(k_list_inset)) / draw::px(k_row_height);
        return row < visible_rows() ? m_first + row : -1;
    }

    bool breadcrumb_bar::point_at(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (!m_open)
            return false;

        const std::int32_t row = row_at(x, y);

        // Off the rows the highlight stays where it was, as the keys left it.
        if (row < 0 || row == m_highlight)
            return false;

        m_highlight = row;
        return true;
    }

    void breadcrumb_bar::choose(std::int32_t row) noexcept
    {
        const std::int32_t segment = m_open_for;
        close();

        if (row < 0 || row >= static_cast<std::int32_t>(m_name_count))
            return;

        if (segment >= 0)
            raise(crumb_event::pick, segment, row);
        else
            raise(crumb_event::navigate, row, -1);
    }

    void breadcrumb_bar::paint_overlay(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const dock_rect list = full_area();
        draw::rounded(dc, list, k_radius, blend_colour(colours.panel, colours.title, 0.8f), colours.accent);

        const std::int32_t inset = draw::px(k_list_inset);
        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t padding = draw::px(k_list_pad);
        const std::int32_t rows = visible_rows();
        const bool scrolls = static_cast<std::int32_t>(m_name_count) > rows;
        const std::int32_t bar = scrolls ? draw::px(k_bar_width) + inset : 0;

        for (std::int32_t i = 0; i < rows; ++i)
        {
            const std::int32_t index = m_first + i;
            const dock_rect row{list.x + inset, list.y + inset + i * row_height, list.width - 2 * inset - bar, row_height};
            std::uint32_t ink = colours.text;

            if (index == m_highlight)
            {
                draw::rounded(dc, row, 2, colours.accent, colours.accent);
                ink = white;
            }

            draw::text(dc, {row.x + padding, row.y, std::max(row.width - 2 * padding, 0), row.height}, m_names[index], ink, draw::text_ellipsis);
        }

        if (scrolls)
        {
            const std::int32_t track = list.height - 2 * inset;
            const std::int32_t thumb = std::max(track * rows / static_cast<std::int32_t>(m_name_count), draw::px(12));
            const std::int32_t span = static_cast<std::int32_t>(m_name_count) - rows;
            const std::int32_t at = span > 0 ? (track - thumb) * m_first / span : 0;
            draw::rounded(dc, {list.x + list.width - inset - draw::px(k_bar_width), list.y + inset + at, draw::px(k_bar_width), thumb}, 2,
                blend_colour(colours.text_dim, black, 0.2f), blend_colour(colours.text_dim, black, 0.2f));
        }
    }

    void breadcrumb_bar::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        m_press = find(x, y);
    }

    bool breadcrumb_bar::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        return point_at(x, y);
    }

    bool breadcrumb_bar::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const bool moved = point_at(x, y);
        const part now = find(x, y);

        if (now == m_hot)
            return moved;

        m_hot = now;
        return true;
    }

    void breadcrumb_bar::on_leave() noexcept
    {
        m_hot = {};
        invalidate();
    }

    bool breadcrumb_bar::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;

        if (!m_open)
            return false;

        const std::int32_t last_first = std::max(static_cast<std::int32_t>(m_name_count) - visible_rows(), 0);
        const std::int32_t first = std::clamp(m_first - notches * 3, 0, last_first);
        const bool moved = first != m_first;
        m_first = first;
        return point_at(m_x, m_y) || moved;
    }

    void breadcrumb_bar::on_click() noexcept
    {
        const bool was_open = m_open;
        const std::int32_t was_for = m_open_for;

        if (was_open)
        {
            const std::int32_t row = row_at(m_x, m_y);

            if (row >= 0)
            {
                choose(row);
                return;
            }

            close();
        }

        const part now = find(m_x, m_y);
        const part pressed_on = m_press;
        m_press = {};

        if (!(now == pressed_on))
            return;

        switch (now.kind)
        {
        case part_kind::segment:
            if (now.index < m_count - 1)
                raise(crumb_event::navigate, now.index, -1);

            break;
        case part_kind::arrow:
            // The click that closed this arrow's list does not open it again.
            if (!(was_open && was_for == now.index))
                open_list(now.index);

            break;
        case part_kind::fold:
            if (!(was_open && was_for < 0))
                open_list(-1);

            break;
        case part_kind::none:
            if (contains(area(), m_x, m_y))
                raise(crumb_event::edit, -1, -1);

            break;
        }
    }

    void breadcrumb_bar::on_focus(bool gained) noexcept
    {
        if (!gained)
            close();

        invalidate();
    }

    bool breadcrumb_bar::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)shift;

        if (!enabled())
            return false;

        if (!m_open)
        {
            if (key == k_key_f2 || (ctrl && key == k_key_l))
            {
                raise(crumb_event::edit, -1, -1);
                return true;
            }

            if (alt && key == k_key_up)
            {
                if (m_count >= 2)
                    raise(crumb_event::navigate, m_count - 2, -1);

                return true;
            }

            if ((key == k_key_down || key == k_key_f4 || key == k_key_enter || key == k_key_space) && m_count > 0)
            {
                open_list(m_count - 1);
                return m_open;
            }

            return false;
        }

        const std::int32_t count = static_cast<std::int32_t>(m_name_count);
        std::int32_t target = m_highlight < 0 ? 0 : m_highlight;

        switch (key)
        {
        case k_key_escape:
            close();
            return true;
        case k_key_tab:
            close();
            return false;
        case k_key_enter:
        case k_key_space:
            choose(m_highlight);
            return true;
        case k_key_up: target -= 1; break;
        case k_key_down: target = m_highlight < 0 ? 0 : target + 1; break;
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
}
