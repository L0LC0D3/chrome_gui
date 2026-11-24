#include <chrome/controls/core/tab_bar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/controls/core/tab_style.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>

namespace chrome
{
    namespace
    {
        // Sizes at 96 dpi. The look of the tabs is in tab_style.
        constexpr std::int32_t k_drag_start = 4;
        constexpr std::int32_t k_wheel_step = 48;
        constexpr std::int32_t k_empty_width = 240;

        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_delete = 0x2E;
        constexpr std::uint32_t k_key_w = 0x57;

        [[nodiscard]] bool inside(const dock_rect& box, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
        }
    }

    std::int32_t tab_bar::add_tab(const char* title, icon_kind icon) noexcept
    {
        if (m_count >= k_max_tabs)
            return -1;

        tab_item& item = m_tabs[m_count];
        std::snprintf(item.title, sizeof(item.title), "%s", title != nullptr ? title : "");
        item.icon = icon;
        item.modified = false;
        item.pinned = false;
        m_stale = true;
        invalidate();

        if (m_selected < 0)
            m_selected = m_count;

        return m_count++;
    }

    void tab_bar::remove_tab(std::int32_t index) noexcept
    {
        if (index < 0 || index >= m_count)
            return;

        for (std::int32_t i = index; i + 1 < m_count; ++i)
            m_tabs[i] = m_tabs[i + 1];

        --m_count;

        if (m_selected > index)
            --m_selected;
        else if (m_selected == index)
            m_selected = std::min(index, m_count - 1);

        m_hot_tab = -1;
        m_hot_close = false;
        m_press = {};
        m_stale = true;
        m_reveal = true;
        invalidate();
    }

    void tab_bar::clear_tabs() noexcept
    {
        m_count = 0;
        m_selected = -1;
        m_hot_tab = -1;
        m_hot_close = false;
        m_press = {};
        m_scroll = 0;
        m_stale = true;
        invalidate();
    }

    void tab_bar::select(std::int32_t index) noexcept
    {
        if (index < 0 || index >= m_count || index == m_selected)
            return;

        m_selected = index;
        m_reveal = true;
        invalidate();
    }

    void tab_bar::set_title(std::int32_t index, const char* title) noexcept
    {
        if (index < 0 || index >= m_count)
            return;

        std::snprintf(m_tabs[index].title, sizeof(m_tabs[index].title), "%s", title != nullptr ? title : "");
        m_stale = true;
        invalidate();
    }

    void tab_bar::set_icon(std::int32_t index, icon_kind icon) noexcept
    {
        if (index < 0 || index >= m_count)
            return;

        m_tabs[index].icon = icon;
        m_stale = true;
        invalidate();
    }

    void tab_bar::set_modified(std::int32_t index, bool modified) noexcept
    {
        if (index < 0 || index >= m_count || m_tabs[index].modified == modified)
            return;

        m_tabs[index].modified = modified;
        m_stale = true;
        invalidate();
    }

    void tab_bar::set_pinned(std::int32_t index, bool pinned) noexcept
    {
        if (index < 0 || index >= m_count || m_tabs[index].pinned == pinned)
            return;

        m_tabs[index].pinned = pinned;
        m_stale = true;
        invalidate();
    }

    std::int32_t tab_bar::first_free() const noexcept
    {
        std::int32_t count = 0;

        while (count < m_count && m_tabs[count].pinned)
            ++count;

        return count;
    }

    bool tab_bar::has_slot(std::int32_t index) const noexcept
    {
        return (m_closable && !m_tabs[index].pinned) || m_tabs[index].modified;
    }

    const char* tab_bar::title(std::int32_t index) const noexcept
    {
        return index >= 0 && index < m_count ? m_tabs[index].title : "";
    }

    bool tab_bar::modified(std::int32_t index) const noexcept
    {
        return index >= 0 && index < m_count && m_tabs[index].modified;
    }

