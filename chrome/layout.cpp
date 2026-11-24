#include <chrome/layout.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    struct layout_container::plan
    {
        // Where each child goes, from the top left of the container.
        dock_rect cell[k_max_children]{};
        bool live[k_max_children]{};
        std::uint32_t live_count{0};
        // The size of all of it, padding included, before the reveal.
        control_size size{};
    };

    namespace
    {
        [[nodiscard]] constexpr std::int32_t at_least_zero(std::int32_t value) noexcept
        {
            return value > 0 ? value : 0;
        }

        // Where a size sits in the room avail, and how big it becomes.
        void align_in(layout_align align, std::int32_t avail, std::int32_t size, std::int32_t& offset, std::int32_t& out) noexcept
        {
            offset = 0;
            out = size;

            switch (align)
            {
            case layout_align::stretch: out = at_least_zero(avail); break;
            case layout_align::center: offset = at_least_zero((avail - size) / 2); break;
            case layout_align::end: offset = at_least_zero(avail - size); break;
            default: break;
            }
        }

        [[nodiscard]] dock_rect intersect(const dock_rect& a, const dock_rect& b) noexcept
        {
            const std::int32_t left = std::max(a.x, b.x);
            const std::int32_t top = std::max(a.y, b.y);
            const std::int32_t right = std::min(a.x + a.width, b.x + b.width);
            const std::int32_t bottom = std::min(a.y + a.height, b.y + b.height);
            return {left, top, at_least_zero(right - left), at_least_zero(bottom - top)};
        }

        // Hands out extra among weights so that the shares add up to exactly extra.
        struct sharer
        {
            std::int32_t extra{0};
            float weights{0.0f};
            float carried{0.0f};
            std::int32_t given{0};

            [[nodiscard]] std::int32_t next(float weight) noexcept
            {
                if (weight <= 0.0f || weights <= 0.0f)
                    return 0;

                carried += static_cast<float>(extra) * weight / weights;
                const std::int32_t target = static_cast<std::int32_t>(std::lround(carried));
                const std::int32_t share = target - given;
                given = target;
                return share;
            }
        };
    }

    layout_slot& layout_container::add(control& item) noexcept
    {
        if (full())
            return m_spare;

        layout_slot& slot = m_slots[m_count++];
        slot = {};
        slot.m_item = &item;
        return slot;
    }

    layout_slot& layout_container::add(layout_container& box) noexcept
    {
        if (full() || &box == this)
            return m_spare;

        layout_slot& slot = m_slots[m_count++];
        slot = {};
        slot.m_box = &box;
        return slot;
    }

    bool layout_container::is_live(const layout_slot& slot) noexcept
    {
        if (slot.m_item != nullptr)
            return slot.m_item->visible();

        return slot.m_box != nullptr && !slot.m_box->empty();
    }

    bool layout_container::empty() const noexcept
    {
        if (!m_visible)
            return true;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (is_live(m_slots[i]))
                return false;
        }

        return true;
    }

    control_size layout_container::slot_size(void* dc, const layout_slot& slot, std::int32_t max_width) noexcept
    {
        const std::int32_t room = at_least_zero(max_width);
        control_size size = slot.m_item != nullptr ? slot.m_item->preferred_size_in(dc, room) : slot.m_box->measure(dc, room);

        if (slot.m_width > 0)
            size.width = slot.m_width;

        if (slot.m_height > 0)
            size.height = slot.m_height;

        return size;
    }

    layout_align layout_container::across_align(const layout_slot& slot) const noexcept
    {
        if (slot.m_align != layout_align::automatic)
            return slot.m_align;

        return m_align == layout_align::automatic ? layout_align::start : m_align;
    }

    layout_align layout_container::horizontal_align(const layout_slot& slot) const noexcept
    {
        if (slot.m_align != layout_align::automatic)
            return slot.m_align;

        if (slot.m_box != nullptr || (slot.m_item != nullptr && slot.m_item->fills_row()))
            return layout_align::stretch;

        return m_align == layout_align::automatic ? layout_align::start : m_align;
    }

    layout_align layout_container::vertical_align(const layout_slot& slot) const noexcept
    {
        if (slot.m_valign != layout_align::automatic)
            return slot.m_valign;

        return m_valign == layout_align::automatic ? layout_align::start : m_valign;
    }

    void layout_container::solve(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept
    {
        out = {};

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            out.live[i] = is_live(m_slots[i]);

            if (out.live[i])
                ++out.live_count;
        }

        if (out.live_count == 0)
            return;

        switch (m_kind)
        {
        case layout_kind::row: solve_row(dc, width, height, fit, out); break;
        case layout_kind::column: solve_column(dc, width, height, fit, out); break;
        case layout_kind::wrap: solve_wrap(dc, width, fit, out); break;
        case layout_kind::grid: solve_grid(dc, width, fit, out); break;
        }
    }

    void layout_container::solve_row(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept
    {
        const std::int32_t inner = at_least_zero(width - m_pad_l - m_pad_r);
        std::int32_t widths[k_max_children]{};
        float grow[k_max_children]{};
        std::int32_t total = m_gap_x * static_cast<std::int32_t>(out.live_count - 1);
        float grow_sum = 0.0f;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            const layout_slot& slot = m_slots[i];
            control_size size = slot_size(dc, slot, inner);
            grow[i] = slot.m_grow;

            // A control that fills the row takes what the others leave, from the width it has on its own.
            if (grow[i] <= 0.0f && slot.m_item != nullptr && slot.m_item->fills_row())
            {
                grow[i] = 1.0f;

                if (slot.m_width <= 0)
                    size.width = slot.m_item->preferred_size(dc).width;
            }

            widths[i] = size.width;
            total += size.width;
            grow_sum += grow[i];
        }

        if (fit && grow_sum > 0.0f)
        {
            // The children that grow also give way when the row is narrower than they are, down to nothing.
            std::int32_t extra = inner - total;

            if (extra < 0)
            {
                std::int32_t room = 0;

                for (std::uint32_t i = 0; i < m_count; ++i)
                    room += out.live[i] && grow[i] > 0.0f ? widths[i] : 0;

                extra = -std::min(-extra, room);
            }

            sharer split{extra, grow_sum};

            for (std::uint32_t i = 0; i < m_count; ++i)
            {
                if (out.live[i])
                    widths[i] = at_least_zero(widths[i] + split.next(grow[i]));
            }

            total += extra;
        }

        // Heights are taken at the widths the children end up with, since a label may wrap.
        std::int32_t heights[k_max_children]{};
        std::int32_t line = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            heights[i] = slot_size(dc, m_slots[i], widths[i]).height;
            line = std::max(line, heights[i]);
        }

        if (fit && height > 0)
            line = std::max(line, height - m_pad_t - m_pad_b);

        std::int32_t x = m_pad_l;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            std::int32_t offset = 0;
            std::int32_t h = 0;
            align_in(across_align(m_slots[i]), line, heights[i], offset, h);
            out.cell[i] = {x, m_pad_t + offset, widths[i], h};
            x += widths[i] + m_gap_x;
        }

        out.size = {fit ? width : total + m_pad_l + m_pad_r, line + m_pad_t + m_pad_b};
    }

    void layout_container::solve_column(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept
    {
        const std::int32_t inner = at_least_zero(width - m_pad_l - m_pad_r);
        control_size sizes[k_max_children]{};
        std::int32_t widest = 0;
        std::int32_t total = m_gap_y * static_cast<std::int32_t>(out.live_count - 1);
        float grow_sum = 0.0f;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            sizes[i] = slot_size(dc, m_slots[i], inner);
            widest = std::max(widest, sizes[i].width);
            total += sizes[i].height;
            grow_sum += m_slots[i].m_grow;
        }

        std::int32_t extra = 0;

        if (fit && height > 0 && grow_sum > 0.0f)
            extra = at_least_zero(height - m_pad_t - m_pad_b - total);

        sharer split{extra, grow_sum};
        std::int32_t y = m_pad_t;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            const std::int32_t h = sizes[i].height + split.next(m_slots[i].m_grow);
            std::int32_t offset = 0;
            std::int32_t w = 0;
            align_in(horizontal_align(m_slots[i]), inner, sizes[i].width, offset, w);
            out.cell[i] = {m_pad_l + offset, y, w, h};
            y += h + m_gap_y;
        }

        out.size = {fit ? width : widest + m_pad_l + m_pad_r, total + extra + m_pad_t + m_pad_b};
    }

    void layout_container::solve_wrap(void* dc, std::int32_t width, bool fit, plan& out) const noexcept
    {
        const std::int32_t inner = at_least_zero(width - m_pad_l - m_pad_r);
        control_size sizes[k_max_children]{};
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t line_height = 0;
        std::int32_t widest = 0;
        std::int32_t first = -1;
        bool closed = false;

        // Aligns the children of the line that ends here across the height of the line.
        const auto close_line = [&](std::int32_t last) {
            for (std::int32_t k = first; k >= 0 && k <= last; ++k)
            {
                if (!out.live[k])
                    continue;

                std::int32_t offset = 0;
                std::int32_t h = 0;
                align_in(across_align(m_slots[k]), line_height, sizes[k].height, offset, h);
                out.cell[k].y = m_pad_t + y + offset;
                out.cell[k].height = h;
            }
        };

        std::int32_t last_live = -1;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            const layout_slot& slot = m_slots[i];
            sizes[i] = slot_size(dc, slot, inner);
            const bool fills = slot.m_item != nullptr && slot.m_item->fills_row();

            // A control that fills the row stands on a line of its own, as wide as the line.
            if (x > 0 && (closed || fills || x + sizes[i].width > inner))
            {
                close_line(last_live);
                y += line_height + m_gap_y;
                x = 0;
                line_height = 0;
                first = -1;
            }

            if (first < 0)
                first = static_cast<std::int32_t>(i);

            const std::int32_t w = fills ? inner : sizes[i].width;
            out.cell[i] = {m_pad_l + x, 0, w, sizes[i].height};
            x += w + m_gap_x;
            widest = std::max(widest, x - m_gap_x);
            line_height = std::max(line_height, sizes[i].height);
            closed = fills;
            last_live = static_cast<std::int32_t>(i);
        }

        close_line(last_live);
        out.size = {fit ? width : widest + m_pad_l + m_pad_r, y + line_height + m_pad_t + m_pad_b};
    }

    void layout_container::solve_grid(void* dc, std::int32_t width, bool fit, plan& out) const noexcept
    {
        const std::int32_t inner = at_least_zero(width - m_pad_l - m_pad_r);
        const std::uint32_t columns = m_columns;
        std::uint32_t column_of[k_max_children]{};
        std::uint32_t row_of[k_max_children]{};
        std::uint32_t span_of[k_max_children]{};
        control_size sizes[k_max_children]{};
        std::int32_t column_width[k_max_columns]{};
        std::uint32_t used = 0;
        std::uint32_t rows = 0;
        std::uint32_t col = 0;
        std::uint32_t row = 0;
        bool any = false;

        // Deal the children into cells.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            const std::uint32_t span = std::min(m_slots[i].m_span, columns);

            if (col + span > columns)
            {
                col = 0;
                ++row;
            }

            column_of[i] = col;
            row_of[i] = row;
            span_of[i] = span;
            used = std::max(used, col + span);
            rows = row + 1;
            col += span;
            any = true;

            if (col >= columns)
            {
                col = 0;
                ++row;
            }
        }

        if (!any)
            return;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (out.live[i])
                sizes[i] = slot_size(dc, m_slots[i], inner);
        }

        // A column is as wide as its widest single cell. A cell that spans needs the columns it covers to add up to its
        // width, and the last of them is widened if they do not.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (out.live[i] && span_of[i] == 1)
                column_width[column_of[i]] = std::max(column_width[column_of[i]], sizes[i].width);
        }

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i] || span_of[i] <= 1)
                continue;

            std::int32_t have = m_gap_x * static_cast<std::int32_t>(span_of[i] - 1);

            for (std::uint32_t c = column_of[i]; c < column_of[i] + span_of[i]; ++c)
                have += column_width[c];

            if (sizes[i].width > have)
                column_width[column_of[i] + span_of[i] - 1] += sizes[i].width - have;
        }

        if (m_uniform)
        {
            std::int32_t widest = 0;

            for (std::uint32_t c = 0; c < used; ++c)
                widest = std::max(widest, column_width[c]);

            for (std::uint32_t c = 0; c < used; ++c)
                column_width[c] = widest;
        }

        std::int32_t total = m_gap_x * static_cast<std::int32_t>(used - 1);

        for (std::uint32_t c = 0; c < used; ++c)
            total += column_width[c];

        if (fit)
        {
            float weights = 0.0f;

            for (std::uint32_t c = 0; c < used; ++c)
                weights += m_column_grow[c];

            if (weights > 0.0f)
            {
                // The columns that grow also give way when the grid is narrower than its cells, down to nothing.
                std::int32_t extra = inner - total;

                if (extra < 0)
                {
                    std::int32_t room = 0;

                    for (std::uint32_t c = 0; c < used; ++c)
                        room += m_column_grow[c] > 0.0f ? column_width[c] : 0;

                    extra = -std::min(-extra, room);
                }

                sharer split{extra, weights};

                for (std::uint32_t c = 0; c < used; ++c)
                    column_width[c] = at_least_zero(column_width[c] + split.next(m_column_grow[c]));

                total += extra;
            }
        }

        std::int32_t column_x[k_max_columns]{};
        std::int32_t x = m_pad_l;

        for (std::uint32_t c = 0; c < used; ++c)
        {
            column_x[c] = x;
            x += column_width[c] + m_gap_x;
        }

        // A row is as tall as its tallest cell, taken at the width the cell ends up with.
        std::int32_t row_height[k_max_children]{};
        std::int32_t cell_width[k_max_children]{};

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            std::int32_t w = m_gap_x * static_cast<std::int32_t>(span_of[i] - 1);

            for (std::uint32_t c = column_of[i]; c < column_of[i] + span_of[i]; ++c)
                w += column_width[c];

            cell_width[i] = w;
            const std::int32_t h = slot_size(dc, m_slots[i], w).height;
            sizes[i].height = h;
            row_height[row_of[i]] = std::max(row_height[row_of[i]], h);
        }

        std::int32_t row_y[k_max_children]{};
        std::int32_t y = m_pad_t;

        for (std::uint32_t r = 0; r < rows; ++r)
        {
            row_y[r] = y;
            y += row_height[r] + m_gap_y;
        }

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!out.live[i])
                continue;

            std::int32_t ox = 0;
            std::int32_t w = 0;
            std::int32_t oy = 0;
            std::int32_t h = 0;
            align_in(horizontal_align(m_slots[i]), cell_width[i], sizes[i].width, ox, w);
            align_in(vertical_align(m_slots[i]), row_height[row_of[i]], sizes[i].height, oy, h);
            out.cell[i] = {column_x[column_of[i]] + ox, row_y[row_of[i]] + oy, w, h};
        }

        out.size = {fit ? width : total + m_pad_l + m_pad_r, y - m_gap_y + m_pad_b};
    }

    control_size layout_container::measure(void* dc, std::int32_t max_width) const noexcept
    {
        if (empty())
            return {};

        plan p;
        solve(dc, max_width, 0, false, p);

        const float shown = static_cast<float>(p.size.height) * m_reveal;
        return {p.size.width, static_cast<std::int32_t>(shown + 0.5f)};
    }

    void layout_container::blank() noexcept
    {
        m_area = {};

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_slots[i].m_item != nullptr)
                m_slots[i].m_item->set_area({});
            else if (m_slots[i].m_box != nullptr)
                m_slots[i].m_box->blank();
        }
    }

    control_size layout_container::arrange_in(void* dc, const dock_rect& area, const dock_rect* clip) noexcept
    {
        plan p;
        const bool partial = m_reveal < 1.0f;
        // A container that is only partly shown is laid out at its full height, and clipped.
        solve(dc, area.width, partial ? 0 : area.height, true, p);

        if (!m_visible || p.live_count == 0)
        {
            blank();
            m_area = {area.x, area.y, area.width, 0};
            return {area.width, 0};
        }

        const std::int32_t shown = partial ? static_cast<std::int32_t>(static_cast<float>(p.size.height) * m_reveal + 0.5f) : p.size.height;
        m_area = {area.x, area.y, area.width, shown};

        dock_rect limit{};
        bool limited = false;

        if (clip != nullptr)
        {
            limit = *clip;
            limited = true;
        }

        if (partial)
        {
            limit = limited ? intersect(limit, m_area) : m_area;
            limited = true;
        }

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            layout_slot& slot = m_slots[i];

            if (!p.live[i])
            {
                if (slot.m_item != nullptr)
                    slot.m_item->set_area({});
                else if (slot.m_box != nullptr)
                    slot.m_box->blank();

                continue;
            }

            const dock_rect cell{area.x + p.cell[i].x, area.y + p.cell[i].y, p.cell[i].width, p.cell[i].height};

            if (slot.m_item != nullptr)
            {
                slot.m_item->set_area(cell);

                if (limited)
                    slot.m_item->set_clip(limit);
                else
                    slot.m_item->clear_clip();
            }
            else
                (void)slot.m_box->arrange_in(dc, cell, limited ? &limit : nullptr);
        }

        return {area.width, shown};
    }
}
