#include <chrome/controls/data/thumbnail_grid.hpp>

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
        // Sizes at 96 dpi.
        constexpr std::int32_t k_pad = 6;
        constexpr std::int32_t k_gap = 6;
        constexpr std::int32_t k_card_pad = 6;
        constexpr std::int32_t k_label_height = 18;
        constexpr std::int32_t k_radius = 5;
        constexpr std::int32_t k_well_radius = 4;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_min_tile = 56;
        constexpr std::int32_t k_max_tile = 200;
        constexpr std::int32_t k_zoom_step = 8;

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

        // The colours a tile without a picture takes, by its position.
        constexpr std::uint32_t k_tints[6] = {0x4F7CAC, 0x6BA368, 0xC9824B, 0x9A6BB5, 0xB5586A, 0x4FA3A5};
    }

    thumbnail_grid& thumbnail_grid::set_source(label_fn label, picture_fn picture, void* user, std::uint32_t count) noexcept
    {
        m_label = label;
        m_picture = picture;
        m_source_user = user;
        m_count = label != nullptr ? std::min(count, k_max_items) : 0;
        clear_selection();
        m_current = k_none;
        m_anchor = k_none;
        m_hover = k_none;
        m_scroll = 0;
        invalidate();
        return *this;
    }

    thumbnail_grid& thumbnail_grid::set_count(std::uint32_t count) noexcept
    {
        m_count = std::min(count, k_max_items);

        for (std::uint32_t i = m_count; i < k_max_items; ++i)
            set_bit(i, false);

        recount();

        if (m_current >= static_cast<std::int32_t>(m_count))
            m_current = k_none;

        if (m_anchor >= static_cast<std::int32_t>(m_count))
            m_anchor = k_none;

        m_hover = k_none;
        set_scroll(m_scroll);
        invalidate();
        return *this;
    }

    thumbnail_grid& thumbnail_grid::set_select_mode(grid_select_mode mode) noexcept
    {
        m_mode = mode;

        if (mode == grid_select_mode::single && m_selected_count > 1)
            select(first_selected());

        return *this;
    }

    thumbnail_grid& thumbnail_grid::set_rows(std::uint32_t rows) noexcept
    {
        m_rows = std::clamp<std::uint32_t>(rows, 1, 12);
        return *this;
    }

    thumbnail_grid& thumbnail_grid::set_tile_size(std::int32_t size) noexcept
    {
        m_tile = std::clamp(size, k_min_tile, k_max_tile);
        set_scroll(m_scroll);
        invalidate();
        return *this;
    }

    const char* thumbnail_grid::label(std::uint32_t index) const noexcept
    {
        return index < m_count && m_label != nullptr ? m_label(m_source_user, index) : nullptr;
    }

    void thumbnail_grid::set_bit(std::uint32_t index, bool on) noexcept
    {
        if (index >= k_max_items)
            return;

        const std::uint64_t mask = std::uint64_t{1} << (index & 63);

        if (on)
            m_bits[index >> 6] |= mask;
        else
            m_bits[index >> 6] &= ~mask;
    }

    void thumbnail_grid::recount() noexcept
    {
        std::uint32_t total = 0;

        for (const std::uint64_t word : m_bits)
            total += static_cast<std::uint32_t>(std::popcount(word));

        m_selected_count = total;
    }

    bool thumbnail_grid::is_selected(std::uint32_t index) const noexcept
    {
        return index < m_count && (m_bits[index >> 6] >> (index & 63) & 1) != 0;
    }

    std::int32_t thumbnail_grid::first_selected() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (is_selected(i))
                return static_cast<std::int32_t>(i);
        }

        return k_none;
    }

    thumbnail_grid& thumbnail_grid::select(std::int32_t index) noexcept
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

    thumbnail_grid& thumbnail_grid::select_all() noexcept
    {
        if (m_mode == grid_select_mode::single)
            return *this;

        select_range(0, static_cast<std::int32_t>(m_count) - 1, false);
        invalidate();
        return *this;
    }

    thumbnail_grid& thumbnail_grid::clear_selection() noexcept
    {
        std::memset(m_bits, 0, sizeof(m_bits));
        m_selected_count = 0;
        invalidate();
        return *this;
    }

    void thumbnail_grid::select_range(std::int32_t from, std::int32_t to, bool keep) noexcept
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

    dock_rect thumbnail_grid::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    std::int32_t thumbnail_grid::card_height() const noexcept
    {
        const std::int32_t image = draw::px(m_tile) - 2 * draw::px(k_card_pad);
        return draw::px(k_card_pad) + image + draw::px(4) + draw::px(k_label_height) + draw::px(4);
    }

    std::int32_t thumbnail_grid::row_step() const noexcept
    {
        return card_height() + draw::px(k_gap);
    }

    namespace
    {
        // How many tiles of a width fit in a space, with the gap between them.
        std::int32_t fit(std::int32_t space, std::int32_t card, std::int32_t gap) noexcept
        {
            return std::max((space + gap) / std::max(card + gap, 1), 1);
        }
    }

    bool thumbnail_grid::bar_needed() const noexcept
    {
        if (m_count == 0)
            return false;

        const std::int32_t cols = fit(inner().width - 2 * draw::px(k_pad), draw::px(m_tile), draw::px(k_gap));
        const std::int32_t rows = (static_cast<std::int32_t>(m_count) + cols - 1) / cols;
        return 2 * draw::px(k_pad) + rows * row_step() - draw::px(k_gap) > inner().height;
    }

    std::int32_t thumbnail_grid::columns() const noexcept
    {
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        return fit(inner().width - bar - 2 * draw::px(k_pad), draw::px(m_tile), draw::px(k_gap));
    }

    std::int32_t thumbnail_grid::content_height() const noexcept
    {
        if (m_count == 0)
            return 0;

        const std::int32_t cols = columns();
        const std::int32_t rows = (static_cast<std::int32_t>(m_count) + cols - 1) / cols;
        return 2 * draw::px(k_pad) + rows * row_step() - draw::px(k_gap);
    }

    std::int32_t thumbnail_grid::max_scroll() const noexcept
    {
        return std::max(content_height() - inner().height, 0);
    }

    std::int32_t thumbnail_grid::bar_left() const noexcept
    {
        const dock_rect box = inner();
        return box.x + box.width - draw::px(k_bar_column);
    }

    thumbnail_grid::thumb_span thumbnail_grid::thumb() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t total = std::max(content_height(), 1);
        thumb_span span;
        span.length = std::clamp(box.height * box.height / total, std::min(draw::px(k_min_thumb), box.height), box.height);
        span.travel = box.height - span.length;
        const std::int32_t reach = max_scroll();
        span.start = box.y + (reach > 0 ? span.travel * m_scroll / reach : 0);
        return span;
    }

    dock_rect thumbnail_grid::card_area(std::int32_t index) const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::int32_t cols = columns();
        const std::int32_t card = draw::px(m_tile);
        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t used = cols * card + (cols - 1) * gap;
        // The tiles start at the left edge, so a wide panel is not left with an empty strip on each side.
        (void)used;
        (void)bar;
        const std::int32_t left = box.x + draw::px(k_pad);
        return {left + (index % cols) * (card + gap), box.y + draw::px(k_pad) + (index / cols) * row_step() - m_scroll, card, card_height()};
    }

    dock_rect thumbnail_grid::label_area(std::uint32_t index) const noexcept
    {
        if (index >= m_count)
            return {};

        const dock_rect card = card_area(static_cast<std::int32_t>(index));
        const dock_rect box = inner();
        const std::int32_t pad = draw::px(k_card_pad);
        const dock_rect text{card.x + draw::px(2), card.y + pad + card.width - 2 * pad + draw::px(4), card.width - draw::px(4), draw::px(k_label_height)};

        if (text.y < box.y || text.y + text.height > box.y + box.height)
            return {};

        return text;
    }

    std::int32_t thumbnail_grid::item_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect box = inner();

        if (m_count == 0 || y < box.y || y >= box.y + box.height || x < box.x || x >= box.x + box.width)
            return k_none;

        if (bar_needed() && x >= bar_left())
            return k_none;

        const std::int32_t cols = columns();
        const dock_rect first = card_area(0);
        const std::int32_t col_step = first.width + draw::px(k_gap);
        const std::int32_t col = (x - first.x) / col_step;
        const std::int32_t row = (y - first.y + row_step() * 4096) / row_step() - 4096;

        if (x < first.x || col >= cols || row < 0)
            return k_none;

        const std::int32_t index = row * cols + col;

        if (index >= static_cast<std::int32_t>(m_count))
            return k_none;

        const dock_rect card = card_area(index);
        const bool inside = x >= card.x && x < card.x + card.width && y >= card.y && y < card.y + card.height;
        return inside ? index : k_none;
    }

    void thumbnail_grid::set_scroll(std::int32_t scroll) noexcept
    {
        const std::int32_t clamped = std::clamp(scroll, 0, max_scroll());

        if (clamped == m_scroll)
            return;

        m_scroll = clamped;
        invalidate();
    }

    void thumbnail_grid::reveal(std::uint32_t index) noexcept
    {
        if (index >= m_count)
            return;

        const dock_rect box = inner();
        const dock_rect card = card_area(static_cast<std::int32_t>(index));
        const std::int32_t pad = draw::px(k_pad) / 2;

        if (card.y < box.y + pad)
            set_scroll(m_scroll - (box.y + pad - card.y));
        else if (card.y + card.height > box.y + box.height - pad)
            set_scroll(m_scroll + card.y + card.height - (box.y + box.height - pad));
    }

    void thumbnail_grid::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void thumbnail_grid::activate(std::int32_t item) noexcept
    {
        if (item >= 0 && item < static_cast<std::int32_t>(m_count) && m_activate != nullptr)
            m_activate(m_activate_user, *this, static_cast<std::uint32_t>(item));
    }

    bool thumbnail_grid::click_item(std::int32_t item) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));
        const bool multi = m_mode == grid_select_mode::multi;

        if (multi && shift_down() && m_anchor != k_none)
        {
            select_range(m_anchor, item, ctrl_down());
        }
        else if (multi && ctrl_down())
        {
            set_bit(static_cast<std::uint32_t>(item), !is_selected(static_cast<std::uint32_t>(item)));
            recount();
            m_anchor = item;
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(static_cast<std::uint32_t>(item), true);
            recount();
            m_anchor = item;
        }

        m_current = item;
        reveal(static_cast<std::uint32_t>(item));
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    bool thumbnail_grid::move_current(std::int32_t item, bool extend) noexcept
    {
        std::uint64_t before[k_words];
        std::memcpy(before, m_bits, sizeof(before));

        if (m_mode == grid_select_mode::multi && extend)
        {
            if (m_anchor == k_none)
                m_anchor = m_current != k_none ? m_current : item;

            select_range(m_anchor, item, false);
        }
        else
        {
            std::memset(m_bits, 0, sizeof(m_bits));
            set_bit(static_cast<std::uint32_t>(item), true);
            recount();
            m_anchor = item;
        }

        m_current = item;
        reveal(static_cast<std::uint32_t>(item));
        return std::memcmp(before, m_bits, sizeof(before)) != 0;
    }

    control_size thumbnail_grid::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size thumbnail_grid::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(120));
        const std::int32_t rows = static_cast<std::int32_t>(m_rows);
        return {width, 2 * draw::px(k_pad) + rows * row_step() - draw::px(k_gap) + 2 * draw::px(1)};
    }

    void thumbnail_grid::on_press(std::int32_t x, std::int32_t y) noexcept
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
                set_scroll(m_scroll + (y < span.start ? -1 : 1) * std::max(inner().height - row_step() / 2, 1));
            }

            invalidate();
            return;
        }

        const std::int32_t item = item_at(x, y);

        if (item == k_none)
        {
            // A click on the empty well lets go of everything.
            if (!ctrl_down() && !shift_down() && m_selected_count > 0)
            {
                clear_selection();
                m_current = k_none;
                notify_change();
            }

            return;
        }

        if (m_drag_fn != nullptr && !(m_mode == grid_select_mode::multi && (ctrl_down() || shift_down())))
        {
            m_drag = drag_kind::lift;
            m_lift_item = item;
            m_lift_x = x;
            m_lift_y = y;

            if (!is_selected(static_cast<std::uint32_t>(item)))
            {
                if (click_item(item))
                    notify_change();
                else
                    invalidate();
            }
            else
            {
                m_current = item;
                invalidate();
            }

            return;
        }

        if (click_item(item))
            notify_change();
        else
            invalidate();
    }

    void thumbnail_grid::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        on_press(x, y);
        activate(item_at(x, y));
    }

    void thumbnail_grid::on_release() noexcept
    {
        // Let go on a selected tile without dragging: the selection narrows to that tile, as a click does.
        if (m_drag == drag_kind::lift && m_lift_item != k_none && m_selected_count > 1)
        {
            if (click_item(m_lift_item))
                notify_change();
        }

        m_drag = drag_kind::none;
        m_lift_item = k_none;
    }

    bool thumbnail_grid::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_drag == drag_kind::lift)
        {
            if (!drag_drop::exceeds_threshold(x - m_lift_x, y - m_lift_y))
                return false;

            drag_payload payload;
            const bool started = m_drag_fn != nullptr && m_drag_fn(m_drag_user, *this, payload) && begin_drag(payload);
            m_drag = drag_kind::none;
            m_lift_item = k_none;
            return started;
        }

        if (m_drag == drag_kind::bar)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - inner().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_scroll;
            set_scroll(span.travel > 0 ? (at * max_scroll() + span.travel / 2) / span.travel : 0);
            return m_scroll != before;
        }

        return false;
    }

    drop_effect thumbnail_grid::on_drag_over(const drag_event& event) noexcept
    {
        const dock_rect box = inner();
        const std::int32_t edge = draw::px(12);

        // Held near the top or the bottom, the tiles move along.
        if (event.y < box.y + edge)
            set_scroll(m_scroll - draw::px(10));
        else if (event.y >= box.y + box.height - edge)
            set_scroll(m_scroll + draw::px(10));

        const std::int32_t item = item_at(event.x, event.y);
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, item, false) : drop_effect::none;
        const std::int32_t shown = effect != drop_effect::none ? item : k_none;

        if (shown != m_drop_item)
        {
            m_drop_item = shown;
            invalidate();
        }

        return effect;
    }

    void thumbnail_grid::on_drag_leave() noexcept
    {
        m_drop_item = k_none;
        invalidate();
    }

    drop_effect thumbnail_grid::on_drop(const drag_event& event) noexcept
    {
        const drop_effect effect = m_drop_fn != nullptr ? m_drop_fn(m_drop_user, *this, event, item_at(event.x, event.y), true) : drop_effect::none;
        m_drop_item = k_none;
        invalidate();
        return effect;
    }

    bool thumbnail_grid::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t item = item_at(x, y);

        if (item == m_hover)
            return false;

        m_hover = item;
        return true;
    }

    bool thumbnail_grid::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;

        if (m_zoomable && ctrl_down())
        {
            const std::int32_t before = m_tile;
            set_tile_size(m_tile + notches * k_zoom_step);

            if (m_tile != before && m_size_handler != nullptr)
                m_size_handler(m_size_user, *this);

            return m_tile != before;
        }

        const std::int32_t before = m_scroll;
        set_scroll(m_scroll - notches * row_step() / 2);

        // At either end the panel gets the wheel, so it can scroll on.
        return m_scroll != before;
    }

    bool thumbnail_grid::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_count == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_count) - 1;
        const std::int32_t cols = columns();
        const std::int32_t page = std::max(inner().height / std::max(row_step(), 1), 1) * cols;
        const std::int32_t from = m_current != k_none ? m_current : 0;
        std::int32_t target = k_none;

        switch (key)
        {
        case k_key_left:
            target = m_current == k_none ? 0 : std::max(from - 1, 0);
            break;

        case k_key_right:
            target = m_current == k_none ? 0 : std::min(from + 1, last);
            break;

        case k_key_up:
            target = m_current == k_none ? 0 : (from - cols >= 0 ? from - cols : from);
            break;

        case k_key_down:
            target = m_current == k_none ? 0 : (from + cols <= last ? from + cols : from);
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
            if (!ctrl || m_mode != grid_select_mode::multi)
                return false;

            select_range(0, last, false);
            notify_change();
            return true;

        case k_key_space:
            if (!ctrl || m_current == k_none)
                return false;

            set_bit(static_cast<std::uint32_t>(m_current), m_mode == grid_select_mode::multi ? !is_selected(static_cast<std::uint32_t>(m_current)) : true);
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

    void thumbnail_grid::paint_card(void* dc, std::int32_t index, const dock_rect& place, bool lit) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool selected = is_selected(static_cast<std::uint32_t>(index));
        const bool hovered = hot() && index == m_hover && enabled();

        // A tile under the pointer lifts a little, which its shadow shows.
        dock_rect card = place;

        if (hovered)
            card.y -= draw::px(1);

        std::uint32_t top = blend_colour(colours.panel, white, 0.10f);
        std::uint32_t bottom = blend_colour(colours.panel, black, 0.04f);
        std::uint32_t edge = blend_colour(colours.title, black, 0.45f);
        std::uint32_t ink = colours.text;

        if (selected && enabled())
        {
            const std::uint32_t body = lit ? colours.accent : blend_colour(colours.accent, colours.panel, 0.45f);
            top = blend_colour(body, white, 0.12f);
            bottom = blend_colour(body, black, 0.28f);
            edge = blend_colour(body, black, 0.5f);
            ink = white;
        }
        else if (hovered)
        {
            top = blend_colour(top, white, 0.08f);
            bottom = blend_colour(bottom, white, 0.04f);
        }

        draw::raised(dc, card, k_radius, top, bottom, edge, true, hovered ? 9 : 6, hovered ? 4 : 2, hovered || selected ? 0.55f : 0.4f);

        // The picture sits in a recess: dark at the top where the card's edge shades it, with a lit lip along the bottom.
        const std::int32_t pad = draw::px(k_card_pad);
        const std::int32_t side = card.width - 2 * pad;
        const dock_rect frame{card.x + pad, card.y + pad, side, side};
        const std::uint32_t lip = blend_colour(bottom, white, 0.22f);
        draw::fill(dc, {frame.x + draw::px(2), frame.y + frame.height, std::max(frame.width - draw::px(4), 0), 1}, lip);
        draw::raised(dc, frame, 3, blend_colour(colours.panel, black, 0.55f), blend_colour(colours.panel, black, 0.38f), blend_colour(colours.panel, black, 0.62f), false, 0, 0, 0.0f);

        const std::int32_t inset = draw::px(3);
        const dock_rect picture{frame.x + inset, frame.y + inset, std::max(frame.width - 2 * inset, 1), std::max(frame.height - 2 * inset, 1)};

        if (m_picture != nullptr)
        {
            m_picture(m_source_user, dc, static_cast<std::uint32_t>(index), picture);
        }
        else
        {
            const std::uint32_t tint = k_tints[static_cast<std::uint32_t>(index) % 6];
            draw::raised(dc, picture, 2, blend_colour(tint, white, 0.18f), blend_colour(tint, black, 0.3f), blend_colour(tint, black, 0.3f), true, 0, 0, 0.0f);
            const char* name = label(static_cast<std::uint32_t>(index));
            char letter[2]{name != nullptr && name[0] != '\0' ? name[0] : '?', '\0'};
            draw::text(dc, picture, letter, 0xFFFFFF, draw::text_bold | draw::text_center);
        }


        // The shade the card's rim throws on the top of the picture.

        if (const char* name = label(static_cast<std::uint32_t>(index)))
        {
            const dock_rect text_box{card.x + pad, frame.y + frame.height + draw::px(4), card.width - 2 * pad, draw::px(k_label_height)};
            draw::text(dc, text_box, name, ink, draw::text_ellipsis | draw::text_center);
        }

        if (lit && index == m_current && enabled())
        {
            const dock_rect ring{card.x - 1, card.y - 1, card.width + 2, card.height + 2};
            draw::outline(dc, ring, k_radius + 1, selected ? blend_colour(colours.accent, white, 0.55f) : colours.accent, 2.0f);
        }
    }

    void thumbnail_grid::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool lit = focused();
        std::uint32_t top = blend_colour(colours.panel, black, 0.30f);
        std::uint32_t bottom = blend_colour(colours.panel, black, 0.16f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.55f);

        if (!enabled())
        {
            top = bottom = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
        }

        // The well the tiles lie in is sunk: darker at the top, where the frame shades it.
        draw::gradient(dc, area(), k_well_radius, top, bottom, edge, false);

        const dock_rect box = inner();
        draw::clip(dc, box);

        if (m_count == 0)
        {
            if (m_empty != nullptr)
                draw::text(dc, box, m_empty, colours.text_dim, draw::text_center);
        }

        const std::int32_t cols = columns();
        const std::int32_t step = row_step();
        const std::int32_t first_row = std::max(m_scroll - draw::px(k_pad), 0) / std::max(step, 1);
        const std::int32_t last_row = (m_scroll + box.height) / std::max(step, 1);

        // Where the first tile is, and how far apart the others are, worked out once for the whole paint.
        const dock_rect origin = card_area(0);
        const std::int32_t across = origin.width + draw::px(k_gap);

        // Hovered and selected tiles last, so their shadows fall on the others and not under them.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::int32_t row = first_row; row <= last_row; ++row)
            {
                for (std::int32_t col = 0; col < cols; ++col)
                {
                    const std::int32_t index = row * cols + col;

                    if (index >= static_cast<std::int32_t>(m_count))
                        break;

                    const bool raised = is_selected(static_cast<std::uint32_t>(index)) || (hot() && index == m_hover);

                    if ((pass == 1) == raised)
                        paint_card(dc, index, {origin.x + col * across, origin.y + row * step, origin.width, origin.height}, lit);
                }
            }
        }

        // The tile a drop would land in.
        if (m_drop_item != k_none && enabled())
        {
            const dock_rect card = card_area(m_drop_item);
            draw::outline(dc, {card.x - 1, card.y - 1, card.width + 2, card.height + 2}, k_radius + 1, colours.accent, 2.0f);
        }

        // A grid longer than it shows has a thin bar down its right edge.
        if (bar_needed())
        {
            const std::int32_t bar = draw::px(k_bar_column);
            const thumb_span span = thumb();
            const std::int32_t width = draw::px(k_thumb_width);
            const std::uint32_t thumb_colour = m_drag == drag_kind::bar ? colours.text_dim : blend_colour(colours.text_dim, blend_colour(colours.panel, black, 0.2f), hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {box.x + box.width - (bar + width) / 2, span.start, width, span.length}, 2, thumb_colour, thumb_colour);
        }

        draw::unclip(dc);
    }
}