    void tab_bar::measure(void* dc) const noexcept
    {
        const dock_rect& box = area();

        if (!m_stale && m_measured_width == box.width)
        {
            if (!m_reveal)
                return;
        }
        else
        {
            std::int32_t natural[k_max_tabs]{};
            std::int32_t sum = 0;

            for (std::int32_t i = 0; i < m_count; ++i)
            {
                const std::int32_t w = tab_style::natural_width(draw::text_width(dc, m_tabs[i].title), m_tabs[i].icon != icon_kind::none,
                    has_slot(i));

                natural[i] = std::clamp(w, tab_style::least_width(), tab_style::most_width());
                sum += natural[i];
            }

            const std::int32_t add_w = m_show_add ? tab_style::add_width() : 0;
            const std::int32_t room = std::max(box.width - add_w, 0);
            std::int32_t total = sum;

            if (sum > room && sum > 0)
            {
                // The longest tabs give up width first: no tab is wider than a cap, found so that the tabs fill the room. The
                // row scrolls when even the least width for every tab is too much.
                const std::int32_t least = tab_style::least_width();
                std::int32_t low = least;
                std::int32_t high = tab_style::most_width();

                while (low < high)
                {
                    const std::int32_t cap = (low + high + 1) / 2;
                    std::int32_t used = 0;

                    for (std::int32_t i = 0; i < m_count; ++i)
                        used += std::max(std::min(natural[i], cap), least);

                    if (used <= room)
                        low = cap;
                    else
                        high = cap - 1;
                }

                total = 0;

                for (std::int32_t i = 0; i < m_count; ++i)
                {
                    m_widths[i] = std::max(std::min(natural[i], low), least);
                    total += m_widths[i];
                }
            }
            else
            {
                for (std::int32_t i = 0; i < m_count; ++i)
                    m_widths[i] = natural[i];
            }

            m_total = total;
            m_overflow = total > room;
            m_measured_width = box.width;
            m_stale = false;
        }

        const std::int32_t strip = strip_area().width;

        if (m_reveal)
        {
            m_reveal = false;

            if (m_overflow && m_selected >= 0)
            {
                std::int32_t left = 0;

                for (std::int32_t i = 0; i < m_selected; ++i)
                    left += m_widths[i];

                const std::int32_t right = left + m_widths[m_selected];

                if (left < m_scroll)
                    m_scroll = left;
                else if (right > m_scroll + strip)
                    m_scroll = right - strip;
            }
        }

        m_scroll = m_overflow ? std::clamp(m_scroll, 0, std::max(m_total - strip, 0)) : 0;
    }

