#include <chrome/controls/data/table_view.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/drag_drop.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_header_height = 24;
        constexpr std::int32_t k_row_height = 22;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_wheel_rows = 3;
        // How near the edge between two titles the mouse has to be to take it, and the least a column can be.
        constexpr std::int32_t k_edge_reach = 4;
        constexpr std::int32_t k_min_column = 32;
        constexpr std::int32_t k_arrow_size = 4;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_a = 0x41;

        [[nodiscard]] bool contains(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }

        [[nodiscard]] char lower(char c) noexcept
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        // Numbers by value, so 9 comes before 10, and anything else without regard to case.
        [[nodiscard]] std::int32_t compare_text(const char* a, const char* b) noexcept
        {
            if (a == nullptr || b == nullptr)
                return (a == nullptr ? 0 : 1) - (b == nullptr ? 0 : 1);

            double number_a = 0.0;
            double number_b = 0.0;
            const bool is_a = std::from_chars(a, a + std::strlen(a), number_a).ec == std::errc{};
            const bool is_b = std::from_chars(b, b + std::strlen(b), number_b).ec == std::errc{};

            if (is_a && is_b && number_a != number_b)
                return number_a < number_b ? -1 : 1;

            for (; *a != '\0' && *b != '\0'; ++a, ++b)
            {
                if (lower(*a) != lower(*b))
                    return lower(*a) < lower(*b) ? -1 : 1;
            }

            if (*a != *b)
                return *a == '\0' ? -1 : 1;

            return std::strcmp(a, b) < 0 ? -1 : (std::strcmp(a, b) > 0 ? 1 : 0);
        }
    }

    bool table_view::add_column(const char* title, std::int32_t width, column_align align, bool sortable) noexcept
    {
        if (m_columns >= k_max_columns)
            return false;

        column& item = m_cols[m_columns++];
        item.title = title;
        item.width = std::max(width, k_min_column);
        item.align = align;
        item.sortable = sortable;
        invalidate();
        return true;
    }

    table_view& table_view::clear_columns() noexcept
    {
        m_columns = 0;
        m_sort_column = k_none;
        m_sort_dir = sort_direction::none;
        sort_rows();
        invalidate();
        return *this;
    }

    table_view& table_view::set_source(cell_fn source, void* user, std::uint32_t count) noexcept
    {
        m_source = source;
        m_source_user = user;
        m_count = source != nullptr ? std::min(count, k_max_rows) : 0;
        std::memset(m_bits, 0, sizeof(m_bits));
        m_selected_count = 0;
        m_current = k_none;
        m_anchor = k_none;
        m_hover = k_none;
        m_first = 0;
        sort_rows();
        invalidate();
        return *this;
    }

    table_view& table_view::set_count(std::uint32_t count) noexcept
    {
        m_count = std::min(count, k_max_rows);

        for (std::uint32_t i = m_count; i < k_max_rows; ++i)
            set_bit(i, false);

        recount();

        if (m_current >= static_cast<std::int32_t>(m_count))
            m_current = k_none;

        if (m_anchor >= static_cast<std::int32_t>(m_count))
            m_anchor = k_none;

        m_hover = k_none;
        sort_rows();
        set_first(m_first);
        invalidate();
        return *this;
    }

    table_view& table_view::set_select_mode(list_select_mode mode) noexcept
    {
        m_mode = mode;

        if (mode == list_select_mode::single && m_selected_count > 1)
            select(first_selected());

        return *this;
    }

    table_view& table_view::set_rows(std::uint32_t rows) noexcept
    {
        m_rows = std::clamp<std::uint32_t>(rows, 1, 64);
        return *this;
    }

    table_view& table_view::set_column_width(std::uint32_t column, std::int32_t width) noexcept
    {
        if (column < m_columns)
        {
            m_cols[column].width = std::max(width, k_min_column);
            invalidate();
        }

        return *this;
    }

    const char* table_view::cell(std::uint32_t row, std::uint32_t column) const noexcept
    {
        return m_source != nullptr && row < m_count && column < m_columns ? m_source(m_source_user, row, column) : nullptr;
    }

    std::int32_t table_view::compare_rows(std::uint32_t column, std::uint32_t a, std::uint32_t b) const noexcept
    {
        if (m_compare != nullptr)
            return m_compare(m_compare_user, column, a, b);

        // The first text has to be used before the second is asked for, as a source may reuse a few buffers.
        const char* text_a = cell(a, column);
        char first[96]{};

        if (text_a != nullptr)
            std::strncpy(first, text_a, sizeof(first) - 1);

        return compare_text(text_a != nullptr ? first : nullptr, cell(b, column));
    }

    void table_view::sort_rows() noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
            m_order[i] = i;

        if (m_sort_dir != sort_direction::none && m_sort_column != k_none && m_source != nullptr && m_count > 1)
        {
            const std::uint32_t column = static_cast<std::uint32_t>(m_sort_column);
            const bool up = m_sort_dir == sort_direction::ascending;

            // Rows taken from the left unless the right one is strictly first, so rows that are equal keep their order.
            const auto right_first = [&](std::uint32_t right, std::uint32_t left) noexcept {
                const std::int32_t order = compare_rows(column, right, left);
                return up ? order < 0 : order > 0;
            };

            for (std::uint32_t width = 1; width < m_count; width *= 2)
            {
                for (std::uint32_t start = 0; start < m_count; start += 2 * width)
                {
                    const std::uint32_t middle = std::min(start + width, m_count);
                    const std::uint32_t end = std::min(start + 2 * width, m_count);
                    std::uint32_t left = start;
                    std::uint32_t right = middle;
                    std::uint32_t out = start;

                    while (left < middle && right < end)
                        m_scratch[out++] = right_first(m_order[right], m_order[left]) ? m_order[right++] : m_order[left++];

                    while (left < middle)
                        m_scratch[out++] = m_order[left++];

                    while (right < end)
                        m_scratch[out++] = m_order[right++];
                }

                std::memcpy(m_order, m_scratch, m_count * sizeof(std::uint32_t));
            }
        }

        for (std::uint32_t i = 0; i < m_count; ++i)
            m_position[m_order[i]] = i;
    }

    table_view& table_view::sort_by(std::uint32_t column, sort_direction direction) noexcept
    {
        if (direction == sort_direction::none)
        {
            m_sort_column = k_none;
            m_sort_dir = direction;
        }
        else if (column < m_columns && m_cols[column].sortable)
        {
            m_sort_column = static_cast<std::int32_t>(column);
            m_sort_dir = direction;
        }
        else
            return *this;

        sort_rows();
        invalidate();
        return *this;
    }

    void table_view::cycle_sort(std::uint32_t column) noexcept
    {
        if (column >= m_columns || !m_cols[column].sortable)
            return;

        // Ascending, then descending, then the source's own order.
        if (m_sort_column != static_cast<std::int32_t>(column))
            sort_by(column, sort_direction::ascending);
        else if (m_sort_dir == sort_direction::ascending)
            sort_by(column, sort_direction::descending);
        else
            sort_by(column, sort_direction::none);

        // The row the keyboard is on stays in view where the sort moved it.
        if (m_current != k_none)
            reveal(static_cast<std::uint32_t>(m_current));

        if (m_sort != nullptr)
            m_sort(m_sort_user, *this, column, m_sort_dir);
    }

    void table_view::set_bit(std::uint32_t row, bool on) noexcept
    {
        if (row >= k_max_rows)
            return;

        const std::uint64_t mask = std::uint64_t{1} << (row & 63);

        if (on)
            m_bits[row >> 6] |= mask;
        else
            m_bits[row >> 6] &= ~mask;
    }

    void table_view::recount() noexcept
    {
        std::uint32_t total = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
            total += is_selected(i) ? 1u : 0u;

        m_selected_count = total;
    }

    bool table_view::is_selected(std::uint32_t row) const noexcept
    {
        return row < m_count && (m_bits[row >> 6] >> (row & 63) & 1) != 0;
    }

    std::int32_t table_view::first_selected() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (is_selected(m_order[i]))
                return static_cast<std::int32_t>(m_order[i]);
        }

        return k_none;
    }

    table_view& table_view::select(std::int32_t row) noexcept
    {
        std::memset(m_bits, 0, sizeof(m_bits));

        if (row >= 0 && row < static_cast<std::int32_t>(m_count))
        {
            set_bit(static_cast<std::uint32_t>(row), true);
            m_current = row;
            m_anchor = row;
            reveal(static_cast<std::uint32_t>(row));
        }

        recount();
        invalidate();
        return *this;
    }

    table_view& table_view::add_to_selection(std::uint32_t row) noexcept
    {
        if (row >= m_count)
            return *this;

        if (m_mode == list_select_mode::single)
            return select(static_cast<std::int32_t>(row));

        set_bit(row, true);
        recount();
        invalidate();
        return *this;
    }

    table_view& table_view::select_all() noexcept
    {
        if (m_mode == list_select_mode::multi)
        {
            select_positions(0, static_cast<std::int32_t>(m_count) - 1, false);
            invalidate();
        }

        return *this;
    }

    table_view& table_view::clear_selection() noexcept
    {
        std::memset(m_bits, 0, sizeof(m_bits));
        m_selected_count = 0;
        invalidate();
        return *this;
    }

    void table_view::select_positions(std::int32_t from, std::int32_t to, bool keep) noexcept
    {
        if (!keep)
            std::memset(m_bits, 0, sizeof(m_bits));

        if (m_count > 0)
        {
            const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
            const std::int32_t low = std::clamp(std::min(from, to), 0, last);
            const std::int32_t high = std::clamp(std::max(from, to), 0, last);

            for (std::int32_t i = low; i <= high; ++i)
                set_bit(m_order[i], true);
        }

        recount();
    }

    dock_rect table_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    dock_rect table_view::header_area() const noexcept
    {
        const dock_rect box = inner();
        return {box.x, box.y, box.width, std::min(draw::px(k_header_height), box.height)};
    }

    dock_rect table_view::body_area() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t header = std::min(draw::px(k_header_height), box.height);
        return {box.x, box.y + header, box.width, box.height - header};
    }

    std::int32_t table_view::visible_rows() const noexcept
    {
        return std::max(body_area().height / std::max(draw::px(k_row_height), 1), 1);
    }

    std::int32_t table_view::max_first() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_count) - visible_rows(), 0);
    }

    bool table_view::bar_needed() const noexcept
    {
        return static_cast<std::int32_t>(m_count) > visible_rows();
    }

    std::int32_t table_view::bar_left() const noexcept
    {
        const dock_rect body = body_area();
        return body.x + body.width - draw::px(k_bar_column);
    }

    table_view::thumb_span table_view::thumb() const noexcept
    {
        const dock_rect body = body_area();
        const std::int32_t rows = visible_rows();
        const std::int32_t count = std::max(static_cast<std::int32_t>(m_count), 1);
        thumb_span span;
        span.length = std::clamp(body.height * rows / count, std::min(draw::px(k_min_thumb), body.height), std::max(body.height, 0));
        span.travel = body.height - span.length;
        const std::int32_t reach = max_first();
        span.start = body.y + (reach > 0 ? span.travel * m_first / reach : 0);
        return span;
    }

    std::int32_t table_view::column_left(std::uint32_t column) const noexcept
    {
        std::int32_t x = inner().x;

        for (std::uint32_t i = 0; i < column && i < m_columns; ++i)
            x += column_pixels(i);

        return x;
    }

    std::int32_t table_view::column_pixels(std::uint32_t column) const noexcept
    {
        std::int32_t width = draw::px(m_cols[column].width);

        // The last column takes what the others leave, short of the bar.
        if (column + 1 == m_columns)
        {
            std::int32_t before = 0;

            for (std::uint32_t i = 0; i < column; ++i)
                before += draw::px(m_cols[i].width);

            const std::int32_t room = inner().width - (bar_needed() ? draw::px(k_bar_column) : 0) - before;
            width = std::max(width, room);
        }

        return width;
    }

    std::int32_t table_view::header_at(std::int32_t x, std::int32_t y, bool& edge) const noexcept
    {
        edge = false;

        if (!contains(header_area(), x, y))
            return k_none;

        const std::int32_t reach = draw::px(k_edge_reach);

        for (std::uint32_t i = 0; i < m_columns; ++i)
        {
            const std::int32_t right = column_left(i) + column_pixels(i);

            if (std::abs(x - right) <= reach)
            {
                edge = true;
                return static_cast<std::int32_t>(i);
            }
        }

        for (std::uint32_t i = 0; i < m_columns; ++i)
        {
            const std::int32_t left = column_left(i);

            if (x >= left && x < left + column_pixels(i))
                return static_cast<std::int32_t>(i);
        }

        return k_none;
    }

    std::int32_t table_view::position_at(std::int32_t y) const noexcept
    {
        const dock_rect body = body_area();

        if (y < body.y || y >= body.y + body.height)
            return k_none;

        const std::int32_t position = m_first + (y - body.y) / std::max(draw::px(k_row_height), 1);
        return position < static_cast<std::int32_t>(m_count) ? position : k_none;
    }

    std::int32_t table_view::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect body = body_area();

        if (x < body.x || x >= body.x + body.width || (bar_needed() && x >= bar_left()))
            return k_none;

        const std::int32_t position = position_at(y);
        return position == k_none ? k_none : static_cast<std::int32_t>(m_order[position]);
    }

    dock_rect table_view::cell_area(std::uint32_t row, std::uint32_t column) const noexcept
    {
        if (row >= m_count || column >= m_columns)
            return {};

        const dock_rect body = body_area();
        const std::int32_t at = static_cast<std::int32_t>(m_position[row]) - m_first;
        const std::int32_t height = draw::px(k_row_height);

        if (at < 0 || (at + 1) * height > body.height)
            return {};

        return {column_left(column), body.y + at * height, column_pixels(column), height};
    }

    drop_effect table_view::on_drag_over(const drag_event& event) noexcept
    {
        const dock_rect body = body_area();
        const std::int32_t edge = draw::px(10);

        // Held near the top or the bottom, the rows move along.
        if (event.y < body.y + edge)
            set_first(m_first - 1);
        else if (event.y >= body.y + body.height - edge)
            set_first(m_first + 1);

        const std::int32_t row = row_at(event.x, event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, row, false) : drop_effect::none;
        const std::int32_t shown = effect != drop_effect::none ? row : k_none;

        if (shown != m_drop_row)
        {
            m_drop_row = shown;
            invalidate();
        }

        return effect;
    }

    void table_view::on_drag_leave() noexcept
    {
        m_drop_row = k_none;
        invalidate();
    }

    drop_effect table_view::on_drop(const drag_event& event) noexcept
    {
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, row_at(event.x, event.y), true) : drop_effect::none;
        m_drop_row = k_none;
        invalidate();
        return effect;
    }

    std::int32_t table_view::position_near(std::int32_t y) const noexcept
    {
        if (m_count == 0)
            return k_none;

        const std::int32_t top = body_area().y;
        const std::int32_t position = m_first + (y - top) / std::max(draw::px(k_row_height), 1) - (y < top ? 1 : 0);
        return std::clamp(position, 0, static_cast<std::int32_t>(m_count) - 1);
    }

    void table_view::set_first(std::int32_t first) noexcept
    {
        const std::int32_t clamped = std::clamp(first, 0, max_first());

        if (clamped == m_first)
            return;

        m_first = clamped;
        invalidate();
    }

    void table_view::reveal(std::uint32_t row) noexcept
    {
        if (row >= m_count)
            return;

        const std::int32_t position = static_cast<std::int32_t>(m_position[row]);
        const std::int32_t rows = visible_rows();

        if (position < m_first)
            set_first(position);
        else if (position >= m_first + rows)
            set_first(position - rows + 1);
    }

    void table_view::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void table_view::activate(std::int32_t row) noexcept
    {
        if (row >= 0 && row < static_cast<std::int32_t>(m_count) && m_activate != nullptr)
            m_activate(m_activate_user, *this, static_cast<std::uint32_t>(row));
    }

    bool table_view::click_position(std::int32_t position) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));
        const std::uint32_t row = m_order[position];
        const bool multi = m_mode == list_select_mode::multi;

        if (multi && shift_down() && m_anchor != k_none)
        {
            select_positions(static_cast<std::int32_t>(m_position[m_anchor]), position, ctrl_down());
        }
        else if (multi && ctrl_down())
        {
            set_bit(row, !is_selected(row));
            recount();
            m_anchor = static_cast<std::int32_t>(row);
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(row, true);
            recount();
            m_anchor = static_cast<std::int32_t>(row);
        }

        m_current = static_cast<std::int32_t>(row);
        reveal(row);
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    bool table_view::move_current(std::int32_t position, bool extend) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));
        const std::uint32_t row = m_order[position];

        if (m_mode == list_select_mode::multi && extend)
        {
            if (m_anchor == k_none)
                m_anchor = m_current != k_none ? m_current : static_cast<std::int32_t>(row);

            select_positions(static_cast<std::int32_t>(m_position[m_anchor]), position, false);
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(row, true);
            recount();
            m_anchor = static_cast<std::int32_t>(row);
        }

        m_current = static_cast<std::int32_t>(row);
        reveal(row);
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    control_size table_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size table_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(120));
        return {width, draw::px(k_header_height) + static_cast<std::int32_t>(m_rows) * draw::px(k_row_height) + 2 * draw::px(1)};
    }

    mouse_cursor table_view::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_drag == drag_kind::resize)
            return mouse_cursor::resize_horizontal;

        bool edge = false;
        (void)header_at(x, y, edge);
        return edge && m_drag == drag_kind::none ? mouse_cursor::resize_horizontal : mouse_cursor::arrow;
    }

    void table_view::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        m_x = x;
        m_y = y;
        m_column = k_none;
        m_press_header = false;

        bool edge = false;
        const std::int32_t column = header_at(x, y, edge);

        if (contains(header_area(), x, y))
        {
            if (column == k_none)
                return;

            m_column = column;

            if (edge)
            {
                // The grab is how far the mouse is from the edge, so the edge does not jump to it.
                m_drag = drag_kind::resize;
                m_grab = x - (column_left(static_cast<std::uint32_t>(column)) + column_pixels(static_cast<std::uint32_t>(column)));
            }
            else
            {
                m_drag = drag_kind::header;
                m_press_header = true;
            }

            invalidate();
            return;
        }

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
            }

            invalidate();
            return;
        }

        const std::int32_t position = position_at(y);

        if (position == k_none)
            return;

        // A table that gives rows to drag does not sweep a selection with a plain press. A row that is selected stays so, with the
        // others, until the pointer lets go without having dragged.
        if (m_drag_fn != nullptr && !(m_mode == list_select_mode::multi && (ctrl_down() || shift_down())))
        {
            m_drag = drag_kind::lift;
            m_lift_position = position;
            m_lift_x = x;
            m_lift_y = y;

            if (!is_selected(m_order[position]))
            {
                if (click_position(position))
                    notify_change();
                else
                    invalidate();
            }
            else
            {
                m_current = static_cast<std::int32_t>(m_order[position]);
                invalidate();
            }

            return;
        }

        m_drag = drag_kind::rows;

        if (click_position(position))
            notify_change();
        else
            invalidate();
    }

    void table_view::on_release() noexcept
    {
        // Let go on a selected row without dragging: the selection narrows to that row, as a click does.
        if (m_drag == drag_kind::lift && m_lift_position != k_none && m_selected_count > 1)
        {
            if (click_position(m_lift_position))
                notify_change();
        }

        m_drag = drag_kind::none;
        m_lift_position = k_none;
    }

    void table_view::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        on_press(x, y);

        if (m_drag == drag_kind::rows || m_drag == drag_kind::lift)
            activate(m_current);
    }

    void table_view::on_click() noexcept
    {
        // A press and a release on the same title sort by it, unless the edge of the title was dragged.
        if (m_drag == drag_kind::none && m_column != k_none && m_press_header)
        {
            bool edge = false;

            if (header_at(m_x, m_y, edge) == m_column && !edge)
                cycle_sort(static_cast<std::uint32_t>(m_column));
        }

        m_press_header = false;
    }

    bool table_view::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (m_drag == drag_kind::lift)
        {
            if (!drag_drop::exceeds_threshold(x - m_lift_x, y - m_lift_y))
                return false;

            // Far enough: the rows go with the pointer. The press is over, so nothing narrows the selection afterwards.
            drag_payload payload;
            const bool started = m_drag_fn != nullptr && m_drag_fn(m_drag_user, *this, payload) && begin_drag(payload);
            m_drag = drag_kind::none;
            m_lift_position = k_none;
            return started;
        }

        if (m_drag == drag_kind::resize && m_column != k_none)
        {
            const std::uint32_t column = static_cast<std::uint32_t>(m_column);
            const std::int32_t pixels = std::max(x - m_grab - column_left(column), draw::px(k_min_column));
            const std::int32_t before = m_cols[column].width;
            // Back to 96 dpi, where the widths are kept.
            m_cols[column].width = std::max(pixels * 100 / std::max(draw::px(100), 1), k_min_column);
            return m_cols[column].width != before;
        }

        if (m_drag == drag_kind::bar)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - body_area().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_first;
            set_first(span.travel > 0 ? (at * max_first() + span.travel / 2) / span.travel : 0);
            return m_first != before;
        }

        if (m_drag != drag_kind::rows || m_count == 0)
            return false;

        const dock_rect body = body_area();

        if (y < body.y)
            set_first(m_first - 1);
        else if (y >= body.y + body.height)
            set_first(m_first + 1);

        const std::int32_t position = position_near(std::clamp(y, body.y, body.y + body.height - 1));
        const std::uint32_t row = m_order[position];

        if (position == k_none || static_cast<std::int32_t>(row) == m_current)
            return false;

        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));

        if (m_mode == list_select_mode::multi)
        {
            select_positions(m_anchor != k_none ? static_cast<std::int32_t>(m_position[m_anchor]) : position, position, ctrl_down());
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(row, true);
            recount();
            m_anchor = static_cast<std::int32_t>(row);
        }

        m_current = static_cast<std::int32_t>(row);

        if (std::memcmp(before, m_bits, sizeof(before)) != 0)
            notify_change();

        return true;
    }

    bool table_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        bool edge = false;
        const std::int32_t header = header_at(x, y, edge);
        const std::int32_t row = contains(body_area(), x, y) && !(bar_needed() && x >= bar_left()) ? position_at(y) : k_none;

        if (row == m_hover && header == m_hover_header)
            return false;

        m_hover = row;
        m_hover_header = header;
        return true;
    }

    bool table_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t before = m_first;
        set_first(m_first - notches * k_wheel_rows);
        return m_first != before;
    }

    bool table_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_count == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
        const std::int32_t page = std::max(visible_rows() - 1, 1);
        const std::int32_t from = m_current != k_none ? static_cast<std::int32_t>(m_position[m_current]) : m_first;
        std::int32_t target = k_none;

        switch (key)
        {
        case k_key_up:
            target = m_current == k_none ? 0 : std::max(from - 1, 0);
            break;

        case k_key_down:
            target = m_current == k_none ? 0 : std::min(from + 1, last);
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
            if (!ctrl || m_mode != list_select_mode::multi)
                return false;

            select_positions(0, last, false);
            notify_change();
            return true;

        case k_key_space:
            if (!ctrl || m_current == k_none)
                return false;

            set_bit(static_cast<std::uint32_t>(m_current), m_mode == list_select_mode::multi ? !is_selected(static_cast<std::uint32_t>(m_current)) : true);
            recount();
            m_anchor = m_current;
            notify_change();
            return true;

        case k_key_enter:
            if (m_current == k_none)
                return false;

            activate(m_current);
            return true;

        default:
            return false;
        }

        if (move_current(target, shift))
            notify_change();
        else
            invalidate();

        return true;
    }

    void table_view::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();
        std::uint32_t face = blend_colour(colours.panel, black, 0.14f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.5f);
        std::uint32_t ink = colours.text;
        std::uint32_t head_face = blend_colour(colours.panel, colours.title, 0.9f);
        const std::uint32_t rule = blend_colour(colours.title, black, 0.5f);

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            head_face = face;
        }

        draw::rounded(dc, area(), k_radius, face, edge);

        const dock_rect box = inner();
        draw::clip(dc, box);

        const dock_rect header = header_area();
        const dock_rect body = body_area();
        const std::int32_t padding = draw::px(k_padding);

        // The header: a title in each column, an arrow on the one that sorts, and a line between them.
        draw::fill(dc, header, head_face);

        for (std::uint32_t c = 0; c < m_columns; ++c)
        {
            const std::int32_t left = column_left(c);
            const std::int32_t width = column_pixels(c);
            const bool sorted = m_sort_column == static_cast<std::int32_t>(c) && m_sort_dir != sort_direction::none;
            const bool hovered = hot() && enabled() && m_hover_header == static_cast<std::int32_t>(c) && m_cols[c].sortable;
            const bool pressed = m_drag == drag_kind::header && m_column == static_cast<std::int32_t>(c);

            if (pressed)
                draw::fill(dc, {left, header.y, width, header.height}, blend_colour(head_face, black, 0.18f));
            else if (hovered)
                draw::fill(dc, {left, header.y, width, header.height}, blend_colour(head_face, colours.hover, 0.7f));

            const std::int32_t arrow = sorted ? draw::px(k_arrow_size) * 2 + padding / 2 : 0;
            std::uint32_t flags = draw::text_ellipsis;

            if (m_cols[c].align == column_align::right)
                flags |= draw::text_right;
            else if (m_cols[c].align == column_align::center)
                flags |= draw::text_center;

            if (m_cols[c].title != nullptr)
                draw::text(dc, {left + padding, header.y, std::max(width - 2 * padding - arrow, 0), header.height}, m_cols[c].title,
                    enabled() ? (sorted ? colours.text : colours.text_dim) : ink, flags);

            if (sorted)
            {
                const float cx = static_cast<float>(left + width - padding) - static_cast<float>(draw::px(k_arrow_size));
                const float cy = static_cast<float>(header.y) + static_cast<float>(header.height) * 0.5f;
                const float half = static_cast<float>(draw::px(k_arrow_size));
                const float tip = m_sort_dir == sort_direction::ascending ? -half * 0.5f : half * 0.5f;
                const draw::point triangle[3] = {{cx - half, cy - tip}, {cx + half, cy - tip}, {cx, cy + tip}};
                draw::polygon(dc, triangle, 3, enabled() ? colours.text : ink);
            }

            if (c + 1 < m_columns)
                draw::fill(dc, {left + width - 1, header.y + draw::px(5), 1, std::max(header.height - draw::px(10), 0)}, rule);
        }

        draw::fill(dc, {header.x, header.y + header.height - 1, header.width, 1}, rule);

        // The rows, kept clear of the bar.
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        draw::clip(dc, {body.x, body.y, std::max(body.width - bar, 0), body.height});

        if (m_count == 0 && m_empty != nullptr)
            draw::text(dc, body, m_empty, colours.text_dim, draw::text_center);

        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t rows = visible_rows();
        const std::uint32_t stripe = blend_colour(face, white, 0.035f);

        for (std::int32_t i = 0; i < rows + 1; ++i)
        {
            const std::int32_t position = m_first + i;

            if (position >= static_cast<std::int32_t>(m_count))
                break;

            const std::uint32_t source = m_order[position];
            const dock_rect row{body.x, body.y + i * row_height, body.width - bar, row_height};
            const bool selected = is_selected(source);
            const bool hovered = hot() && enabled() && position == m_hover;
            std::uint32_t row_ink = ink;

            if (selected && enabled())
            {
                const std::uint32_t fill = lit ? colours.accent : blend_colour(colours.accent, colours.panel, 0.55f);
                const bool cursor = lit && static_cast<std::int32_t>(source) == m_current;
                draw::rounded(dc, row, 2, fill, cursor ? blend_colour(colours.accent, white, 0.45f) : fill);
                row_ink = lit ? white : colours.text;
            }
            else if (hovered)
            {
                const std::uint32_t fill = blend_colour(face, colours.hover, 0.7f);
                draw::rounded(dc, row, 2, fill, fill);
            }
            else if (m_striped && (position & 1) != 0)
            {
                draw::fill(dc, row, stripe);
            }

            if (lit && static_cast<std::int32_t>(source) == m_current && !selected && enabled())
                draw::rounded(dc, row, 2, hovered ? blend_colour(face, colours.hover, 0.7f) : face, colours.accent);

            for (std::uint32_t c = 0; c < m_columns; ++c)
            {
                const char* text = m_source != nullptr ? m_source(m_source_user, source, c) : nullptr;

                if (text == nullptr)
                    continue;

                std::uint32_t flags = draw::text_ellipsis;

                if (m_cols[c].align == column_align::right)
                    flags |= draw::text_right;
                else if (m_cols[c].align == column_align::center)
                    flags |= draw::text_center;

                draw::text(dc, {column_left(c) + padding, row.y, std::max(column_pixels(c) - 2 * padding, 0), row.height}, text, row_ink, flags);
            }
        }

        // The row a drop would land in.
        if (m_drop_row != k_none && enabled())
        {
            const dock_rect cell = cell_area(static_cast<std::uint32_t>(m_drop_row), 0);

            if (cell.width > 0)
                draw::outline(dc, {body.x, cell.y, std::max(body.width - bar, 0), cell.height}, 2, colours.accent, 2.0f);
        }

        // A thin line between the columns, over the rows.
        const std::uint32_t grid = blend_colour(face, white, 0.06f);

        for (std::uint32_t c = 0; c + 1 < m_columns; ++c)
            draw::fill(dc, {column_left(c) + column_pixels(c) - 1, body.y, 1, body.height}, grid);

        draw::unclip(dc);

        if (bar > 0)
        {
            const thumb_span span = thumb();
            const std::int32_t width = draw::px(k_thumb_width);
            const bool grabbed = m_drag == drag_kind::bar;
            const std::uint32_t thumb_colour = grabbed ? colours.text_dim : blend_colour(colours.text_dim, face, hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {body.x + body.width - (bar + width) / 2, span.start, width, span.length}, 2, thumb_colour, thumb_colour);
        }

        draw::unclip(dc);
    }
}
