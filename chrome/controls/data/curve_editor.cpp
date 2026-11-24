#include <chrome/controls/data/curve_editor.hpp>

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
        constexpr std::int32_t k_ruler_left = 46;
        constexpr std::int32_t k_ruler_top = 20;
        constexpr std::int32_t k_key_radius = 4;
        constexpr std::int32_t k_pick_radius = 8;
        constexpr std::int32_t k_handle_pick = 7;
        constexpr std::int32_t k_handle_length = 36;
        constexpr std::int32_t k_move_threshold = 3;
        constexpr std::int32_t k_grid_spacing = 78;

        constexpr std::uint32_t k_key_delete = 0x2E;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_a = 0x41;
        constexpr std::uint32_t k_key_b = 0x42;
        constexpr std::uint32_t k_key_f = 0x46;
        constexpr std::uint32_t k_key_l = 0x4C;
        constexpr std::uint32_t k_key_s = 0x53;
        constexpr std::uint32_t k_key_t = 0x54;

        constexpr float k_limit = 1.0e6f;

        void format_number(char* out, std::size_t capacity, float value, float step) noexcept
        {
            const int decimals = step >= 1.0f ? 0 : std::clamp(static_cast<int>(std::ceil(-std::log10(step))), 0, 4);

            if (std::fabs(value) < step * 1.0e-3f)
                value = 0.0f;

            std::snprintf(out, capacity, "%.*f", decimals, static_cast<double>(value));
        }
    }

    curve_editor& curve_editor::add_curve(curve& data, const char* name, std::uint32_t colour) noexcept
    {
        if (m_count < k_max_curves)
        {
            m_curves[m_count] = {&data, name, colour, true};
            ++m_count;
            invalidate();
        }

        return *this;
    }

    curve_editor& curve_editor::clear_curves() noexcept
    {
        m_count = 0;
        m_selection->clear();
        invalidate();
        return *this;
    }

    curve_editor& curve_editor::set_curve_visible(std::uint32_t index, bool visible) noexcept
    {
        if (index < m_count)
        {
            m_curves[index].visible = visible;
            invalidate();
        }

        return *this;
    }

    curve_editor& curve_editor::set_selection(key_selection* selection) noexcept
    {
        m_selection = selection != nullptr ? selection : &m_own_selection;
        invalidate();
        return *this;
    }

    void curve_editor::fix_view() noexcept
    {
        m_t0 = std::clamp(m_t0, -k_limit, k_limit);
        m_t1 = std::clamp(m_t1, -k_limit, k_limit);
        m_v0 = std::clamp(m_v0, -k_limit, k_limit);
        m_v1 = std::clamp(m_v1, -k_limit, k_limit);

        if (m_t1 - m_t0 < 1.0e-4f)
            m_t1 = m_t0 + 1.0e-4f;

        if (m_v1 - m_v0 < 1.0e-5f)
            m_v1 = m_v0 + 1.0e-5f;
    }

    curve_editor& curve_editor::set_view(float time_from, float time_to, float value_from, float value_to) noexcept
    {
        m_t0 = time_from;
        m_t1 = time_to;
        m_v0 = value_from;
        m_v1 = value_to;
        fix_view();
        invalidate();
        return *this;
    }

    curve_editor& curve_editor::frame_all() noexcept
    {
        float t0 = 0.0f, t1 = 0.0f, v0 = 0.0f, v1 = 0.0f;
        bool any = false;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            if (!m_curves[c].visible)
                continue;

            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                const curve_key& key = m_curves[c].data->key(k);

                if (!any)
                {
                    t0 = t1 = key.time;
                    v0 = v1 = key.value;
                    any = true;
                }

                t0 = std::min(t0, key.time);
                t1 = std::max(t1, key.time);
                v0 = std::min(v0, key.value);
                v1 = std::max(v1, key.value);
            }
        }

        if (!any)
        {
            t0 = 0.0f;
            t1 = 1.0f;
            v0 = 0.0f;
            v1 = 1.0f;
        }

        const float time_pad = std::max((t1 - t0) * 0.08f, 0.05f);
        const float value_pad = std::max((v1 - v0) * 0.14f, 0.05f);
        return set_view(t0 - time_pad, t1 + time_pad, v0 - value_pad, v1 + value_pad);
    }

    curve_editor& curve_editor::frame_selected() noexcept
    {
        float t0 = 0.0f, t1 = 0.0f, v0 = 0.0f, v1 = 0.0f;
        bool any = false;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                if (!m_curves[c].visible || !m_selection->is_selected(c, k))
                    continue;

                const curve_key& key = m_curves[c].data->key(k);

                if (!any)
                {
                    t0 = t1 = key.time;
                    v0 = v1 = key.value;
                    any = true;
                }

                t0 = std::min(t0, key.time);
                t1 = std::max(t1, key.time);
                v0 = std::min(v0, key.value);
                v1 = std::max(v1, key.value);
            }
        }

        if (!any)
            return frame_all();

        const float time_pad = std::max((t1 - t0) * 0.15f, 0.25f);
        const float value_pad = std::max((v1 - v0) * 0.2f, 0.25f);
        return set_view(t0 - time_pad, t1 + time_pad, v0 - value_pad, v1 + value_pad);
    }

    curve_editor& curve_editor::set_time(float time) noexcept
    {
        if (time != m_time)
        {
            m_time = time;
            invalidate();
        }

        return *this;
    }

    dock_rect curve_editor::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    dock_rect curve_editor::plot() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t left = draw::px(k_ruler_left);
        const std::int32_t top = draw::px(k_ruler_top);
        return {box.x + left, box.y + top, std::max(box.width - left, 1), std::max(box.height - top, 1)};
    }

    float curve_editor::x_of(float time) const noexcept
    {
        const dock_rect p = plot();
        return static_cast<float>(p.x) + (time - m_t0) / (m_t1 - m_t0) * static_cast<float>(p.width);
    }

    float curve_editor::y_of(float value) const noexcept
    {
        const dock_rect p = plot();
        return static_cast<float>(p.y + p.height) - (value - m_v0) / (m_v1 - m_v0) * static_cast<float>(p.height);
    }

    float curve_editor::time_at(float x) const noexcept
    {
        const dock_rect p = plot();
        return m_t0 + (x - static_cast<float>(p.x)) / static_cast<float>(p.width) * (m_t1 - m_t0);
    }

    float curve_editor::value_at(float y) const noexcept
    {
        const dock_rect p = plot();
        return m_v0 + (static_cast<float>(p.y + p.height) - y) / static_cast<float>(p.height) * (m_v1 - m_v0);
    }

    curve_editor::point_f curve_editor::key_point(std::uint32_t curve_index, std::uint32_t key) const noexcept
    {
        const curve_key& data = m_curves[curve_index].data->key(key);
        return {x_of(data.time), y_of(data.value)};
    }

    bool curve_editor::has_handles(std::uint32_t curve_index, std::uint32_t key, bool out) const noexcept
    {
        const curve& data = *m_curves[curve_index].data;
        const key_mode mode = data.key(key).mode;

        if (mode != key_mode::smooth && mode != key_mode::free)
            return false;

        return out ? key + 1 < data.count() : key > 0;
    }

    curve_editor::point_f curve_editor::handle_point(std::uint32_t curve_index, std::uint32_t key, bool out) const noexcept
    {
        const curve_key& data = m_curves[curve_index].data->key(key);
        const dock_rect p = plot();
        const float per_time = static_cast<float>(p.width) / (m_t1 - m_t0);
        const float per_value = static_cast<float>(p.height) / (m_v1 - m_v0);
        const float slope = out ? data.out_slope : data.in_slope;
        float dx = per_time;
        float dy = -slope * per_value;
        const float length = std::sqrt(dx * dx + dy * dy);

        if (length > 0.0f)
        {
            dx = dx / length * static_cast<float>(draw::px(k_handle_length));
            dy = dy / length * static_cast<float>(draw::px(k_handle_length));
        }

        const point_f at = key_point(curve_index, key);
        return out ? point_f{at.x + dx, at.y + dy} : point_f{at.x - dx, at.y - dy};
    }

    curve_editor::hit curve_editor::hit_test(std::int32_t x, std::int32_t y) const noexcept
    {
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        hit result;

        // The handles of selected keys come first, since they lie over whatever is under them.
        const float handle_reach = static_cast<float>(draw::px(k_handle_pick));
        float best = handle_reach * handle_reach;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            if (!m_curves[c].visible)
                continue;

            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                if (!m_selection->is_selected(c, k))
                    continue;

                for (int side = 1; side <= 2; ++side)
                {
                    if (!has_handles(c, k, side == 2))
                        continue;

                    const point_f at = handle_point(c, k, side == 2);
                    const float distance = (at.x - fx) * (at.x - fx) + (at.y - fy) * (at.y - fy);

                    if (distance <= best)
                    {
                        best = distance;
                        result = {static_cast<std::int32_t>(c), static_cast<std::int32_t>(k), side};
                    }
                }
            }
        }

        if (result.curve >= 0)
            return result;

        const float reach = static_cast<float>(draw::px(k_pick_radius));
        best = reach * reach;

        // Later curves are drawn on top, so they win a tie.
        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            if (!m_curves[c].visible)
                continue;

            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                const point_f at = key_point(c, k);
                const float distance = (at.x - fx) * (at.x - fx) + (at.y - fy) * (at.y - fy);

                if (distance <= best)
                {
                    best = distance;
                    result = {static_cast<std::int32_t>(c), static_cast<std::int32_t>(k), 0};
                }
            }
        }

        return result;
    }

    control_size curve_editor::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size curve_editor::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(240)), draw::px(m_height)};
    }

    void curve_editor::notify_change(bool final) noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this, final);
    }

    void curve_editor::notify_select() noexcept
    {
        invalidate();

        if (m_select != nullptr)
            m_select(m_select_user, *this);
    }

    void curve_editor::zoom(float factor_time, float factor_value, float anchor_x, float anchor_y) noexcept
    {
        const float time = time_at(anchor_x);
        const float value = value_at(anchor_y);
        m_t0 = time - (time - m_t0) * factor_time;
        m_t1 = time + (m_t1 - time) * factor_time;
        m_v0 = value - (value - m_v0) * factor_value;
        m_v1 = value + (m_v1 - value) * factor_value;
        fix_view();
        invalidate();
    }

    void curve_editor::set_mode_of_selected(key_mode mode) noexcept
    {
        bool any = false;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                if (m_selection->is_selected(c, k))
                {
                    m_curves[c].data->set_mode(k, mode);
                    any = true;
                }
            }
        }

        if (any)
            notify_change(true);
    }

    void curve_editor::remove_selected() noexcept
    {
        bool any = false;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            for (std::uint32_t k = m_curves[c].data->count(); k > 0; --k)
            {
                if (m_selection->is_selected(c, k - 1))
                {
                    m_curves[c].data->remove_key(k - 1);
                    m_selection->key_removed(c, k - 1);
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

    void curve_editor::select_all() noexcept
    {
        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
                m_selection->set(c, k, m_curves[c].visible);
        }

        notify_select();
    }

    void curve_editor::clear_selection() noexcept
    {
        m_selection->clear();
        notify_select();
    }

    void curve_editor::box_select() noexcept
    {
        const float left = static_cast<float>(std::min(m_press_x, m_x));
        const float right = static_cast<float>(std::max(m_press_x, m_x));
        const float top = static_cast<float>(std::min(m_press_y, m_y));
        const float bottom = static_cast<float>(std::max(m_press_y, m_y));
        *m_selection = m_base;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            if (!m_curves[c].visible)
                continue;

            for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
            {
                const point_f at = key_point(c, k);

                if (at.x >= left && at.x <= right && at.y >= top && at.y <= bottom)
                    m_selection->set(c, k, true);
            }
        }
    }

    void curve_editor::add_key_at(std::int32_t x, std::int32_t y) noexcept
    {
        const dock_rect p = plot();

        if (x < p.x || y < p.y || x >= p.x + p.width || y >= p.y + p.height)
            return;

        const float time = time_at(static_cast<float>(x));
        std::int32_t best = -1;
        float best_distance = 0.0f;

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            if (!m_curves[c].visible)
                continue;

            const float distance = std::fabs(y_of(m_curves[c].data->evaluate(time)) - static_cast<float>(y));

            if (best < 0 || distance < best_distance)
            {
                best = static_cast<std::int32_t>(c);
                best_distance = distance;
            }
        }

        if (best < 0)
            return;

        // On the curve the key lands on it. Away from it, the key goes where the pointer is.
        curve& data = *m_curves[best].data;
        const float value = best_distance <= static_cast<float>(draw::px(14)) ? data.evaluate(time) : value_at(static_cast<float>(y));
        const std::uint32_t index = data.add_key(time, value);

        if (index == curve::k_none)
            return;

        m_selection->key_inserted(static_cast<std::uint32_t>(best), index);
        m_selection->clear();
        m_selection->set(static_cast<std::uint32_t>(best), index, true);
        notify_select();
        notify_change(true);
    }

    void curve_editor::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        m_press = {};
        m_press_x = x;
        m_press_y = y;
        m_x = x;
        m_y = y;
        m_changed = false;
        const dock_rect p = plot();

        // The ruler on top moves the time cursor, and with Ctrl pans.
        if (y < p.y && x >= p.x)
        {
            if (ctrl_down())
            {
                m_drag = drag_kind::pan_time;
            }
            else
            {
                m_drag = drag_kind::scrub;
                m_time = std::max(time_at(static_cast<float>(x)), 0.0f);

                if (m_time_fn != nullptr)
                    m_time_fn(m_time_user, *this, m_time);
            }

            invalidate();
            return;
        }

        if (x < p.x && y >= p.y)
        {
            m_drag = drag_kind::pan_value;
            return;
        }

        if (x < p.x || y < p.y)
            return;

        const hit target = hit_test(x, y);

        if (target.curve >= 0)
        {
            m_press = target;

            if (target.handle != 0)
            {
                m_drag = drag_kind::handle;
                return;
            }

            const auto c = static_cast<std::uint32_t>(target.curve);
            const auto k = static_cast<std::uint32_t>(target.key);
            const bool selected = m_selection->is_selected(c, k);

            if (ctrl_down() || shift_down())
            {
                m_selection->set(c, k, !selected);
                notify_select();

                if (selected)
                    return;
            }
            else if (!selected)
            {
                m_selection->clear();
                m_selection->set(c, k, true);
                notify_select();
            }

            for (std::uint32_t i = 0; i < m_count; ++i)
            {
                for (std::uint32_t j = 0; j < m_curves[i].data->count(); ++j)
                {
                    m_orig_time[i][j] = m_curves[i].data->key(j).time;
                    m_orig_value[i][j] = m_curves[i].data->key(j).value;
                }
            }

            m_drag = drag_kind::lift;
            invalidate();
            return;
        }

        // Empty space: a box to select with. Without Ctrl or Shift it lets go of what was selected.
        if (!ctrl_down() && !shift_down() && m_selection->count() > 0)
        {
            m_selection->clear();
            notify_select();
        }

        m_base = *m_selection;
        m_drag = drag_kind::box;
        invalidate();
    }

    void curve_editor::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        add_key_at(x, y);
        m_drag = drag_kind::none;
    }

    void curve_editor::apply_move(std::int32_t x, std::int32_t y) noexcept
    {
        const dock_rect p = plot();
        const float per_time = static_cast<float>(p.width) / (m_t1 - m_t0);
        const float per_value = static_cast<float>(p.height) / (m_v1 - m_v0);
        float dt = static_cast<float>(x - m_press_x) / per_time;
        float dv = -static_cast<float>(y - m_press_y) / per_value;

        // Shift holds the keys to the direction the pointer went furthest.
        if (shift_down())
        {
            if (std::abs(x - m_press_x) > std::abs(y - m_press_y))
                dv = 0.0f;
            else
                dt = 0.0f;
        }

        for (std::uint32_t c = 0; c < m_count; ++c)
        {
            curve& data = *m_curves[c].data;
            const std::uint32_t count = data.count();

            // Keys that move toward their neighbours go first, so none is held back by one that has not moved yet.
            for (std::uint32_t n = 0; n < count; ++n)
            {
                const std::uint32_t k = dt >= 0.0f ? count - 1 - n : n;

                if (m_selection->is_selected(c, k))
                    data.move_key(k, m_orig_time[c][k] + dt, m_orig_value[c][k] + dv);
            }
        }

        m_changed = true;
        notify_change(false);
    }

    void curve_editor::apply_handle(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_press.curve < 0)
            return;

        curve& data = *m_curves[m_press.curve].data;
        const auto k = static_cast<std::uint32_t>(m_press.key);
        const curve_key key = data.key(k);
        const float time = time_at(static_cast<float>(x));
        const float value = value_at(static_cast<float>(y));

        if (m_press.handle == 2)
        {
            const float dx = std::max(time - key.time, 1.0e-3f);
            data.set_slopes(k, key.in_slope, (value - key.value) / dx);
        }
        else
        {
            const float dx = std::max(key.time - time, 1.0e-3f);
            data.set_slopes(k, (key.value - value) / dx, key.out_slope);
        }

        m_changed = true;
        notify_change(false);
    }

    bool curve_editor::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        const dock_rect p = plot();
        const std::int32_t last_x = m_x;
        const std::int32_t last_y = m_y;
        m_x = x;
        m_y = y;

        switch (m_drag)
        {
        case drag_kind::scrub:
            m_time = std::max(time_at(static_cast<float>(x)), 0.0f);

            if (m_time_fn != nullptr)
                m_time_fn(m_time_user, *this, m_time);

            return true;

        case drag_kind::pan_time:
        {
            const float shift = static_cast<float>(x - last_x) / static_cast<float>(p.width) * (m_t1 - m_t0);
            m_t0 -= shift;
            m_t1 -= shift;
            fix_view();
            return true;
        }

        case drag_kind::pan_value:
        {
            const float shift = static_cast<float>(y - last_y) / static_cast<float>(p.height) * (m_v1 - m_v0);
            m_v0 += shift;
            m_v1 += shift;
            fix_view();
            return true;
        }

        case drag_kind::lift:
        {
            const std::int32_t reach = draw::px(k_move_threshold);

            if (std::abs(x - m_press_x) <= reach && std::abs(y - m_press_y) <= reach)
                return false;

            m_drag = drag_kind::move;
            apply_move(x, y);
            return true;
        }

        case drag_kind::move:
            apply_move(x, y);
            return true;

        case drag_kind::handle:
            apply_handle(x, y);
            return true;

        case drag_kind::box:
            box_select();
            notify_select();
            return true;

        default:
            return false;
        }
    }

    bool curve_editor::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const dock_rect p = plot();
        const hit now = x >= p.x && y >= p.y ? hit_test(x, y) : hit{};

        if (now.curve == m_hover.curve && now.key == m_hover.key && now.handle == m_hover.handle)
            return false;

        m_hover = now;
        return true;
    }

    void curve_editor::on_leave() noexcept
    {
        if (m_hover.curve >= 0)
        {
            m_hover = {};
            invalidate();
        }
    }

    void curve_editor::on_release() noexcept
    {
        if (m_drag == drag_kind::lift && m_press.curve >= 0 && m_selection->count() > 1 && !ctrl_down() && !shift_down())
        {
            // Let go on a selected key without moving: the selection narrows to that key, as a click does.
            m_selection->clear();
            m_selection->set(static_cast<std::uint32_t>(m_press.curve), static_cast<std::uint32_t>(m_press.key), true);
            notify_select();
        }

        if ((m_drag == drag_kind::move || m_drag == drag_kind::handle) && m_changed)
            notify_change(true);

        m_changed = false;
        m_drag = drag_kind::none;
        invalidate();
    }

    bool curve_editor::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        const float factor = std::pow(0.85f, static_cast<float>(notches));
        float time_factor = factor;
        float value_factor = factor;

        if (ctrl_down())
            value_factor = 1.0f;
        else if (shift_down())
            time_factor = 1.0f;

        const dock_rect p = plot();
        const float anchor_x = std::clamp(static_cast<float>(x), static_cast<float>(p.x), static_cast<float>(p.x + p.width));
        const float anchor_y = std::clamp(static_cast<float>(y), static_cast<float>(p.y), static_cast<float>(p.y + p.height));
        zoom(time_factor, value_factor, anchor_x, anchor_y);
        return true;
    }

    bool curve_editor::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)shift;

        if (alt)
            return false;

        switch (key)
        {
        case k_key_delete:
            remove_selected();
            return true;

        case k_key_f:
        case k_key_home:
            if (m_selection->count() > 0 && key == k_key_f)
                frame_selected();
            else
                frame_all();

            return true;

        case k_key_a:
            if (!ctrl)
                return false;

            select_all();
            return true;

        case k_key_s:
            if (ctrl)
                return false;

            set_mode_of_selected(key_mode::smooth);
            return true;

        case k_key_l:
            set_mode_of_selected(key_mode::linear);
            return true;

        case k_key_t:
            set_mode_of_selected(key_mode::stepped);
            return true;

        case k_key_b:
            set_mode_of_selected(key_mode::free);
            return true;

        default:
            return false;
        }
    }

    void curve_editor::paint(void* dc) const noexcept
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

        draw::gradient(dc, area(), 4, top, bottom, edge, false);

        const dock_rect box = inner();
        const dock_rect p = plot();
        draw::clip(dc, box);

        const std::uint32_t grid_major = blend_colour(bottom, white, 0.10f);
        const std::uint32_t grid_minor = blend_colour(bottom, white, 0.045f);
        const std::uint32_t grid_zero = blend_colour(bottom, white, 0.26f);
        const std::uint32_t ruler_face = blend_colour(colours.panel, black, 0.06f);
        const std::uint32_t ruler_ink = colours.text_dim;
        char text[32];

        // The grid, under everything: time lines across the plot, value lines along it.
        draw::clip(dc, p);
        const float time_step = nice_step((m_t1 - m_t0) * static_cast<float>(draw::px(k_grid_spacing)) / static_cast<float>(p.width));
        const float value_step = nice_step((m_v1 - m_v0) * static_cast<float>(draw::px(k_grid_spacing) * 7 / 10) / static_cast<float>(p.height));
        const float time_minor = time_step / 5.0f;
        const float value_minor = value_step / 5.0f;

        if (time_minor * static_cast<float>(p.width) / (m_t1 - m_t0) >= 9.0f)
        {
            const float first = std::ceil(m_t0 / time_minor);

            for (int i = 0; i < 400 && (first + static_cast<float>(i)) * time_minor <= m_t1; ++i)
                draw::fill(dc, {static_cast<std::int32_t>(x_of((first + static_cast<float>(i)) * time_minor)), p.y, 1, p.height}, grid_minor);
        }

        if (value_minor * static_cast<float>(p.height) / (m_v1 - m_v0) >= 9.0f)
        {
            const float first = std::ceil(m_v0 / value_minor);

            for (int i = 0; i < 400 && (first + static_cast<float>(i)) * value_minor <= m_v1; ++i)
                draw::fill(dc, {p.x, static_cast<std::int32_t>(y_of((first + static_cast<float>(i)) * value_minor)), p.width, 1}, grid_minor);
        }

        {
            const float first = std::ceil(m_t0 / time_step);

            for (int i = 0; i < 200 && (first + static_cast<float>(i)) * time_step <= m_t1; ++i)
            {
                const float t = (first + static_cast<float>(i)) * time_step;
                draw::fill(dc, {static_cast<std::int32_t>(x_of(t)), p.y, 1, p.height}, std::fabs(t) < time_step * 1.0e-3f ? grid_zero : grid_major);
            }
        }

        {
            const float first = std::ceil(m_v0 / value_step);

            for (int i = 0; i < 200 && (first + static_cast<float>(i)) * value_step <= m_v1; ++i)
            {
                const float v = (first + static_cast<float>(i)) * value_step;
                draw::fill(dc, {p.x, static_cast<std::int32_t>(y_of(v)), p.width, 1}, std::fabs(v) < value_step * 1.0e-3f ? grid_zero : grid_major);
            }
        }

        // The curves: a darker line a pixel lower under each, which lifts it off the grid.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::uint32_t c = 0; c < m_count; ++c)
            {
                if (!m_curves[c].visible || m_curves[c].data->count() == 0)
                    continue;

                const curve& data = *m_curves[c].data;
                const std::uint32_t colour = pass == 0 ? blend_colour(bottom, black, 0.5f) : m_curves[c].colour;
                const float width = pass == 0 ? 3.5f : 1.8f;
                const float lower = pass == 0 ? 1.5f : 0.0f;
                draw::point buffer[64];
                int used = 0;

                const auto flush = [&]() {
                    if (used >= 2)
                        draw::polyline(dc, buffer, static_cast<std::uint32_t>(used), colour, width);

                    used = 0;
                };

                const auto push = [&](float x, float y) {
                    y = std::clamp(y + lower, -30000.0f, 30000.0f);
                    x = std::clamp(x, -30000.0f, 30000.0f);

                    if (used == 64)
                    {
                        const draw::point carried = buffer[63];
                        flush();
                        buffer[0] = carried;
                        used = 1;
                    }

                    buffer[used++] = {x, y};
                };

                const float left = static_cast<float>(p.x) - 4.0f;
                const float right = static_cast<float>(p.x + p.width) + 4.0f;
                const float first_x = x_of(data.key(0).time);

                if (first_x > left)
                {
                    push(left, y_of(data.key(0).value));
                    push(first_x, y_of(data.key(0).value));
                }

                for (std::uint32_t i = 0; i + 1 < data.count(); ++i)
                {
                    const curve_key& a = data.key(i);
                    const curve_key& b = data.key(i + 1);
                    const float x0 = x_of(a.time);
                    const float x1 = x_of(b.time);

                    if (x1 < left || x0 > right)
                    {
                        flush();
                        continue;
                    }

                    if (used == 0)
                        push(x0, y_of(a.value));

                    if (a.mode == key_mode::stepped)
                    {
                        push(x1, y_of(a.value));
                        push(x1, y_of(b.value));
                    }
                    else if (a.mode == key_mode::linear)
                    {
                        push(x1, y_of(b.value));
                    }
                    else
                    {
                        const int samples = std::clamp(static_cast<int>((x1 - x0) / 3.0f), 2, 400);

                        for (int s = 1; s <= samples; ++s)
                        {
                            const float t = a.time + (b.time - a.time) * static_cast<float>(s) / static_cast<float>(samples);
                            push(x_of(t), y_of(data.evaluate_segment(i, t)));
                        }
                    }
                }

                const float last_x = x_of(data.key(data.count() - 1).time);

                if (last_x < right)
                {
                    if (used == 0)
                        push(last_x, y_of(data.key(data.count() - 1).value));

                    push(right, y_of(data.key(data.count() - 1).value));
                }

                flush();
            }
        }

        // The time cursor.
        if (m_time >= 0.0f)
        {
            const float x = x_of(m_time);

            if (x >= static_cast<float>(p.x) && x <= static_cast<float>(p.x + p.width))
                draw::fill(dc, {static_cast<std::int32_t>(x), p.y, 1, p.height}, colours.accent);
        }

        // The keys and, on selected ones, their handles. Selected keys are drawn last, so they lie over the others.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::uint32_t c = 0; c < m_count; ++c)
            {
                if (!m_curves[c].visible)
                    continue;

                for (std::uint32_t k = 0; k < m_curves[c].data->count(); ++k)
                {
                    const bool selected = m_selection->is_selected(c, k);

                    if ((pass == 1) != selected)
                        continue;

                    const point_f at = key_point(c, k);

                    if (at.x < static_cast<float>(p.x) - 20.0f || at.x > static_cast<float>(p.x + p.width) + 20.0f)
                        continue;

                    const bool hovered = hot() && m_hover.curve == static_cast<std::int32_t>(c) && m_hover.key == static_cast<std::int32_t>(k) && m_hover.handle == 0;
                    const float radius = static_cast<float>(draw::px(k_key_radius)) + (selected || hovered ? 1.0f : 0.0f);

                    if (selected)
                    {
                        for (int side = 1; side <= 2; ++side)
                        {
                            if (!has_handles(c, k, side == 2))
                                continue;

                            const point_f tip = handle_point(c, k, side == 2);
                            const draw::point line[2] = {{at.x, at.y}, {tip.x, tip.y}};
                            draw::polyline(dc, line, 2, blend_colour(m_curves[c].colour, bottom, 0.45f), 1.2f);
                            const bool grabbed = hot() && m_hover.curve == static_cast<std::int32_t>(c) && m_hover.key == static_cast<std::int32_t>(k) && m_hover.handle == side;
                            draw::disc(dc, tip.x, tip.y + 1.0f, 4.0f, blend_colour(bottom, black, 0.6f), blend_colour(bottom, black, 0.6f), 0.0f);
                            draw::disc(dc, tip.x, tip.y, 3.6f, grabbed ? white : m_curves[c].colour, blend_colour(m_curves[c].colour, black, 0.5f), 1.0f);
                        }
                    }

                    // A darker disc a little lower is the key's shadow.
                    draw::disc(dc, at.x, at.y + 1.6f, radius + 0.6f, blend_colour(bottom, black, 0.65f), blend_colour(bottom, black, 0.65f), 0.0f);
                    const std::uint32_t face = selected ? white : blend_colour(m_curves[c].colour, white, hovered ? 0.5f : 0.15f);
                    draw::disc(dc, at.x, at.y, radius, face, selected ? colours.accent : blend_colour(m_curves[c].colour, black, 0.55f), selected ? 2.0f : 1.2f);
                }
            }
        }

        // The box being dragged out.
        if (m_drag == drag_kind::box)
        {
            const dock_rect rect{std::min(m_press_x, m_x), std::min(m_press_y, m_y), std::abs(m_x - m_press_x), std::abs(m_y - m_press_y)};
            draw::outline(dc, rect, 0, colours.accent, 1.0f);
        }

        // What the colours stand for, in the corner.
        {
            std::int32_t x = p.x + draw::px(8);

            for (std::uint32_t c = 0; c < m_count; ++c)
            {
                if (m_curves[c].name == nullptr)
                    continue;

                const std::int32_t width = draw::text_width(dc, m_curves[c].name);
                draw::disc(dc, static_cast<float>(x) + 4.0f, static_cast<float>(p.y + draw::px(11)), 3.5f, m_curves[c].visible ? m_curves[c].colour : ruler_ink, blend_colour(bottom, black, 0.5f), 1.0f);
                draw::text(dc, {x + draw::px(12), p.y + draw::px(3), width + 4, draw::px(16)}, m_curves[c].name, m_curves[c].visible ? colours.text : ruler_ink);
                x += width + draw::px(26);
            }
        }

        draw::unclip(dc);

        // The rulers lie over the plot's edges, each a little raised from it.
        draw::gradient(dc, {box.x, box.y, box.width, draw::px(k_ruler_top)}, 0, blend_colour(ruler_face, white, 0.07f), ruler_face, blend_colour(bottom, black, 0.5f), false);
        draw::gradient(dc, {box.x, p.y, draw::px(k_ruler_left), box.height - draw::px(k_ruler_top)}, 0, blend_colour(ruler_face, white, 0.03f), ruler_face,
            blend_colour(bottom, black, 0.5f), false);

        {
            draw::clip(dc, {p.x, box.y, p.width, draw::px(k_ruler_top)});
            const float first = std::ceil(m_t0 / time_step);

            for (int i = 0; i < 200 && (first + static_cast<float>(i)) * time_step <= m_t1; ++i)
            {
                const float t = (first + static_cast<float>(i)) * time_step;
                const std::int32_t x = static_cast<std::int32_t>(x_of(t));
                draw::fill(dc, {x, box.y + draw::px(k_ruler_top) - draw::px(5), 1, draw::px(5)}, ruler_ink);
                format_number(text, sizeof(text), t, time_step);
                draw::text(dc, {x + draw::px(4), box.y + 1, draw::px(60), draw::px(k_ruler_top) - 2}, text, ruler_ink);
            }

            draw::unclip(dc);
        }

        {
            draw::clip(dc, {box.x, p.y, draw::px(k_ruler_left), p.height});
            const float first = std::ceil(m_v0 / value_step);

            for (int i = 0; i < 200 && (first + static_cast<float>(i)) * value_step <= m_v1; ++i)
            {
                const float v = (first + static_cast<float>(i)) * value_step;
                const std::int32_t y = static_cast<std::int32_t>(y_of(v));
                draw::fill(dc, {p.x - draw::px(5), y, draw::px(5), 1}, ruler_ink);
                format_number(text, sizeof(text), v, value_step);
                draw::text(dc, {box.x, y - draw::px(8), draw::px(k_ruler_left) - draw::px(8), draw::px(16)}, text, ruler_ink, draw::text_right);
            }

            draw::unclip(dc);
        }

        // The cursor's mark on the top ruler, a triangle pointing down at the line, made of rows of whole pixels so it lies exactly on it.
        if (m_time >= 0.0f)
        {
            const float line = x_of(m_time);

            if (line >= static_cast<float>(p.x) && line <= static_cast<float>(p.x + p.width))
            {
                const std::int32_t column = static_cast<std::int32_t>(line);
                const std::int32_t height = draw::px(9);
                const std::int32_t reach = draw::px(5);
                const std::int32_t bottom = box.y + draw::px(k_ruler_top);

                for (std::int32_t row = 0; row < height; ++row)
                {
                    const std::int32_t half = reach * (height - 1 - row) / std::max(height - 1, 1);
                    draw::fill(dc, {column - half, bottom - height + row, 2 * half + 1, 1}, colours.accent);
                }
            }
        }

        draw::unclip(dc);
    }
}