    dock_rect tab_bar::strip_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t add_w = m_show_add ? tab_style::add_width() : 0;
        return {box.x, box.y, m_overflow ? std::max(box.width - add_w, 0) : box.width, box.height};
    }

    dock_rect tab_bar::tab_rect(std::int32_t index) const noexcept
    {
        const dock_rect& box = area();
        std::int32_t x = box.x - m_scroll;

        for (std::int32_t i = 0; i < index; ++i)
            x += m_widths[i];

        return {x, box.y + tab_style::top(), m_widths[index], box.height - tab_style::top()};
    }

    dock_rect tab_bar::close_rect(const dock_rect& tab) const noexcept
    {
        return tab_style::close_rect(tab);
    }

    dock_rect tab_bar::add_rect() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t w = tab_style::add_width();
        const std::int32_t x = m_overflow ? box.x + box.width - w : box.x + m_total;
        return {x, box.y + tab_style::top(), w, box.height - tab_style::top() - 1};
    }

    bool tab_bar::close_shown(std::int32_t index) const noexcept
    {
        return m_closable && !m_tabs[index].pinned && (index == m_selected || index == m_hot_tab);
    }

    tab_bar::place tab_bar::find(std::int32_t x, std::int32_t y) const noexcept
    {
        place result;

        if (m_show_add && inside(add_rect(), x, y))
        {
            result.add = true;
            return result;
        }

        if (!inside(strip_area(), x, y))
            return result;

        for (std::int32_t i = 0; i < m_count; ++i)
        {
            const dock_rect tab = tab_rect(i);

            if (!inside(tab, x, y))
                continue;

            result.tab = i;
            result.close = close_shown(i) && inside(close_rect(tab), x, y);
            return result;
        }

        return result;
    }

    control_size tab_bar::preferred_size(void* dc) const noexcept
    {
        measure(dc);
        const std::int32_t natural = m_count == 0 ? draw::px(k_empty_width) : m_total + (m_show_add ? tab_style::add_width() : 0);
        return {m_width > 0 ? draw::px(m_width) : natural, tab_style::height()};
    }

    void tab_bar::paint(void* dc) const noexcept
    {
        measure(dc);
        const theme_colours& colours = theme();
        const dock_rect& box = area();
        const std::uint32_t bar = tab_style::bar_colour();
        const std::uint32_t line = tab_style::line_colour();
        const bool live = enabled();

        tab_style::paint_strip(dc, box);
        draw::clip(dc, strip_area());

        // The tabs that are not chosen first, so the chosen one lies over the line below them.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::int32_t i = 0; i < m_count; ++i)
            {
                const bool chosen = i == m_selected;

                if ((pass == 1) != chosen)
                    continue;

                const dock_rect tab = tab_rect(i);

                if (tab.x + tab.width <= box.x || tab.x >= box.x + box.width)
                    continue;

                const bool hot = i == m_hot_tab && live;
                tab_style::tab_look look;
                look.title = m_tabs[i].title;
                look.icon = m_tabs[i].icon;
                look.chosen = chosen;
                look.hot = hot;
                look.lifted = m_dragging && i == m_press.tab;
                look.enabled = live;
                look.slot = has_slot(i);
                look.close_shown = close_shown(i);
                look.close_hot = hot && m_hot_close;
                look.modified = m_tabs[i].modified;
                tab_style::paint_tab(dc, tab, look);
            }
        }

        draw::unclip(dc);

        if (m_overflow)
        {
            // A line at an end of the row where more tabs lie out of sight.
            const dock_rect strip = strip_area();

            if (m_scroll > 0)
                draw::fill(dc, {strip.x, strip.y + tab_style::top(), 1, strip.height - tab_style::top() - 1}, line);

            if (m_scroll < m_total - strip.width)
                draw::fill(dc, {strip.x + strip.width - 1, strip.y + tab_style::top(), 1, strip.height - tab_style::top() - 1}, line);
        }

        if (m_show_add)
        {
            const dock_rect add = add_rect();
            const bool hot = m_hot_add && live;

            if (hot)
                draw::rounded(dc, {add.x + 2, add.y + 3, add.width - 4, add.height - 6}, 3, blend_colour(bar, colours.hover, 1.0f), blend_colour(bar, colours.text_dim, 0.4f));

            const std::int32_t size = draw::px(12);
            draw::icon(dc, icon_kind::plus, {add.x + (add.width - size) / 2, add.y + (add.height - size) / 2, size, size},
                !live ? blend_colour(colours.text_dim, colours.panel, 0.4f) : hot ? colours.text : colours.text_dim);
        }
    }

    void tab_bar::raise(tab_event what, std::int32_t index) noexcept
    {
        if (m_handler != nullptr)
            m_handler(m_user, *this, what, index);
    }

    void tab_bar::move_tab(std::int32_t from, std::int32_t to) noexcept
    {
        if (from == to || from < 0 || to < 0 || from >= m_count || to >= m_count)
            return;

        const tab_item item = m_tabs[from];
        const std::int32_t width = m_widths[from];
        const std::int32_t step = to > from ? 1 : -1;

        for (std::int32_t i = from; i != to; i += step)
        {
            m_tabs[i] = m_tabs[i + step];
            m_widths[i] = m_widths[i + step];
        }

        m_tabs[to] = item;
        m_widths[to] = width;

        if (m_selected == from)
            m_selected = to;
        else if (step > 0 && m_selected > from && m_selected <= to)
            --m_selected;
        else if (step < 0 && m_selected >= to && m_selected < from)
            ++m_selected;

        invalidate();
    }

    void tab_bar::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        m_press_x = x;
        m_dragging = false;
        m_press = find(x, y);

        if (m_press.tab >= 0 && !m_press.close && m_press.tab != m_selected)
        {
            m_selected = m_press.tab;
            m_reveal = true;
            invalidate();
            raise(tab_event::select, m_selected);
        }
    }

    bool tab_bar::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (m_press.tab < 0 || m_press.close || !m_reorderable || !enabled() || m_tabs[m_press.tab].pinned)
            return false;

        if (!m_dragging)
        {
            const std::int32_t moved = x > m_press_x ? x - m_press_x : m_press_x - x;

            if (moved < draw::px(k_drag_start))
                return false;

            m_dragging = true;
        }

        std::int32_t target = m_count - 1;

        for (std::int32_t i = 0; i < m_count; ++i)
        {
            const dock_rect tab = tab_rect(i);

            if (x < tab.x + tab.width)
            {
                target = i;
                break;
            }
        }

        target = std::max(target, first_free());

        if (target != m_press.tab)
        {
            const std::int32_t from = m_press.tab;
            move_tab(from, target);
            m_press.tab = target;
            raise(tab_event::move, target);
        }

        return true;
    }

    bool tab_bar::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        const place now = find(x, y);
        const bool changed = now.tab != m_hot_tab || now.close != m_hot_close || now.add != m_hot_add;
        m_hot_tab = now.tab;
        m_hot_close = now.close;
        m_hot_add = now.add;
        return changed;
    }

    void tab_bar::on_release() noexcept
    {
        m_dragging = false;
        invalidate();
    }

    void tab_bar::on_leave() noexcept
    {
        m_hot_tab = -1;
        m_hot_close = false;
        m_hot_add = false;
        invalidate();
    }

    void tab_bar::on_click() noexcept
    {
        const place now = find(m_x, m_y);
        const place pressed = m_press;
        m_press = {};

        if (pressed.add && now.add)
            raise(tab_event::add, m_count);
        else if (pressed.close && now.close && now.tab == pressed.tab)
            raise(tab_event::close, pressed.tab);
    }

    bool tab_bar::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;

        if (!m_overflow)
            return false;

        const std::int32_t before = m_scroll;
        const std::int32_t limit = std::max(m_total - strip_area().width, 0);
        m_scroll = std::clamp(m_scroll - notches * draw::px(k_wheel_step), 0, limit);

        if (m_scroll == before)
            return false;

        invalidate();
        return true;
    }

    bool tab_bar::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_count == 0 || !enabled())
            return false;

        std::int32_t target = -1;

        switch (key)
        {
        case k_key_left:
            target = std::max(m_selected - 1, 0);
            break;
        case k_key_right:
            target = std::min(m_selected + 1, m_count - 1);
            break;
        case k_key_home:
            target = 0;
            break;
        case k_key_end:
            target = m_count - 1;
            break;
        case k_key_delete:
            if (!m_closable || m_selected < 0 || m_tabs[m_selected].pinned)
                return false;

            raise(tab_event::close, m_selected);
            return true;
        case k_key_w:
            if (!ctrl || !m_closable || m_selected < 0 || m_tabs[m_selected].pinned)
                return false;

            raise(tab_event::close, m_selected);
            return true;
        default:
            return false;
        }

        if (ctrl && shift && (key == k_key_left || key == k_key_right))
        {
            if (!m_reorderable || m_selected < 0 || m_tabs[m_selected].pinned)
                return true;

            const std::int32_t from = m_selected;
            target = std::max(target, first_free());

            if (target != from)
            {
                move_tab(from, target);
                m_reveal = true;
                raise(tab_event::move, target);
            }

            return true;
        }

        if (target >= 0 && target != m_selected)
        {
            m_selected = target;
            m_reveal = true;
            invalidate();
            raise(tab_event::select, m_selected);
        }

        return true;
    }

    void tab_bar::on_focus(bool gained) noexcept
    {
        (void)gained;
        invalidate();
    }
}
