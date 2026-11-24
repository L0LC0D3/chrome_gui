#include <chrome/controls/data/list_view.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/drag_drop.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <bit>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_row_height = 22;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_radius = 3;
        // The column the bar takes on the right, and how wide the thumb is inside it.
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        // Rows moved by one notch of the wheel.
        constexpr std::int32_t k_wheel_rows = 3;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_a = 0x41;
    }

    list_view& list_view::set_items(const char* const* items, std::uint32_t count) noexcept
    {
        m_items = items;
        m_source = nullptr;
        m_source_user = nullptr;
        m_count = items != nullptr ? std::min(count, k_max_items) : 0;
        clear_selection();
        m_current = k_none;
        m_anchor = k_none;
        m_hover = k_none;
        m_first = 0;
        invalidate();
        return *this;
    }

    list_view& list_view::set_source(text_fn source, void* user, std::uint32_t count) noexcept
    {
        m_items = nullptr;
        m_source = source;
        m_source_user = user;
        m_count = source != nullptr ? std::min(count, k_max_items) : 0;
        clear_selection();
        m_current = k_none;
        m_anchor = k_none;
        m_hover = k_none;
        m_first = 0;
        invalidate();
        return *this;
    }

    list_view& list_view::set_count(std::uint32_t count) noexcept
    {
        m_count = std::min(count, k_max_items);

        // What is past the new end is no longer selected.
        for (std::uint32_t i = m_count; i < k_max_items; ++i)
            set_bit(i, false);

        recount();

        if (m_current >= static_cast<std::int32_t>(m_count))
            m_current = k_none;

        if (m_anchor >= static_cast<std::int32_t>(m_count))
            m_anchor = k_none;

        m_hover = k_none;
        set_first(m_first);
        invalidate();
        return *this;
    }

    list_view& list_view::set_select_mode(list_select_mode mode) noexcept
    {
        m_mode = mode;

        // A single list keeps at most one row.
        if (mode == list_select_mode::single && m_selected_count > 1)
            select(first_selected());

        return *this;
    }

    list_view& list_view::set_rows(std::uint32_t rows) noexcept
    {
        m_rows = std::clamp<std::uint32_t>(rows, 1, 64);
        return *this;
    }

    const char* list_view::text(std::uint32_t index) const noexcept
    {
        if (index >= m_count)
            return nullptr;

        return m_source != nullptr ? m_source(m_source_user, index) : m_items[index];
    }

    void list_view::set_bit(std::uint32_t index, bool on) noexcept
    {
        if (index >= k_max_items)
            return;

        const std::uint64_t mask = std::uint64_t{1} << (index & 63);

        if (on)
            m_bits[index >> 6] |= mask;
        else
            m_bits[index >> 6] &= ~mask;
    }

    void list_view::recount() noexcept
    {
        std::uint32_t total = 0;

        for (const std::uint64_t word : m_bits)
            total += static_cast<std::uint32_t>(std::popcount(word));

        m_selected_count = total;
    }

    bool list_view::is_selected(std::uint32_t index) const noexcept
    {
        return index < m_count && (m_bits[index >> 6] >> (index & 63) & 1) != 0;
    }

    std::int32_t list_view::first_selected() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (is_selected(i))
                return static_cast<std::int32_t>(i);
        }

        return k_none;
    }

    list_view& list_view::select(std::int32_t index) noexcept
    {
        std::memset(m_bits, 0, sizeof(m_bits));

        if (index >= 0 && index < static_cast<std::int32_t>(m_count))
        {
            set_bit(static_cast<std::uint32_t>(index), true);
            m_current = index;
            m_anchor = index;
            reveal(static_cast<std::uint32_t>(index));
        }

        recount();
        invalidate();
        return *this;
    }

    list_view& list_view::add_to_selection(std::uint32_t index) noexcept
    {
        if (index >= m_count)
            return *this;

        if (m_mode == list_select_mode::single)
            return select(static_cast<std::int32_t>(index));

        set_bit(index, true);
        recount();
        invalidate();
        return *this;
    }

    list_view& list_view::select_all() noexcept
    {
        if (m_mode == list_select_mode::single)
            return *this;

        select_range(0, static_cast<std::int32_t>(m_count) - 1, false);
        invalidate();
        return *this;
    }

    list_view& list_view::clear_selection() noexcept
    {
        std::memset(m_bits, 0, sizeof(m_bits));
        m_selected_count = 0;
        invalidate();
        return *this;
    }

    void list_view::select_range(std::int32_t from, std::int32_t to, bool keep) noexcept
    {
        if (!keep)
            std::memset(m_bits, 0, sizeof(m_bits));

        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
        const std::int32_t low = std::clamp(std::min(from, to), 0, std::max(last, 0));
        const std::int32_t high = std::clamp(std::max(from, to), 0, std::max(last, 0));

        if (m_count > 0)
        {
            for (std::int32_t i = low; i <= high; ++i)
                set_bit(static_cast<std::uint32_t>(i), true);
        }

        recount();
    }

    dock_rect list_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    std::int32_t list_view::visible_rows() const noexcept
    {
        return std::max(inner().height / std::max(draw::px(k_row_height), 1), 1);
    }

    std::int32_t list_view::max_first() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_count) - visible_rows(), 0);
    }

    bool list_view::bar_needed() const noexcept
    {
        return static_cast<std::int32_t>(m_count) > visible_rows();
    }

    std::int32_t list_view::bar_left() const noexcept
    {
        const dock_rect box = inner();
        return box.x + box.width - draw::px(k_bar_column);
    }

    list_view::thumb_span list_view::thumb() const noexcept
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

    std::int32_t list_view::row_at(std::int32_t y) const noexcept
    {
        const dock_rect box = inner();

        if (y < box.y || y >= box.y + box.height)
            return k_none;

        const std::int32_t row = m_first + (y - box.y) / std::max(draw::px(k_row_height), 1);
        return row < static_cast<std::int32_t>(m_count) ? row : k_none;
    }

    std::int32_t list_view::row_near(std::int32_t y) const noexcept
    {
        if (m_count == 0)
            return k_none;

        const std::int32_t row = m_first + (y - inner().y) / std::max(draw::px(k_row_height), 1) - (y < inner().y ? 1 : 0);
        return std::clamp(row, 0, static_cast<std::int32_t>(m_count) - 1);
    }

    void list_view::set_first(std::int32_t first) noexcept
    {
        const std::int32_t clamped = std::clamp(first, 0, max_first());

        if (clamped == m_first)
            return;

        m_first = clamped;
        invalidate();
    }

    void list_view::reveal(std::uint32_t index) noexcept
    {
        if (index >= m_count)
            return;

        const std::int32_t row = static_cast<std::int32_t>(index);
        const std::int32_t rows = visible_rows();

        if (row < m_first)
            set_first(row);
        else if (row >= m_first + rows)
            set_first(row - rows + 1);
    }

    void list_view::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void list_view::activate(std::int32_t row) noexcept
    {
        if (row >= 0 && row < static_cast<std::int32_t>(m_count) && m_activate != nullptr)
            m_activate(m_activate_user, *this, static_cast<std::uint32_t>(row));
    }

    bool list_view::click_row(std::int32_t row) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));
        const bool multi = m_mode == list_select_mode::multi;

        if (multi && shift_down() && m_anchor != k_none)
        {
            // A range from the anchor, which stays where it was. Ctrl keeps what was selected before.
            select_range(m_anchor, row, ctrl_down());
        }
        else if (multi && ctrl_down())
        {
            set_bit(static_cast<std::uint32_t>(row), !is_selected(static_cast<std::uint32_t>(row)));
            recount();
            m_anchor = row;
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(static_cast<std::uint32_t>(row), true);
            recount();
            m_anchor = row;
        }

        m_current = row;
        reveal(static_cast<std::uint32_t>(row));
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    bool list_view::move_current(std::int32_t row, bool extend) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));

        if (m_mode == list_select_mode::multi && extend)
        {
            if (m_anchor == k_none)
                m_anchor = m_current != k_none ? m_current : row;

            select_range(m_anchor, row, false);
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(static_cast<std::uint32_t>(row), true);
            recount();
            m_anchor = row;
        }

        m_current = row;
        reveal(static_cast<std::uint32_t>(row));
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    control_size list_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size list_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(80));
        return {width, static_cast<std::int32_t>(m_rows) * draw::px(k_row_height) + 2 * draw::px(1)};
    }

    void list_view::on_press(std::int32_t x, std::int32_t y) noexcept
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
                // A click on the track moves a page.
                set_first(m_first + (y < span.start ? -1 : 1) * std::max(visible_rows() - 1, 1));
            }

            invalidate();
            return;
        }

        const std::int32_t row = row_at(y);

        if (row == k_none)
            return;

        // A list that gives rows to drag does not sweep a selection with a plain press. A row that is selected stays so, with the
        // others, until the pointer lets go without having dragged.
        if (m_drag_fn != nullptr && !(m_mode == list_select_mode::multi && (ctrl_down() || shift_down())))
        {
            m_drag = drag_kind::lift;
            m_lift_row = row;
            m_lift_x = x;
            m_lift_y = y;

            if (!is_selected(static_cast<std::uint32_t>(row)))
            {
                if (click_row(row))
                    notify_change();
                else
                    invalidate();
            }
            else
            {
                m_current = row;
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

    void list_view::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        on_press(x, y);
        activate(row_at(y));
    }

    void list_view::on_release() noexcept
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

    std::int32_t list_view::slot_at(std::int32_t y) const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t height = std::max(draw::px(k_row_height), 1);

        if (m_drop_mode == list_drop_mode::on_row)
            return row_at(y);

        const std::int32_t slot = m_first + (y - box.y + height / 2) / height;
        return std::clamp(slot, 0, static_cast<std::int32_t>(m_count));
    }

    drop_effect list_view::on_drag_over(const drag_event& event) noexcept
    {
        const dock_rect box = inner();
        const std::int32_t edge = draw::px(10);

        // Held near the top or the bottom, the rows move along.
        if (event.y < box.y + edge)
            set_first(m_first - 1);
        else if (event.y >= box.y + box.height - edge)
            set_first(m_first + 1);

        const std::int32_t slot = slot_at(event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, slot, false) : drop_effect::none;
        const std::int32_t shown = effect != drop_effect::none ? slot : k_none;

        if (shown != m_drop_slot)
        {
            m_drop_slot = shown;
            invalidate();
        }

        return effect;
    }

    void list_view::on_drag_leave() noexcept
    {
        m_drop_slot = k_none;
        invalidate();
    }

    drop_effect list_view::on_drop(const drag_event& event) noexcept
    {
        const std::int32_t slot = slot_at(event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, slot, true) : drop_effect::none;
        m_drop_slot = k_none;
        invalidate();
        return effect;
    }

    bool list_view::on_drag(std::int32_t x, std::int32_t y) noexcept
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

        if (m_drag != drag_kind::rows || m_count == 0)
            return false;

        // Dragging past the top or bottom edge moves the rows along.
        const dock_rect box = inner();

        // A list with no height has nothing to drag over.
        if (box.height <= 0)
            return false;

        if (y < box.y)
            set_first(m_first - 1);
        else if (y >= box.y + box.height)
            set_first(m_first + 1);

        const std::int32_t row = row_near(std::clamp(y, box.y, box.y + box.height - 1));

        if (row == k_none || row == m_current)
            return false;

        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));

        if (m_mode == list_select_mode::multi)
            select_range(m_anchor != k_none ? m_anchor : row, row, ctrl_down());
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(static_cast<std::uint32_t>(row), true);
            recount();
            m_anchor = row;
        }

        m_current = row;

        if (std::memcmp(before, m_bits, sizeof(before)) != 0)
            notify_change();

        return true;
    }

    bool list_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t row = bar_needed() && x >= bar_left() ? k_none : row_at(y);

        if (row == m_hover)
            return false;

        m_hover = row;
        return true;
    }

    bool list_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t before = m_first;
        set_first(m_first - notches * k_wheel_rows);

        // At either end the panel gets the wheel, so it can scroll on.
        return m_first != before;
    }

    bool list_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_count == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
        const std::int32_t page = std::max(visible_rows() - 1, 1);
        const std::int32_t from = m_current != k_none ? m_current : (m_first > 0 ? m_first : 0);
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

            select_range(0, last, false);
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

    void list_view::paint(void* dc) const noexcept
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

        const dock_rect box = inner();
        draw::clip(dc, box);

        if (m_count == 0)
        {
            if (m_empty != nullptr)
                draw::text(dc, box, m_empty, colours.text_dim, draw::text_center);
        }

        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t rows = visible_rows();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::uint32_t stripe = blend_colour(face, white, 0.035f);

        for (std::int32_t i = 0; i < rows + 1; ++i)
        {
            const std::int32_t index = m_first + i;

            if (index >= static_cast<std::int32_t>(m_count))
                break;

            const dock_rect row{box.x, box.y + i * row_height, box.width - bar, row_height};
            const bool selected = is_selected(static_cast<std::uint32_t>(index));
            std::uint32_t row_ink = ink;

            if (selected && enabled())
            {
                const std::uint32_t fill = lit ? colours.accent : blend_colour(colours.accent, colours.panel, 0.55f);
                const bool cursor = lit && index == m_current;
                draw::rounded(dc, row, 2, fill, cursor ? blend_colour(colours.accent, white, 0.45f) : fill);
                row_ink = lit ? white : colours.text;
            }
            else if (hot() && index == m_hover && enabled())
            {
                const std::uint32_t fill = blend_colour(face, colours.hover, 0.7f);
                draw::rounded(dc, row, 2, fill, fill);
            }
            else if (m_striped && (index & 1) != 0)
            {
                draw::fill(dc, row, stripe);
            }

            if (lit && index == m_current && !selected && enabled())
                draw::rounded(dc, row, 2, hot() && index == m_hover ? blend_colour(face, colours.hover, 0.7f) : face, colours.accent);

            if (const char* label = text(static_cast<std::uint32_t>(index)))
                draw::text(dc, {row.x + padding, row.y, std::max(row.width - 2 * padding, 0), row.height}, label, row_ink, draw::text_ellipsis);
        }

        // Where a drag would land: a line between two rows, or an outline round one.
        if (m_drop_slot != k_none && enabled())
        {
            if (m_drop_mode == list_drop_mode::on_row)
            {
                const std::int32_t at = m_drop_slot - m_first;

                if (at >= 0 && at < rows)
                    draw::outline(dc, {box.x + 1, box.y + at * row_height + 1, box.width - bar - 2, row_height - 2}, 2, colours.accent, 2.0f);
            }
            else
            {
                const std::int32_t at = m_drop_slot - m_first;

                if (at >= 0 && at <= rows)
                {
                    const std::int32_t line_y = std::clamp(box.y + at * row_height, box.y + 1, box.y + box.height - 1);
                    const std::int32_t cap = draw::px(3);
                    draw::fill(dc, {box.x + cap, line_y - 1, box.width - bar - 2 * cap, 2}, colours.accent);
                    draw::disc(dc, static_cast<float>(box.x + cap), static_cast<float>(line_y), static_cast<float>(cap), colours.accent, colours.accent, 0.0f);
                }
            }
        }

        // A list longer than it shows has a thin bar down its right edge.
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
