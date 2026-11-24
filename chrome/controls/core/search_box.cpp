#include <chrome/controls/core/search_box.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_input_height = 30;
        constexpr std::int32_t k_compact_height = 24;
        constexpr std::int32_t k_chip_height = 22;
        constexpr std::int32_t k_chip_pad = 10;
        constexpr std::int32_t k_chip_gap = 6;
        constexpr std::int32_t k_rows_gap = 6;
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_pill_max = 96;
        constexpr std::int32_t k_pill_cross = 16;
        constexpr std::int32_t k_clear_width = 26;
        // The box has to keep this much for the text for another pill to be let in.
        constexpr std::int32_t k_room_for_pill = 170;

        constexpr std::uint32_t k_key_return = 0x0D;
        constexpr std::uint32_t k_key_escape = 0x1B;
        constexpr std::uint32_t k_key_tab = 0x09;
        constexpr std::uint32_t k_key_back = 0x08;

        [[nodiscard]] char fold(char c) noexcept
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
        }
    }

    search_box::search_box() noexcept
    {
        m_field.set_frameless(true).set_max_length(120).set_on_action(&search_box::field_acted, this);
        const bool added = m_inner.add(m_field);
        (void)added;
    }

    search_box& search_box::set_placeholder(const char* text) noexcept
    {
        m_placeholder = text;
        m_field.set_placeholder(text);
        return *this;
    }

    search_box& search_box::set_text(const char* utf8) noexcept
    {
        m_field.set_text(utf8);
        invalidate();
        return *this;
    }

    std::int32_t search_box::get_text(char* out, std::int32_t capacity) const noexcept
    {
        return m_field.get_text(out, capacity);
    }

    void search_box::clear() noexcept
    {
        const bool had = m_pill_count > 0 || !text_empty();
        m_pill_count = 0;
        m_field.set_text("");
        invalidate();

        if (had)
            notify_change();
    }

    std::int32_t search_box::add_chip(const char* label) noexcept
    {
        if (m_chip_count >= k_max_chips)
            return -1;

        m_chips[m_chip_count].label = label;
        invalidate();
        return static_cast<std::int32_t>(m_chip_count++);
    }

    search_box& search_box::set_chip_active(std::uint32_t index, bool active) noexcept
    {
        if (index < m_chip_count)
        {
            m_chips[index].active = active;
            invalidate();
        }

        return *this;
    }

    search_box& search_box::set_chip_count(std::uint32_t index, std::int32_t count) noexcept
    {
        if (index < m_chip_count)
        {
            m_chips[index].count = count;
            invalidate();
        }

        return *this;
    }

    search_box& search_box::set_chip_colour(std::uint32_t index, std::uint32_t colour) noexcept
    {
        if (index < m_chip_count)
        {
            m_chips[index].colour = colour;
            invalidate();
        }

        return *this;
    }

    bool search_box::any_chip() const noexcept
    {
        for (std::uint32_t i = 0; i < m_chip_count; ++i)
        {
            if (m_chips[i].active)
                return true;
        }

        return false;
    }

    bool search_box::has_filter() const noexcept
    {
        return m_pill_count > 0 || !text_empty() || any_chip();
    }

    bool search_box::contains_fold(const char* text, const char* word) noexcept
    {
        if (word == nullptr || *word == '\0')
            return true;

        if (text == nullptr)
            return false;

        for (; *text != '\0'; ++text)
        {
            const char* t = text;
            const char* w = word;

            while (*t != '\0' && *w != '\0' && fold(*t) == fold(*w))
            {
                ++t;
                ++w;
            }

            if (*w == '\0')
                return true;
        }

        return false;
    }

    bool search_box::matches(const char* utf8) const noexcept
    {
        for (std::uint32_t i = 0; i < m_pill_count; ++i)
        {
            if (!contains_fold(utf8, m_pills_text[i]))
                return false;
        }

        char typed[256];
        (void)m_field.get_text(typed, sizeof(typed));
        return contains_fold(utf8, typed);
    }

    void search_box::notify_change() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    bool search_box::field_acted(void* user, text_field& source, text_action action) noexcept
    {
        (void)source;
        auto& self = *static_cast<search_box*>(user);

        if (action == text_action::changed)
            self.notify_change();

        return true;
    }

    bool search_box::commit_pill() noexcept
    {
        char typed[k_pill_length];
        (void)m_field.get_text(typed, sizeof(typed));

        // Without the spaces round it.
        char* begin = typed;

        while (*begin == ' ')
            ++begin;

        std::size_t length = std::strlen(begin);

        while (length > 0 && begin[length - 1] == ' ')
            --length;

        if (length == 0 || m_pill_count >= k_max_pills || m_room < draw::px(k_room_for_pill))
            return false;

        begin[length] = '\0';

        // The same word twice is one.
        for (std::uint32_t i = 0; i < m_pill_count; ++i)
        {
            if (contains_fold(m_pills_text[i], begin) && contains_fold(begin, m_pills_text[i]))
            {
                m_field.set_text("");
                notify_change();
                return true;
            }
        }

        std::memcpy(m_pills_text[m_pill_count], begin, length + 1);
        ++m_pill_count;
        m_field.set_text("");
        notify_change();
        return true;
    }

    void search_box::remove_pill(std::uint32_t index) noexcept
    {
        if (index >= m_pill_count)
            return;

        for (std::uint32_t i = index + 1; i < m_pill_count; ++i)
            std::memcpy(m_pills_text[i - 1], m_pills_text[i], k_pill_length);

        --m_pill_count;
        notify_change();
    }

    void search_box::toggle_chip(std::uint32_t index) noexcept
    {
        if (index >= m_chip_count)
            return;

        m_chips[index].active = !m_chips[index].active;
        notify_change();
    }

    std::int32_t search_box::chip_width(void* dc, std::uint32_t index) const noexcept
    {
        const chip& item = m_chips[index];
        std::int32_t width = 2 * draw::px(k_chip_pad) + draw::text_width(dc, item.label != nullptr ? item.label : "");

        if (item.count >= 0)
        {
            char number[16];
            std::snprintf(number, sizeof(number), "%d", item.count);
            width += draw::px(6) + draw::text_width(dc, number);
        }

        return width;
    }

    std::int32_t search_box::lay_out_chips(void* dc, std::int32_t width, std::int32_t x, std::int32_t y, dock_rect* out) const noexcept
    {
        if (m_chip_count == 0)
            return 0;

        const std::int32_t height = draw::px(k_chip_height);
        const std::int32_t gap = draw::px(k_chip_gap);
        std::int32_t cx = x;
        std::int32_t cy = y;
        std::int32_t rows = 1;

        for (std::uint32_t i = 0; i < m_chip_count; ++i)
        {
            const std::int32_t w = chip_width(dc, i);

            if (cx > x && cx + w > x + width)
            {
                cx = x;
                cy += height + gap;
                ++rows;
            }

            if (out != nullptr)
                out[i] = {cx, cy, w, height};

            cx += w + gap;
        }

        return rows;
    }

    std::int32_t search_box::input_height() const noexcept
    {
        return draw::px(m_compact ? k_compact_height : k_input_height);
    }

    control_size search_box::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size search_box::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(160));
        std::int32_t height = input_height();
        const std::int32_t rows = lay_out_chips(dc, width, 0, 0, nullptr);

        if (rows > 0)
            height += draw::px(k_rows_gap) + rows * draw::px(k_chip_height) + (rows - 1) * draw::px(k_chip_gap);

        return {width, height};
    }

    panel_mouse search_box::mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept
    {
        panel_mouse mouse;
        mouse.event = event;
        mouse.x = x;
        mouse.y = y;
        mouse.ctrl = ctrl_down();
        mouse.shift = shift_down();
        return mouse;
    }

    void search_box::place() const noexcept
    {
        m_field.set_area(m_field_rect);
        m_field.set_enabled(enabled());
    }

    search_box::target search_box::target_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const auto inside = [x, y](const dock_rect& r) noexcept {
            return r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
        };

        for (std::uint32_t i = 0; i < m_chip_count; ++i)
        {
            if (inside(m_chip_rects[i]))
                return {target_kind::chip, static_cast<std::int32_t>(i)};
        }

        if (inside(m_clear_rect))
            return {target_kind::clear, 0};

        for (std::uint32_t i = 0; i < m_pill_count; ++i)
        {
            const dock_rect& pill = m_pill_rects[i];
            const dock_rect cross{pill.x + pill.width - draw::px(k_pill_cross), pill.y, draw::px(k_pill_cross), pill.height};

            if (inside(cross))
                return {target_kind::pill_cross, static_cast<std::int32_t>(i)};
        }

        if (inside(m_row_rect))
            return {target_kind::field, 0};

        return {};
    }

    void search_box::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        m_drag_field = false;
        const target hit = target_at(x, y);

        switch (hit.kind)
        {
        case target_kind::chip:
            toggle_chip(static_cast<std::uint32_t>(hit.index));
            break;

        case target_kind::clear:
            clear();
            m_inner.set_focus(&m_field);
            break;

        case target_kind::pill_cross:
            remove_pill(static_cast<std::uint32_t>(hit.index));
            m_inner.set_focus(&m_field);
            break;

        case target_kind::field:
            if (x >= m_field_rect.x && x < m_field_rect.x + m_field_rect.width)
            {
                (void)m_inner.on_mouse(mouse_at(mouse_event::down, x, y));
                m_drag_field = true;
            }
            else
            {
                // The magnifier and the room round the text: the caret goes to the end.
                m_inner.set_focus(&m_field);
                m_field.select(m_field.length(), m_field.length());
            }

            break;

        default:
            break;
        }

        invalidate();
    }

    void search_box::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        const target hit = target_at(x, y);

        if (hit.kind == target_kind::field && x >= m_field_rect.x && x < m_field_rect.x + m_field_rect.width)
        {
            m_drag_field = true;
            (void)m_inner.on_mouse(mouse_at(mouse_event::double_click, x, y));
            invalidate();
            return;
        }

        on_press(x, y);
    }

    bool search_box::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;

        if (!m_drag_field)
            return false;

        (void)m_inner.on_mouse(mouse_at(mouse_event::move, x, y));
        return true;
    }

    bool search_box::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        place();
        m_x = x;
        m_y = y;
        const target hit = target_at(x, y);
        bool changed = m_inner.on_mouse(mouse_at(mouse_event::move, x, y));

        if (!(hit == m_hover))
        {
            m_hover = hit;
            changed = true;
        }

        return changed;
    }

    void search_box::on_release() noexcept
    {
        if (m_drag_field)
            (void)m_inner.on_mouse(mouse_at(mouse_event::up, m_x, m_y));

        m_drag_field = false;
        invalidate();
    }

    void search_box::on_leave() noexcept
    {
        (void)m_inner.on_mouse(mouse_at(mouse_event::leave, m_x, m_y));
        m_hover = {};
        invalidate();
    }

    bool search_box::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        // Tab goes on to the next control.
        if (key == k_key_tab)
        {
            m_inner.blur();
            invalidate();
            return false;
        }

        if (ctrl && !alt && !shift && key >= '1' && key <= '9' && key - '1' < m_chip_count)
        {
            toggle_chip(key - '1');
            return true;
        }

        if (!ctrl && !alt)
        {
            if (key == k_key_return)
            {
                const bool made = m_pills && commit_pill();
                (void)made;

                if (m_submit != nullptr)
                    m_submit(m_submit_user, *this);

                invalidate();
                return true;
            }

            if (key == k_key_back && text_empty() && m_pill_count > 0)
            {
                remove_pill(m_pill_count - 1);
                return true;
            }

            if (key == k_key_escape)
            {
                if (!text_empty())
                {
                    m_field.set_text("");
                    notify_change();
                }
                else
                {
                    request_blur();
                }

                return true;
            }
        }

        const bool used = m_inner.on_key(key, ctrl, shift, alt);
        invalidate();
        return used;
    }

    void search_box::on_char(std::uint32_t code) noexcept
    {
        m_inner.on_char(code);
        invalidate();
    }

    void search_box::on_focus(bool gained) noexcept
    {
        if (gained && m_inner.focused() == nullptr)
            m_inner.set_focus(&m_field);
        else if (!gained)
            m_inner.blur();

        invalidate();
    }

    bool search_box::animate(std::uint64_t now_ms) noexcept
    {
        place();
        const bool more = m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return more;
    }

    void search_box::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool live = enabled();
        const bool lit = focused();
        const dock_rect& whole = area();

        std::uint32_t face = blend_colour(colours.panel, black, 0.28f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.text_dim, colours.panel, hot() ? 0.25f : 0.55f);
        std::uint32_t ink = colours.text;
        std::uint32_t dim = colours.text_dim;

        if (!live)
        {
            face = blend_colour(colours.panel, colours.title, 0.4f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            dim = ink;
        }

        const dock_rect row{whole.x, whole.y, whole.width, input_height()};
        m_row_rect = row;
        draw::rounded(dc, row, k_radius, face, edge);

        // The magnifier: a ring and a short handle.
        const std::int32_t cy = row.y + row.height / 2;
        const std::int32_t ix = row.x + draw::px(10);
        const float unit = static_cast<float>(draw::px(1));
        draw::outline(dc, {ix, cy - draw::px(6), draw::px(11), draw::px(11)}, 6, dim, 1.5f);
        const draw::point handle[2] = {{static_cast<float>(ix) + 9.2f * unit, static_cast<float>(cy) + 3.2f * unit},
            {static_cast<float>(ix) + 13.0f * unit, static_cast<float>(cy) + 7.0f * unit}};
        draw::polyline(dc, handle, 2, dim, 1.8f * unit);

        // The pills after it, each with a cross that takes it away.
        std::int32_t x = ix + draw::px(20);
        const std::int32_t clear_room = draw::px(k_clear_width);
        const std::int32_t limit = row.x + row.width - clear_room;
        const std::int32_t pill_margin = draw::px(m_compact ? 3 : 5);
        const std::int32_t pill_height = row.height - 2 * pill_margin;

        for (std::uint32_t i = 0; i < m_pill_count; ++i)
        {
            const std::int32_t text_w = std::min(draw::text_width(dc, m_pills_text[i]), draw::px(k_pill_max));
            const std::int32_t w = text_w + draw::px(10) + draw::px(k_pill_cross);
            const dock_rect pill{x, row.y + pill_margin, w, pill_height};
            m_pill_rects[i] = pill;
            const bool over_cross = live && m_hover.kind == target_kind::pill_cross && m_hover.index == static_cast<std::int32_t>(i);
            const std::uint32_t fill = blend_colour(colours.accent, colours.panel, live ? 0.55f : 0.2f);
            draw::rounded(dc, pill, pill_height / 2, fill, blend_colour(colours.accent, black, 0.3f));
            draw::text(dc, {pill.x + draw::px(8), pill.y, text_w, pill.height}, m_pills_text[i], live ? white : dim, draw::text_ellipsis);
            const dock_rect cross{pill.x + pill.width - draw::px(k_pill_cross), pill.y, draw::px(k_pill_cross), pill.height};

            if (over_cross)
                draw::ellipse(dc, {cross.x + draw::px(2), cross.y + (cross.height - draw::px(12)) / 2, draw::px(12), draw::px(12)}, blend_colour(fill, white, 0.25f), blend_colour(fill, white, 0.25f));

            draw::icon(dc, icon_kind::cross, {cross.x + draw::px(4), cross.y + (cross.height - draw::px(8)) / 2, draw::px(8), draw::px(8)}, white);
            x += w + draw::px(4);
        }

        for (std::uint32_t i = m_pill_count; i < k_max_pills; ++i)
            m_pill_rects[i] = {};

        // What is left is the text, and the cross at the right that clears everything.
        m_field_rect = {x, row.y + draw::px(1), std::max(limit - x, draw::px(20)), row.height - 2 * draw::px(1)};
        m_room = m_field_rect.width;
        place();
        const bool clearable = live && (m_pill_count > 0 || !text_empty());

        if (clearable)
        {
            m_clear_rect = {limit, row.y, clear_room, row.height};
            const bool over = m_hover.kind == target_kind::clear;

            if (over)
                draw::ellipse(dc, {limit + (clear_room - draw::px(16)) / 2, cy - draw::px(8), draw::px(16), draw::px(16)}, blend_colour(face, white, 0.14f), blend_colour(face, white, 0.14f));

            draw::icon(dc, icon_kind::cross, {limit + (clear_room - draw::px(8)) / 2, cy - draw::px(4), draw::px(8), draw::px(8)}, over ? ink : dim);
        }
        else
        {
            m_clear_rect = {};
        }

        m_inner.paint(dc);

        // The chips under the box: filled and in colour when on.
        for (std::uint32_t i = m_chip_count; i < k_max_chips; ++i)
            m_chip_rects[i] = {};

        const std::int32_t top = row.y + row.height + draw::px(k_rows_gap);
        (void)lay_out_chips(dc, whole.width, whole.x, top, m_chip_rects);

        for (std::uint32_t i = 0; i < m_chip_count; ++i)
        {
            const chip& item = m_chips[i];
            const dock_rect& box = m_chip_rects[i];
            const bool over = live && m_hover.kind == target_kind::chip && m_hover.index == static_cast<std::int32_t>(i);
            const std::uint32_t base = item.colour != 0 ? item.colour : colours.accent;
            std::uint32_t chip_face = blend_colour(colours.panel, black, over ? 0.12f : 0.22f);
            std::uint32_t chip_edge = blend_colour(colours.text_dim, colours.panel, over ? 0.3f : 0.55f);
            std::uint32_t chip_ink = colours.text_dim;
            std::uint32_t count_ink = blend_colour(colours.text_dim, colours.panel, 0.2f);

            if (item.active)
            {
                chip_face = over ? blend_colour(base, white, 0.12f) : base;
                chip_edge = blend_colour(base, black, 0.35f);
                chip_ink = white;
                count_ink = blend_colour(white, base, 0.3f);
            }
            else if (over)
            {
                chip_ink = colours.text;
            }

            if (!live)
            {
                chip_face = blend_colour(colours.panel, colours.title, 0.4f);
                chip_edge = blend_colour(colours.panel, black, 0.2f);
                chip_ink = dim;
                count_ink = dim;
            }

            draw::rounded(dc, box, box.height / 2, chip_face, chip_edge);
            const std::int32_t pad = draw::px(k_chip_pad);
            const std::int32_t label_w = draw::text_width(dc, item.label != nullptr ? item.label : "");
            draw::text(dc, {box.x + pad, box.y, label_w + 2, box.height}, item.label != nullptr ? item.label : "", chip_ink);

            if (item.count >= 0)
            {
                char number[16];
                std::snprintf(number, sizeof(number), "%d", item.count);
                draw::text(dc, {box.x + pad + label_w + draw::px(6), box.y, box.width - pad - label_w - draw::px(6), box.height}, number, count_ink);
            }
        }
    }
}
