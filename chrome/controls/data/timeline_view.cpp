#include <chrome/controls/data/timeline_view.hpp>

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
        constexpr std::int32_t k_ruler_height = 24;
        constexpr std::int32_t k_row_height = 26;
        constexpr std::int32_t k_key_half = 5;
        constexpr std::int32_t k_pick_slack = 3;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_move_threshold = 3;
        constexpr std::int32_t k_tick_spacing = 74;

        constexpr float k_min_pps = 4.0f;
        constexpr float k_max_pps = 6000.0f;

        constexpr std::uint32_t k_key_delete = 0x2E;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_a = 0x41;
        constexpr std::uint32_t k_key_f = 0x46;

        void format_time(char* out, std::size_t capacity, float time, float step) noexcept
        {
            const int decimals = step >= 1.0f ? 0 : std::clamp(static_cast<int>(std::ceil(-std::log10(step))), 0, 4);

            if (std::fabs(time) < step * 1.0e-3f)
                time = 0.0f;

            std::snprintf(out, capacity, "%.*f", decimals, static_cast<double>(time));
        }
    }

    timeline_view& timeline_view::add_track(curve& data, const char* name, std::uint32_t colour) noexcept
    {
        if (m_count < k_max_tracks)
        {
            m_tracks[m_count] = {&data, name, colour};
            ++m_count;
            invalidate();
        }

        return *this;
    }

    timeline_view& timeline_view::clear_tracks() noexcept
    {
        m_count = 0;
        m_selection->clear();
        m_scroll = 0;
        invalidate();
        return *this;
    }

    timeline_view& timeline_view::set_selection(key_selection* selection) noexcept
    {
        m_selection = selection != nullptr ? selection : &m_own_selection;
        invalidate();
        return *this;
    }

    timeline_view& timeline_view::set_fps(float fps) noexcept
    {
        m_fps = std::clamp(fps, 1.0f, 240.0f);
        invalidate();
        return *this;
    }

    timeline_view& timeline_view::set_time(float time) noexcept
    {
        if (time != m_time)
        {
            m_time = time;
            invalidate();
        }

        return *this;
    }

    timeline_view& timeline_view::set_range(float start, float end) noexcept
    {
        m_range_start = start;
        m_range_end = end;
        invalidate();
        return *this;
    }

    timeline_view& timeline_view::set_view(float time_from, float pixels_per_second) noexcept
    {
        m_t0 = time_from;
        m_pps = std::clamp(pixels_per_second, k_min_pps, k_max_pps);
        invalidate();
        return *this;
    }

    timeline_view& timeline_view::set_rows(std::uint32_t rows) noexcept
    {
        m_rows = std::clamp<std::uint32_t>(rows, 1, 16);
        return *this;
    }

    timeline_view& timeline_view::frame_all() noexcept
    {
        m_frame_pending = true;
        resolve_frame();
        invalidate();
        return *this;
    }

    void timeline_view::resolve_frame() const noexcept
    {
        const dock_rect rows = rows_area();

        // Without a size yet there is nothing to fit to; the first paint tries again.
        if (area().width <= 0 || rows.width <= 1)
            return;

        float first = 0.0f;
        float last = 0.0f;
        bool any = false;

        for (std::uint32_t t = 0; t < m_count; ++t)
        {
            const curve& data = *m_tracks[t].data;

            if (data.count() == 0)
                continue;

            const float low = data.key(0).time;
            const float high = data.key(data.count() - 1).time;
            first = any ? std::min(first, low) : low;
            last = any ? std::max(last, high) : high;
            any = true;
        }

        if (m_range_end > m_range_start)
        {
            first = any ? std::min(first, m_range_start) : m_range_start;
            last = any ? std::max(last, m_range_end) : m_range_end;
            any = true;
        }

        if (!any)
        {
            first = 0.0f;
            last = 1.0f;
        }

        const float span = std::max(last - first, 0.1f);
        const float width = static_cast<float>(rows.width);
        m_pps = std::clamp(width / scale() / (span * 1.12f), k_min_pps, k_max_pps);
        m_t0 = first - span * 0.06f;
        m_frame_pending = false;
    }

    dock_rect timeline_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    std::int32_t timeline_view::row_height() const noexcept
    {
        return draw::px(k_row_height);
    }

    bool timeline_view::bar_needed() const noexcept
    {
        const dock_rect box = inner();
        return static_cast<std::int32_t>(m_count) * row_height() > box.height - draw::px(k_ruler_height);
    }

    dock_rect timeline_view::rows_area() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t label = draw::px(m_label_width);
        const std::int32_t ruler = draw::px(k_ruler_height);
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        return {box.x + label, box.y + ruler, std::max(box.width - label - bar, 1), std::max(box.height - ruler, 1)};
    }

    std::int32_t timeline_view::max_scroll() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_count) * row_height() - rows_area().height, 0);
    }

    float timeline_view::scale() const noexcept
    {
        return static_cast<float>(draw::px(100)) / 100.0f;
    }

    float timeline_view::per_second() const noexcept
    {
        return m_pps * scale();
    }

    float timeline_view::x_of(float time) const noexcept
    {
        return static_cast<float>(rows_area().x) + (time - m_t0) * per_second();
    }

    float timeline_view::time_at(float x) const noexcept
    {
        return m_t0 + (x - static_cast<float>(rows_area().x)) / per_second();
    }

    float timeline_view::snapped(float time) const noexcept
    {
        return m_snap ? std::round(time * m_fps) / m_fps : time;
    }

    std::int32_t timeline_view::row_top(std::uint32_t row) const noexcept
    {
        return rows_area().y + static_cast<std::int32_t>(row) * row_height() - m_scroll;
    }

    timeline_view::hit timeline_view::hit_test(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect rows = rows_area();

        if (x < rows.x || x >= rows.x + rows.width || y < rows.y || y >= rows.y + rows.height)
            return {};

        const std::int32_t row = (y - rows.y + m_scroll) / row_height();

        if (row < 0 || row >= static_cast<std::int32_t>(m_count))
            return {};

        const curve& data = *m_tracks[row].data;
        const float reach = static_cast<float>(draw::px(k_key_half + k_pick_slack));
        float best = reach;
        hit result;

        for (std::uint32_t k = 0; k < data.count(); ++k)
        {
            const float distance = std::fabs(x_of(data.key(k).time) - static_cast<float>(x));

            if (distance <= best)
            {
                best = distance;
                result = {row, static_cast<std::int32_t>(k)};
            }
        }

        return result;
    }

    void timeline_view::set_scroll(std::int32_t scroll) noexcept
    {
        const std::int32_t clamped = std::clamp(scroll, 0, max_scroll());

        if (clamped != m_scroll)
        {
            m_scroll = clamped;
            invalidate();
        }
    }

    control_size timeline_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size timeline_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(320));
        return {width, draw::px(k_ruler_height) + static_cast<std::int32_t>(m_rows) * row_height() + 2 * draw::px(1)};
    }

    void timeline_view::move_time(float time) noexcept
    {
        m_time = std::max(snapped(time), 0.0f);
        invalidate();

        if (m_time_fn != nullptr)
            m_time_fn(m_time_user, *this, m_time);
    }

    void timeline_view::notify_change(bool final) noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this, final);
    }

    void timeline_view::notify_select() noexcept
    {
        invalidate();

        if (m_select != nullptr)
            m_select(m_select_user, *this);
    }

    void timeline_view::remove_selected() noexcept
    {
        bool any = false;

        for (std::uint32_t t = 0; t < m_count; ++t)
        {
            for (std::uint32_t k = m_tracks[t].data->count(); k > 0; --k)
            {
                if (m_selection->is_selected(t, k - 1))
                {
                    m_tracks[t].data->remove_key(k - 1);
                    m_selection->key_removed(t, k - 1);
                    any = true;
                }
            }
        }

        m_hover = {};

        if (any)
        {
            notify_select();
            notify_change(true);
        }
    }

    void timeline_view::select_all() noexcept
    {
        for (std::uint32_t t = 0; t < m_count; ++t)
        {
            for (std::uint32_t k = 0; k < m_tracks[t].data->count(); ++k)
                m_selection->set(t, k, true);
        }

        notify_select();
    }

    void timeline_view::clear_selection() noexcept
    {
        m_selection->clear();
        notify_select();
    }

    void timeline_view::box_select() noexcept
    {
        const float left = static_cast<float>(std::min(m_press_x, m_x));
        const float right = static_cast<float>(std::max(m_press_x, m_x));
        const std::int32_t top = std::min(m_press_y, m_y);
        const std::int32_t bottom = std::max(m_press_y, m_y);
        *m_selection = m_base;

        for (std::uint32_t t = 0; t < m_count; ++t)
        {
            const std::int32_t centre = row_top(t) + row_height() / 2;

            if (centre < top || centre > bottom)
                continue;

            for (std::uint32_t k = 0; k < m_tracks[t].data->count(); ++k)
            {
                const float x = x_of(m_tracks[t].data->key(k).time);

                if (x >= left && x <= right)
                    m_selection->set(t, k, true);
            }
        }
    }

    void timeline_view::apply_move(std::int32_t x) noexcept
    {
        const float dt = static_cast<float>(x - m_press_x) / per_second();

        for (std::uint32_t t = 0; t < m_count; ++t)
        {
            curve& data = *m_tracks[t].data;
            const std::uint32_t count = data.count();

            // Keys that move toward their neighbours go first, so none is held back by one that has not moved yet.
            for (std::uint32_t n = 0; n < count; ++n)
            {
                const std::uint32_t k = dt >= 0.0f ? count - 1 - n : n;

                if (m_selection->is_selected(t, k))
                    data.move_key(k, snapped(m_orig_time[t][k] + dt), data.key(k).value);
            }
        }

        m_changed = true;
        notify_change(false);
    }

    void timeline_view::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        m_press = {};
        m_press_x = x;
        m_press_y = y;
        m_x = x;
        m_y = y;
        m_changed = false;
        const dock_rect box = inner();
        const dock_rect rows = rows_area();

        // The thin bar on the right.
        if (bar_needed() && x >= box.x + box.width - draw::px(k_bar_column) && y >= rows.y)
        {
            const std::int32_t total = static_cast<std::int32_t>(m_count) * row_height();
            const std::int32_t length = std::clamp(rows.height * rows.height / std::max(total, 1), std::min(draw::px(k_min_thumb), rows.height), rows.height);
            const std::int32_t travel = rows.height - length;
            const std::int32_t start = rows.y + (max_scroll() > 0 ? travel * m_scroll / max_scroll() : 0);

            if (y >= start && y < start + length)
            {
                m_drag = drag_kind::bar;
                m_grab = y - start;
            }
            else
            {
                set_scroll(m_scroll + (y < start ? -1 : 1) * rows.height);
            }

            return;
        }

        // The ruler moves the time cursor, and with Ctrl pans.
        if (y < rows.y && x >= rows.x)
        {
            if (ctrl_down())
            {
                m_drag = drag_kind::pan;
            }
            else
            {
                m_drag = drag_kind::scrub;
                move_time(time_at(static_cast<float>(x)));
            }

            return;
        }

        // A name selects all the keys of its row.
        if (x < rows.x && y >= rows.y)
        {
            const std::int32_t row = (y - rows.y + m_scroll) / row_height();

            if (row >= 0 && row < static_cast<std::int32_t>(m_count))
            {
                if (!ctrl_down() && !shift_down())
                    m_selection->clear();

                for (std::uint32_t k = 0; k < m_tracks[row].data->count(); ++k)
                    m_selection->set(static_cast<std::uint32_t>(row), k, true);

                notify_select();
            }

            return;
        }

        if (x < rows.x || y < rows.y)
            return;

        const hit target = hit_test(x, y);

        if (target.track >= 0)
        {
            m_press = target;
            const auto t = static_cast<std::uint32_t>(target.track);
            const auto k = static_cast<std::uint32_t>(target.key);
            const bool selected = m_selection->is_selected(t, k);

            if (ctrl_down() || shift_down())
            {
                m_selection->set(t, k, !selected);
                notify_select();

                if (selected)
                    return;
            }
            else if (!selected)
            {
                m_selection->clear();
                m_selection->set(t, k, true);
                notify_select();
            }

            for (std::uint32_t i = 0; i < m_count; ++i)
            {
                for (std::uint32_t j = 0; j < m_tracks[i].data->count(); ++j)
                    m_orig_time[i][j] = m_tracks[i].data->key(j).time;
            }

            m_drag = drag_kind::lift;
            invalidate();
            return;
        }

        if (!ctrl_down() && !shift_down() && m_selection->count() > 0)
        {
            m_selection->clear();
            notify_select();
        }

        m_base = *m_selection;
        m_drag = drag_kind::box;
        invalidate();
    }

    void timeline_view::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        const dock_rect rows = rows_area();

        if (x < rows.x || y < rows.y || x >= rows.x + rows.width || y >= rows.y + rows.height)
            return;

        const std::int32_t row = (y - rows.y + m_scroll) / row_height();

        if (row < 0 || row >= static_cast<std::int32_t>(m_count))
            return;

        curve& data = *m_tracks[row].data;
        const float time = std::max(snapped(time_at(static_cast<float>(x))), 0.0f);
        const std::uint32_t index = data.add_key(time, data.evaluate(time));

        if (index == curve::k_none)
            return;

        m_selection->key_inserted(static_cast<std::uint32_t>(row), index);
        m_selection->clear();
        m_selection->set(static_cast<std::uint32_t>(row), index, true);
        notify_select();
        notify_change(true);
    }

    bool timeline_view::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t last_x = m_x;
        m_x = x;
        m_y = y;

        switch (m_drag)
        {
        case drag_kind::scrub:
            move_time(time_at(static_cast<float>(x)));
            return true;

        case drag_kind::pan:
            m_t0 -= static_cast<float>(x - last_x) / per_second();
            return true;

        case drag_kind::bar:
        {
            const dock_rect rows = rows_area();
            const std::int32_t total = static_cast<std::int32_t>(m_count) * row_height();
            const std::int32_t length = std::clamp(rows.height * rows.height / std::max(total, 1), std::min(draw::px(k_min_thumb), rows.height), rows.height);
            const std::int32_t travel = std::max(rows.height - length, 1);
            const std::int32_t at = std::clamp(y - m_grab - rows.y, 0, travel);
            set_scroll((at * max_scroll() + travel / 2) / travel);
            return true;
        }

        case drag_kind::lift:
        {
            const std::int32_t reach = draw::px(k_move_threshold);

            if (std::abs(x - m_press_x) <= reach)
                return false;

            m_drag = drag_kind::move;
            apply_move(x);
            return true;
        }

        case drag_kind::move:
            apply_move(x);
            return true;

        case drag_kind::box:
            box_select();
            notify_select();
            return true;

        default:
            return false;
        }
    }

    bool timeline_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const hit now = hit_test(x, y);

        if (now.track == m_hover.track && now.key == m_hover.key)
            return false;

        m_hover = now;
        return true;
    }

    void timeline_view::on_leave() noexcept
    {
        if (m_hover.track >= 0)
        {
            m_hover = {};
            invalidate();
        }
    }

    void timeline_view::on_release() noexcept
    {
        if (m_drag == drag_kind::lift && m_press.track >= 0 && m_selection->count() > 1 && !ctrl_down() && !shift_down())
        {
            // Let go on a selected key without moving: the selection narrows to that key, as a click does.
            m_selection->clear();
            m_selection->set(static_cast<std::uint32_t>(m_press.track), static_cast<std::uint32_t>(m_press.key), true);
            notify_select();
        }

        if (m_drag == drag_kind::move && m_changed)
            notify_change(true);

        m_changed = false;
        m_drag = drag_kind::none;
        invalidate();
    }

    bool timeline_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)y;

        if (ctrl_down())
        {
            const float anchor = time_at(static_cast<float>(x));
            m_pps = std::clamp(m_pps * std::pow(1.15f, static_cast<float>(notches)), k_min_pps, k_max_pps);
            m_t0 = anchor - (static_cast<float>(x) - static_cast<float>(rows_area().x)) / per_second();
            invalidate();
            return true;
        }

        if (shift_down())
        {
            m_t0 -= static_cast<float>(notches) * static_cast<float>(draw::px(60)) / per_second();
            invalidate();
            return true;
        }

        const std::int32_t before = m_scroll;
        set_scroll(m_scroll - notches * row_height());

        // With nothing left to scroll the panel gets the wheel.
        return m_scroll != before;
    }

    bool timeline_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)shift;

        if (alt)
            return false;

        switch (key)
        {
        case k_key_delete:
            remove_selected();
            return true;

        case k_key_a:
            if (!ctrl)
                return false;

            select_all();
            return true;

        case k_key_f:
        case k_key_home:
            frame_all();
            return true;

        case k_key_left:
        case k_key_right:
        {
            const bool forward = key == k_key_right;

            if (!ctrl)
            {
                move_time(std::round(m_time * m_fps) / m_fps + (forward ? 1.0f : -1.0f) / m_fps);
                return true;
            }

            // To the key before or after the cursor, in any row.
            bool found = false;
            float target = 0.0f;
            const float gap = 0.25f / m_fps;

            for (std::uint32_t t = 0; t < m_count; ++t)
            {
                for (std::uint32_t k = 0; k < m_tracks[t].data->count(); ++k)
                {
                    const float time = m_tracks[t].data->key(k).time;

                    if (forward ? time > m_time + gap : time < m_time - gap)
                    {
                        if (!found || (forward ? time < target : time > target))
                            target = time;

                        found = true;
                    }
                }
            }

            if (found)
                move_time(target);

            return true;
        }

        default:
            return false;
        }
    }

    void timeline_view::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();
        std::uint32_t top = blend_colour(colours.panel, black, 0.32f);
        std::uint32_t bottom = blend_colour(colours.panel, black, 0.18f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.55f);

        if (!enabled())
        {
            top = bottom = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
        }

        if (m_frame_pending)
            resolve_frame();

        draw::gradient(dc, area(), 4, top, bottom, edge, false);

        const dock_rect box = inner();
        const dock_rect rows = rows_area();
        const std::int32_t row_h = row_height();
        const std::int32_t ruler_h = draw::px(k_ruler_height);
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::uint32_t ruler_face = blend_colour(colours.panel, black, 0.06f);
        const std::uint32_t ruler_ink = colours.text_dim;
        const std::uint32_t line_dark = blend_colour(bottom, black, 0.5f);
        draw::clip(dc, box);

        // The column of names is a little darker than the rows, as a recess beside them.
        draw::gradient(dc, {box.x, rows.y, rows.x - box.x, box.height - ruler_h}, 0, blend_colour(bottom, black, 0.22f), blend_colour(bottom, black, 0.1f), line_dark, false);

        const std::int32_t first_row = std::max(m_scroll / row_h, 0);
        const std::int32_t last_row = std::min((m_scroll + rows.height) / row_h, static_cast<std::int32_t>(m_count) - 1);

        // The stripes of the rows, across the names and the time.
        for (std::int32_t row = first_row; row <= last_row; ++row)
        {
            const std::int32_t y = row_top(static_cast<std::uint32_t>(row));
            const std::uint32_t stripe = (row & 1) != 0 ? blend_colour(bottom, white, 0.045f) : blend_colour(bottom, white, 0.012f);
            draw::clip(dc, {rows.x, rows.y, rows.width, rows.height});
            draw::fill(dc, {rows.x, y, rows.width, row_h}, stripe);
            draw::fill(dc, {rows.x, y + row_h - 1, rows.width, 1}, line_dark);
            draw::unclip(dc);
        }

        // Time ticks: the step is the roundest that keeps the labels apart.
        const float raw = static_cast<float>(draw::px(k_tick_spacing)) / per_second();
        float step = 0.0f;

        if (m_display == time_display::frames)
            step = std::max(std::round(nice_step(raw * m_fps)), 1.0f) / m_fps;
        else
            step = nice_step(raw);

        const float minor = step / 5.0f;
        const bool minors = minor * per_second() >= 8.0f && (m_display == time_display::seconds || step * m_fps >= 4.9f);

        draw::clip(dc, rows);

        // What lies outside the span that plays is darker.
        if (m_range_end > m_range_start)
        {
            const float start = x_of(m_range_start);
            const float end = x_of(m_range_end);
            const std::uint32_t outside = blend_colour(bottom, black, 0.3f);

            if (start > static_cast<float>(rows.x))
                draw::fill(dc, {rows.x, rows.y, static_cast<std::int32_t>(std::min(start, static_cast<float>(rows.x + rows.width)) - static_cast<float>(rows.x)), rows.height}, outside);

            if (end < static_cast<float>(rows.x + rows.width))
            {
                const std::int32_t from = static_cast<std::int32_t>(std::max(end, static_cast<float>(rows.x)));
                draw::fill(dc, {from, rows.y, rows.x + rows.width - from, rows.height}, outside);
            }
        }

        const std::uint32_t grid_major = blend_colour(bottom, white, 0.10f);
        const std::uint32_t grid_minor = blend_colour(bottom, white, 0.04f);
        const float first_tick = std::ceil(m_t0 / step);

        if (minors)
        {
            const float first_minor = std::ceil(m_t0 / minor);

            for (int i = 0; i < 800; ++i)
            {
                const float x = x_of((first_minor + static_cast<float>(i)) * minor);

                if (x > static_cast<float>(rows.x + rows.width))
                    break;

                draw::fill(dc, {static_cast<std::int32_t>(x), rows.y, 1, rows.height}, grid_minor);
            }
        }

        for (int i = 0; i < 400; ++i)
        {
            const float x = x_of((first_tick + static_cast<float>(i)) * step);

            if (x > static_cast<float>(rows.x + rows.width))
                break;

            draw::fill(dc, {static_cast<std::int32_t>(x), rows.y, 1, rows.height}, grid_major);
        }

        // The keys: raised diamonds with a shadow under each.
        const float half = static_cast<float>(draw::px(k_key_half));

        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::int32_t row = first_row; row <= last_row; ++row)
            {
                const curve& data = *m_tracks[row].data;
                const float cy = static_cast<float>(row_top(static_cast<std::uint32_t>(row)) + row_h / 2);

                for (std::uint32_t k = 0; k < data.count(); ++k)
                {
                    const bool selected = m_selection->is_selected(static_cast<std::uint32_t>(row), k);

                    if ((pass == 1) != selected)
                        continue;

                    const float cx = x_of(data.key(k).time);

                    if (cx < static_cast<float>(rows.x) - half - 2.0f || cx > static_cast<float>(rows.x + rows.width) + half + 2.0f)
                        continue;

                    const bool hovered = hot() && m_hover.track == row && m_hover.key == static_cast<std::int32_t>(k);
                    const float h = half + (selected || hovered ? 1.0f : 0.0f);
                    const std::uint32_t track_colour = m_tracks[row].colour;
                    const draw::point shadow[4] = {{cx, cy - h + 1.8f}, {cx + h + 0.6f, cy + 1.8f}, {cx, cy + h + 2.4f}, {cx - h - 0.6f, cy + 1.8f}};
                    draw::polygon(dc, shadow, 4, blend_colour(bottom, black, 0.7f));
                    const std::uint32_t rim = selected ? colours.accent : blend_colour(track_colour, black, 0.55f);
                    const draw::point outer[4] = {{cx, cy - h}, {cx + h, cy}, {cx, cy + h}, {cx - h, cy}};
                    draw::polygon(dc, outer, 4, rim);
                    const float inner_h = h - (selected ? 2.0f : 1.4f);
                    const draw::point face[4] = {{cx, cy - inner_h}, {cx + inner_h, cy}, {cx, cy + inner_h}, {cx - inner_h, cy}};
                    draw::polygon(dc, face, 4, selected ? white : blend_colour(track_colour, white, hovered ? 0.5f : 0.18f));
                    // The lit upper half.
                    const draw::point shine[3] = {{cx - inner_h, cy}, {cx, cy - inner_h}, {cx + inner_h, cy}};
                    draw::polygon(dc, shine, 3, blend_colour(selected ? white : track_colour, white, selected ? 0.0f : 0.55f));
                }
            }
        }

        // The cursor, through the rows.
        {
            const float x = x_of(m_time);

            if (x >= static_cast<float>(rows.x) && x <= static_cast<float>(rows.x + rows.width))
                draw::fill(dc, {static_cast<std::int32_t>(x), rows.y, 1, rows.height}, colours.accent);
        }

        // The box being dragged out.
        if (m_drag == drag_kind::box)
            draw::outline(dc, {std::min(m_press_x, m_x), std::min(m_press_y, m_y), std::abs(m_x - m_press_x), std::abs(m_y - m_press_y)}, 0, colours.accent, 1.0f);

        draw::unclip(dc);

        // The names, with the colour each row's keys are drawn in.
        draw::clip(dc, {box.x, rows.y, rows.x - box.x - 1, rows.height});

        for (std::int32_t row = first_row; row <= last_row; ++row)
        {
            const std::int32_t y = row_top(static_cast<std::uint32_t>(row));
            const float cy = static_cast<float>(y + row_h / 2);
            draw::disc(dc, static_cast<float>(box.x + draw::px(12)), cy + 1.0f, 4.6f, line_dark, line_dark, 0.0f);
            draw::disc(dc, static_cast<float>(box.x + draw::px(12)), cy, 4.2f, m_tracks[row].colour, blend_colour(m_tracks[row].colour, black, 0.5f), 1.0f);

            if (m_tracks[row].name != nullptr)
                draw::text(dc, {box.x + draw::px(24), y, rows.x - box.x - draw::px(30), row_h}, m_tracks[row].name, colours.text, draw::text_ellipsis);
        }

        draw::unclip(dc);

        // The ridge between the names and the rows.
        draw::fill(dc, {rows.x - 1, rows.y, 1, rows.height}, line_dark);
        draw::fill(dc, {rows.x, rows.y, 1, rows.height}, blend_colour(bottom, white, 0.08f));

        // The ruler lies over the top, a little raised.
        draw::gradient(dc, {box.x, box.y, box.width, ruler_h}, 0, blend_colour(ruler_face, white, 0.07f), ruler_face, line_dark, false);

        {
            draw::clip(dc, {rows.x, box.y, rows.width, ruler_h});
            char text[32];

            if (minors)
            {
                const float first_minor = std::ceil(m_t0 / minor);

                for (int i = 0; i < 800; ++i)
                {
                    const float x = x_of((first_minor + static_cast<float>(i)) * minor);

                    if (x > static_cast<float>(rows.x + rows.width))
                        break;

                    draw::fill(dc, {static_cast<std::int32_t>(x), box.y + ruler_h - draw::px(3), 1, draw::px(3)}, blend_colour(ruler_ink, ruler_face, 0.5f));
                }
            }

            for (int i = 0; i < 400; ++i)
            {
                const float t = (first_tick + static_cast<float>(i)) * step;
                const float x = x_of(t);

                if (x > static_cast<float>(rows.x + rows.width))
                    break;

                draw::fill(dc, {static_cast<std::int32_t>(x), box.y + ruler_h - draw::px(6), 1, draw::px(6)}, ruler_ink);

                if (m_display == time_display::frames)
                    std::snprintf(text, sizeof(text), "%d", static_cast<int>(std::lround(t * m_fps)));
                else
                    format_time(text, sizeof(text), t, step);

                draw::text(dc, {static_cast<std::int32_t>(x) + draw::px(4), box.y + 1, draw::px(64), ruler_h - 2}, text, ruler_ink);
            }

            // The cursor's mark: a flag with a point, made of rows of whole pixels so it lies exactly on the line.
            const std::int32_t column = static_cast<std::int32_t>(x_of(m_time));
            const std::int32_t bottom = box.y + ruler_h;
            const std::int32_t reach = draw::px(5);
            const std::int32_t body = draw::px(7);
            const std::int32_t point = draw::px(6);

            for (std::int32_t row = 0; row < body + point; ++row)
            {
                const std::int32_t half = row < body ? reach : reach * (body + point - 1 - row) / std::max(point - 1, 1);
                draw::fill(dc, {column - half, bottom - body - point + row, 2 * half + 1, 1}, colours.accent);
            }

            draw::unclip(dc);
        }

        // The bar on the right, when the rows are more than fit.
        if (bar > 0)
        {
            const std::int32_t total = static_cast<std::int32_t>(m_count) * row_h;
            const std::int32_t length = std::clamp(rows.height * rows.height / std::max(total, 1), std::min(draw::px(k_min_thumb), rows.height), rows.height);
            const std::int32_t travel = rows.height - length;
            const std::int32_t start = rows.y + (max_scroll() > 0 ? travel * m_scroll / max_scroll() : 0);
            const std::int32_t width = draw::px(k_thumb_width);
            const std::uint32_t thumb = m_drag == drag_kind::bar ? colours.text_dim : blend_colour(colours.text_dim, bottom, hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {box.x + box.width - (bar + width) / 2, start, width, length}, 2, thumb, thumb);
        }

        draw::unclip(dc);
    }
}
