#include <chrome/controls/data/proportion_bar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        // Sizes at 96 dpi.
        constexpr std::int32_t k_margin_y = 3;
        constexpr std::int32_t k_inset = 2;
        constexpr std::int32_t k_caption_gap = 8;
        constexpr std::int32_t k_radius = 4;
        constexpr std::int32_t k_text_pad = 4;
        constexpr std::int32_t k_min_label = 30;
        constexpr std::int32_t k_hatch_step = 6;
        constexpr std::int32_t k_overflow_height = 3;

        // Parts under this share of the whole are drawn together when they sit side by side.
        constexpr float k_small_share = 0.005f;
        constexpr float k_ease_ms = 80.0f;
        constexpr std::uint32_t k_overflow_colour = 0xD04A4A;

        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;

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

    std::uint32_t proportion_bar::segment_colour(std::uint32_t seed) noexcept
    {
        const float hue = static_cast<float>(((seed * 2654435761u) >> 8) & 0xFFFFFFu) / 16777216.0f * 6.0f;
        constexpr float saturation = 0.48f;
        constexpr float value = 0.80f;
        const float chroma = value * saturation;
        const float second = chroma * (1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f));
        const float floor = value - chroma;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        switch (static_cast<std::int32_t>(hue))
        {
        case 0: r = chroma; g = second; break;
        case 1: r = second; g = chroma; break;
        case 2: g = chroma; b = second; break;
        case 3: g = second; b = chroma; break;
        case 4: r = second; b = chroma; break;
        default: r = chroma; b = second; break;
        }

        const auto channel = [&](float c) noexcept { return static_cast<std::uint32_t>((c + floor) * 255.0f + 0.5f); };
        return (channel(r) << 16) | (channel(g) << 8) | channel(b);
    }

    proportion_bar& proportion_bar::set_caption(const char* name, const char* value) noexcept
    {
        copy_text(m_caption_name, sizeof(m_caption_name), name);
        copy_text(m_caption_value, sizeof(m_caption_value), value);
        invalidate();
        return *this;
    }

    proportion_bar& proportion_bar::set_unit(const char* unit) noexcept
    {
        copy_text(m_unit, sizeof(m_unit), unit);
        return *this;
    }

    proportion_bar& proportion_bar::set_help(const char* text) noexcept
    {
        m_help = text;

        if (m_hover < 0)
            control::set_tooltip(m_help);

        return *this;
    }

    void proportion_bar::set_segments(const bar_segment* segments, std::uint32_t count) noexcept
    {
        item next[k_max_segments];
        const std::uint32_t n = segments != nullptr ? std::min(count, k_max_segments) : 0;
        float sum = 0.0f;
        bool moving = false;

        for (std::uint32_t i = 0; i < n; ++i)
        {
            const bar_segment& in = segments[i];
            item& out = next[i];
            out.id = in.id;
            copy_text(out.label, sizeof(out.label), in.label);
            copy_text(out.detail, sizeof(out.detail), in.detail);
            out.value = std::max(in.value, 0.0f);
            out.colour = in.colour == k_auto_colour ? segment_colour(in.id) : in.colour;
            out.idle = in.idle;

            const std::int32_t old = index_of(in.id);
            out.shown = old >= 0 ? m_items[old].shown : out.value;
            moving = moving || out.shown != out.value;
            sum += out.value;
        }

        for (std::uint32_t i = 0; i < n; ++i)
            m_items[i] = next[i];

        m_count = n;
        m_sum = sum;
        m_moving = moving;
        m_slot_count = 0;
        invalidate();
    }

    void proportion_bar::set_selected(std::uint32_t id) noexcept
    {
        m_has_selection = true;
        m_selected = id;
        invalidate();
    }

    void proportion_bar::clear_selection() noexcept
    {
        m_has_selection = false;
        invalidate();
    }

    std::int32_t proportion_bar::index_of(std::uint32_t id) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i].id == id)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    control_size proportion_bar::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size proportion_bar::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {max_width, draw::px(m_height)};
    }

    dock_rect proportion_bar::track_area() const noexcept
    {
        const dock_rect& box = area();
        const bool caption = m_caption_name[0] != '\0' || m_caption_value[0] != '\0';
        const std::int32_t left = caption ? draw::px(m_caption_width) + draw::px(k_caption_gap) : 0;
        const std::int32_t margin = draw::px(k_margin_y);
        return {box.x + left, box.y + margin, std::max(box.width - left, 1), std::max(box.height - 2 * margin, 1)};
    }

    void proportion_bar::make_slots(std::int32_t track_x, std::int32_t track_width) const noexcept
    {
        (void)track_x;
        m_slot_count = 0;
        float sum = 0.0f;

        for (std::uint32_t i = 0; i < m_count; ++i)
            sum += m_items[i].shown;

        m_full = std::max(m_scale, sum);

        if (m_full <= 0.0f || m_count == 0)
            return;

        float before = 0.0f;
        std::uint32_t i = 0;

        const auto edge = [&](float cumulative) noexcept
        { return static_cast<std::int32_t>(std::lround(cumulative / m_full * static_cast<float>(track_width))); };

        const auto tiny = [&](std::uint32_t index) noexcept
        { return m_items[index].shown / m_full < k_small_share && !(m_has_selection && m_items[index].id == m_selected); };

        while (i < m_count)
        {
            std::uint32_t end = i + 1;
            float length = m_items[i].shown;

            if (tiny(i))
            {
                // A run of small parts of the same kind is one slot, since each would be a pixel or less.
                float run = length;
                std::uint32_t j = i + 1;

                while (j < m_count && tiny(j) && m_items[j].idle == m_items[i].idle)
                {
                    run += m_items[j].shown;
                    ++j;
                }

                if (j - i >= 2)
                {
                    end = j;
                    length = run;
                }
            }

            const std::int32_t from = edge(before);
            const std::int32_t to = edge(before + length);
            before += length;

            if (to > from && m_slot_count < k_max_segments)
                m_slots[m_slot_count++] = {from, to - from, i, end - i};

            i = end;
        }
    }

    std::int32_t proportion_bar::slot_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect track = track_area();
        const std::int32_t inset = draw::px(k_inset);

        if (y < track.y || y >= track.y + track.height || x < track.x + inset || x >= track.x + track.width - inset)
            return -1;

        make_slots(track.x + inset, track.width - 2 * inset);

        for (std::uint32_t i = 0; i < m_slot_count; ++i)
        {
            const std::int32_t from = track.x + inset + m_slots[i].x;

            if (x >= from && x < from + m_slots[i].width)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    dock_rect proportion_bar::tooltip_area() const noexcept
    {
        if (m_hover < 0 || static_cast<std::uint32_t>(m_hover) >= m_slot_count)
            return area();

        const dock_rect track = track_area();
        const slot& s = m_slots[m_hover];
        return {track.x + draw::px(k_inset) + s.x, track.y, s.width, track.height};
    }

    void proportion_bar::show_tooltip(std::int32_t slot_index) noexcept
    {
        if (slot_index < 0 || static_cast<std::uint32_t>(slot_index) >= m_slot_count)
        {
            control::set_tooltip(m_help);
            return;
        }

        const slot& s = m_slots[slot_index];
        float value = 0.0f;

        for (std::uint32_t i = 0; i < s.count; ++i)
            value += m_items[s.first + i].value;

        const float base = m_scale > 0.0f ? m_scale : m_sum;
        char share[24]{};

        if (base > 0.0f)
            std::snprintf(share, sizeof(share), ", %.1f%%", static_cast<double>(value / base * 100.0f));

        char* text = m_tip[m_tip_next];
        m_tip_next ^= 1u;

        if (s.count > 1)
        {
            std::snprintf(text, sizeof(m_tip[0]), "%u small parts%s\n%.2f %s%s", s.count, m_items[s.first].idle ? " (idle)" : "", static_cast<double>(value),
                m_unit, share);
        }
        else
        {
            const item& it = m_items[s.first];
            std::snprintf(text, sizeof(m_tip[0]), "%s%s\n%.2f %s%s%s%s", it.label[0] != '\0' ? it.label : "(unnamed)", it.idle ? " (idle)" : "",
                static_cast<double>(value), m_unit, share, it.detail[0] != '\0' ? "\n" : "", it.detail);
        }

        control::set_tooltip(text);
    }

    void proportion_bar::select_index(std::uint32_t index, bool by_user) noexcept
    {
        if (index >= m_count)
            return;

        const bool changed = !m_has_selection || m_selected != m_items[index].id;
        m_has_selection = true;
        m_selected = m_items[index].id;
        invalidate();

        if (changed && by_user && m_on_select != nullptr)
            m_on_select(m_on_select_user, *this, m_selected);
    }

    void proportion_bar::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t s = slot_at(x, y);

        if (s >= 0 && m_slots[s].count == 1)
            select_index(m_slots[s].first, true);
    }

    void proportion_bar::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        on_press(x, y);
        const std::int32_t s = slot_at(x, y);

        if (s >= 0 && m_slots[s].count == 1 && m_on_activate != nullptr)
            m_on_activate(m_on_activate_user, *this, m_items[m_slots[s].first].id);
    }

    bool proportion_bar::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t s = slot_at(x, y);

        if (s == m_hover)
            return false;

        m_hover = s;
        show_tooltip(s);
        return true;
    }

    void proportion_bar::on_leave() noexcept
    {
        m_hover = -1;
        control::set_tooltip(m_help);
        invalidate();
    }

    bool proportion_bar::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (ctrl || shift || alt || m_count == 0)
            return false;

        const std::int32_t current = m_has_selection ? index_of(m_selected) : -1;

        switch (key)
        {
        case k_key_left:
            select_index(current > 0 ? static_cast<std::uint32_t>(current - 1) : 0, true);
            return true;
        case k_key_right:
            select_index(current >= 0 ? std::min(static_cast<std::uint32_t>(current + 1), m_count - 1) : 0, true);
            return true;
        case k_key_home:
            select_index(0, true);
            return true;
        case k_key_end:
            select_index(m_count - 1, true);
            return true;
        case k_key_enter:
        case k_key_space:
            if (current >= 0 && m_on_activate != nullptr)
                m_on_activate(m_on_activate_user, *this, m_selected);

            return current >= 0;
        default:
            return false;
        }
    }

    bool proportion_bar::animate(std::uint64_t now_ms) noexcept
    {
        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 16.0f;
        const float step = 1.0f - std::exp(-dt / k_ease_ms);
        const float settled = std::max(m_sum, 0.001f) * 0.0005f;
        bool more = false;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            item& it = m_items[i];

            if (it.shown == it.value)
                continue;

            it.shown += (it.value - it.shown) * step;

            if (std::fabs(it.value - it.shown) < settled)
                it.shown = it.value;

            more = more || it.shown != it.value;
        }

        m_moving = more;
        m_stamp = more ? now_ms : 0;
        invalidate();
        return more;
    }

    void proportion_bar::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const dock_rect& box = area();
        const dock_rect track = track_area();
        const bool on = enabled();

        if (track.x > box.x)
        {
            const dock_rect caption{box.x, box.y, track.x - box.x - draw::px(k_caption_gap), box.height};
            // The name gives way to the value, which is what changes and has to stay whole.
            const std::int32_t value_width = draw::text_width(dc, m_caption_value);
            const dock_rect name{caption.x, caption.y, std::max(caption.width - value_width - draw::px(6), 0), caption.height};
            draw::text(dc, name, m_caption_name, on ? (m_emphasis ? colours.accent : colours.text) : colours.text_dim, draw::text_bold | draw::text_ellipsis);
            draw::text(dc, caption, m_caption_value, colours.text_dim, draw::text_right);
        }

        const std::uint32_t well = blend_colour(colours.panel, 0x000000, 0.28f);
        draw::rounded(dc, track, k_radius, well, blend_colour(well, colours.text_dim, 0.25f));

        const std::int32_t inset = draw::px(k_inset);
        const dock_rect inner{track.x + inset, track.y + inset, std::max(track.width - 2 * inset, 1), std::max(track.height - 2 * inset, 1)};
        make_slots(inner.x, inner.width);

        for (std::uint32_t k = 0; k < m_slot_count; ++k)
        {
            const slot& s = m_slots[k];
            const item& first = m_items[s.first];
            const bool merged = s.count > 1;
            const bool picked = m_has_selection && !merged && first.id == m_selected;
            const bool over = static_cast<std::int32_t>(k) == m_hover && on;
            const std::int32_t gap = s.width > 2 ? 1 : 0;
            const dock_rect cell{inner.x + s.x, inner.y, s.width - gap, inner.height};

            std::uint32_t face = merged ? blend_colour(colours.text_dim, colours.panel, 0.35f) : first.colour;

            if (first.idle)
                face = blend_colour(colours.panel, colours.text_dim, 0.18f);

            if (over)
                face = blend_colour(face, 0xFFFFFF, 0.18f);

            if (!on)
                face = blend_colour(face, colours.panel, 0.55f);

            draw::fill(dc, cell, face);

            if (first.idle)
            {
                // Diagonal lines, cut to the cell, say that nothing was being done here.
                const std::uint32_t line = blend_colour(face, colours.text_dim, 0.35f);
                const std::int32_t step = draw::px(k_hatch_step);
                draw::clip(dc, cell);

                for (std::int32_t x = cell.x - cell.height; x < cell.x + cell.width; x += step)
                {
                    const draw::point points[2] = {{static_cast<float>(x), static_cast<float>(cell.y + cell.height)},
                        {static_cast<float>(x + cell.height), static_cast<float>(cell.y)}};
                    draw::polyline(dc, points, 2, line, 1.0f);
                }

                draw::unclip(dc);
            }

            if (s.width >= draw::px(k_min_label))
            {
                float value = 0.0f;

                for (std::uint32_t i = 0; i < s.count; ++i)
                    value += m_items[s.first + i].value;

                char text[96]{};

                if (merged)
                    std::snprintf(text, sizeof(text), "%u small", s.count);
                else
                    std::snprintf(text, sizeof(text), "%s  %.1f", first.label, static_cast<double>(value));

                const std::int32_t room = cell.width - 2 * draw::px(k_text_pad);

                if (!merged && draw::text_width(dc, text) > room)
                    std::snprintf(text, sizeof(text), "%s", first.label);

                const std::uint32_t ink = first.idle || merged ? colours.text_dim : (is_light(face) ? 0x101418u : 0xFFFFFFu);
                draw::text(dc, {cell.x + draw::px(k_text_pad), cell.y, std::max(room, 1), cell.height}, text, ink, draw::text_ellipsis);
            }

            // The ring sits inside the cell, in whichever of dark and light stands out from the face.
            if (picked)
            {
                const std::int32_t ring = draw::px(1);
                draw::outline(dc, {cell.x + ring, cell.y + ring, std::max(cell.width - 2 * ring, 1), std::max(cell.height - 2 * ring, 1)}, 2,
                    is_light(face) ? 0x101418u : 0xFFFFFFu, 2.0f);
            }
        }

        if (m_budget > 0.0f && m_full > 0.0f)
        {
            const std::int32_t at = inner.x + static_cast<std::int32_t>(std::lround(m_budget / m_full * static_cast<float>(inner.width)));

            // Past the budget the track wears a red edge, and a line stands where the budget ends.
            if (at < inner.x + inner.width - 1)
            {
                draw::fill(dc, {at, inner.y, inner.x + inner.width - at, draw::px(k_overflow_height)}, k_overflow_colour);
                draw::fill(dc, {at, track.y - draw::px(2), std::max(draw::px(1), 1), track.height + draw::px(4)}, colours.text);
            }
        }
    }
}
