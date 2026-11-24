#include <chrome/controls/data/graph_view.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace chrome
{
    namespace
    {
        // Sizes in graph units, which are pixels at 96 dpi and zoom 1.
        constexpr float k_header = 26.0f;
        constexpr float k_row = 22.0f;
        constexpr float k_foot = 8.0f;
        constexpr float k_state_height = 44.0f;
        constexpr float k_pin_radius = 5.0f;
        constexpr float k_corner = 6.0f;
        constexpr float k_min_zoom = 0.25f;
        constexpr float k_max_zoom = 2.5f;
        // Below this zoom the text is left out, since the font does not shrink with the nodes.
        constexpr float k_text_zoom = 0.7f;
        constexpr float k_grid = 24.0f;
        constexpr std::int32_t k_move_threshold = 3;

        constexpr std::uint32_t k_key_delete = 0x2E;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_a = 0x41;
        constexpr std::uint32_t k_key_d = 0x44;
        constexpr std::uint32_t k_key_f = 0x46;

        // The ids of the rows of the menus.
        constexpr std::int32_t k_menu_duplicate = 1;
        constexpr std::int32_t k_menu_delete = 2;
        constexpr std::int32_t k_menu_disconnect = 3;
        constexpr std::int32_t k_menu_downstream = 4;
        constexpr std::int32_t k_menu_upstream = 5;
        constexpr std::int32_t k_menu_frame_selected = 6;
        constexpr std::int32_t k_menu_disconnect_pin = 7;
        constexpr std::int32_t k_menu_knob = 8;
        constexpr std::int32_t k_menu_clear_label = 9;
        constexpr std::int32_t k_menu_delete_link = 10;
        constexpr std::int32_t k_menu_select_source = 11;
        constexpr std::int32_t k_menu_select_target = 12;
        constexpr std::int32_t k_menu_new = 13;
        constexpr std::int32_t k_menu_select_all = 14;
        constexpr std::int32_t k_menu_arrange = 15;
        constexpr std::int32_t k_menu_frame_all = 16;

        constexpr std::uint32_t k_good = 0x3FA66B;
        constexpr std::uint32_t k_bad = 0xE5534B;

        [[nodiscard]] std::int32_t round_to_int(float value) noexcept
        {
            return static_cast<std::int32_t>(std::floor(value + 0.5f));
        }

        [[nodiscard]] bool inside(const dock_rect& box, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= box.x && y >= box.y && x < box.x + box.width && y < box.y + box.height;
        }

        // The distance from a point to a segment, squared.
        [[nodiscard]] float segment_distance(float px, float py, float ax, float ay, float bx, float by) noexcept
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float length = dx * dx + dy * dy;
            float t = length > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / length : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float cx = ax + dx * t - px;
            const float cy = ay + dy * t - py;
            return cx * cx + cy * cy;
        }
    }

    graph_view& graph_view::set_graph(graph* data) noexcept
    {
        m_graph = data;
        m_drag = drag_kind::none;
        m_hover = {};
        m_frame_pending = true;
        m_frame_selected = false;
        invalidate();
        return *this;
    }

    graph_view& graph_view::frame_all() noexcept
    {
        m_frame_pending = true;
        m_auto_frame = true;
        m_frame_selected = false;
        invalidate();
        return *this;
    }

    graph_view& graph_view::frame_selected() noexcept
    {
        m_frame_pending = true;
        m_auto_frame = true;
        m_frame_selected = true;
        invalidate();
        return *this;
    }

    control_size graph_view::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size graph_view::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        return {m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(240)), draw::px(m_height)};
    }

    dock_rect graph_view::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    float graph_view::scale() const noexcept
    {
        return m_zoom * static_cast<float>(draw::px(100)) / 100.0f;
    }

    float graph_view::sx(float graph_x) const noexcept
    {
        return static_cast<float>(inner().x) + (graph_x - m_pan_x) * scale();
    }

    float graph_view::sy(float graph_y) const noexcept
    {
        return static_cast<float>(inner().y) + (graph_y - m_pan_y) * scale();
    }

    float graph_view::gx(float screen_x) const noexcept
    {
        return m_pan_x + (screen_x - static_cast<float>(inner().x)) / scale();
    }

    float graph_view::gy(float screen_y) const noexcept
    {
        return m_pan_y + (screen_y - static_cast<float>(inner().y)) / scale();
    }

    float graph_view::centre_x() const noexcept
    {
        const dock_rect box = inner();
        return gx(static_cast<float>(box.x + box.width / 2));
    }

    float graph_view::centre_y() const noexcept
    {
        const dock_rect box = inner();
        return gy(static_cast<float>(box.y + box.height / 2));
    }

    graph_view::row_plan graph_view::plan(const graph_node& node) noexcept
    {
        row_plan result{};
        std::uint32_t outputs[graph_node_desc::k_max_pins];
        std::uint32_t output_count = 0;

        for (std::uint32_t i = 0; i < node.pin_count; ++i)
        {
            if (node.pins[i].dir == pin_dir::output)
                outputs[output_count++] = i;
        }

        std::uint32_t next = 0;

        // Each input takes a row, and the next output sits across from it, unless the input has a bar that needs the width.
        for (std::uint32_t i = 0; i < node.pin_count; ++i)
        {
            if (node.pins[i].dir != pin_dir::input)
                continue;

            const auto row = static_cast<std::uint8_t>(result.rows++);
            result.row[i] = row;

            if (!node.pins[i].has_value && next < output_count)
                result.row[outputs[next++]] = row;
        }

        while (next < output_count)
            result.row[outputs[next++]] = static_cast<std::uint8_t>(result.rows++);

        return result;
    }

    float graph_view::node_height(const graph_node& node) const noexcept
    {
        if (m_graph->style() == graph_style::states)
            return k_state_height;

        return k_header + static_cast<float>(plan(node).rows) * k_row + k_foot;
    }

    graph_view::point_f graph_view::pin_at(const graph_node& node, std::uint32_t pin) const noexcept
    {
        // A state has its ports in the middle of its sides.
        if (m_graph->style() == graph_style::states)
            return {node.pins[pin].dir == pin_dir::input ? node.x : node.x + node.width, node.y + k_state_height * 0.5f};

        const row_plan rows = plan(node);
        const float y = node.y + k_header + (static_cast<float>(rows.row[pin]) + 0.5f) * k_row;
        return {node.pins[pin].dir == pin_dir::input ? node.x : node.x + node.width, y};
    }

    dock_rect graph_view::bar_of(const graph_node& node, std::uint32_t pin) const noexcept
    {
        const graph_pin& data = node.pins[pin];

        if (!data.has_value || data.dir != pin_dir::input || m_graph->style() != graph_style::flow || m_graph->link_into(node.id, pin) != nullptr)
            return {};

        const point_f centre = pin_at(node, pin);
        const float s = scale();
        const float left = sx(node.x + node.width * 0.42f);
        const float right = sx(node.x + node.width - 10.0f);
        const float height = 14.0f * s;
        return {round_to_int(left), round_to_int(sy(centre.y) - height * 0.5f), std::max(round_to_int(right - left), 1), std::max(round_to_int(height), 1)};
    }

    bool graph_view::link_ends(const graph_link& link, point_f& from, point_f& to) const noexcept
    {
        const graph_node* a = m_graph->find_node(link.from_node);
        const graph_node* b = m_graph->find_node(link.to_node);

        if (a == nullptr || b == nullptr || link.from_pin >= a->pin_count || link.to_pin >= b->pin_count)
            return false;

        if (m_graph->style() == graph_style::states)
        {
            // An arrow from the edge of one node to the edge of the other, along the line between their middles. A link that has one
            // running the other way is moved to its right, so the two lie side by side.
            const float s = scale();
            const point_f ca = {sx(a->x + a->width * 0.5f), sy(a->y + k_state_height * 0.5f)};
            const point_f cb = {sx(b->x + b->width * 0.5f), sy(b->y + k_state_height * 0.5f)};
            float dx = cb.x - ca.x;
            float dy = cb.y - ca.y;
            float length = std::hypot(dx, dy);

            if (length < 1.0f)
            {
                dx = 1.0f;
                dy = 0.0f;
                length = 1.0f;
            }

            const float ux = dx / length;
            const float uy = dy / length;
            const auto edge = [&](const point_f& centre, float half_width, float half_height, float vx, float vy) {
                const float tx = std::fabs(vx) > 1.0e-4f ? half_width / std::fabs(vx) : 1.0e9f;
                const float ty = std::fabs(vy) > 1.0e-4f ? half_height / std::fabs(vy) : 1.0e9f;
                const float t = std::min(tx, ty);
                return point_f{centre.x + vx * t, centre.y + vy * t};
            };

            from = edge(ca, a->width * 0.5f * s, k_state_height * 0.5f * s, ux, uy);
            to = edge(cb, b->width * 0.5f * s, k_state_height * 0.5f * s, -ux, -uy);

            bool reverse = false;

            for (std::uint32_t i = 0; i < m_graph->link_count() && !reverse; ++i)
                reverse = m_graph->link_at(i).from_node == link.to_node && m_graph->link_at(i).to_node == link.from_node;

            if (reverse)
            {
                const float lane = 9.0f * s;
                from = {from.x + uy * lane, from.y - ux * lane};
                to = {to.x + uy * lane, to.y - ux * lane};
            }

            return true;
        }

        const point_f p = pin_at(*a, link.from_pin);
        const point_f q = pin_at(*b, link.to_pin);
        from = {sx(p.x), sy(p.y)};
        to = {sx(q.x), sy(q.y)};
        return true;
    }

    float graph_view::tangent_of(const point_f& from, const point_f& to) const noexcept
    {
        return std::max(std::fabs(to.x - from.x) * 0.5f, 56.0f * scale());
    }

    std::uint32_t graph_view::link_curve(const point_f& from, const point_f& to, point_f* out, std::uint32_t capacity) const noexcept
    {
        if (m_graph->style() == graph_style::states)
        {
            out[0] = from;
            out[1] = to;
            return 2;
        }

        const float t = tangent_of(from, to);
        const float chord = std::hypot(to.x - from.x, to.y - from.y) + t;
        const auto segments = static_cast<std::uint32_t>(std::clamp(static_cast<int>(chord / 9.0f), 10, static_cast<int>(capacity) - 1));
        const float x1 = from.x + t;
        const float x2 = to.x - t;

        for (std::uint32_t i = 0; i <= segments; ++i)
        {
            const float u = static_cast<float>(i) / static_cast<float>(segments);
            const float v = 1.0f - u;
            const float b0 = v * v * v;
            const float b1 = 3.0f * v * v * u;
            const float b2 = 3.0f * v * u * u;
            const float b3 = u * u * u;
            out[i] = {b0 * from.x + b1 * x1 + b2 * x2 + b3 * to.x, (b0 + b1) * from.y + (b2 + b3) * to.y};
        }

        return segments + 1;
    }

    graph_view::hit graph_view::hit_test(std::int32_t x, std::int32_t y) const noexcept
    {
        hit result;

        if (m_graph == nullptr)
            return result;

        const float s = scale();
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float reach = std::max(8.0f * s, static_cast<float>(draw::px(7)));

        for (std::uint32_t n = m_graph->node_count(); n > 0; --n)
        {
            const graph_node& node = m_graph->node_at(n - 1);
            const float left = sx(node.x);
            const float top = sy(node.y);
            const float right = sx(node.x + node.width);
            const float bottom = sy(node.y + node_height(node));

            if (fx < left - reach || fx > right + reach || fy < top || fy > bottom)
                continue;

            float best = reach * reach;

            for (std::uint32_t p = 0; p < node.pin_count; ++p)
            {
                const point_f at = pin_at(node, p);
                const float dx = sx(at.x) - fx;
                const float dy = sy(at.y) - fy;
                const float distance = dx * dx + dy * dy;

                if (distance <= best)
                {
                    best = distance;
                    result = {hit_kind::pin, node.id, p, 0};
                }
            }

            if (result.kind == hit_kind::pin)
                return result;

            for (std::uint32_t p = 0; p < node.pin_count; ++p)
            {
                const dock_rect bar = bar_of(node, p);

                if (bar.width > 0 && inside(bar, x, y))
                    return {hit_kind::value, node.id, p, 0};
            }

            if (fx >= left && fx <= right)
                return {hit_kind::node, node.id, 0, 0};
        }

        const float line_reach = static_cast<float>(draw::px(6));

        for (std::uint32_t l = m_graph->link_count(); l > 0; --l)
        {
            const graph_link& link = m_graph->link_at(l - 1);
            point_f a;
            point_f b;

            if (!link_ends(link, a, b))
                continue;

            const float t = tangent_of(a, b);

            if (fx < std::min(a.x, b.x) - t - line_reach || fx > std::max(a.x, b.x) + t + line_reach || fy < std::min(a.y, b.y) - line_reach ||
                fy > std::max(a.y, b.y) + line_reach)
                continue;

            if (link.knob)
            {
                const float dx = (a.x + b.x) * 0.5f - fx;
                const float dy = (a.y + b.y) * 0.5f - fy;
                const float knob_reach = 8.0f * std::max(m_zoom, 0.8f) * static_cast<float>(draw::px(100)) / 100.0f;

                if (dx * dx + dy * dy <= knob_reach * knob_reach)
                    return {hit_kind::link, 0, 0, link.id};
            }

            point_f curve[64];
            const std::uint32_t count = link_curve(a, b, curve, 64);

            for (std::uint32_t i = 0; i + 1 < count; ++i)
            {
                if (segment_distance(fx, fy, curve[i].x, curve[i].y, curve[i + 1].x, curve[i + 1].y) <= line_reach * line_reach)
                    return {hit_kind::link, 0, 0, link.id};
            }
        }

        return result;
    }

    void graph_view::notify_change(bool final) noexcept
    {
        invalidate();

        if (m_graph != nullptr)
            m_graph->touch();

        if (m_change != nullptr)
            m_change(m_change_user, *this, final);
    }

    void graph_view::notify_select() noexcept
    {
        invalidate();

        if (m_select != nullptr)
            m_select(m_select_user, *this);
    }

    void graph_view::frame_now(bool selected_only) const noexcept
    {
        m_frame_pending = false;

        if (m_graph == nullptr)
            return;

        const bool only = selected_only && m_graph->selected_nodes() > 0;
        float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
        bool any = false;

        for (std::uint32_t i = 0; i < m_graph->node_count(); ++i)
        {
            const graph_node& node = m_graph->node_at(i);

            if (only && !node.selected)
                continue;

            const float bottom = node.y + node_height(node);

            if (!any)
            {
                x0 = node.x;
                y0 = node.y;
                x1 = node.x + node.width;
                y1 = bottom;
                any = true;
            }

            x0 = std::min(x0, node.x);
            y0 = std::min(y0, node.y);
            x1 = std::max(x1, node.x + node.width);
            y1 = std::max(y1, bottom);
        }

        const dock_rect box = inner();
        const float dpi = static_cast<float>(draw::px(100)) / 100.0f;

        if (!any)
        {
            m_zoom = 1.0f;
            m_pan_x = -static_cast<float>(box.width) * 0.5f / dpi;
            m_pan_y = -static_cast<float>(box.height) * 0.5f / dpi;
            return;
        }

        const float pad = 40.0f;
        const float fit_x = static_cast<float>(box.width) / dpi / (x1 - x0 + 2.0f * pad);
        const float fit_y = static_cast<float>(box.height) / dpi / (y1 - y0 + 2.0f * pad);
        m_zoom = std::clamp(std::min(fit_x, fit_y), k_min_zoom, 1.25f);
        const float s = m_zoom * dpi;
        m_pan_x = (x0 + x1) * 0.5f - static_cast<float>(box.width) * 0.5f / s;
        m_pan_y = (y0 + y1) * 0.5f - static_cast<float>(box.height) * 0.5f / s;
    }

    void graph_view::zoom_at(float factor, float anchor_x, float anchor_y) noexcept
    {
        const float before_x = gx(anchor_x);
        const float before_y = gy(anchor_y);
        m_auto_frame = false;
        m_zoom = std::clamp(m_zoom * factor, k_min_zoom, k_max_zoom);
        const dock_rect box = inner();
        m_pan_x = before_x - (anchor_x - static_cast<float>(box.x)) / scale();
        m_pan_y = before_y - (anchor_y - static_cast<float>(box.y)) / scale();
        invalidate();
    }

    void graph_view::remove_selected() noexcept
    {
        if (m_graph == nullptr || m_graph->remove_selected() == 0)
            return;

        m_hover = {};
        notify_select();
        notify_change(true);
    }

    void graph_view::duplicate_selected() noexcept
    {
        if (m_graph == nullptr || m_graph->duplicate_selected(24.0f, 24.0f) == 0)
            return;

        notify_select();
        notify_change(true);
    }

    void graph_view::select_all() noexcept
    {
        if (m_graph == nullptr)
            return;

        m_graph->select_all();
        notify_select();
    }

    void graph_view::arrange() noexcept
    {
        if (m_graph == nullptr || m_graph->node_count() == 0)
            return;

        constexpr std::uint32_t k_nodes = graph::k_max_nodes;
        constexpr std::uint32_t k_links = graph::k_max_links;
        const std::uint32_t count = m_graph->node_count();
        const bool states = m_graph->style() == graph_style::states;
        const float gap_x = states ? 150.0f : 72.0f;
        const float gap_y = states ? 84.0f : 24.0f;

        std::uint32_t from[k_links];
        std::uint32_t to[k_links];
        std::uint32_t links = 0;
        std::uint32_t incoming[k_nodes]{};

        for (std::uint32_t i = 0; i < m_graph->link_count(); ++i)
        {
            const std::int32_t a = m_graph->index_of(m_graph->link_at(i).from_node);
            const std::int32_t b = m_graph->index_of(m_graph->link_at(i).to_node);

            if (a >= 0 && b >= 0 && a != b)
            {
                from[links] = static_cast<std::uint32_t>(a);
                to[links] = static_cast<std::uint32_t>(b);
                ++incoming[b];
                ++links;
            }
        }

        // A depth first walk from the nodes nothing leads to finds the links that run back, and the order the nodes finish in.
        std::uint8_t state[k_nodes]{};
        bool back[k_links]{};
        std::uint32_t finished[k_nodes];
        std::uint32_t finished_count = 0;
        std::uint32_t stack[k_nodes];
        std::uint32_t scan[k_nodes];

        for (std::uint32_t pass = 0; pass < 2; ++pass)
        {
            for (std::uint32_t root = 0; root < count; ++root)
            {
                if (state[root] != 0 || (pass == 0 && incoming[root] != 0))
                    continue;

                std::uint32_t top = 0;
                stack[0] = root;
                scan[0] = 0;
                state[root] = 1;
                top = 1;

                while (top > 0)
                {
                    const std::uint32_t at = stack[top - 1];
                    bool descended = false;

                    while (scan[top - 1] < links)
                    {
                        const std::uint32_t l = scan[top - 1]++;

                        if (from[l] != at)
                            continue;

                        if (state[to[l]] == 1)
                        {
                            back[l] = true;
                        }
                        else if (state[to[l]] == 0)
                        {
                            state[to[l]] = 1;
                            stack[top] = to[l];
                            scan[top] = 0;
                            ++top;
                            descended = true;
                            break;
                        }
                    }

                    if (!descended)
                    {
                        state[at] = 2;
                        finished[finished_count++] = at;
                        --top;
                    }
                }
            }
        }

        // The columns: a node sits one column right of the furthest node that leads to it.
        std::uint32_t depth[k_nodes]{};
        std::uint32_t columns = 1;

        for (std::uint32_t k = finished_count; k > 0; --k)
        {
            const std::uint32_t at = finished[k - 1];

            for (std::uint32_t l = 0; l < links; ++l)
            {
                if (from[l] == at && !back[l])
                    depth[to[l]] = std::max(depth[to[l]], depth[at] + 1);
            }

            columns = std::max(columns, depth[at] + 1);
        }

        float centre_y[k_nodes]{};
        float x = 0.0f;

        for (std::uint32_t column = 0; column < columns; ++column)
        {
            std::uint32_t members[k_nodes];
            float key[k_nodes];
            std::uint32_t used = 0;
            float widest = 0.0f;

            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (depth[i] != column)
                    continue;

                // Where the nodes that lead here sit, so this one lands level with them; nodes with none keep their place.
                float sum = 0.0f;
                std::uint32_t found = 0;

                for (std::uint32_t l = 0; l < links; ++l)
                {
                    if (to[l] == i && !back[l] && depth[from[l]] < column)
                    {
                        sum += centre_y[from[l]];
                        ++found;
                    }
                }

                const graph_node& node = m_graph->node_at(i);
                const float own = node.y + node_height(node) * 0.5f;
                float value = found > 0 ? sum / static_cast<float>(found) : own;

                // The sort is by this value, and the nodes that tie keep the order they had.
                value += static_cast<float>(i) * 1.0e-3f;

                std::uint32_t at = used++;

                while (at > 0 && key[at - 1] > value)
                {
                    key[at] = key[at - 1];
                    members[at] = members[at - 1];
                    --at;
                }

                key[at] = value;
                members[at] = i;
                widest = std::max(widest, node.width);
            }

            float total = 0.0f;

            for (std::uint32_t m = 0; m < used; ++m)
                total += node_height(m_graph->node_at(members[m])) + (m > 0 ? gap_y : 0.0f);

            // Each column is centred on the middle of the nodes that feed it, or on the first column's middle, so the graph stays level.
            float level = 0.0f;
            std::uint32_t levelled = 0;

            for (std::uint32_t m = 0; m < used; ++m)
            {
                if (column > 0)
                {
                    level += key[m];
                    ++levelled;
                }
            }

            const float middle = levelled > 0 ? level / static_cast<float>(levelled) : 0.0f;
            float y = middle - total * 0.5f;

            for (std::uint32_t m = 0; m < used; ++m)
            {
                graph_node& node = m_graph->node_at(members[m]);
                const float height = node_height(node);
                node.x = m_snap > 0.0f ? std::floor(x / m_snap + 0.5f) * m_snap : x;
                node.y = m_snap > 0.0f ? std::floor(y / m_snap + 0.5f) * m_snap : y;
                centre_y[members[m]] = node.y + height * 0.5f;
                y += height + gap_y;
            }

            x += widest + gap_x;
        }

        frame_all();
        notify_change(true);
    }

    void graph_view::select_link(std::uint32_t id, bool add) noexcept
    {
        const graph_link* current = m_graph->find_link(id);
        const bool was = current != nullptr && current->selected;

        if (!add)
            m_graph->clear_selection();

        if (graph_link* link = m_graph->find_link(id))
            link->selected = add ? !was : true;

        notify_select();
    }

    void graph_view::nudge(float dx, float dy) noexcept
    {
        if (m_graph->selected_nodes() == 0)
        {
            m_auto_frame = false;
            m_pan_x -= dx;
            m_pan_y -= dy;
            invalidate();
            return;
        }

        for (std::uint32_t i = 0; i < m_graph->node_count(); ++i)
        {
            graph_node& node = m_graph->node_at(i);

            if (node.selected)
            {
                node.x += dx;
                node.y += dy;
            }
        }

        notify_change(true);
    }

    void graph_view::box_select() noexcept
    {
        const float left = gx(static_cast<float>(std::min(m_press_x, m_x)));
        const float right = gx(static_cast<float>(std::max(m_press_x, m_x)));
        const float top = gy(static_cast<float>(std::min(m_press_y, m_y)));
        const float bottom = gy(static_cast<float>(std::max(m_press_y, m_y)));

        for (std::uint32_t i = 0; i < m_graph->node_count(); ++i)
        {
            graph_node& node = m_graph->node_at(i);
            bool selected = false;

            for (std::uint32_t b = 0; b < m_base_count; ++b)
                selected = selected || m_base_nodes[b] == node.id;

            const bool overlaps = node.x <= right && node.x + node.width >= left && node.y <= bottom && node.y + node_height(node) >= top;
            node.selected = selected || overlaps;
        }
    }

    void graph_view::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        m_press = {};
        m_press_x = x;
        m_press_y = y;
        m_x = x;
        m_y = y;
        m_changed = false;
        m_detach = 0;

        if (m_graph == nullptr)
            return;

        // Space or Alt held pans, wherever the press is.
        if ((GetKeyState(VK_SPACE) & 0x8000) != 0 || (GetKeyState(VK_MENU) & 0x8000) != 0)
        {
            m_drag = drag_kind::pan;
            return;
        }

        const hit target = hit_test(x, y);
        m_press = target;

        switch (target.kind)
        {
        case hit_kind::pin:
        {
            const graph_node* node = m_graph->find_node(target.node);
            const graph_pin& pin = node->pins[target.pin];

            // A press on a linked input takes the link up by its output end, once the pointer has moved.
            if (pin.dir == pin_dir::input && !pin.multi)
            {
                if (const graph_link* existing = m_graph->link_into(target.node, target.pin))
                {
                    m_detach = existing->id;
                    m_link_node = existing->from_node;
                    m_link_pin = existing->from_pin;
                    m_link_from_output = true;
                    m_drag = drag_kind::detach;
                    return;
                }
            }

            m_link_node = target.node;
            m_link_pin = target.pin;
            m_link_from_output = pin.dir == pin_dir::output;
            m_drag = drag_kind::link;
            invalidate();
            return;
        }

        case hit_kind::value:
            m_value_origin = m_graph->find_node(target.node)->pins[target.pin].value;
            m_drag = drag_kind::value;
            return;

        case hit_kind::link:
            select_link(target.link, ctrl_down() || shift_down());
            return;

        case hit_kind::node:
        {
            graph_node* node = m_graph->find_node(target.node);
            const bool selected = node->selected;

            if (ctrl_down() || shift_down())
            {
                node->selected = !selected;
                notify_select();

                if (selected)
                    return;
            }
            else if (!selected)
            {
                m_graph->clear_selection();
                node->selected = true;
                notify_select();
            }

            m_graph->raise(target.node);
            m_orig_count = 0;

            for (std::uint32_t i = 0; i < m_graph->node_count(); ++i)
            {
                const graph_node& item = m_graph->node_at(i);

                if (item.selected)
                {
                    m_orig_id[m_orig_count] = item.id;
                    m_orig_x[m_orig_count] = item.x;
                    m_orig_y[m_orig_count] = item.y;
                    ++m_orig_count;
                }
            }

            m_drag = drag_kind::lift;
            invalidate();
            return;
        }

        case hit_kind::none:
            break;
        }

        // Empty space: a box to select with. Without Ctrl or Shift it lets go of what was selected.
        if (!ctrl_down() && !shift_down() && (m_graph->selected_nodes() > 0 || m_graph->selected_links() > 0))
        {
            m_graph->clear_selection();
            notify_select();
        }

        m_base_count = 0;

        for (std::uint32_t i = 0; i < m_graph->node_count(); ++i)
        {
            if (m_graph->node_at(i).selected)
                m_base_nodes[m_base_count++] = m_graph->node_at(i).id;
        }

        m_drag = drag_kind::box;
        invalidate();
    }

    void graph_view::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_graph == nullptr)
            return;

        const hit target = hit_test(x, y);

        switch (target.kind)
        {
        case hit_kind::link:
        {
            const graph_link* link = m_graph->find_link(target.link);

            if (link != nullptr)
            {
                m_graph->set_link_knob(target.link, !link->knob);
                notify_change(true);
            }

            m_drag = drag_kind::none;
            return;
        }

        case hit_kind::value:
        {
            graph_pin& pin = m_graph->find_node(target.node)->pins[target.pin];
            pin.value = pin.reset_value;
            m_drag = drag_kind::none;
            notify_change(true);
            return;
        }

        case hit_kind::none:
            m_drag = drag_kind::none;

            if (m_create != nullptr)
                m_create(m_create_user, *this, gx(static_cast<float>(x)), gy(static_cast<float>(y)), 0, 0);

            return;

        default:
            on_press(x, y);
            return;
        }
    }

    bool graph_view::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_graph == nullptr)
            return false;

        const std::int32_t last_x = m_x;
        const std::int32_t last_y = m_y;
        m_x = x;
        m_y = y;
        const std::int32_t reach = draw::px(k_move_threshold);

        switch (m_drag)
        {
        case drag_kind::pan:
            m_auto_frame = false;
            m_pan_x -= static_cast<float>(x - last_x) / scale();
            m_pan_y -= static_cast<float>(y - last_y) / scale();
            return true;

        case drag_kind::lift:
            if (std::abs(x - m_press_x) <= reach && std::abs(y - m_press_y) <= reach)
                return false;

            m_drag = drag_kind::move;
            [[fallthrough]];

        case drag_kind::move:
        {
            float dx = static_cast<float>(x - m_press_x) / scale();
            float dy = static_cast<float>(y - m_press_y) / scale();

            // The node that was pressed lands on the grid and the rest keep their distance from it.
            if (m_snap > 0.0f)
            {
                for (std::uint32_t i = 0; i < m_orig_count; ++i)
                {
                    if (m_orig_id[i] == m_press.node)
                    {
                        dx = std::floor((m_orig_x[i] + dx) / m_snap + 0.5f) * m_snap - m_orig_x[i];
                        dy = std::floor((m_orig_y[i] + dy) / m_snap + 0.5f) * m_snap - m_orig_y[i];
                        break;
                    }
                }
            }

            for (std::uint32_t i = 0; i < m_orig_count; ++i)
            {
                if (graph_node* node = m_graph->find_node(m_orig_id[i]))
                {
                    node->x = m_orig_x[i] + dx;
                    node->y = m_orig_y[i] + dy;
                }
            }

            m_changed = true;
            notify_change(false);
            return true;
        }

        case drag_kind::detach:
            if (std::abs(x - m_press_x) <= reach && std::abs(y - m_press_y) <= reach)
                return false;

            m_graph->remove_link(m_detach);
            m_detach = 0;
            m_drag = drag_kind::link;
            m_changed = true;
            m_hover = hit_test(x, y);
            notify_change(false);
            return true;

        case drag_kind::link:
            m_hover = hit_test(x, y);
            return true;

        case drag_kind::value:
        {
            graph_node* node = m_graph->find_node(m_press.node);

            if (node == nullptr)
                return false;

            graph_pin& pin = node->pins[m_press.pin];
            const dock_rect bar = bar_of(*node, m_press.pin);
            const float fine = shift_down() ? 0.1f : 1.0f;
            const float span = (pin.high - pin.low) * fine;
            pin.value = std::clamp(m_value_origin + static_cast<float>(x - m_press_x) / static_cast<float>(std::max(bar.width, 1)) * span, pin.low, pin.high);
            m_changed = true;
            notify_change(false);
            return true;
        }

        case drag_kind::box:
            box_select();
            notify_select();
            return true;

        case drag_kind::none:
            return false;
        }

        return false;
    }

    bool graph_view::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const hit now = hit_test(x, y);

        if (now.kind == m_hover.kind && now.node == m_hover.node && now.pin == m_hover.pin && now.link == m_hover.link)
            return false;

        m_hover = now;
        return true;
    }

    void graph_view::on_leave() noexcept
    {
        if (m_hover.kind != hit_kind::none)
        {
            m_hover = {};
            invalidate();
        }
    }

    void graph_view::on_release() noexcept
    {
        const drag_kind kind = m_drag;
        m_drag = drag_kind::none;

        if (m_graph == nullptr)
        {
            invalidate();
            return;
        }

        switch (kind)
        {
        case drag_kind::lift:
            // Let go on a selected node without moving: the selection narrows to that node, as a click does.
            if (m_graph->selected_nodes() > 1 && !ctrl_down() && !shift_down())
            {
                m_graph->clear_selection();

                if (graph_node* node = m_graph->find_node(m_press.node))
                    node->selected = true;

                notify_select();
            }

            break;

        case drag_kind::move:
        case drag_kind::value:
            if (m_changed)
                notify_change(true);

            break;

        case drag_kind::link:
        {
            const hit target = hit_test(m_x, m_y);

            if (target.kind == hit_kind::pin)
            {
                if (m_graph->add_link(m_link_node, m_link_pin, target.node, target.pin) != 0)
                    m_changed = true;
            }
            else if (target.kind == hit_kind::node && m_graph->style() == graph_style::states)
            {
                // A state takes a link let go anywhere on it, on its first port that accepts it.
                const graph_node* node = m_graph->find_node(target.node);

                for (std::uint32_t p = 0; node != nullptr && p < node->pin_count; ++p)
                {
                    if (m_graph->add_link(m_link_node, m_link_pin, target.node, p) != 0)
                    {
                        m_changed = true;
                        break;
                    }
                }
            }
            else if (target.kind == hit_kind::none && m_create != nullptr)
            {
                m_create(m_create_user, *this, gx(static_cast<float>(m_x)), gy(static_cast<float>(m_y)), m_link_node, m_link_pin);
            }

            if (m_changed)
                notify_change(true);

            break;
        }

        default:
            break;
        }

        m_changed = false;
        m_hover = hit_test(m_x, m_y);
        invalidate();
    }

    void graph_view::select_reach(bool downstream) noexcept
    {
        bool changed = true;

        for (std::uint32_t pass = 0; changed && pass <= graph::k_max_nodes; ++pass)
        {
            changed = false;

            for (std::uint32_t i = 0; i < m_graph->link_count(); ++i)
            {
                const graph_link& link = m_graph->link_at(i);
                graph_node* a = m_graph->find_node(downstream ? link.from_node : link.to_node);
                graph_node* b = m_graph->find_node(downstream ? link.to_node : link.from_node);

                if (a != nullptr && b != nullptr && a->selected && !b->selected)
                {
                    b->selected = true;
                    changed = true;
                }
            }
        }

        notify_select();
    }

    void graph_view::disconnect_selected() noexcept
    {
        bool any = false;

        for (std::uint32_t i = m_graph->link_count(); i > 0; --i)
        {
            const graph_link link = m_graph->link_at(i - 1);
            const graph_node* a = m_graph->find_node(link.from_node);
            const graph_node* b = m_graph->find_node(link.to_node);

            if ((a != nullptr && a->selected) || (b != nullptr && b->selected))
                any = m_graph->remove_link(link.id) || any;
        }

        if (any)
            notify_change(true);
    }

    void graph_view::on_right_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_right_x = x;
        m_right_y = y;
        m_press_x = x;
        m_press_y = y;
        m_right_panned = false;
        m_right_hit = {};

        if (m_graph == nullptr)
            return;

        m_right_hit = hit_test(x, y);

        // What is pressed becomes the selection, unless it already is part of it, so the menu acts on what the user sees chosen.
        switch (m_right_hit.kind)
        {
        case hit_kind::node:
        case hit_kind::pin:
        case hit_kind::value:
        {
            graph_node* node = m_graph->find_node(m_right_hit.node);

            if (node != nullptr && !node->selected)
            {
                m_graph->clear_selection();
                node->selected = true;
                notify_select();
            }

            break;
        }

        case hit_kind::link:
        {
            const graph_link* link = m_graph->find_link(m_right_hit.link);

            if (link != nullptr && !link->selected)
                select_link(m_right_hit.link, false);

            break;
        }

        case hit_kind::none:
            break;
        }

        invalidate();
    }

    bool graph_view::on_right_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_right_panned)
        {
            const std::int32_t reach = draw::px(k_move_threshold + 1);

            if (std::abs(x - m_press_x) <= reach && std::abs(y - m_press_y) <= reach)
                return false;

            m_right_panned = true;
        }

        m_auto_frame = false;
        m_pan_x -= static_cast<float>(x - m_right_x) / scale();
        m_pan_y -= static_cast<float>(y - m_right_y) / scale();
        m_right_x = x;
        m_right_y = y;
        return true;
    }

    void graph_view::on_right_release() noexcept
    {
        invalidate();
    }

    context_menu* graph_view::context_menu_at(std::int32_t x, std::int32_t y) noexcept
    {
        // A drag that panned the view is not a click, so no menu follows it.
        if (m_graph == nullptr || m_right_panned)
            return nullptr;

        m_menu.clear_items();
        m_menu.set_on_select(&graph_view::menu_chosen_static, this);
        m_menu_x = gx(static_cast<float>(x));
        m_menu_y = gy(static_cast<float>(y));

        switch (m_right_hit.kind)
        {
        case hit_kind::node:
        case hit_kind::pin:
        case hit_kind::value:
        {
            if (m_right_hit.kind == hit_kind::pin)
            {
                m_menu.add_item("Disconnect pin", k_menu_disconnect_pin).set_enabled(m_graph->pin_linked(m_right_hit.node, m_right_hit.pin));
                m_menu.add_separator();
            }

            bool linked = false;

            for (std::uint32_t i = 0; i < m_graph->link_count() && !linked; ++i)
            {
                const graph_node* a = m_graph->find_node(m_graph->link_at(i).from_node);
                const graph_node* b = m_graph->find_node(m_graph->link_at(i).to_node);
                linked = (a != nullptr && a->selected) || (b != nullptr && b->selected);
            }

            m_menu.add_item("Duplicate", k_menu_duplicate).set_shortcut("Ctrl+D");
            m_menu.add_item("Delete", k_menu_delete).set_shortcut("Del");
            m_menu.add_separator();
            m_menu.add_item("Disconnect all links", k_menu_disconnect).set_enabled(linked);
            m_menu.add_item("Select downstream", k_menu_downstream);
            m_menu.add_item("Select upstream", k_menu_upstream);
            m_menu.add_separator();
            m_menu.add_item("Frame selected", k_menu_frame_selected).set_shortcut("F");
            break;
        }

        case hit_kind::link:
        {
            const graph_link* link = m_graph->find_link(m_right_hit.link);

            if (link == nullptr)
                return nullptr;

            m_menu.add_check("Knob", k_menu_knob).set_checked(link->knob);
            m_menu.add_item("Clear label", k_menu_clear_label).set_enabled(link->label[0] != 0);
            m_menu.add_separator();
            m_menu.add_item("Delete link", k_menu_delete_link).set_shortcut("Del");
            m_menu.add_separator();
            m_menu.add_item("Select source node", k_menu_select_source);
            m_menu.add_item("Select target node", k_menu_select_target);
            break;
        }

        case hit_kind::none:
            m_menu.add_item("New node here", k_menu_new).set_enabled(m_create != nullptr);
            m_menu.add_separator();
            m_menu.add_item("Select all", k_menu_select_all).set_shortcut("Ctrl+A");
            m_menu.add_item("Arrange", k_menu_arrange);
            m_menu.add_item("Frame all", k_menu_frame_all).set_shortcut("F");
            break;
        }

        return &m_menu;
    }

    void graph_view::menu_chosen_static(void* user, context_menu& source, menu_item& item) noexcept
    {
        (void)source;
        static_cast<graph_view*>(user)->menu_chosen(item.id());
    }

    void graph_view::menu_chosen(std::int32_t id) noexcept
    {
        if (m_graph == nullptr)
            return;

        switch (id)
        {
        case k_menu_duplicate:
            duplicate_selected();
            break;

        case k_menu_delete:
            remove_selected();
            break;

        case k_menu_disconnect:
            disconnect_selected();
            break;

        case k_menu_downstream:
            select_reach(true);
            break;

        case k_menu_upstream:
            select_reach(false);
            break;

        case k_menu_frame_selected:
            frame_selected();
            break;

        case k_menu_disconnect_pin:
        {
            bool any = false;

            for (std::uint32_t i = m_graph->link_count(); i > 0; --i)
            {
                const graph_link link = m_graph->link_at(i - 1);
                const bool on_pin = (link.from_node == m_right_hit.node && link.from_pin == m_right_hit.pin) ||
                    (link.to_node == m_right_hit.node && link.to_pin == m_right_hit.pin);

                if (on_pin)
                    any = m_graph->remove_link(link.id) || any;
            }

            if (any)
                notify_change(true);

            break;
        }

        case k_menu_knob:
            if (const graph_link* link = m_graph->find_link(m_right_hit.link))
            {
                m_graph->set_link_knob(m_right_hit.link, !link->knob);
                notify_change(true);
            }

            break;

        case k_menu_clear_label:
            if (const graph_link* link = m_graph->find_link(m_right_hit.link))
            {
                m_graph->set_link_knob(m_right_hit.link, link->knob, "");
                notify_change(true);
            }

            break;

        case k_menu_delete_link:
            if (m_graph->remove_link(m_right_hit.link))
            {
                notify_select();
                notify_change(true);
            }

            break;

        case k_menu_select_source:
        case k_menu_select_target:
            if (const graph_link* link = m_graph->find_link(m_right_hit.link))
            {
                const std::uint32_t node = id == k_menu_select_source ? link->from_node : link->to_node;
                m_graph->clear_selection();

                if (graph_node* chosen = m_graph->find_node(node))
                    chosen->selected = true;

                notify_select();
            }

            break;

        case k_menu_new:
            if (m_create != nullptr)
                m_create(m_create_user, *this, m_menu_x, m_menu_y, 0, 0);

            break;

        case k_menu_select_all:
            select_all();
            break;

        case k_menu_arrange:
            arrange();
            break;

        case k_menu_frame_all:
            frame_all();
            break;

        default:
            break;
        }

        invalidate();
    }

    bool graph_view::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        zoom_at(std::pow(1.12f, static_cast<float>(notches)), static_cast<float>(x), static_cast<float>(y));
        return true;
    }

    bool graph_view::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt || m_graph == nullptr)
            return false;

        const float step = (m_snap > 0.0f ? m_snap : 8.0f) * (shift ? 4.0f : 1.0f);

        switch (key)
        {
        case k_key_delete:
            remove_selected();
            return true;

        case k_key_f:
        case k_key_home:
            m_frame_pending = true;
            m_frame_selected = key == k_key_f;
            invalidate();
            return true;

        case k_key_a:
            if (!ctrl)
                return false;

            select_all();
            return true;

        case k_key_d:
            if (!ctrl)
                return false;

            duplicate_selected();
            return true;

        case k_key_left:
            nudge(-step, 0.0f);
            return true;

        case k_key_right:
            nudge(step, 0.0f);
            return true;

        case k_key_up:
            nudge(0.0f, -step);
            return true;

        case k_key_down:
            nudge(0.0f, step);
            return true;

        default:
            return false;
        }
    }

    void graph_view::paint_grid(void* dc, const dock_rect& plot, std::uint32_t base) const noexcept
    {
        const float s = scale();
        const float spacing = k_grid * s;

        if (spacing < 7.0f)
            return;

        const std::uint32_t minor = blend_colour(base, 0xFFFFFF, 0.04f);
        const std::uint32_t major = blend_colour(base, 0xFFFFFF, 0.09f);
        const float first_x = std::floor(m_pan_x / k_grid);
        const float first_y = std::floor(m_pan_y / k_grid);

        for (int i = 0; i < 400; ++i)
        {
            const float line = first_x + static_cast<float>(i);
            const float x = sx(line * k_grid);

            if (x > static_cast<float>(plot.x + plot.width))
                break;

            const bool strong = static_cast<std::int32_t>(line) % 5 == 0;
            draw::fill(dc, {round_to_int(x), plot.y, 1, plot.height}, strong ? major : minor);
        }

        for (int i = 0; i < 400; ++i)
        {
            const float line = first_y + static_cast<float>(i);
            const float y = sy(line * k_grid);

            if (y > static_cast<float>(plot.y + plot.height))
                break;

            const bool strong = static_cast<std::int32_t>(line) % 5 == 0;
            draw::fill(dc, {plot.x, round_to_int(y), plot.width, 1}, strong ? major : minor);
        }
    }

    void graph_view::draw_link(void* dc, const point_f& from, const point_f& to, std::uint32_t colour_from, std::uint32_t colour_to, float width, bool halo) const noexcept
    {
        point_f curve[64];
        draw::point points[64];
        const std::uint32_t count = link_curve(from, to, curve, 64);

        for (std::uint32_t i = 0; i < count; ++i)
            points[i] = {curve[i].x, curve[i].y};

        const std::uint32_t shadow = blend_colour(theme().panel, 0x000000, 0.7f);

        // A darker line a little lower under it lifts the link off the grid.
        draw::point lowered[64];

        for (std::uint32_t i = 0; i < count; ++i)
            lowered[i] = {points[i].x, points[i].y + 1.5f};

        draw::polyline(dc, lowered, count, shadow, width + 2.0f);

        if (halo)
            draw::polyline(dc, points, count, theme().accent, width + 3.5f);

        if (colour_from == colour_to)
        {
            draw::polyline(dc, points, count, colour_from, width);
            return;
        }

        // The colour runs from one end to the other in a few steps, each a stretch of the curve.
        constexpr std::uint32_t k_steps = 5;

        for (std::uint32_t step = 0; step < k_steps; ++step)
        {
            const std::uint32_t first = step * (count - 1) / k_steps;
            const std::uint32_t last = (step + 1) * (count - 1) / k_steps;
            const float mix = (static_cast<float>(step) + 0.5f) / static_cast<float>(k_steps);
            draw::polyline(dc, points + first, last - first + 1, blend_colour(colour_from, colour_to, mix), width);
        }
    }

    void graph_view::paint_node(void* dc, const graph_node& node) const noexcept
    {
        const theme_colours& colours = theme();
        const float s = scale();
        const dock_rect view = inner();
        const dock_rect box = {round_to_int(sx(node.x)), round_to_int(sy(node.y)), std::max(round_to_int(node.width * s), 2), std::max(round_to_int(node_height(node) * s), 2)};
        const std::int32_t margin = draw::px(12);

        if (box.x > view.x + view.width + margin || box.x + box.width < view.x - margin || box.y > view.y + view.height + margin || box.y + box.height < view.y - margin)
            return;

        const bool detail = m_zoom >= k_text_zoom;
        const std::int32_t radius = std::max(round_to_int(k_corner * m_zoom), 2);
        const std::uint32_t body_top = blend_colour(colours.panel, 0xFFFFFF, 0.09f);
        const std::uint32_t body_bottom = blend_colour(colours.panel, 0x000000, 0.04f);
        const std::uint32_t edge = node.selected ? colours.accent : blend_colour(colours.title, 0x000000, 0.55f);

        if (m_graph->style() == graph_style::states)
        {
            const std::int32_t pill = std::max(round_to_int(10.0f * m_zoom), 3);
            draw::raised(dc, box, pill, blend_colour(node.colour, 0xFFFFFF, 0.14f), blend_colour(node.colour, 0x000000, 0.3f),
                node.selected ? colours.accent : blend_colour(node.colour, 0x000000, 0.55f), true, std::max(round_to_int(7.0f * m_zoom), 2), std::max(round_to_int(3.0f * m_zoom), 1), 0.5f);

            if (node.selected)
                draw::outline(dc, box, pill, colours.accent, 2.0f);

            if (detail)
                draw::text(dc, {box.x + round_to_int(8.0f * s), box.y, std::max(box.width - round_to_int(16.0f * s), 1), box.height}, node.title, 0xFFFFFF,
                    draw::text_bold | draw::text_center | draw::text_ellipsis);

            // While a link is being made, the node under the pointer shows whether it would take it.
            if (m_drag == drag_kind::link && m_hover.kind == hit_kind::node && m_hover.node == node.id)
            {
                bool fine = false;

                for (std::uint32_t p = 0; p < node.pin_count; ++p)
                    fine = fine || m_graph->check_link(m_link_node, m_link_pin, node.id, p) == graph::link_check::ok;

                draw::outline(dc, box, pill, fine ? k_good : k_bad, 2.5f);
            }

            // The ports show when the node is chosen, hovered or a link is being made.
            if (node.selected || m_drag == drag_kind::link || (m_hover.node == node.id && m_hover.kind != hit_kind::none && m_hover.kind != hit_kind::link))
            {
                for (std::uint32_t p = 0; p < node.pin_count; ++p)
                {
                    const point_f at = pin_at(node, p);
                    const bool hovered = m_hover.kind == hit_kind::pin && m_hover.node == node.id && m_hover.pin == p;
                    const std::uint32_t tint = m_graph->type_colour(node.pins[p].type);
                    draw::disc(dc, sx(at.x), sy(at.y), std::max(4.5f * s, 3.0f) + (hovered ? 1.0f : 0.0f), blend_colour(tint, 0xFFFFFF, hovered ? 0.5f : 0.0f), blend_colour(tint, 0x000000, 0.5f), 1.4f);
                }
            }

            return;
        }

        draw::raised(dc, box, radius, body_top, body_bottom, edge, true, std::max(round_to_int(7.0f * m_zoom), 2), std::max(round_to_int(3.0f * m_zoom), 1), 0.5f);

        // The header: a gradient box whose lower corners are cut off by the clip, so only the top ones are round.
        const std::int32_t header = std::max(round_to_int(k_header * s), 2);
        const std::uint32_t header_edge = blend_colour(node.colour, 0x000000, 0.5f);
        draw::clip(dc, {box.x, box.y, box.width, header});
        draw::gradient(dc, {box.x, box.y, box.width, header + 2 * draw::px(radius)}, radius, blend_colour(node.colour, 0xFFFFFF, 0.16f), blend_colour(node.colour, 0x000000, 0.28f), header_edge, true);
        draw::unclip(dc);
        draw::fill(dc, {box.x + 1, box.y + header, std::max(box.width - 2, 1), 1}, header_edge);

        if (node.selected)
            draw::outline(dc, box, radius, colours.accent, 2.0f);

        if (detail)
            draw::text(dc, {box.x + round_to_int(9.0f * s), box.y, std::max(box.width - round_to_int(18.0f * s), 1), header}, node.title, 0xFFFFFF, draw::text_bold | draw::text_ellipsis);

        const float pin_radius = std::max(k_pin_radius * s, 3.0f);

        for (std::uint32_t p = 0; p < node.pin_count; ++p)
        {
            const graph_pin& pin = node.pins[p];
            const point_f at = pin_at(node, p);
            const float cx = sx(at.x);
            const float cy = sy(at.y);
            const std::uint32_t tint = m_graph->type_colour(pin.type);
            const bool linked = m_graph->pin_linked(node.id, p);
            const bool hovered = m_hover.kind == hit_kind::pin && m_hover.node == node.id && m_hover.pin == p;
            const bool source = m_drag == drag_kind::link && m_link_node == node.id && m_link_pin == p;
            float radius_now = pin_radius + (hovered ? 1.0f : 0.0f);

            // While a link is being made, the pin under the pointer shows whether it would take it.
            if (hovered && m_drag == drag_kind::link)
            {
                const bool fine = m_graph->check_link(m_link_node, m_link_pin, node.id, p) == graph::link_check::ok;
                draw::disc(dc, cx, cy, radius_now + 3.5f, fine ? k_good : k_bad, fine ? k_good : k_bad, 0.0f);
            }
            else if (source)
            {
                draw::disc(dc, cx, cy, radius_now + 3.5f, colours.accent, colours.accent, 0.0f);
            }

            draw::disc(dc, cx, cy + 1.0f, radius_now + 0.8f, blend_colour(body_bottom, 0x000000, 0.6f), blend_colour(body_bottom, 0x000000, 0.6f), 0.0f);
            draw::disc(dc, cx, cy, radius_now, linked ? tint : blend_colour(body_bottom, 0x000000, 0.45f), hovered ? blend_colour(tint, 0xFFFFFF, 0.45f) : tint, 1.6f);

            if (!detail)
                continue;

            const std::int32_t row_height = std::max(round_to_int(k_row * s), 2);
            const std::int32_t text_y = round_to_int(cy) - row_height / 2;
            const std::int32_t gap = round_to_int(11.0f * s);

            if (pin.dir == pin_dir::input)
            {
                const dock_rect bar = bar_of(node, p);
                const std::int32_t right = bar.width > 0 ? bar.x - round_to_int(4.0f * s) : box.x + box.width / 2;
                draw::text(dc, {box.x + gap, text_y, std::max(right - box.x - gap, 1), row_height}, pin.name, colours.text, draw::text_ellipsis);

                if (bar.width > 0)
                {
                    const float fraction = std::clamp((pin.value - pin.low) / (pin.high - pin.low), 0.0f, 1.0f);
                    const std::uint32_t face = blend_colour(body_bottom, 0x000000, 0.4f);
                    draw::rounded(dc, bar, 3, face, blend_colour(face, 0x000000, 0.5f));
                    const std::int32_t filled = static_cast<std::int32_t>(static_cast<float>(bar.width - 2) * fraction);

                    if (filled > 0)
                        draw::rounded(dc, {bar.x + 1, bar.y + 1, filled, std::max(bar.height - 2, 1)}, 2, blend_colour(tint, 0x000000, 0.45f), blend_colour(tint, 0x000000, 0.2f));

                    char text[16];
                    std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(pin.value));
                    draw::text(dc, bar, text, colours.text, draw::text_center);
                }
            }
            else
            {
                draw::text(dc, {box.x + box.width / 2, text_y, std::max(box.width / 2 - gap, 1), row_height}, pin.name, colours.text, draw::text_right | draw::text_ellipsis);
            }
        }
    }

    void graph_view::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        std::uint32_t top = blend_colour(colours.panel, black, 0.32f);
        std::uint32_t bottom = blend_colour(colours.panel, black, 0.22f);
        std::uint32_t edge = focused() ? colours.accent : blend_colour(colours.title, black, 0.55f);

        if (!enabled())
        {
            top = bottom = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
        }

        draw::gradient(dc, area(), 4, top, bottom, edge, false);

        const dock_rect box = inner();

        if (m_graph == nullptr || box.width < 4 || box.height < 4)
            return;

        draw::clip(dc, box);

        const bool resized = m_auto_frame && (box.width != m_framed_width || box.height != m_framed_height);

        if ((m_frame_pending || resized) && box.width > draw::px(16) && box.height > draw::px(16))
        {
            frame_now(m_frame_selected);
            m_auto_frame = true;
            m_framed_width = box.width;
            m_framed_height = box.height;
        }

        paint_grid(dc, box, bottom);

        const float s = scale();
        const float line_width = std::clamp(2.2f * s, 1.5f, 4.5f);

        // The links, the selected ones last so they lie over the rest.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::uint32_t l = 0; l < m_graph->link_count(); ++l)
            {
                const graph_link& link = m_graph->link_at(l);

                if (link.selected != (pass == 1))
                    continue;

                point_f a;
                point_f b;

                if (!link_ends(link, a, b))
                    continue;

                const float t = tangent_of(a, b);

                if (std::max(a.x, b.x) + t < static_cast<float>(box.x) || std::min(a.x, b.x) - t > static_cast<float>(box.x + box.width) ||
                    std::max(a.y, b.y) < static_cast<float>(box.y) || std::min(a.y, b.y) > static_cast<float>(box.y + box.height))
                    continue;

                const graph_node* from = m_graph->find_node(link.from_node);
                const graph_node* to = m_graph->find_node(link.to_node);
                std::uint32_t colour_a = m_graph->type_colour(from->pins[link.from_pin].type);
                std::uint32_t colour_b = m_graph->type_colour(to->pins[link.to_pin].type);
                const bool hovered = m_hover.kind == hit_kind::link && m_hover.link == link.id;

                if (hovered)
                {
                    colour_a = blend_colour(colour_a, 0xFFFFFF, 0.35f);
                    colour_b = blend_colour(colour_b, 0xFFFFFF, 0.35f);
                }

                draw_link(dc, a, b, colour_a, colour_b, line_width + (hovered ? 0.8f : 0.0f), link.selected);

                if (m_graph->style() == graph_style::states)
                {
                    const float length = std::max(std::hypot(b.x - a.x, b.y - a.y), 1.0f);
                    const float ux = (b.x - a.x) / length;
                    const float uy = (b.y - a.y) / length;
                    const float size = 10.0f * std::max(s, 0.8f);
                    const draw::point arrow[3] = {{b.x, b.y}, {b.x - ux * size - uy * size * 0.5f, b.y - uy * size + ux * size * 0.5f},
                        {b.x - ux * size + uy * size * 0.5f, b.y - uy * size - ux * size * 0.5f}};
                    draw::polygon(dc, arrow, 3, colour_b);
                }
            }
        }

        // The knobs sit on the links, with the label above.
        for (std::uint32_t l = 0; l < m_graph->link_count(); ++l)
        {
            const graph_link& link = m_graph->link_at(l);
            point_f a;
            point_f b;

            if (!link.knob || !link_ends(link, a, b))
                continue;

            const float mx = (a.x + b.x) * 0.5f;
            const float my = (a.y + b.y) * 0.5f;
            const float radius = 5.5f * std::max(m_zoom, 0.8f) * static_cast<float>(draw::px(100)) / 100.0f;
            const bool hovered = m_hover.kind == hit_kind::link && m_hover.link == link.id;
            const graph_node* from = m_graph->find_node(link.from_node);
            const std::uint32_t tint = m_graph->type_colour(from->pins[link.from_pin].type);

            draw::disc(dc, mx, my + 1.6f, radius + 0.8f, blend_colour(bottom, black, 0.7f), blend_colour(bottom, black, 0.7f), 0.0f);
            draw::disc(dc, mx, my, radius, blend_colour(tint, 0xFFFFFF, hovered ? 0.7f : 0.35f), link.selected ? colours.accent : blend_colour(tint, black, 0.5f), link.selected ? 2.2f : 1.4f);

            if (link.label[0] != '\0' && m_zoom >= k_text_zoom)
            {
                const std::int32_t width = draw::text_width(dc, link.label) + draw::px(14);
                const std::int32_t height = draw::px(18);
                dock_rect pill = {round_to_int(mx) - width / 2, round_to_int(my - radius) - height - draw::px(4), width, height};

                // Between states, the label goes to the outer side of its own lane, so two links between the same states do not hide each other's knob.
                if (m_graph->style() == graph_style::states)
                {
                    const float length = std::max(std::hypot(b.x - a.x, b.y - a.y), 1.0f);
                    const float away = radius + static_cast<float>(height) * 0.5f + static_cast<float>(draw::px(4));
                    pill.x = round_to_int(mx + (b.y - a.y) / length * away) - width / 2;
                    pill.y = round_to_int(my - (b.x - a.x) / length * away) - height / 2;
                }

                draw::rounded(dc, pill, 9, blend_colour(colours.panel, black, 0.12f), link.selected ? colours.accent : blend_colour(colours.title, black, 0.5f));
                draw::text(dc, pill, link.label, colours.text, draw::text_center);
            }
        }

        for (std::uint32_t n = 0; n < m_graph->node_count(); ++n)
            paint_node(dc, m_graph->node_at(n));

        // The link being made runs from where it started to the pointer.
        if (m_drag == drag_kind::link)
        {
            const graph_node* node = m_graph->find_node(m_link_node);

            if (node != nullptr && m_link_pin < node->pin_count)
            {
                const point_f at = pin_at(*node, m_link_pin);
                const point_f start = {sx(at.x), sy(at.y)};
                const point_f end = {static_cast<float>(m_x), static_cast<float>(m_y)};
                const std::uint32_t tint = m_graph->type_colour(node->pins[m_link_pin].type);

                if (m_link_from_output)
                    draw_link(dc, start, end, tint, tint, line_width, false);
                else
                    draw_link(dc, end, start, tint, tint, line_width, false);
            }
        }

        if (m_drag == drag_kind::box)
        {
            const dock_rect rect = {std::min(m_press_x, m_x), std::min(m_press_y, m_y), std::abs(m_x - m_press_x), std::abs(m_y - m_press_y)};
            draw::outline(dc, rect, 0, colours.accent, 1.0f);
        }

        if (m_graph->node_count() == 0)
            draw::text(dc, box, "Double click to add a node", colours.text_dim, draw::text_center);

        // What the pin colours stand for in the corner, and the zoom opposite.
        {
            const std::int32_t row = box.y + box.height - draw::px(20);
            std::int32_t at = box.x + draw::px(10);

            if (m_graph->style() == graph_style::flow)
            {
                for (std::uint32_t t = 0; t < graph::k_max_types; ++t)
                {
                    if (!m_graph->has_type(static_cast<std::uint8_t>(t)))
                        continue;

                    const char* name = m_graph->type_name(static_cast<std::uint8_t>(t));
                    const std::int32_t width = draw::text_width(dc, name);
                    draw::disc(dc, static_cast<float>(at) + 4.0f, static_cast<float>(row + draw::px(9)), 3.5f, m_graph->type_colour(static_cast<std::uint8_t>(t)),
                        blend_colour(bottom, black, 0.5f), 1.0f);
                    draw::text(dc, {at + draw::px(12), row, width + 4, draw::px(18)}, name, colours.text_dim);
                    at += width + draw::px(26);
                }
            }

            char zoom_text[16];
            std::snprintf(zoom_text, sizeof(zoom_text), "%d%%", round_to_int(m_zoom * 100.0f));
            draw::text(dc, {box.x + box.width - draw::px(70), row, draw::px(60), draw::px(18)}, zoom_text, colours.text_dim, draw::text_right);
        }

        draw::unclip(dc);
    }
}
