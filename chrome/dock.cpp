#include <chrome/dock.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace chrome
{
    namespace
    {
        [[nodiscard]] bool contains(const dock_rect& rect, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
        }

        float g_scale = 1.0f;

        [[nodiscard]] dock_rect shifted(dock_rect rect, std::int32_t dx, std::int32_t dy) noexcept
        {
            rect.x += dx;
            rect.y += dy;
            return rect;
        }
    }

    void dock_model::set_scale(float scale) noexcept
    {
        g_scale = std::clamp(scale, 0.5f, 4.0f);
    }

    float dock_model::scale() noexcept
    {
        return g_scale;
    }

    std::int32_t dock_model::scaled(std::int32_t pixels) noexcept
    {
        return static_cast<std::int32_t>(std::lround(static_cast<float>(pixels) * g_scale));
    }

    std::int32_t dock_model::splitter_width() noexcept { return std::max(scaled(2), 2); }
    std::int32_t dock_model::splitter_grab() noexcept { return std::max(scaled(2), 1); }
    std::int32_t dock_model::min_panel_size() noexcept { return scaled(64); }
    std::int32_t dock_model::tab_height() noexcept { return scaled(30); }
    std::int32_t dock_model::compass_cell() noexcept { return scaled(34); }
    std::int32_t dock_model::compass_gap() noexcept { return scaled(6); }
    std::int32_t dock_model::compass_extent() noexcept { return 3 * compass_cell() + 2 * compass_gap(); }
    std::int32_t dock_model::root_inset() noexcept { return scaled(14); }

    dock_rect dock_model::compass_tile(const dock_rect& compass, drop_zone zone) noexcept
    {
        std::int32_t column = 1;
        std::int32_t row = 1;

        switch (zone)
        {
        case drop_zone::left:
            column = 0;
            break;
        case drop_zone::right:
            column = 2;
            break;
        case drop_zone::top:
            row = 0;
            break;
        case drop_zone::bottom:
            row = 2;
            break;
        case drop_zone::tabs:
            break;
        case drop_zone::none:
            return {};
        }

        return {compass.x + column * (compass_cell() + compass_gap()), compass.y + row * (compass_cell() + compass_gap()),
            compass_cell(), compass_cell()};
    }

    dock_rect dock_model::root_tile(const dock_rect& area, drop_zone zone) noexcept
    {
        const std::int32_t size = compass_cell();

        switch (zone)
        {
        case drop_zone::root_left:
            return {area.x + root_inset(), area.y + (area.height - size) / 2, size, size};
        case drop_zone::root_right:
            return {area.x + area.width - root_inset() - size, area.y + (area.height - size) / 2, size, size};
        case drop_zone::root_top:
            return {area.x + (area.width - size) / 2, area.y + root_inset(), size, size};
        case drop_zone::root_bottom:
            return {area.x + (area.width - size) / 2, area.y + area.height - root_inset() - size, size, size};
        default:
            return {};
        }
    }

    dock_rect dock_model::root_strip(const dock_rect& area, drop_zone zone) noexcept
    {
        const std::int32_t w = static_cast<std::int32_t>(static_cast<float>(area.width) * k_root_share);
        const std::int32_t h = static_cast<std::int32_t>(static_cast<float>(area.height) * k_root_share);

        switch (zone)
        {
        case drop_zone::root_left:
            return {area.x, area.y, w, area.height};
        case drop_zone::root_right:
            return {area.x + area.width - w, area.y, w, area.height};
        case drop_zone::root_top:
            return {area.x, area.y, area.width, h};
        case drop_zone::root_bottom:
            return {area.x, area.y + area.height - h, area.width, h};
        default:
            return {};
        }
    }

    dock_model::dock_model() noexcept
    {
        m_roots[k_main_root].used = true;
        m_roots[k_main_root].serial = ++m_serial;
    }

    panel_id dock_model::add_panel(const panel_desc& desc) noexcept
    {
        if (m_panel_count >= k_max_panels || desc.title == nullptr || desc.handle == nullptr)
            return k_no_panel;

        panel& entry = m_panels[m_panel_count];
        entry = {};
        entry.used = true;
        entry.permanent = desc.permanent;
        entry.title = desc.title;
        entry.handle = desc.handle;
        return m_panel_count++;
    }

    bool dock_model::is_open(panel_id id) const noexcept
    {
        return id < m_panel_count && m_panels[id].group != k_no_node;
    }

    void dock_model::set_headerless(panel_id id, bool headerless) noexcept
    {
        if (id < m_panel_count)
            m_panels[id].headerless = headerless;
    }

    bool dock_model::is_headerless(panel_id id) const noexcept
    {
        return id < m_panel_count && m_panels[id].headerless;
    }

    std::int32_t dock_model::header_height(node_id group) const noexcept
    {
        if (tab_count(group) == 1 && is_headerless(tab_at(group, 0)))
            return 0;

        return tab_height();
    }

    bool dock_model::is_permanent(panel_id id) const noexcept
    {
        return id < m_panel_count && (m_locked || m_panels[id].permanent);
    }

    const char* dock_model::title_of(panel_id id) const noexcept
    {
        return id < m_panel_count ? m_panels[id].title : "";
    }

    void* dock_model::handle_of(panel_id id) const noexcept
    {
        return id < m_panel_count ? m_panels[id].handle : nullptr;
    }

    node_id dock_model::group_of(panel_id id) const noexcept
    {
        return id < m_panel_count ? m_panels[id].group : k_no_node;
    }

    node_id dock_model::alloc_node(root_id owner) noexcept
    {
        for (node_id id = 0; id < k_max_nodes; ++id)
        {
            if (!m_nodes[id].used)
            {
                m_nodes[id] = {};
                m_nodes[id].used = true;
                m_nodes[id].root = owner;
                m_nodes[id].serial = ++m_serial;
                return id;
            }
        }

        return k_no_node;
    }

    std::uint32_t dock_model::free_nodes() const noexcept
    {
        std::uint32_t count = 0;

        for (const node& entry : m_nodes)
        {
            if (!entry.used)
                ++count;
        }

        return count;
    }

    void dock_model::replace_child(node_id parent, node_id old_child, node_id new_child, root_id owner) noexcept
    {
        if (parent == k_no_node)
            m_roots[owner].top = new_child;
        else if (m_nodes[parent].first == old_child)
            m_nodes[parent].first = new_child;
        else
            m_nodes[parent].second = new_child;

        m_nodes[new_child].parent = parent;
    }

    void dock_model::remove_group(node_id group) noexcept
    {
        const root_id owner = m_nodes[group].root;
        const node_id parent = m_nodes[group].parent;

        if (parent == k_no_node)
        {
            m_roots[owner].top = k_no_node;

            // A floating window with nothing in it is gone. The main root stays, empty.
            if (m_roots[owner].floating)
                m_roots[owner].used = false;
        }
        else
        {
            const node_id sibling = m_nodes[parent].first == group ? m_nodes[parent].second : m_nodes[parent].first;
            replace_child(m_nodes[parent].parent, parent, sibling, owner);
            m_nodes[parent].used = false;
        }

        m_nodes[group].used = false;
    }

    void dock_model::detach(panel_id id) noexcept
    {
        const node_id group = m_panels[id].group;

        if (group == k_no_node)
            return;

        node& entry = m_nodes[group];
        std::uint32_t index = 0;

        while (index < entry.tab_count && entry.tabs[index] != id)
            ++index;

        for (std::uint32_t i = index; i + 1 < entry.tab_count; ++i)
            entry.tabs[i] = entry.tabs[i + 1];

        --entry.tab_count;
        m_panels[id].group = k_no_node;

        if (entry.active == id)
            entry.active = entry.tab_count == 0 ? k_no_panel : entry.tabs[std::min(index, entry.tab_count - 1)];

        if (entry.tab_count == 0)
            remove_group(group);
    }

    void dock_model::attach(panel_id id, node_id group, std::uint32_t index) noexcept
    {
        node& entry = m_nodes[group];
        const std::uint32_t at = std::min(index, entry.tab_count);

        for (std::uint32_t i = entry.tab_count; i > at; --i)
            entry.tabs[i] = entry.tabs[i - 1];

        entry.tabs[at] = id;
        ++entry.tab_count;
        entry.active = id;
        m_panels[id].group = group;
    }

    bool dock_model::dock_tabs(panel_id id, root_id owner, node_id group, std::uint32_t index) noexcept
    {
        if (id >= m_panel_count || owner >= k_max_roots || !m_roots[owner].used)
            return false;

        if (group == k_no_node)
        {
            if (m_roots[owner].top != k_no_node || free_nodes() < 1)
                return false;

            detach(id);
            const node_id fresh = alloc_node(owner);
            m_roots[owner].top = fresh;
            attach(id, fresh, 0);
            return true;
        }

        if (group >= k_max_nodes || !m_nodes[group].used || !m_nodes[group].group || m_nodes[group].root != owner)
            return false;

        if (m_panels[id].group == group)
        {
            node& entry = m_nodes[group];
            std::uint32_t current = 0;

            while (entry.tabs[current] != id)
                ++current;

            // Take it out and put it back, at the position counted without it.
            if (index != k_tab_end && index > current)
                --index;

            for (std::uint32_t i = current; i + 1 < entry.tab_count; ++i)
                entry.tabs[i] = entry.tabs[i + 1];

            --entry.tab_count;
            attach(id, group, index);
            return true;
        }

        detach(id);
        attach(id, group, index);
        return true;
    }

    bool dock_model::dock_split(panel_id id, node_id target, drop_zone side, float share) noexcept
    {
        if (id >= m_panel_count || side < drop_zone::left || side > drop_zone::bottom)
            return false;

        if (target >= k_max_nodes || !m_nodes[target].used || !m_nodes[target].group)
            return false;

        // Splitting a group off itself with nothing else in it changes nothing.
        if (m_panels[id].group == target && m_nodes[target].tab_count == 1)
            return false;

        // Detaching may free nodes, but counting without that credit keeps the check simple.
        if (free_nodes() < 2)
            return false;

        detach(id);
        const root_id owner = m_nodes[target].root;
        const node_id fresh = alloc_node(owner);
        const node_id split = alloc_node(owner);
        const node_id parent = m_nodes[target].parent;
        replace_child(parent, target, split, owner);

        const bool new_first = side == drop_zone::left || side == drop_zone::top;
        const float taken = std::clamp(share, 0.1f, 0.9f);
        node& entry = m_nodes[split];
        entry.group = false;
        entry.axis = side == drop_zone::left || side == drop_zone::right ? dock_axis::horizontal : dock_axis::vertical;
        entry.first = new_first ? fresh : target;
        entry.second = new_first ? target : fresh;
        entry.ratio = new_first ? taken : 1.0f - taken;
        m_nodes[fresh].parent = split;
        m_nodes[target].parent = split;
        attach(id, fresh, 0);
        return true;
    }

    bool dock_model::dock_root_split(panel_id id, root_id owner, drop_zone side, float share) noexcept
    {
        if (id >= m_panel_count || owner >= k_max_roots || !m_roots[owner].used || side < drop_zone::left || side > drop_zone::bottom)
            return false;

        const node_id before = m_roots[owner].top;

        if (before == k_no_node)
            return dock_tabs(id, owner, k_no_node);

        // A panel alone in the root is already all of it.
        if (m_nodes[before].group && m_nodes[before].tab_count == 1 && m_panels[id].group == before)
            return false;

        if (free_nodes() < 2)
            return false;

        detach(id);
        const node_id old_top = m_roots[owner].top;

        if (old_top == k_no_node)
            return dock_tabs(id, owner, k_no_node);

        const node_id fresh = alloc_node(owner);
        const node_id split = alloc_node(owner);
        const bool new_first = side == drop_zone::left || side == drop_zone::top;
        const float taken = std::clamp(share, 0.1f, 0.9f);
        node& entry = m_nodes[split];
        entry.group = false;
        entry.axis = side == drop_zone::left || side == drop_zone::right ? dock_axis::horizontal : dock_axis::vertical;
        entry.first = new_first ? fresh : old_top;
        entry.second = new_first ? old_top : fresh;
        entry.ratio = new_first ? taken : 1.0f - taken;
        entry.parent = k_no_node;
        m_roots[owner].top = split;
        m_nodes[fresh].parent = split;
        m_nodes[old_top].parent = split;
        attach(id, fresh, 0);
        return true;
    }

    root_id dock_model::float_panel(panel_id id, const dock_rect& screen) noexcept
    {
        if (id >= m_panel_count)
            return k_no_root;

        // Already alone in its own floating window: move that window instead of making another.
        const node_id current = m_panels[id].group;

        if (current != k_no_node && m_nodes[current].tab_count == 1 && m_nodes[current].parent == k_no_node &&
            m_roots[m_nodes[current].root].floating)
        {
            m_roots[m_nodes[current].root].area = screen;
            return m_nodes[current].root;
        }

        root_id slot = k_no_root;

        for (root_id i = 1; i < k_max_roots; ++i)
        {
            if (!m_roots[i].used)
            {
                slot = i;
                break;
            }
        }

        if (slot == k_no_root || free_nodes() < 1)
            return k_no_root;

        detach(id);
        m_roots[slot] = {};
        m_roots[slot].used = true;
        m_roots[slot].floating = true;
        m_roots[slot].serial = ++m_serial;
        m_roots[slot].area = screen;
        const node_id fresh = alloc_node(slot);
        m_roots[slot].top = fresh;
        attach(id, fresh, 0);
        return slot;
    }

    bool dock_model::close_panel(panel_id id) noexcept
    {
        if (!is_open(id) || is_permanent(id))
            return false;

        record_home(id);
        detach(id);

        if (m_focus == id)
            m_focus = k_no_panel;

        return true;
    }

    void dock_model::record_home(panel_id id) noexcept
    {
        panel& entry = m_panels[id];
        entry.home = home_kind::none;
        entry.home_ref = k_no_panel;
        const node_id group = entry.group;

        if (group == k_no_node)
            return;

        const node& place = m_nodes[group];

        // With company: a tab beside the neighbour, which is the one before it, or the one after for the first tab.
        if (place.tab_count > 1)
        {
            std::uint32_t index = 0;

            while (index < place.tab_count && place.tabs[index] != id)
                ++index;

            entry.home = home_kind::tabs;
            entry.home_ref = index > 0 ? place.tabs[index - 1] : place.tabs[1];
            entry.home_index = index;
            return;
        }

        const root& owner = m_roots[place.root];

        if (owner.floating)
        {
            entry.home = home_kind::floating;
            entry.home_area = owner.area;
            return;
        }

        // Alone at the top of the main window there is nothing to be beside.
        if (place.parent == k_no_node)
            return;

        // Beside the nearest group of the other side of the split.
        const node& cut = m_nodes[place.parent];
        const bool first = cut.first == group;
        node_id leaf = first ? cut.second : cut.first;

        while (!m_nodes[leaf].group)
            leaf = first ? m_nodes[leaf].first : m_nodes[leaf].second;

        const node& near = m_nodes[leaf];
        entry.home = home_kind::split;
        entry.home_ref = near.active != k_no_panel ? near.active : near.tabs[0];
        entry.home_zone = cut.axis == dock_axis::horizontal ? (first ? drop_zone::left : drop_zone::right)
                                                            : (first ? drop_zone::top : drop_zone::bottom);
        entry.home_share = first ? cut.ratio : 1.0f - cut.ratio;
    }

    bool dock_model::restore_home(panel_id id) noexcept
    {
        if (id >= m_panel_count || m_panels[id].group != k_no_node)
            return false;

        const panel entry = m_panels[id];
        const bool ref_open = entry.home_ref < m_panel_count && m_panels[entry.home_ref].group != k_no_node;

        if (entry.home == home_kind::tabs && ref_open)
        {
            const node_id group = m_panels[entry.home_ref].group;
            return dock_tabs(id, m_nodes[group].root, group, entry.home_index);
        }

        if (entry.home == home_kind::split && ref_open)
            return dock_split(id, m_panels[entry.home_ref].group, entry.home_zone, entry.home_share);

        if (entry.home == home_kind::floating)
            return float_panel(id, entry.home_area) != k_no_root;

        // The panel it was next to is closed too. When that one was in a floating window, this one joins it there.
        if (entry.home_ref < m_panel_count && m_panels[entry.home_ref].home == home_kind::floating)
            return float_panel(id, m_panels[entry.home_ref].home_area) != k_no_root;

        return false;
    }

    void dock_model::set_focused(panel_id id) noexcept
    {
        m_focus = is_open(id) ? id : k_no_panel;
    }

    void dock_model::reset() noexcept
    {
        for (panel& entry : m_panels)
        {
            entry.group = k_no_node;
            entry.home = home_kind::none;
            entry.home_ref = k_no_panel;
        }

        for (node& entry : m_nodes)
            entry.used = false;

        for (root_id i = 1; i < k_max_roots; ++i)
            m_roots[i] = {};

        m_roots[k_main_root].top = k_no_node;
        m_focus = k_no_panel;
    }

    // Saved layout text, one item per line:
    //   root <id> <floating> <x> <y> <width> <height>     then its tree, depth first
    //   S <axis> <ratio>                                   a split, then its first and second child
    //   G <count>                                          a tab group, then count lines of
    //   T <active> "<title>"
    //   H <kind> <zone> <share> <index> <x> <y> <w> <h> "<title>" "<ref>"
    //                                                      where a closed panel was docked: kind 1 a tab next to ref at
    //                                                      index, 2 a split beside ref on a zone taking a share, 3 floating
    //   F "<title>"                                        the focused panel
    namespace
    {
        struct writer
        {
            char* out;
            std::size_t capacity;
            std::size_t length{0};
            bool failed{false};

            void put(const char* format, ...) noexcept
            {
                if (failed)
                    return;

                va_list args;
                va_start(args, format);
                const int written = std::vsnprintf(out + length, capacity - length, format, args);
                va_end(args);

                if (written < 0 || static_cast<std::size_t>(written) >= capacity - length)
                    failed = true;
                else
                    length += static_cast<std::size_t>(written);
            }
        };
    }

    std::size_t dock_model::save(char* out, std::size_t capacity) const noexcept
    {
        if (out == nullptr || capacity == 0)
            return 0;

        writer w{out, capacity};

        struct emit
        {
            const dock_model& model;
            writer& w;

            void node_out(node_id id) const noexcept
            {
                const node& entry = model.m_nodes[id];

                if (entry.group)
                {
                    w.put("G %u\n", entry.tab_count);

                    for (std::uint32_t i = 0; i < entry.tab_count; ++i)
                    {
                        const panel_id tab = entry.tabs[i];
                        w.put("T %u \"%s\"\n", tab == entry.active ? 1u : 0u, model.m_panels[tab].title);
                    }

                    return;
                }

                w.put("S %u %.4f\n", entry.axis == dock_axis::horizontal ? 0u : 1u, static_cast<double>(entry.ratio));
                node_out(entry.first);
                node_out(entry.second);
            }
        };

        const emit e{*this, w};

        for (root_id r = 0; r < k_max_roots; ++r)
        {
            const root& entry = m_roots[r];

            if (!entry.used || entry.top == k_no_node)
                continue;

            w.put("root %u %u %d %d %d %d\n", r, entry.floating ? 1u : 0u, entry.area.x, entry.area.y, entry.area.width,
                entry.area.height);
            e.node_out(entry.top);
        }

        // Where each closed panel was docked, so it can open there again.
        for (panel_id p = 0; p < m_panel_count; ++p)
        {
            const panel& entry = m_panels[p];

            if (entry.group != k_no_node || entry.home == home_kind::none)
                continue;

            const char* ref = entry.home_ref < m_panel_count ? m_panels[entry.home_ref].title : "";
            w.put("H %u %u %.4f %u %d %d %d %d \"%s\" \"%s\"\n", static_cast<unsigned>(entry.home), static_cast<unsigned>(entry.home_zone),
                static_cast<double>(entry.home_share), static_cast<unsigned>(entry.home_index), entry.home_area.x, entry.home_area.y,
                entry.home_area.width, entry.home_area.height, entry.title, ref);
        }

        if (m_focus != k_no_panel)
            w.put("F \"%s\"\n", m_panels[m_focus].title);

        return w.failed ? 0 : w.length;
    }

    struct dock_model::loader
    {
        dock_model& model;
        const char* cursor;
        const char* end;
        bool ok{true};

        // The next line, without its end. false at the end of the text.
        bool next_line(char* line, std::size_t capacity) noexcept
        {
            while (cursor < end && (*cursor == '\n' || *cursor == '\r'))
                ++cursor;

            if (cursor >= end)
                return false;

            std::size_t n = 0;

            while (cursor < end && *cursor != '\n' && *cursor != '\r')
            {
                if (n + 1 < capacity)
                    line[n++] = *cursor;

                ++cursor;
            }

            line[n] = 0;
            return true;
        }

        // The panel whose title is between the quotes of a line, or k_no_panel.
        [[nodiscard]] panel_id find_panel(const char* line) const noexcept
        {
            const char* open = std::strchr(line, '"');
            const char* close = std::strrchr(line, '"');

            if (open == nullptr || close == open)
                return k_no_panel;

            const std::size_t length = static_cast<std::size_t>(close - open - 1);

            for (panel_id p = 0; p < model.m_panel_count; ++p)
            {
                if (std::strlen(model.m_panels[p].title) == length && std::strncmp(model.m_panels[p].title, open + 1, length) == 0)
                    return p;
            }

            return k_no_panel;
        }

        // The panel with this title, which is length characters at begin, or k_no_panel.
        [[nodiscard]] panel_id find_title(const char* begin, std::size_t length) const noexcept
        {
            for (panel_id p = 0; p < model.m_panel_count; ++p)
            {
                if (std::strlen(model.m_panels[p].title) == length && std::strncmp(model.m_panels[p].title, begin, length) == 0)
                    return p;
            }

            return k_no_panel;
        }

        // An H line: where a closed panel was docked. A line that does not make sense is skipped, since the layout
        // itself is fine without it.
        void read_home(const char* line) noexcept
        {
            unsigned kind = 0;
            unsigned zone = 0;
            unsigned index = 0;
            float share = 0.3f;
            int x = 0;
            int y = 0;
            int w = 0;
            int h = 0;
            int used = 0;

            if (std::sscanf(line, "H %u %u %f %u %d %d %d %d%n", &kind, &zone, &share, &index, &x, &y, &w, &h, &used) != 8 || kind < 1 || kind > 3)
                return;

            const char* quote[4]{};
            const char* cursor = line + used;

            for (const char*& q : quote)
            {
                q = std::strchr(cursor, '"');

                if (q == nullptr)
                    return;

                cursor = q + 1;
            }

            const panel_id id = find_title(quote[0] + 1, static_cast<std::size_t>(quote[1] - quote[0] - 1));
            const panel_id ref = find_title(quote[2] + 1, static_cast<std::size_t>(quote[3] - quote[2] - 1));

            if (id == k_no_panel || zone > static_cast<unsigned>(drop_zone::bottom))
                return;

            panel& entry = model.m_panels[id];
            entry.home = static_cast<home_kind>(kind);
            entry.home_ref = ref;
            entry.home_zone = static_cast<drop_zone>(zone);
            entry.home_share = std::clamp(share, 0.1f, 0.9f);
            entry.home_index = index;
            entry.home_area = {x, y, std::max(w, 1), std::max(h, 1)};
        }

        // Reads one subtree. k_no_node when nothing in it is left. ok says whether the text was fine.
        node_id read_node(root_id owner, int depth) noexcept
        {
            char line[160];

            if (depth > 16 || !next_line(line, sizeof(line)))
            {
                ok = false;
                return k_no_node;
            }

            if (line[0] == 'G')
            {
                const std::uint32_t count = static_cast<std::uint32_t>(std::strtoul(line + 1, nullptr, 10));
                const node_id group = count <= k_max_panels ? model.alloc_node(owner) : k_no_node;

                if (group == k_no_node)
                {
                    ok = false;
                    return k_no_node;
                }

                panel_id active = k_no_panel;

                for (std::uint32_t i = 0; i < count; ++i)
                {
                    if (!next_line(line, sizeof(line)) || line[0] != 'T')
                    {
                        ok = false;
                        break;
                    }

                    const panel_id id = find_panel(line);

                    if (id == k_no_panel || model.m_panels[id].group != k_no_node)
                        continue;

                    model.attach(id, group, k_tab_end);

                    if (line[2] == '1')
                        active = id;
                }

                if (model.m_nodes[group].tab_count == 0)
                {
                    model.m_nodes[group].used = false;
                    return k_no_node;
                }

                if (active != k_no_panel)
                    model.m_nodes[group].active = active;

                return group;
            }

            if (line[0] == 'S')
            {
                char* rest = nullptr;
                const unsigned long axis = std::strtoul(line + 1, &rest, 10);
                const float ratio = std::strtof(rest, nullptr);
                const node_id split = axis <= 1 ? model.alloc_node(owner) : k_no_node;

                if (split == k_no_node)
                {
                    ok = false;
                    return k_no_node;
                }

                const node_id first = read_node(owner, depth + 1);
                const node_id second = ok ? read_node(owner, depth + 1) : k_no_node;

                if (!ok || (first == k_no_node && second == k_no_node))
                {
                    model.m_nodes[split].used = false;
                    return k_no_node;
                }

                // One side lost all its panels: the other one takes the whole space.
                if (first == k_no_node || second == k_no_node)
                {
                    model.m_nodes[split].used = false;
                    return first != k_no_node ? first : second;
                }

                node& entry = model.m_nodes[split];
                entry.group = false;
                entry.axis = axis == 0 ? dock_axis::horizontal : dock_axis::vertical;
                entry.ratio = std::clamp(ratio, 0.05f, 0.95f);
                entry.first = first;
                entry.second = second;
                model.m_nodes[first].parent = split;
                model.m_nodes[second].parent = split;
                return split;
            }

            ok = false;
            return k_no_node;
        }
    };

    bool dock_model::load(const char* text, std::size_t size) noexcept
    {
        if (text == nullptr)
            return false;

        const dock_model snapshot = *this;
        reset();
        loader read{*this, text, text + size};
        char line[160];
        panel_id focus = k_no_panel;
        bool main_seen = false;

        while (read.ok && read.next_line(line, sizeof(line)))
        {
            if (line[0] == 'F')
            {
                focus = read.find_panel(line);
                continue;
            }

            if (line[0] == 'H')
            {
                read.read_home(line);
                continue;
            }

            unsigned id = 0;
            unsigned floating = 0;
            int x = 0;
            int y = 0;
            int w = 0;
            int h = 0;

            if (std::sscanf(line, "root %u %u %d %d %d %d", &id, &floating, &x, &y, &w, &h) != 6 || id >= k_max_roots ||
                (id == k_main_root) != (floating == 0) || (id != k_main_root && m_roots[id].used))
            {
                read.ok = false;
                break;
            }

            if (id == k_main_root)
            {
                main_seen = true;
            }
            else
            {
                m_roots[id] = {};
                m_roots[id].used = true;
                m_roots[id].floating = true;
                m_roots[id].serial = ++m_serial;
                m_roots[id].area = {x, y, std::max(w, 1), std::max(h, 1)};
            }

            const node_id top = read.read_node(id, 0);

            if (!read.ok)
                break;

            m_roots[id].top = top;

            if (top == k_no_node && id != k_main_root)
                m_roots[id].used = false;
        }

        bool permanent_open = true;

        for (panel_id p = 0; p < m_panel_count; ++p)
        {
            if (m_panels[p].permanent && m_panels[p].group == k_no_node)
                permanent_open = false;
        }

        if (!read.ok || !main_seen || !permanent_open)
        {
            *this = snapshot;
            return false;
        }

        set_focused(focus);
        return true;
    }

    bool dock_model::apply_drop(panel_id id, const drop_target& target, std::uint32_t index) noexcept
    {
        switch (target.zone)
        {
        case drop_zone::tabs:
            return dock_tabs(id, target.root, target.group, index);
        case drop_zone::left:
        case drop_zone::right:
        case drop_zone::top:
        case drop_zone::bottom:
            return dock_split(id, target.group, target.zone);
        case drop_zone::root_left:
            return dock_root_split(id, target.root, drop_zone::left);
        case drop_zone::root_right:
            return dock_root_split(id, target.root, drop_zone::right);
        case drop_zone::root_top:
            return dock_root_split(id, target.root, drop_zone::top);
        case drop_zone::root_bottom:
            return dock_root_split(id, target.root, drop_zone::bottom);
        case drop_zone::none:
            break;
        }

        return false;
    }

    bool dock_model::apply_root_drop(root_id source, const drop_target& target, std::uint32_t index) noexcept
    {
        if (source == k_main_root || source >= k_max_roots || !m_roots[source].used || target.zone == drop_zone::none ||
            target.root == source)
            return false;

        panel_id members[k_max_panels]{};
        std::uint32_t count = 0;
        panel_id active = k_no_panel;

        for (const node& entry : m_nodes)
        {
            if (!entry.used || !entry.group || entry.root != source)
                continue;

            if (active == k_no_panel)
                active = entry.active;

            for (std::uint32_t i = 0; i < entry.tab_count; ++i)
                members[count++] = entry.tabs[i];
        }

        if (count == 0 || !apply_drop(members[0], target, index))
            return false;

        const node_id group = m_panels[members[0]].group;

        for (std::uint32_t i = 1; i < count; ++i)
            (void)dock_tabs(members[i], m_nodes[group].root, group, index == k_tab_end ? k_tab_end : index + i);

        if (active != k_no_panel)
            set_active(group, active);

        return true;
    }

    bool dock_model::root_used(root_id id) const noexcept
    {
        return id < k_max_roots && m_roots[id].used;
    }

    bool dock_model::root_floating(root_id id) const noexcept
    {
        return id < k_max_roots && m_roots[id].floating;
    }

    std::uint32_t dock_model::root_serial(root_id id) const noexcept
    {
        return id < k_max_roots ? m_roots[id].serial : 0;
    }

    node_id dock_model::root_node(root_id id) const noexcept
    {
        return id < k_max_roots ? m_roots[id].top : k_no_node;
    }

    void dock_model::set_root_area(root_id id, const dock_rect& screen) noexcept
    {
        if (id < k_max_roots)
            m_roots[id].area = screen;
    }

    dock_rect dock_model::root_area(root_id id) const noexcept
    {
        return id < k_max_roots ? m_roots[id].area : dock_rect{};
    }

    void dock_model::place(node_id id, const dock_rect& area, dock_place_fn fn, void* user) noexcept
    {
        node& entry = m_nodes[id];
        entry.rect = area;
        entry.bar = {};

        if (entry.group)
        {
            if (fn != nullptr)
                fn(user, id, area);

            return;
        }

        const bool across = entry.axis == dock_axis::horizontal;
        const std::int32_t total = across ? area.width : area.height;
        const std::int32_t room = std::max(total - splitter_width(), 0);
        // A child keeps its minimum while there is room for two. Otherwise they split what is left.
        const std::int32_t low = room >= 2 * min_panel_size() ? min_panel_size() : 0;
        const std::int32_t high = room - low;
        const std::int32_t first_size = std::clamp(static_cast<std::int32_t>(static_cast<float>(room) * entry.ratio), low, high);

        dock_rect a = area;
        dock_rect b = area;

        if (across)
        {
            a.width = first_size;
            entry.bar = {area.x + first_size, area.y, splitter_width(), area.height};
            b.x = area.x + first_size + splitter_width();
            b.width = room - first_size;
        }
        else
        {
            a.height = first_size;
            entry.bar = {area.x, area.y + first_size, area.width, splitter_width()};
            b.y = area.y + first_size + splitter_width();
            b.height = room - first_size;
        }

        place(entry.first, a, fn, user);
        place(entry.second, b, fn, user);
    }

    void dock_model::layout(root_id id, dock_place_fn fn, void* user) noexcept
    {
        if (id >= k_max_roots || !m_roots[id].used || m_roots[id].top == k_no_node)
            return;

        place(m_roots[id].top, {0, 0, m_roots[id].area.width, m_roots[id].area.height}, fn, user);
    }

    bool dock_model::node_used(node_id id) const noexcept
    {
        return id < k_max_nodes && m_nodes[id].used;
    }

    bool dock_model::is_group(node_id id) const noexcept
    {
        return node_used(id) && m_nodes[id].group;
    }

    std::uint32_t dock_model::node_serial(node_id id) const noexcept
    {
        return id < k_max_nodes ? m_nodes[id].serial : 0;
    }

    root_id dock_model::root_of(node_id id) const noexcept
    {
        return node_used(id) ? m_nodes[id].root : k_no_root;
    }

    dock_rect dock_model::rect_of(node_id id) const noexcept
    {
        return node_used(id) ? m_nodes[id].rect : dock_rect{};
    }

    std::uint32_t dock_model::tab_count(node_id id) const noexcept
    {
        return is_group(id) ? m_nodes[id].tab_count : 0;
    }

    panel_id dock_model::tab_at(node_id id, std::uint32_t index) const noexcept
    {
        return is_group(id) && index < m_nodes[id].tab_count ? m_nodes[id].tabs[index] : k_no_panel;
    }

    panel_id dock_model::active_tab(node_id id) const noexcept
    {
        return is_group(id) ? m_nodes[id].active : k_no_panel;
    }

    void dock_model::set_active(node_id id, panel_id panel) noexcept
    {
        if (!is_group(id))
            return;

        for (std::uint32_t i = 0; i < m_nodes[id].tab_count; ++i)
        {
            if (m_nodes[id].tabs[i] == panel)
            {
                m_nodes[id].active = panel;
                return;
            }
        }
    }

    node_id dock_model::hit_splitter(root_id owner, std::int32_t x, std::int32_t y) const noexcept
    {
        for (node_id id = 0; id < k_max_nodes; ++id)
        {
            const node& entry = m_nodes[id];

            if (!entry.used || entry.group || entry.root != owner || entry.bar.width == 0 || entry.bar.height == 0)
                continue;

            const std::int32_t grab = splitter_grab();
            const dock_rect wide{entry.bar.x - grab, entry.bar.y - grab, entry.bar.width + 2 * grab, entry.bar.height + 2 * grab};

            if (contains(wide, x, y))
                return id;
        }

        return k_no_node;
    }

    dock_axis dock_model::axis_of(node_id id) const noexcept
    {
        return node_used(id) ? m_nodes[id].axis : dock_axis::horizontal;
    }

    float dock_model::ratio_of(node_id id) const noexcept
    {
        return node_used(id) ? m_nodes[id].ratio : 0.0f;
    }

    void dock_model::drag_splitter(node_id id, std::int32_t x, std::int32_t y) noexcept
    {
        if (!node_used(id) || m_nodes[id].group)
            return;

        node& entry = m_nodes[id];
        const bool across = entry.axis == dock_axis::horizontal;
        const std::int32_t room = (across ? entry.rect.width : entry.rect.height) - splitter_width();

        if (room <= 0)
            return;

        const std::int32_t start = across ? entry.rect.x : entry.rect.y;
        const std::int32_t position = (across ? x : y) - start - splitter_width() / 2;
        entry.ratio = std::clamp(static_cast<float>(position) / static_cast<float>(room), 0.0f, 1.0f);
    }

    drop_target dock_model::find_drop(std::int32_t sx, std::int32_t sy, panel_id dragged, root_id skip_root) const noexcept
    {
        // Floating windows sit above the main one, so they are asked first.
        for (std::uint32_t step = 0; step < k_max_roots; ++step)
        {
            const root_id id = step + 1 < k_max_roots ? step + 1 : k_main_root;
            const root& entry = m_roots[id];

            if (!entry.used || id == skip_root || !contains(entry.area, sx, sy))
                continue;

            drop_target out;
            out.root = id;
            out.inside = true;

            if (entry.top == k_no_node)
            {
                out.zone = drop_zone::tabs;
                out.preview = entry.area;
                return out;
            }

            // The edge tiles of the root come first. They are not offered when the panel is already all of the root.
            const node& top = m_nodes[entry.top];
            const bool whole = top.group && top.tab_count == 1 && dragged < m_panel_count && m_panels[dragged].group == entry.top;

            if (!whole)
            {
                out.root_hint = true;
                out.root_area = entry.area;
                const drop_zone sides[4] = {drop_zone::root_left, drop_zone::root_right, drop_zone::root_top, drop_zone::root_bottom};

                for (const drop_zone side : sides)
                {
                    if (contains(root_tile(entry.area, side), sx, sy))
                    {
                        out.zone = side;
                        out.preview = root_strip(entry.area, side);
                        return out;
                    }
                }
            }

            const std::int32_t lx = sx - entry.area.x;
            const std::int32_t ly = sy - entry.area.y;

            for (node_id n = 0; n < k_max_nodes; ++n)
            {
                const node& group = m_nodes[n];

                if (!group.used || !group.group || group.root != id || !contains(group.rect, lx, ly))
                    continue;

                out.group = n;
                const dock_rect whole = shifted(group.rect, entry.area.x, entry.area.y);

                const bool lone = dragged < m_panel_count && m_panels[dragged].group == n && group.tab_count == 1;
                const dock_rect body{whole.x, whole.y + header_height(n), whole.width, whole.height - header_height(n)};

                // A group shows its compass when it is big enough to hold it and the panel has somewhere to go in it.
                if (!lone && body.width >= compass_extent() + 24 && body.height >= compass_extent() + 24)
                {
                    out.hint = true;
                    out.compass = {body.x + (body.width - compass_extent()) / 2, body.y + (body.height - compass_extent()) / 2,
                        compass_extent(), compass_extent()};
                }

                // The tab strip takes the panel as a tab. A lone panel put back on its own strip stays where it is.
                if (ly < group.rect.y + header_height(n))
                {
                    if (lone)
                    {
                        out.keep = true;
                        return out;
                    }

                    out.zone = drop_zone::tabs;
                    out.preview = whole;
                    return out;
                }

                // Only the tiles of the compass dock. Everywhere else in the body the panel floats.
                if (!out.hint)
                    return out;

                const drop_zone zones[5] = {drop_zone::tabs, drop_zone::left, drop_zone::right, drop_zone::top, drop_zone::bottom};

                for (const drop_zone zone : zones)
                {
                    if (!contains(compass_tile(out.compass, zone), sx, sy))
                        continue;

                    out.zone = zone;

                    switch (zone)
                    {
                    case drop_zone::left:
                        out.preview = {whole.x, whole.y, whole.width / 2, whole.height};
                        break;
                    case drop_zone::right:
                        out.preview = {whole.x + whole.width - whole.width / 2, whole.y, whole.width / 2, whole.height};
                        break;
                    case drop_zone::top:
                        out.preview = {body.x, body.y, body.width, body.height / 2};
                        break;
                    case drop_zone::bottom:
                        out.preview = {body.x, body.y + body.height - body.height / 2, body.width, body.height / 2};
                        break;
                    default:
                        out.preview = whole;
                        break;
                    }

                    break;
                }

                return out;
            }

            return out;
        }

        return {};
    }
}
