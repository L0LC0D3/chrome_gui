#include <chrome/controls/data/flame_graph.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace chrome
{
    namespace
    {
        // Sizes at 96 dpi.
        constexpr std::int32_t k_inset = 2;
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_text_pad = 4;
        constexpr std::int32_t k_min_label = 28;
        constexpr std::int32_t k_hatch_step = 6;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_end = 0x23;

        void copy_text(char* out, std::size_t capacity, const char* text) noexcept
        {
            std::snprintf(out, capacity, "%s", text != nullptr ? text : "");
        }

        [[nodiscard]] bool is_light(std::uint32_t colour) noexcept
        {
            const float r = static_cast<float>((colour >> 16) & 0xFF);
            const float g = static_cast<float>((colour >> 8) & 0xFF);
            const float b = static_cast<float>(colour & 0xFF);
            return 0.299f * r + 0.587f * g + 0.114f * b > 150.0f;
        }
    }

    flame_graph& flame_graph::set_unit(const char* unit) noexcept
    {
        copy_text(m_unit, sizeof(m_unit), unit);
        return *this;
    }

    flame_graph& flame_graph::set_help(const char* text) noexcept
    {
        m_help = text;

        if (m_hover < 0)
            control::set_tooltip(m_help);

        return *this;
    }

    void flame_graph::set_boxes(const flame_box* boxes, std::uint32_t count, float whole) noexcept
    {
        std::uint32_t n = 0;
        std::uint32_t rows = 1;
        float end = 0.0f;

        for (std::uint32_t i = 0; boxes != nullptr && i < count && n < k_max_boxes; ++i)
        {
            const flame_box& in = boxes[i];

            if (in.depth >= k_max_rows)
                continue;

            item& out = m_items[n++];
            out.id = in.id;
            out.depth = in.depth;
            out.start = std::max(in.start, 0.0f);
            out.length = std::max(in.length, 0.0f);
            copy_text(out.label, sizeof(out.label), in.label);
            copy_text(out.detail, sizeof(out.detail), in.detail);
            out.colour = in.colour;
            out.idle = in.idle;
            rows = std::max(rows, in.depth + 1);
            end = std::max(end, out.start + out.length);
        }

        m_count = n;
        m_rows = rows;
        m_whole = whole > 0.0f ? whole : end;
        m_hover = -1;
        invalidate();
    }

    std::int32_t flame_graph::height_for_rows() const noexcept
    {
        return static_cast<std::int32_t>(std::max(m_rows, m_min_rows)) * draw::px(m_row_height) + 2 * draw::px(k_inset) + draw::px(1);
    }

    void flame_graph::set_selected(std::uint32_t id) noexcept
    {
        m_has_selection = true;
        m_selected = id;
        invalidate();
    }

    void flame_graph::clear_selection() noexcept
    {
        m_has_selection = false;
        invalidate();
    }

    std::int32_t flame_graph::index_of(std::uint32_t id) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i].id == id)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    control_size flame_graph::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size flame_graph::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {max_width, height_for_rows()};
    }

    dock_rect flame_graph::well_area() const noexcept
    {
        const dock_rect& box = area();
        const std::int32_t inset = draw::px(k_inset);
        return {box.x + inset, box.y + inset, std::max(box.width - 2 * inset, 1), std::max(box.height - 2 * inset, 1)};
    }

    dock_rect flame_graph::box_area(const item& it) const noexcept
    {
        const dock_rect well = well_area();
        const std::int32_t row = draw::px(m_row_height);

        if (m_whole <= 0.0f)
            return {well.x, well.y + static_cast<std::int32_t>(it.depth) * row, 0, row - 1};

        const auto edge = [&](float at) noexcept
        { return static_cast<std::int32_t>(std::lround(std::min(at / m_whole, 1.0f) * static_cast<float>(well.width))); };

        const std::int32_t from = edge(it.start);
        const std::int32_t to = edge(it.start + it.length);
        return {well.x + from, well.y + static_cast<std::int32_t>(it.depth) * row, std::max(to - from, 0), row - 1};
    }

    std::int32_t flame_graph::box_at(std::int32_t x, std::int32_t y) const noexcept
    {
        // The deepest box is drawn last, and a box never overlaps one of its own row, so the last hit wins.
        std::int32_t found = -1;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const dock_rect b = box_area(m_items[i]);

            if (b.width > 0 && x >= b.x && x < b.x + b.width && y >= b.y && y < b.y + b.height)
                found = static_cast<std::int32_t>(i);
        }

        return found;
    }

    dock_rect flame_graph::tooltip_area() const noexcept
    {
        if (m_hover < 0 || static_cast<std::uint32_t>(m_hover) >= m_count)
            return area();

        return box_area(m_items[m_hover]);
    }

    void flame_graph::show_tooltip(std::int32_t index) noexcept
    {
        if (index < 0 || static_cast<std::uint32_t>(index) >= m_count)
        {
            control::set_tooltip(m_help);
            return;
        }

        const item& it = m_items[index];
        char* text = m_tip[m_tip_next];
        m_tip_next ^= 1u;
        char share[24]{};

        if (m_whole > 0.0f)
            std::snprintf(share, sizeof(share), ", %.1f%%", static_cast<double>(it.length / m_whole * 100.0f));

        std::snprintf(text, sizeof(m_tip[0]), "%s%s\n%.2f %s%s%s%s", it.label[0] != '\0' ? it.label : "(unnamed)", it.idle ? " (idle)" : "",
            static_cast<double>(it.length), m_unit, share, it.detail[0] != '\0' ? "\n" : "", it.detail);
        control::set_tooltip(text);
    }

    void flame_graph::select_index(std::int32_t index, bool by_user) noexcept
    {
        if (index < 0 || static_cast<std::uint32_t>(index) >= m_count)
            return;

        const bool changed = !m_has_selection || m_selected != m_items[index].id;
        m_has_selection = true;
        m_selected = m_items[index].id;
        invalidate();

        if (changed && by_user && m_on_select != nullptr)
            m_on_select(m_on_select_user, *this, m_selected);
    }

    void flame_graph::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        select_index(box_at(x, y), true);
    }

    void flame_graph::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t hit = box_at(x, y);
        select_index(hit, true);

        if (hit >= 0 && m_on_activate != nullptr)
            m_on_activate(m_on_activate_user, *this, m_items[hit].id);
    }

    bool flame_graph::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t hit = box_at(x, y);

        if (hit == m_hover)
            return false;

        m_hover = hit;
        show_tooltip(hit);
        return true;
    }

    void flame_graph::on_leave() noexcept
    {
        m_hover = -1;
        control::set_tooltip(m_help);
        invalidate();
    }

    bool flame_graph::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (ctrl || shift || alt || m_count == 0)
            return false;

        const std::int32_t current = m_has_selection ? index_of(m_selected) : -1;
        const auto count = static_cast<std::int32_t>(m_count);

        if (key == k_key_enter || key == k_key_space)
        {
            if (current >= 0 && m_on_activate != nullptr)
                m_on_activate(m_on_activate_user, *this, m_selected);

            return current >= 0;
        }

        if (current < 0)
        {
            if (key != k_key_left && key != k_key_right && key != k_key_up && key != k_key_down && key != k_key_home && key != k_key_end)
                return false;

            select_index(key == k_key_end ? count - 1 : 0, true);
            return true;
        }

        const std::uint32_t depth = m_items[current].depth;
        std::int32_t target = -1;

        switch (key)
        {
        case k_key_left:
            for (std::int32_t i = current - 1; i >= 0 && target < 0; --i)
                target = m_items[i].depth == depth ? i : -1;

            break;
        case k_key_right:
            for (std::int32_t i = current + 1; i < count && target < 0; ++i)
                target = m_items[i].depth == depth ? i : -1;

            break;
        case k_key_up:
            for (std::int32_t i = current - 1; i >= 0 && target < 0; --i)
                target = depth > 0 && m_items[i].depth == depth - 1 ? i : -1;

            break;
        case k_key_down:
            target = current + 1 < count && m_items[current + 1].depth == depth + 1 ? current + 1 : -1;
            break;
        case k_key_home:
            target = 0;
            break;
        case k_key_end:
            target = count - 1;
            break;
        default:
            return false;
        }

        if (target >= 0)
            select_index(target, true);

        return true;
    }

    void flame_graph::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const bool on = enabled();
        const std::uint32_t well = blend_colour(colours.panel, 0x000000, 0.28f);
        draw::rounded(dc, area(), k_radius, well, blend_colour(well, colours.text_dim, 0.25f));
        draw::clip(dc, well_area());

        for (std::uint32_t k = 0; k < m_count; ++k)
        {
            const item& it = m_items[k];
            const dock_rect cell = box_area(it);

            if (cell.width <= 0)
                continue;

            const bool picked = m_has_selection && it.id == m_selected;
            const bool over = static_cast<std::int32_t>(k) == m_hover && on;
            const dock_rect shown{cell.x, cell.y, cell.width > 2 ? cell.width - 1 : cell.width, cell.height};
            std::uint32_t face = it.idle ? blend_colour(colours.panel, colours.text_dim, 0.18f) : it.colour;

            if (over)
                face = blend_colour(face, 0xFFFFFF, 0.18f);

            if (!on)
                face = blend_colour(face, colours.panel, 0.55f);

            draw::fill(dc, shown, face);

            if (it.idle && shown.width > 3)
            {
                const std::uint32_t line = blend_colour(face, colours.text_dim, 0.35f);
                const std::int32_t step = draw::px(k_hatch_step);
                draw::clip(dc, shown);

                for (std::int32_t x = shown.x - shown.height; x < shown.x + shown.width; x += step)
                {
                    const draw::point points[2] = {{static_cast<float>(x), static_cast<float>(shown.y + shown.height)},
                        {static_cast<float>(x + shown.height), static_cast<float>(shown.y)}};
                    draw::polyline(dc, points, 2, line, 1.0f);
                }

                draw::unclip(dc);
            }

            if (shown.width >= draw::px(k_min_label))
            {
                const std::int32_t room = shown.width - 2 * draw::px(k_text_pad);
                const std::uint32_t ink = it.idle ? colours.text_dim : (is_light(face) ? 0x101418u : 0xFFFFFFu);
                char text[96]{};
                std::snprintf(text, sizeof(text), "%s  %.2f", it.label, static_cast<double>(it.length));

                // The time goes after the name when both fit, otherwise the name alone.
                if (draw::text_width(dc, text) > room)
                    std::snprintf(text, sizeof(text), "%s", it.label);

                draw::text(dc, {shown.x + draw::px(k_text_pad), shown.y, std::max(room, 1), shown.height}, text, ink, draw::text_ellipsis);
            }

            if (picked)
            {
                const std::int32_t ring = draw::px(1);
                draw::outline(dc, {shown.x + ring, shown.y + ring, std::max(shown.width - 2 * ring, 1), std::max(shown.height - 2 * ring, 1)}, 2,
                    is_light(face) ? 0x101418u : 0xFFFFFFu, 2.0f);
            }
        }

        draw::unclip(dc);
    }
}
