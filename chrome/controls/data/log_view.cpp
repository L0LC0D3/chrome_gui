#include <chrome/controls/data/log_view.hpp>

#include <chrome/controls/core/clipboard.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_row_height = 21;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_wheel_rows = 3;
        constexpr std::int32_t k_icon_column = 16;
        constexpr std::int32_t k_column_gap = 8;

        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_insert = 0x2D;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_a = 0x41;
        constexpr std::uint32_t k_key_c = 0x43;

        constexpr std::uint32_t k_colour_info = 0x6CA6E0;
        constexpr std::uint32_t k_colour_debug = 0x8794A3;
        constexpr std::uint32_t k_colour_trace = 0x66707B;
        constexpr std::uint32_t k_colour_warn = 0xE2B340;
        constexpr std::uint32_t k_colour_error = 0xE5534B;
        constexpr std::uint32_t k_colour_fatal = 0xD35FD1;

        constexpr const char* k_severity_names[] = {"Trace", "Debug", "Info", "Warning", "Error", "Fatal"};

        // What a copy of the whole list can hold, in UTF-16 units. More is cut.
        constexpr std::int32_t k_copy_capacity = 1 << 17;
        wchar_t g_copy[k_copy_capacity];

        [[nodiscard]] std::uint32_t severity_colour(std::uint8_t severity) noexcept
        {
            switch (severity)
            {
            case 0: return k_colour_trace;
            case 1: return k_colour_debug;
            case 2: return k_colour_info;
            case 3: return k_colour_warn;
            case 4: return k_colour_error;
            default: return k_colour_fatal;
            }
        }

        // The mark at the left of a row: a disc for what is only said, a triangle for a warning, a disc with a cross for an error.
        void draw_mark(void* dc, std::uint8_t severity, float cx, float cy, std::uint32_t background) noexcept
        {
            const std::uint32_t colour = severity_colour(severity);
            const float r = static_cast<float>(draw::px(4));

            if (severity <= 1)
            {
                draw::disc(dc, cx, cy, r - 0.5f, background, colour, 1.4f);
            }
            else if (severity == 2)
            {
                draw::disc(dc, cx, cy, r, colour, colour, 0.0f);
            }
            else if (severity == 3)
            {
                const float h = r + 1.0f;
                const draw::point triangle[3] = {{cx, cy - h}, {cx + h + 0.5f, cy + h - 1.0f}, {cx - h - 0.5f, cy + h - 1.0f}};
                draw::polygon(dc, triangle, 3, colour);
            }
            else
            {
                const float big = r + 1.5f;
                draw::disc(dc, cx, cy, big, colour, severity == 5 ? 0xFFFFFF : colour, severity == 5 ? 1.2f : 0.0f);
                const float s = big * 0.42f;
                const draw::point a[2] = {{cx - s, cy - s}, {cx + s, cy + s}};
                const draw::point b[2] = {{cx + s, cy - s}, {cx - s, cy + s}};
                draw::polyline(dc, a, 2, 0xFFFFFF, 1.5f);
                draw::polyline(dc, b, 2, 0xFFFFFF, 1.5f);
            }
        }
    }

    log_view& log_view::set_source(row_fn source, void* user, std::uint32_t count) noexcept
    {
        m_source = source;
        m_source_user = user;
        m_count = source != nullptr ? count : 0;
        m_sel_lo = k_none;
        m_sel_hi = k_none;
        m_anchor = k_none;
        m_current = k_none;
        m_hover = k_none;
        m_first = m_follow && m_count > 0 ? max_first() : 0;
        invalidate();
        return *this;
    }

    log_view& log_view::set_count(std::uint32_t count) noexcept
    {
        m_count = m_source != nullptr ? count : 0;
        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;

        if (m_sel_lo > last)
        {
            m_sel_lo = k_none;
            m_sel_hi = k_none;
        }
        else if (m_sel_hi > last)
        {
            m_sel_hi = last;
        }

        if (m_anchor > last)
            m_anchor = k_none;

        if (m_current > last)
            m_current = k_none;

        m_hover = k_none;

        if (m_follow)
            m_first = max_first();
        else
            set_first(m_first);

        invalidate();
        return *this;
    }

    log_view& log_view::set_rows(std::uint32_t rows) noexcept
    {
        m_rows = std::clamp<std::uint32_t>(rows, 1, 64);
        return *this;
    }

    log_view& log_view::set_follow(bool follow) noexcept
    {
        m_follow = follow;

        if (follow && m_count > 0)
            m_first = max_first();

        invalidate();
        return *this;
    }

    log_view& log_view::select(std::int32_t first, std::int32_t last) noexcept
    {
        const std::int32_t end = static_cast<std::int32_t>(m_count) - 1;

        if (first < 0 || last < 0 || first > end || last > end)
            return clear_selection();

        m_sel_lo = std::min(first, last);
        m_sel_hi = std::max(first, last);
        m_anchor = first;
        m_current = last;
        invalidate();
        return *this;
    }

    log_view& log_view::select_all() noexcept
    {
        if (m_count == 0)
            return *this;

        m_sel_lo = 0;
        m_sel_hi = static_cast<std::int32_t>(m_count) - 1;
        m_anchor = 0;
        m_current = m_sel_hi;
        invalidate();
        return *this;
    }

    log_view& log_view::clear_selection() noexcept
    {
        m_sel_lo = k_none;
        m_sel_hi = k_none;
        m_anchor = k_none;
        m_current = k_none;
        invalidate();
        return *this;
    }

    dock_rect log_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    std::int32_t log_view::visible_rows() const noexcept
    {
        return std::max(inner().height / std::max(draw::px(k_row_height), 1), 1);
    }

    std::int32_t log_view::max_first() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_count) - visible_rows(), 0);
    }

    bool log_view::bar_needed() const noexcept
    {
        return static_cast<std::int32_t>(m_count) > visible_rows();
    }

    std::int32_t log_view::bar_left() const noexcept
    {
        const dock_rect box = inner();
        return box.x + box.width - draw::px(k_bar_column);
    }

    log_view::thumb_span log_view::thumb() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t rows = visible_rows();
        const std::int32_t count = std::max(static_cast<std::int32_t>(m_count), 1);
        thumb_span span;
        span.length = std::clamp(box.height * rows / count, std::min(draw::px(k_min_thumb), box.height), box.height);
        span.travel = box.height - span.length;
        const std::int32_t reach = max_first();
        span.start = box.y + (reach > 0 ? span.travel * m_first / reach : 0);
        return span;
    }

    std::int32_t log_view::row_at_point(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect box = inner();

        if (x < box.x || x >= box.x + box.width || y < box.y || y >= box.y + box.height)
            return k_none;

        const std::int32_t row = m_first + (y - box.y) / std::max(draw::px(k_row_height), 1);
        return row < static_cast<std::int32_t>(m_count) ? row : k_none;
    }

    std::int32_t log_view::row_near(std::int32_t y) const noexcept
    {
        if (m_count == 0)
            return k_none;

        const std::int32_t row = m_first + (y - inner().y) / std::max(draw::px(k_row_height), 1) - (y < inner().y ? 1 : 0);
        return std::clamp(row, 0, static_cast<std::int32_t>(m_count) - 1);
    }

    void log_view::set_first(std::int32_t first) noexcept
    {
        const std::int32_t clamped = std::clamp(first, 0, max_first());

        if (clamped == m_first)
            return;

        m_first = clamped;
        invalidate();
    }

    void log_view::set_follow_state(bool follow) noexcept
    {
        if (follow == m_follow)
            return;

        m_follow = follow;

        if (m_follow_handler != nullptr)
            m_follow_handler(m_follow_user, *this);
    }

    void log_view::scrolled_by_user() noexcept
    {
        set_follow_state(m_first >= max_first());
    }

    void log_view::reveal(std::int32_t index) noexcept
    {
        if (index < 0 || index >= static_cast<std::int32_t>(m_count))
            return;

        const std::int32_t rows = visible_rows();

        if (index < m_first)
            set_first(index);
        else if (index >= m_first + rows)
            set_first(index - rows + 1);
    }

    void log_view::scroll_to_end() noexcept
    {
        set_first(max_first());
        scrolled_by_user();
    }

    void log_view::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void log_view::move_current(std::int32_t row, bool extend) noexcept
    {
        if (extend)
        {
            if (m_anchor == k_none)
                m_anchor = m_current != k_none ? m_current : row;
        }
        else
        {
            m_anchor = row;
        }

        m_sel_lo = std::min(m_anchor, row);
        m_sel_hi = std::max(m_anchor, row);
        m_current = row;
        reveal(row);
    }

    control_size log_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size log_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {std::max(max_width, draw::px(160)), static_cast<std::int32_t>(m_rows) * draw::px(k_row_height) + 2 * draw::px(1)};
    }

    void log_view::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;

        if (bar_needed() && x >= bar_left())
        {
            const thumb_span span = thumb();

            if (y >= span.start && y < span.start + span.length)
            {
                m_drag = drag_kind::bar;
                m_grab = y - span.start;
            }
            else
            {
                set_first(m_first + (y < span.start ? -1 : 1) * std::max(visible_rows() - 1, 1));
                scrolled_by_user();
            }

            invalidate();
            return;
        }

        const std::int32_t row = row_at_point(x, y);

        if (row == k_none)
            return;

        m_drag = drag_kind::rows;
        move_current(row, shift_down());
        notify_change();
    }

    bool log_view::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        (void)x;

        if (m_drag == drag_kind::bar)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - inner().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_first;
            set_first(span.travel > 0 ? (at * max_first() + span.travel / 2) / span.travel : 0);
            scrolled_by_user();
            return m_first != before;
        }

        if (m_drag != drag_kind::rows || m_count == 0)
            return false;

        // Dragging past the top or bottom edge moves the rows along.
        const dock_rect box = inner();

        if (y < box.y)
            set_first(m_first - 1);
        else if (y >= box.y + box.height)
            set_first(m_first + 1);

        scrolled_by_user();
        const std::int32_t row = row_near(std::clamp(y, box.y, box.y + box.height - 1));

        if (row == k_none || row == m_current)
            return false;

        const std::int32_t lo = m_sel_lo;
        const std::int32_t hi = m_sel_hi;
        move_current(row, true);

        if (lo != m_sel_lo || hi != m_sel_hi)
            notify_change();

        return true;
    }

    bool log_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t row = bar_needed() && x >= bar_left() ? k_none : row_at_point(x, y);

        if (row == m_hover)
            return false;

        m_hover = row;
        return true;
    }

    bool log_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t before = m_first;
        set_first(m_first - notches * k_wheel_rows);
        scrolled_by_user();
        return m_first != before;
    }

    bool log_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_count == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
        const std::int32_t page = std::max(visible_rows() - 1, 1);
        // With nothing chosen, the keys start from the end the view is following, or from the top row on show.
        const std::int32_t from = m_current != k_none ? m_current : (m_follow ? last : m_first);
        std::int32_t target = k_none;

        switch (key)
        {
        case k_key_up:
            target = m_current == k_none ? from : std::max(from - 1, 0);
            break;

        case k_key_down:
            target = m_current == k_none ? from : std::min(from + 1, last);
            break;

        case k_key_page_up:
            target = std::max(from - page, 0);
            break;

        case k_key_page_down:
            target = std::min(from + page, last);
            break;

        case k_key_home:
            target = 0;
            break;

        case k_key_end:
            target = last;
            break;

        case k_key_a:
            if (!ctrl)
                return false;

            select_all();
            notify_change();
            return true;

        case k_key_c:
        case k_key_insert:
            if (!ctrl)
                return false;

            (void)copy_selection();
            return true;

        default:
            return false;
        }

        move_current(target, shift);
        scrolled_by_user();
        notify_change();
        return true;
    }

    bool log_view::copy_selection() const noexcept
    {
        if (m_sel_lo == k_none || m_source == nullptr)
            return false;

        std::int32_t used = 0;

        for (std::int32_t i = m_sel_lo; i <= m_sel_hi && used < k_copy_capacity - 2; ++i)
        {
            log_row row;
            m_source(m_source_user, static_cast<std::uint32_t>(i), row);
            char line[600];
            const char* severity = k_severity_names[std::min<std::uint8_t>(row.severity, 5)];
            int length = std::snprintf(line, sizeof(line), "[%s] [%s] [%s] %s", row.time != nullptr ? row.time : "", severity,
                row.tag != nullptr ? row.tag : "", row.text != nullptr ? row.text : "");

            if (row.count > 1 && length > 0 && length < static_cast<int>(sizeof(line)))
                length += std::snprintf(line + length, sizeof(line) - static_cast<std::size_t>(length), " (x%u)", row.count);

            const int wide = MultiByteToWideChar(CP_UTF8, 0, line, -1, g_copy + used, k_copy_capacity - used - 2);

            if (wide <= 1)
                break;

            used += wide - 1;
            g_copy[used++] = L'\r';
            g_copy[used++] = L'\n';
        }

        // The last line break is not part of the text.
        if (used >= 2)
            used -= 2;

        g_copy[used] = L'\0';
        return used > 0 && clipboard::set(g_copy, used);
    }

    void log_view::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();
        const std::uint32_t face = blend_colour(colours.panel, black, 0.14f);
        const std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.5f);

        draw::rounded(dc, area(), k_radius, face, edge);

        const dock_rect box = inner();
        draw::clip(dc, box);

        // The size may have changed since the count was set (or not been known yet), so the first row is made to fit now.
        m_first = m_follow ? max_first() : std::clamp(m_first, 0, max_first());

        if (m_count == 0 && m_empty != nullptr)
            draw::text(dc, box, m_empty, colours.text_dim, draw::text_center);

        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t gap = draw::px(k_column_gap);
        const std::int32_t rows = visible_rows();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::uint32_t stripe = blend_colour(face, white, 0.025f);
        const std::int32_t time_width = m_show_time ? draw::text_width(dc, "00:00:00.000") : 0;
        const std::int32_t tag_width = m_show_tag ? draw::text_width(dc, "memory") + draw::px(4) : 0;

        for (std::int32_t i = 0; i < rows + 1; ++i)
        {
            const std::int32_t index = m_first + i;

            if (index >= static_cast<std::int32_t>(m_count))
                break;

            log_row data;
            m_source(m_source_user, static_cast<std::uint32_t>(index), data);
            const std::uint8_t severity = std::min<std::uint8_t>(data.severity, 5);
            const dock_rect row{box.x, box.y + i * row_height, box.width - bar, row_height};
            const bool selected = is_selected(index) && enabled();
            const bool hover = hot() && index == m_hover && enabled();

            // A row is tinted by its severity, a selected one by the accent.
            std::uint32_t background = face;

            if (selected)
            {
                background = lit ? colours.accent : blend_colour(colours.accent, colours.panel, 0.55f);
            }
            else
            {
                if ((index & 1) != 0)
                    background = stripe;

                if (severity == 3)
                    background = blend_colour(background, k_colour_warn, 0.09f);
                else if (severity >= 4)
                    background = blend_colour(background, severity == 4 ? k_colour_error : k_colour_fatal, 0.12f);

                if (hover)
                    background = blend_colour(background, colours.hover, 0.5f);
            }

            if (background != face)
                draw::fill(dc, row, background);

            if (lit && index == m_current)
                draw::outline(dc, {row.x, row.y, row.width, row.height}, 2, selected ? blend_colour(colours.accent, white, 0.45f) : colours.accent, 1.0f);

            std::uint32_t ink = colours.text;

            if (selected && lit)
                ink = white;
            else if (severity <= 1)
                ink = colours.text_dim;
            else if (severity == 3)
                ink = blend_colour(colours.text, k_colour_warn, 0.55f);
            else if (severity >= 4)
                ink = blend_colour(colours.text, severity == 4 ? k_colour_error : k_colour_fatal, 0.5f);

            const std::uint32_t quiet = selected && lit ? blend_colour(white, colours.accent, 0.25f) : colours.text_dim;
            const float cx = static_cast<float>(row.x + padding + draw::px(5));
            const float cy = static_cast<float>(row.y) + static_cast<float>(row.height) * 0.5f;
            draw_mark(dc, severity, cx, cy, background);

            std::int32_t x = row.x + padding + draw::px(k_icon_column);
            const std::int32_t right = row.x + row.width - padding / 2;

            if (m_show_time && data.time != nullptr)
            {
                draw::text(dc, {x, row.y, time_width, row.height}, data.time, quiet);
                x += time_width + gap;
            }

            if (m_show_tag && data.tag != nullptr)
            {
                draw::text(dc, {x, row.y, tag_width, row.height}, data.tag, quiet, draw::text_ellipsis);
                x += tag_width + gap;
            }

            std::int32_t text_right = right;

            if (data.count > 1)
            {
                char label[16] = "99999+";

                if (data.count <= 99999)
                    std::snprintf(label, sizeof(label), "%u", data.count);

                const std::int32_t badge_width = draw::text_width(dc, label) + draw::px(12);
                const dock_rect badge{right - badge_width, row.y + draw::px(3), badge_width, row.height - draw::px(6)};
                const std::uint32_t badge_face = selected && lit ? blend_colour(colours.accent, black, 0.35f) : blend_colour(face, white, 0.16f);
                draw::rounded(dc, badge, 8, badge_face, badge_face);
                draw::text(dc, badge, label, selected && lit ? white : colours.text, draw::text_center);
                text_right = badge.x - gap;
            }

            if (data.text != nullptr)
                draw::text(dc, {x, row.y, std::max(text_right - x, 0), row.height}, data.text, ink, draw::text_ellipsis);
        }

        if (bar > 0)
        {
            const thumb_span span = thumb();
            const std::int32_t width = draw::px(k_thumb_width);
            const bool grabbed = m_drag == drag_kind::bar;
            const std::uint32_t thumb_colour = grabbed ? colours.text_dim : blend_colour(colours.text_dim, face, hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {box.x + box.width - (bar + width) / 2, span.start, width, span.length}, 2, thumb_colour, thumb_colour);
        }

        draw::unclip(dc);
    }
}
