#include <chrome/controls/data/tree_view.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/drag_drop.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_row_height = 22;
        constexpr std::int32_t k_padding = 6;
        constexpr std::int32_t k_radius = 3;
        // How far each level moves right, which is also the width of the chevron's square.
        constexpr std::int32_t k_indent = 18;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_wheel_rows = 3;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_a = 0x41;

        [[nodiscard]] bool contains(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }
    }

    tree_view::node_id tree_view::add(node_id parent, const char* text, bool expanded) noexcept
    {
        if (m_count >= k_max_nodes || (parent != k_root && !valid(parent)))
            return k_none;

        const node_id id = static_cast<node_id>(++m_count);
        node& item = m_nodes[id];
        item = node{};
        item.text = text;
        item.parent = parent;
        item.expanded = expanded;
        node& owner = m_nodes[parent];

        if (owner.last_child != k_none)
            m_nodes[owner.last_child].next = id;
        else
            owner.first_child = id;

        owner.last_child = id;
        rebuild();
        invalidate();
        return id;
    }

    tree_view& tree_view::clear() noexcept
    {
        m_nodes[0] = node{};
        m_nodes[0].expanded = true;
        m_count = 0;
        m_visible = 0;
        m_selected_count = 0;
        m_drop_node = k_none;
        m_current = k_none;
        m_anchor = k_none;
        m_hover = k_none;
        m_first = 0;
        invalidate();
        return *this;
    }

    tree_view& tree_view::set_title(const char* title) noexcept
    {
        m_title = title;
        invalidate();
        return *this;
    }

    bool tree_view::add_column(const char* title, std::int32_t width, column_align align, icon_kind header_icon, bool clickable) noexcept
    {
        if (m_columns >= k_max_columns)
            return false;

        m_cols[m_columns++] = {title, std::max(width, 16), align, header_icon, clickable};
        invalidate();
        return true;
    }

    tree_view& tree_view::clear_columns() noexcept
    {
        m_columns = 0;
        invalidate();
        return *this;
    }

    std::int32_t tree_view::header_height() const noexcept
    {
        return has_header() ? draw::px(k_row_height) : 0;
    }

    std::int32_t tree_view::columns_width() const noexcept
    {
        std::int32_t total = 0;

        for (std::uint32_t i = 0; i < m_columns; ++i)
            total += draw::px(m_cols[i].width);

        return total;
    }

    std::int32_t tree_view::column_left(std::uint32_t column_index) const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        std::int32_t left = box.x + box.width - bar - columns_width();

        for (std::uint32_t i = 0; i < column_index && i < m_columns; ++i)
            left += draw::px(m_cols[i].width);

        return left;
    }

    std::int32_t tree_view::column_at(std::int32_t x) const noexcept
    {
        for (std::uint32_t i = 0; i < m_columns; ++i)
        {
            const std::int32_t left = column_left(i);

            if (x >= left && x < left + draw::px(m_cols[i].width))
                return static_cast<std::int32_t>(i);
        }

        return k_none;
    }

    tree_view& tree_view::set_select_mode(list_select_mode mode) noexcept
    {
        m_mode = mode;

        // A single tree keeps at most one row.
        if (mode == list_select_mode::single && m_selected_count > 1)
            select(first_selected());

        return *this;
    }

    tree_view& tree_view::set_rows(std::uint32_t rows) noexcept
    {
        m_rows_shown = std::clamp<std::uint32_t>(rows, 1, 64);
        return *this;
    }

    void tree_view::rebuild() noexcept
    {
        m_nodes[0].expanded = true;

        for (std::uint32_t i = 1; i <= m_count; ++i)
            m_nodes[i].row = k_none;

        m_visible = 0;
        node_id at = m_nodes[0].first_child;
        std::int32_t depth = 0;

        while (at != k_none)
        {
            node& item = m_nodes[at];
            item.depth = depth;
            item.row = static_cast<std::int32_t>(m_visible);
            m_rows[m_visible++] = at;

            if (item.expanded && item.first_child != k_none)
            {
                at = item.first_child;
                ++depth;
                continue;
            }

            // Out of a branch that is done, into the next row of the one above.
            while (at != k_none && m_nodes[at].next == k_none)
            {
                at = m_nodes[at].parent;
                --depth;

                if (at == k_root)
                    at = k_none;
            }

            if (at != k_none)
                at = m_nodes[at].next;
        }

        // What the keyboard was on may be under a closed row now. It goes to the closed row.
        while (valid(m_current) && m_nodes[m_current].row == k_none)
            m_current = m_nodes[m_current].parent;

        if (m_current == k_root)
            m_current = k_none;

        if (valid(m_anchor) && m_nodes[m_anchor].row == k_none)
            m_anchor = m_current;

        m_hover = k_none;
        set_first(m_first);
    }

    tree_view& tree_view::set_expanded(node_id node_index, bool expanded) noexcept
    {
        if (valid(node_index) && m_nodes[node_index].expanded != expanded)
        {
            m_nodes[node_index].expanded = expanded;
            rebuild();
            invalidate();
        }

        return *this;
    }

    tree_view& tree_view::expand_all() noexcept
    {
        for (std::uint32_t i = 1; i <= m_count; ++i)
            m_nodes[i].expanded = true;

        rebuild();
        invalidate();
        return *this;
    }

    tree_view& tree_view::collapse_all() noexcept
    {
        for (std::uint32_t i = 1; i <= m_count; ++i)
            m_nodes[i].expanded = false;

        rebuild();
        invalidate();
        return *this;
    }

    void tree_view::set_selected(node_id node_index, bool on) noexcept
    {
        node& item = m_nodes[node_index];

        if (item.selected == on)
            return;

        item.selected = on;
        m_selected_count += on ? 1u : static_cast<std::uint32_t>(-1);
        m_changed = true;
    }

    void tree_view::clear_all() noexcept
    {
        for (std::uint32_t i = 1; i <= m_count; ++i)
            set_selected(static_cast<node_id>(i), false);
    }

    tree_view& tree_view::select(node_id node_index) noexcept
    {
        clear_all();

        if (valid(node_index))
        {
            // The rows above it open, so it shows.
            bool opened = false;

            for (node_id up = m_nodes[node_index].parent; up > k_root; up = m_nodes[up].parent)
            {
                opened = opened || !m_nodes[up].expanded;
                m_nodes[up].expanded = true;
            }

            if (opened)
                rebuild();

            set_selected(node_index, true);
            m_current = node_index;
            m_anchor = node_index;
            reveal(node_index);
        }

        invalidate();
        return *this;
    }

    tree_view& tree_view::add_to_selection(node_id node_index) noexcept
    {
        if (!valid(node_index))
            return *this;

        if (m_mode == list_select_mode::single)
            return select(node_index);

        set_selected(node_index, true);
        invalidate();
        return *this;
    }

    tree_view& tree_view::select_all() noexcept
    {
        if (m_mode == list_select_mode::multi)
        {
            for (std::uint32_t i = 1; i <= m_count; ++i)
                set_selected(static_cast<node_id>(i), true);

            invalidate();
        }

        return *this;
    }

    tree_view& tree_view::clear_selection() noexcept
    {
        clear_all();
        invalidate();
        return *this;
    }

    tree_view::node_id tree_view::first_selected() const noexcept
    {
        for (std::uint32_t i = 0; i < m_visible; ++i)
        {
            if (m_nodes[m_rows[i]].selected)
                return m_rows[i];
        }

        // A selected row can be hidden by a row that was closed from code.
        for (std::uint32_t i = 1; i <= m_count; ++i)
        {
            if (m_nodes[i].selected)
                return static_cast<node_id>(i);
        }

        return k_none;
    }

    void tree_view::select_rows(std::int32_t from, std::int32_t to, bool keep) noexcept
    {
        if (!keep)
            clear_all();

        if (m_visible == 0)
            return;

        const std::int32_t last = static_cast<std::int32_t>(m_visible) - 1;
        const std::int32_t low = std::clamp(std::min(from, to), 0, last);
        const std::int32_t high = std::clamp(std::max(from, to), 0, last);

        for (std::int32_t i = low; i <= high; ++i)
            set_selected(m_rows[i], true);
    }

    dock_rect tree_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        const std::int32_t header = header_height();
        return {area().x + edge, area().y + edge + header, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge - header, 0)};
    }

    std::int32_t tree_view::visible_rows() const noexcept
    {
        return std::max(inner().height / std::max(draw::px(k_row_height), 1), 1);
    }

    std::int32_t tree_view::max_first() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_visible) - visible_rows(), 0);
    }

    bool tree_view::bar_needed() const noexcept
    {
        return static_cast<std::int32_t>(m_visible) > visible_rows();
    }

    std::int32_t tree_view::bar_left() const noexcept
    {
        const dock_rect box = inner();
        return box.x + box.width - draw::px(k_bar_column);
    }

    tree_view::thumb_span tree_view::thumb() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t rows = visible_rows();
        const std::int32_t count = std::max(static_cast<std::int32_t>(m_visible), 1);
        thumb_span span;
        span.length = std::clamp(box.height * rows / count, std::min(draw::px(k_min_thumb), box.height), box.height);
        span.travel = box.height - span.length;
        const std::int32_t reach = max_first();
        span.start = box.y + (reach > 0 ? span.travel * m_first / reach : 0);
        return span;
    }

    std::int32_t tree_view::row_at(std::int32_t y) const noexcept
    {
        const dock_rect box = inner();

        if (y < box.y || y >= box.y + box.height)
            return k_none;

        const std::int32_t row = m_first + (y - box.y) / std::max(draw::px(k_row_height), 1);
        return row < static_cast<std::int32_t>(m_visible) ? row : k_none;
    }

    std::int32_t tree_view::row_near(std::int32_t y) const noexcept
    {
        if (m_visible == 0)
            return k_none;

        const std::int32_t row = m_first + (y - inner().y) / std::max(draw::px(k_row_height), 1) - (y < inner().y ? 1 : 0);
        return std::clamp(row, 0, static_cast<std::int32_t>(m_visible) - 1);
    }

    dock_rect tree_view::chevron_of(std::int32_t row) const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t indent = draw::px(k_indent);
        const std::int32_t height = draw::px(k_row_height);
        return {box.x + m_nodes[m_rows[row]].depth * indent + draw::px(2), box.y + (row - m_first) * height, indent, height};
    }

    void tree_view::set_first(std::int32_t first) noexcept
    {
        const std::int32_t clamped = std::clamp(first, 0, max_first());

        if (clamped == m_first)
            return;

        m_first = clamped;
        invalidate();
    }

    void tree_view::reveal(node_id node_index) noexcept
    {
        if (!valid(node_index) || m_nodes[node_index].row == k_none)
            return;

        const std::int32_t row = m_nodes[node_index].row;
        const std::int32_t rows = visible_rows();

        if (row < m_first)
            set_first(row);
        else if (row >= m_first + rows)
            set_first(row - rows + 1);
    }

    void tree_view::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void tree_view::activate(node_id node_index) noexcept
    {
        if (valid(node_index) && m_activate != nullptr)
            m_activate(m_activate_user, *this, node_index);
    }

    void tree_view::toggle(node_id node_index) noexcept
    {
        if (!has_children(node_index))
            return;

        m_changed = false;
        node& item = m_nodes[node_index];

        if (item.expanded)
        {
            // The rows right under it, which are deeper, are about to be hidden.
            bool hid_selected = false;

            for (std::int32_t i = item.row + 1; i < static_cast<std::int32_t>(m_visible) && m_nodes[m_rows[i]].depth > item.depth; ++i)
            {
                if (m_nodes[m_rows[i]].selected)
                {
                    set_selected(m_rows[i], false);
                    hid_selected = true;
                }
            }

            item.expanded = false;
            rebuild();

            if (hid_selected && !item.selected)
            {
                set_selected(node_index, true);
                m_current = node_index;
                m_anchor = node_index;
            }
        }
        else
        {
            item.expanded = true;
            rebuild();
        }

        invalidate();

        if (m_toggle != nullptr)
            m_toggle(m_toggle_user, *this, node_index);

        if (m_changed)
            notify_change();
    }

    bool tree_view::click_row(std::int32_t row) noexcept
    {
        m_changed = false;
        const node_id picked = m_rows[row];
        const bool multi = m_mode == list_select_mode::multi;

        if (multi && shift_down() && valid(m_anchor) && m_nodes[m_anchor].row != k_none)
        {
            select_rows(m_nodes[m_anchor].row, row, ctrl_down());
        }
        else if (multi && ctrl_down())
        {
            set_selected(picked, !m_nodes[picked].selected);
            m_anchor = picked;
        }
        else
        {
            clear_all();
            set_selected(picked, true);
            m_anchor = picked;
        }

        m_current = picked;
        reveal(picked);
        return m_changed;
    }

    bool tree_view::move_current(std::int32_t row, bool extend) noexcept
    {
        m_changed = false;
        const node_id target = m_rows[row];

        if (m_mode == list_select_mode::multi && extend)
        {
            if (!valid(m_anchor) || m_nodes[m_anchor].row == k_none)
                m_anchor = valid(m_current) ? m_current : target;

            select_rows(m_nodes[m_anchor].row, row, false);
        }
        else
        {
            clear_all();
            set_selected(target, true);
            m_anchor = target;
        }

        m_current = target;
        reveal(target);
        return m_changed;
    }

    control_size tree_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size tree_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(80));
        return {width, static_cast<std::int32_t>(m_rows_shown) * draw::px(k_row_height) + 2 * draw::px(1) + header_height()};
    }

    void tree_view::on_press(std::int32_t x, std::int32_t y) noexcept
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
            }

            invalidate();
            return;
        }

        const std::int32_t row = row_at(y);

        if (row == k_none)
            return;

        // A click on a cell of a clickable column is for the cell. It does not select the row.
        const std::int32_t column_hit = column_at(x);

        if (column_hit != k_none && m_cols[column_hit].clickable)
        {
            if (m_cell_click != nullptr)
                m_cell_click(m_cell_click_user, *this, m_rows[row], static_cast<std::uint32_t>(column_hit));

            invalidate();
            return;
        }

        // The chevron opens or closes the row and leaves the selection alone.
        if (has_children(m_rows[row]) && contains(chevron_of(row), x, y))
        {
            toggle(m_rows[row]);
            return;
        }

        // A tree that gives rows to drag does not sweep a selection with a plain press. A row that is selected stays so, with the
        // others, until the pointer lets go without having dragged.
        if (m_drag_fn != nullptr && !(m_mode == list_select_mode::multi && (ctrl_down() || shift_down())))
        {
            m_drag = drag_kind::lift;
            m_lift_row = row;
            m_lift_x = x;
            m_lift_y = y;

            if (!is_selected(m_rows[row]))
            {
                if (click_row(row))
                    notify_change();
                else
                    invalidate();
            }
            else
            {
                m_current = m_rows[row];
                invalidate();
            }

            return;
        }

        m_drag = drag_kind::rows;

        if (click_row(row))
            notify_change();
        else
            invalidate();
    }

    void tree_view::on_release() noexcept
    {
        // Let go on a selected row without dragging: the selection narrows to that row, as a click does.
        if (m_drag == drag_kind::lift && m_lift_row != k_none && m_selected_count > 1)
        {
            if (click_row(m_lift_row))
                notify_change();
        }

        m_drag = drag_kind::none;
        m_lift_row = k_none;
    }

    void tree_view::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t row = row_at(y);

        if (row == k_none || (bar_needed() && x >= bar_left()))
        {
            on_press(x, y);
            return;
        }

        const node_id picked = m_rows[row];
        const std::int32_t column_hit = column_at(x);

        // A second click on a clickable cell is one more click on it.
        if (column_hit != k_none && m_cols[column_hit].clickable)
        {
            on_press(x, y);
            return;
        }

        // A second click on the chevron is another toggle. Anywhere else it selects, opens or closes, and activates.
        if (has_children(picked) && contains(chevron_of(row), x, y))
        {
            toggle(picked);
            return;
        }

        on_press(x, y);

        if (has_children(picked))
            toggle(picked);

        activate(picked);
    }

    tree_view::node_id tree_view::node_at(std::int32_t y) const noexcept
    {
        const std::int32_t row = row_at(y);
        return row == k_none ? k_none : m_rows[row];
    }

    dock_rect tree_view::label_area(node_id node) const noexcept
    {
        if (!valid(node) || m_nodes[node].row == k_none)
            return {};

        const dock_rect box = inner();
        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t index = m_nodes[node].row - m_first;
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;

        if (index < 0 || index * row_height >= box.height)
            return {};

        const std::int32_t indent = draw::px(k_indent);
        std::int32_t left = box.x + m_nodes[node].depth * indent + draw::px(2) + indent + draw::px(2);

        if (m_icon != nullptr && m_icon(m_icon_user, node, m_nodes[node].expanded).kind != icon_kind::none)
            left += std::max(row_height - draw::px(8), draw::px(8)) + draw::px(6);

        return {left, box.y + index * row_height, std::max(box.x + box.width - bar - columns_width() - left, 0), row_height};
    }

    drop_effect tree_view::on_drag_over(const drag_event& event) noexcept
    {
        const dock_rect box = inner();
        const std::int32_t edge = draw::px(10);

        // Held near the top or the bottom, the rows move along.
        if (event.y < box.y + edge)
            set_first(m_first - 1);
        else if (event.y >= box.y + box.height - edge)
            set_first(m_first + 1);

        const node_id over = node_at(event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, over == k_none ? k_root : over, false) : drop_effect::none;
        const node_id shown = effect != drop_effect::none ? (over == k_none ? k_root : over) : k_none;

        if (shown != m_drop_node)
        {
            m_drop_node = shown;
            invalidate();
        }

        return effect;
    }

    void tree_view::on_drag_leave() noexcept
    {
        m_drop_node = k_none;
        invalidate();
    }

    drop_effect tree_view::on_drop(const drag_event& event) noexcept
    {
        const node_id over = node_at(event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, over == k_none ? k_root : over, true) : drop_effect::none;
        m_drop_node = k_none;
        invalidate();
        return effect;
    }

    bool tree_view::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_drag == drag_kind::lift)
        {
            if (!drag_drop::exceeds_threshold(x - m_lift_x, y - m_lift_y))
                return false;

            // Far enough: the rows go with the pointer. The press is over, so nothing narrows the selection afterwards.
            drag_payload payload;
            const bool started = m_drag_fn != nullptr && m_drag_fn(m_drag_user, *this, payload) && begin_drag(payload);
            m_drag = drag_kind::none;
            m_lift_row = k_none;
            return started;
        }

        if (m_drag == drag_kind::bar)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - inner().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_first;
            set_first(span.travel > 0 ? (at * max_first() + span.travel / 2) / span.travel : 0);
            return m_first != before;
        }

        if (m_drag != drag_kind::rows || m_visible == 0)
            return false;

        const dock_rect box = inner();

        if (y < box.y)
            set_first(m_first - 1);
        else if (y >= box.y + box.height)
            set_first(m_first + 1);

        const std::int32_t row = row_near(std::clamp(y, box.y, box.y + box.height - 1));

        if (row == k_none || m_rows[row] == m_current)
            return false;

        m_changed = false;

        if (m_mode == list_select_mode::multi && valid(m_anchor) && m_nodes[m_anchor].row != k_none)
        {
            select_rows(m_nodes[m_anchor].row, row, ctrl_down());
        }
        else
        {
            clear_all();
            set_selected(m_rows[row], true);
            m_anchor = m_rows[row];
        }

        m_current = m_rows[row];

        if (m_changed)
            notify_change();

        return true;
    }

    bool tree_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t row = bar_needed() && x >= bar_left() ? k_none : row_at(y);

        if (row == m_hover)
            return false;

        m_hover = row;
        return true;
    }

    bool tree_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t before = m_first;
        set_first(m_first - notches * k_wheel_rows);
        return m_first != before;
    }

    bool tree_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_visible == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_visible) - 1;
        const std::int32_t page = std::max(visible_rows() - 1, 1);
        const bool placed = valid(m_current) && m_nodes[m_current].row != k_none;
        const std::int32_t from = placed ? m_nodes[m_current].row : m_first;
        std::int32_t target = k_none;

        switch (key)
        {
        case k_key_up:
            target = placed ? std::max(from - 1, 0) : 0;
            break;

        case k_key_down:
            target = placed ? std::min(from + 1, last) : 0;
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

        case k_key_left:
            if (!placed)
                return false;

            // A row that is open closes, and one that is not goes to its parent.
            if (has_children(m_current) && m_nodes[m_current].expanded)
            {
                toggle(m_current);
                return true;
            }

            if (parent_of(m_current) == k_none)
                return true;

            target = m_nodes[parent_of(m_current)].row;
            break;

        case k_key_right:
            if (!placed || !has_children(m_current))
                return false;

            // A row that is closed opens, and one that is open goes to its first child.
            if (!m_nodes[m_current].expanded)
            {
                toggle(m_current);
                return true;
            }

            target = m_nodes[m_nodes[m_current].first_child].row;
            break;

        case k_key_a:
            if (!ctrl || m_mode != list_select_mode::multi)
                return false;

            m_changed = false;
            select_rows(0, last, false);

            if (m_changed)
                notify_change();

            return true;

        case k_key_space:
            if (!ctrl || !placed)
                return false;

            m_changed = false;
            set_selected(m_current, m_mode == list_select_mode::multi ? !m_nodes[m_current].selected : true);
            m_anchor = m_current;

            if (m_changed)
                notify_change();

            return true;

        case k_key_enter:
            if (!placed)
                return false;

            activate(m_current);
            return true;

        default:
            return false;
        }

        if (target == k_none)
            return true;

        if (move_current(target, shift))
            notify_change();
        else
            invalidate();

        return true;
    }

    void tree_view::paint_header(void* dc, const dock_rect& frame) const noexcept
    {
        const theme_colours& colours = theme();
        const std::int32_t height = header_height();
        const dock_rect strip{frame.x, frame.y, frame.width, height};
        const std::uint32_t face = blend_colour(colours.title, 0x000000, 0.12f);
        draw::fill(dc, strip, face);
        draw::fill(dc, {strip.x, strip.y + strip.height - 1, strip.width, 1}, blend_colour(colours.title, 0x000000, 0.45f));

        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t columns_left = column_left(0);
        const std::int32_t tree_left = strip.x + padding;

        if (m_title != nullptr)
            draw::text(dc, {tree_left, strip.y, std::max(columns_left - tree_left - padding, 0), height}, m_title, colours.text_dim,
                draw::text_ellipsis);

        for (std::uint32_t c = 0; c < m_columns; ++c)
        {
            const std::int32_t left = column_left(c);
            const std::int32_t width = draw::px(m_cols[c].width);
            // A thin rule between the columns.
            draw::fill(dc, {left, strip.y + draw::px(4), 1, std::max(height - draw::px(8), 0)}, blend_colour(colours.title, 0x000000, 0.3f));

            if (m_cols[c].header_icon != icon_kind::none)
            {
                const std::int32_t size = draw::px(14);
                draw::icon(dc, m_cols[c].header_icon, {left + (width - size) / 2, strip.y + (height - size) / 2, size, size}, colours.text_dim);
            }
            else if (m_cols[c].title != nullptr)
            {
                const std::uint32_t flags = m_cols[c].align == column_align::right ? draw::text_right | draw::text_ellipsis
                    : m_cols[c].align == column_align::center ? draw::text_center | draw::text_ellipsis
                                                               : draw::text_ellipsis;
                draw::text(dc, {left + padding / 2, strip.y, std::max(width - padding, 0), height}, m_cols[c].title, colours.text_dim, flags);
            }
        }
    }

    void tree_view::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();
        std::uint32_t face = blend_colour(colours.panel, black, 0.14f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.5f);
        std::uint32_t ink = colours.text;

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
        }

        draw::rounded(dc, area(), k_radius, face, edge);

        const std::int32_t frame_edge = draw::px(1);
        const dock_rect frame{area().x + frame_edge, area().y + frame_edge, std::max(area().width - 2 * frame_edge, 0),
            std::max(area().height - 2 * frame_edge, 0)};
        const dock_rect box = inner();
        draw::clip(dc, frame);

        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t indent = draw::px(k_indent);
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t rows = visible_rows();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::int32_t extra = columns_width();

        if (has_header())
            paint_header(dc, frame);

        if (m_visible == 0 && m_empty != nullptr)
            draw::text(dc, box, m_empty, colours.text_dim, draw::text_center);

        for (std::int32_t i = 0; i < rows + 1; ++i)
        {
            const std::int32_t index = m_first + i;

            if (index >= static_cast<std::int32_t>(m_visible))
                break;

            const node_id id = m_rows[index];
            const node& item = m_nodes[id];
            const dock_rect row{box.x, box.y + i * row_height, box.width - bar, row_height};
            const bool hovered = hot() && index == m_hover && enabled();
            std::uint32_t row_ink = ink;
            std::uint32_t arrow = colours.text_dim;

            if (item.selected && enabled())
            {
                const std::uint32_t fill = lit ? colours.accent : blend_colour(colours.accent, colours.panel, 0.55f);
                const bool cursor = lit && id == m_current;
                draw::rounded(dc, row, 2, fill, cursor ? blend_colour(colours.accent, white, 0.45f) : fill);
                row_ink = lit ? white : colours.text;
                arrow = row_ink;
            }
            else if (hovered)
            {
                const std::uint32_t fill = blend_colour(face, colours.hover, 0.7f);
                draw::rounded(dc, row, 2, fill, fill);
            }

            if (lit && id == m_current && !item.selected && enabled())
                draw::rounded(dc, row, 2, hovered ? blend_colour(face, colours.hover, 0.7f) : face, colours.accent);

            // A dimmed row fades towards the face of the tree, in the chevron, the text, the picture and the cells.
            const bool dimmed = m_dim != nullptr && m_dim(m_dim_user, id);
            const auto faded = [&](std::uint32_t colour) { return dimmed ? blend_colour(colour, face, 0.55f) : colour; };

            if (dimmed)
            {
                row_ink = faded(row_ink);
                arrow = faded(arrow);
            }

            const std::int32_t left = box.x + item.depth * indent + draw::px(2);

            if (item.first_child != k_none)
            {
                const float cx = static_cast<float>(left) + static_cast<float>(indent) * 0.5f;
                const float cy = static_cast<float>(row.y) + static_cast<float>(row.height) * 0.5f;
                const float half = static_cast<float>(draw::px(4)) * 0.8f;

                if (item.expanded)
                {
                    const draw::point down[3] = {{cx - half, cy - half * 0.5f}, {cx, cy + half * 0.5f}, {cx + half, cy - half * 0.5f}};
                    draw::polyline(dc, down, 3, arrow, 1.5f);
                }
                else
                {
                    const draw::point right[3] = {{cx - half * 0.5f, cy - half}, {cx + half * 0.5f, cy}, {cx - half * 0.5f, cy + half}};
                    draw::polyline(dc, right, 3, arrow, 1.5f);
                }
            }

            std::int32_t text_left = left + indent + draw::px(2);

            if (m_icon != nullptr)
            {
                const row_icon icon = m_icon(m_icon_user, id, item.expanded);

                if (icon.kind != icon_kind::none)
                {
                    const std::int32_t size = std::max(row.height - draw::px(8), draw::px(8));
                    draw::icon(dc, icon.kind, {text_left, row.y + (row.height - size) / 2, size, size}, lit && item.selected ? row_ink : faded(icon.colour));
                    text_left += size + draw::px(6);
                }
            }

            if (item.text != nullptr)
                draw::text(dc, {text_left, row.y, std::max(row.x + row.width - extra - text_left - padding, 0), row.height}, item.text, row_ink,
                    draw::text_ellipsis);

            if (m_cell != nullptr)
            {
                for (std::uint32_t c = 0; c < m_columns; ++c)
                {
                    cell_content content;
                    m_cell(m_cell_user, id, c, content);
                    const std::int32_t cell_left = column_left(c);
                    const std::int32_t cell_width = draw::px(m_cols[c].width);
                    std::int32_t at = cell_left + padding / 2;
                    const std::int32_t room = std::max(cell_width - padding, 0);
                    const std::int32_t size = std::max(row.height - draw::px(8), draw::px(8));
                    const std::uint32_t icon_colour = lit && item.selected ? row_ink : faded(content.icon.colour);

                    if (content.icon.kind != icon_kind::none && content.text == nullptr)
                    {
                        // A picture alone sits in the middle, or at the edge the column is aligned to.
                        const std::int32_t x = m_cols[c].align == column_align::left ? at
                            : m_cols[c].align == column_align::right ? cell_left + cell_width - padding / 2 - size
                            : cell_left + (cell_width - size) / 2;
                        draw::icon(dc, content.icon.kind, {x, row.y + (row.height - size) / 2, size, size}, icon_colour);
                        continue;
                    }

                    if (content.icon.kind != icon_kind::none)
                    {
                        draw::icon(dc, content.icon.kind, {at, row.y + (row.height - size) / 2, size, size}, icon_colour);
                        at += size + draw::px(4);
                    }

                    if (content.text != nullptr)
                    {
                        const std::uint32_t flags = m_cols[c].align == column_align::right ? draw::text_right | draw::text_ellipsis
                            : m_cols[c].align == column_align::center ? draw::text_center | draw::text_ellipsis
                                                                       : draw::text_ellipsis;
                        draw::text(dc, {at, row.y, std::max(cell_left + cell_width - at - padding / 2, 0), row.height}, content.text,
                            lit && item.selected ? row_ink : faded(colours.text_dim), flags);
                    }
                }
            }

            if (id == m_drop_node && enabled())
                draw::outline(dc, {row.x + 1, row.y + 1, row.width - 2, row.height - 2}, 2, colours.accent, 2.0f);
        }

        // A drop on the empty part is for the root, so the whole tree is outlined.
        if (m_drop_node == k_root && enabled())
            draw::outline(dc, {box.x + 1, box.y + 1, box.width - 2, box.height - 2}, 2, colours.accent, 2.0f);

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
