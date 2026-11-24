#include <chrome/controls/data/graph.hpp>

#include <algorithm>
#include <cstring>

namespace chrome
{
    namespace
    {
        template <std::size_t N>
        void copy_text(char (&out)[N], const char* text) noexcept
        {
            std::size_t length = 0;

            if (text != nullptr)
            {
                length = std::strlen(text);
                length = std::min(length, N - 1);
                std::memcpy(out, text, length);
            }

            out[length] = '\0';
        }
    }

    graph_node_desc& graph_node_desc::set_title(const char* text) noexcept
    {
        copy_text(title, text);
        return *this;
    }

    graph_node_desc& graph_node_desc::input(const char* name, std::uint8_t type, bool multi) noexcept
    {
        if (pin_count < k_max_pins)
        {
            graph_pin& pin = pins[pin_count++];
            pin = {};
            copy_text(pin.name, name);
            pin.type = type;
            pin.dir = pin_dir::input;
            pin.multi = multi;
        }

        return *this;
    }

    graph_node_desc& graph_node_desc::output(const char* name, std::uint8_t type) noexcept
    {
        if (pin_count < k_max_pins)
        {
            graph_pin& pin = pins[pin_count++];
            pin = {};
            copy_text(pin.name, name);
            pin.type = type;
            pin.dir = pin_dir::output;
            pin.multi = true;
        }

        return *this;
    }

    graph_node_desc& graph_node_desc::value(float initial, float low, float high) noexcept
    {
        if (pin_count > 0 && pins[pin_count - 1].dir == pin_dir::input)
        {
            graph_pin& pin = pins[pin_count - 1];
            pin.has_value = true;
            pin.low = low;
            pin.high = high > low ? high : low + 1.0f;
            pin.value = std::clamp(initial, pin.low, pin.high);
            pin.reset_value = pin.value;
        }

        return *this;
    }

    graph& graph::set_type(std::uint8_t index, const char* name, std::uint32_t colour) noexcept
    {
        if (index < k_max_types)
        {
            m_type_name[index] = name;
            m_type_colour[index] = colour;
            ++m_revision;
        }

        return *this;
    }

    std::uint32_t graph::type_colour(std::uint8_t type) const noexcept
    {
        return type < k_max_types ? m_type_colour[type] : 0x9AA0A6;
    }

    const char* graph::type_name(std::uint8_t type) const noexcept
    {
        return type < k_max_types && m_type_name[type] != nullptr ? m_type_name[type] : "any";
    }

    void graph::clear() noexcept
    {
        m_node_count = 0;
        m_link_count = 0;
        ++m_revision;
    }

    std::uint32_t graph::add_node(const graph_node_desc& desc, float x, float y) noexcept
    {
        if (m_node_count >= k_max_nodes)
            return 0;

        graph_node& node = m_nodes[m_node_count++];
        static_cast<graph_node_desc&>(node) = desc;
        node.id = m_next_id++;
        node.x = x;
        node.y = y;
        node.selected = false;
        ++m_revision;
        return node.id;
    }

    bool graph::remove_node(std::uint32_t id) noexcept
    {
        const std::int32_t index = index_of(id);

        if (index < 0)
            return false;

        std::uint32_t kept = 0;

        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].from_node != id && m_links[i].to_node != id)
                m_links[kept++] = m_links[i];
        }

        m_link_count = kept;

        for (std::uint32_t i = static_cast<std::uint32_t>(index); i + 1 < m_node_count; ++i)
            m_nodes[i] = m_nodes[i + 1];

        --m_node_count;
        ++m_revision;
        return true;
    }

    std::int32_t graph::index_of(std::uint32_t id) const noexcept
    {
        for (std::uint32_t i = 0; i < m_node_count; ++i)
        {
            if (m_nodes[i].id == id)
                return static_cast<std::int32_t>(i);
        }

        return -1;
    }

    graph_node* graph::find_node(std::uint32_t id) noexcept
    {
        const std::int32_t index = index_of(id);
        return index >= 0 ? &m_nodes[index] : nullptr;
    }

    const graph_node* graph::find_node(std::uint32_t id) const noexcept
    {
        const std::int32_t index = index_of(id);
        return index >= 0 ? &m_nodes[index] : nullptr;
    }

    void graph::raise(std::uint32_t id) noexcept
    {
        const std::int32_t index = index_of(id);

        if (index < 0 || static_cast<std::uint32_t>(index) + 1 == m_node_count)
            return;

        const graph_node moved = m_nodes[index];

        for (std::uint32_t i = static_cast<std::uint32_t>(index); i + 1 < m_node_count; ++i)
            m_nodes[i] = m_nodes[i + 1];

        m_nodes[m_node_count - 1] = moved;
    }

    void graph::set_title(std::uint32_t id, const char* text) noexcept
    {
        if (graph_node* node = find_node(id))
        {
            copy_text(node->title, text);
            ++m_revision;
        }
    }

    bool graph::types_match(std::uint8_t from, std::uint8_t to) const noexcept
    {
        if (m_compatible != nullptr)
            return m_compatible(m_compatible_user, from, to);

        return from == to || from == k_any || to == k_any;
    }

    bool graph::reaches(std::uint32_t from, std::uint32_t to) const noexcept
    {
        if (from == to)
            return true;

        bool seen[k_max_nodes]{};
        std::uint32_t stack[k_max_nodes];
        std::uint32_t top = 0;
        stack[top++] = from;
        seen[index_of(from)] = true;

        while (top > 0)
        {
            const std::uint32_t at = stack[--top];

            for (std::uint32_t i = 0; i < m_link_count; ++i)
            {
                if (m_links[i].from_node != at)
                    continue;

                if (m_links[i].to_node == to)
                    return true;

                const std::int32_t next = index_of(m_links[i].to_node);

                if (next >= 0 && !seen[next])
                {
                    seen[next] = true;
                    stack[top++] = m_links[i].to_node;
                }
            }
        }

        return false;
    }

    graph::link_check graph::check_link(std::uint32_t node_a, std::uint32_t pin_a, std::uint32_t node_b, std::uint32_t pin_b) const noexcept
    {
        const graph_node* a = find_node(node_a);
        const graph_node* b = find_node(node_b);

        if (a == nullptr || b == nullptr || pin_a >= a->pin_count || pin_b >= b->pin_count)
            return link_check::missing;

        if (node_a == node_b)
            return link_check::same_node;

        if (a->pins[pin_a].dir == b->pins[pin_b].dir)
            return link_check::same_direction;

        const bool a_is_source = a->pins[pin_a].dir == pin_dir::output;
        const graph_node& from = a_is_source ? *a : *b;
        const graph_node& to = a_is_source ? *b : *a;
        const std::uint32_t from_pin = a_is_source ? pin_a : pin_b;
        const std::uint32_t to_pin = a_is_source ? pin_b : pin_a;

        if (!types_match(from.pins[from_pin].type, to.pins[to_pin].type))
            return link_check::type_mismatch;

        // The same link again is allowed, and changes nothing.
        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].from_node == from.id && m_links[i].from_pin == from_pin && m_links[i].to_node == to.id && m_links[i].to_pin == to_pin)
                return link_check::ok;
        }

        if (!m_allow_cycles && reaches(to.id, from.id))
            return link_check::cycle;

        const bool replaces = !to.pins[to_pin].multi && link_into(to.id, to_pin) != nullptr;

        if (m_link_count >= k_max_links && !replaces)
            return link_check::full;

        return link_check::ok;
    }

    std::uint32_t graph::add_link(std::uint32_t node_a, std::uint32_t pin_a, std::uint32_t node_b, std::uint32_t pin_b) noexcept
    {
        if (check_link(node_a, pin_a, node_b, pin_b) != link_check::ok)
            return 0;

        const bool a_is_source = find_node(node_a)->pins[pin_a].dir == pin_dir::output;
        const std::uint32_t from_node = a_is_source ? node_a : node_b;
        const std::uint32_t from_pin = a_is_source ? pin_a : pin_b;
        const std::uint32_t to_node = a_is_source ? node_b : node_a;
        const std::uint32_t to_pin = a_is_source ? pin_b : pin_a;

        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].from_node == from_node && m_links[i].from_pin == from_pin && m_links[i].to_node == to_node && m_links[i].to_pin == to_pin)
                return m_links[i].id;
        }

        if (!find_node(to_node)->pins[to_pin].multi)
        {
            if (const graph_link* old = link_into(to_node, to_pin))
                remove_link(old->id);
        }

        graph_link& link = m_links[m_link_count++];
        link = {};
        link.id = m_next_id++;
        link.from_node = from_node;
        link.from_pin = from_pin;
        link.to_node = to_node;
        link.to_pin = to_pin;
        ++m_revision;
        return link.id;
    }

    bool graph::remove_link(std::uint32_t id) noexcept
    {
        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].id != id)
                continue;

            for (std::uint32_t j = i; j + 1 < m_link_count; ++j)
                m_links[j] = m_links[j + 1];

            --m_link_count;
            ++m_revision;
            return true;
        }

        return false;
    }

    graph_link* graph::find_link(std::uint32_t id) noexcept
    {
        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].id == id)
                return &m_links[i];
        }

        return nullptr;
    }

    const graph_link* graph::find_link(std::uint32_t id) const noexcept
    {
        return const_cast<graph*>(this)->find_link(id);
    }

    const graph_link* graph::link_into(std::uint32_t node, std::uint32_t pin) const noexcept
    {
        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].to_node == node && m_links[i].to_pin == pin)
                return &m_links[i];
        }

        return nullptr;
    }

    const graph_link* graph::link_from(std::uint32_t node, std::uint32_t pin) const noexcept
    {
        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            if (m_links[i].from_node == node && m_links[i].from_pin == pin)
                return &m_links[i];
        }

        return nullptr;
    }

    bool graph::pin_linked(std::uint32_t node, std::uint32_t pin) const noexcept
    {
        return link_into(node, pin) != nullptr || link_from(node, pin) != nullptr;
    }

    void graph::set_link_knob(std::uint32_t id, bool knob, const char* label) noexcept
    {
        if (graph_link* link = find_link(id))
        {
            link->knob = knob;

            if (label != nullptr)
                copy_text(link->label, label);

            ++m_revision;
        }
    }

    void graph::select_all() noexcept
    {
        for (std::uint32_t i = 0; i < m_node_count; ++i)
            m_nodes[i].selected = true;

        for (std::uint32_t i = 0; i < m_link_count; ++i)
            m_links[i].selected = false;
    }

    void graph::clear_selection() noexcept
    {
        for (std::uint32_t i = 0; i < m_node_count; ++i)
            m_nodes[i].selected = false;

        for (std::uint32_t i = 0; i < m_link_count; ++i)
            m_links[i].selected = false;
    }

    std::uint32_t graph::selected_nodes() const noexcept
    {
        std::uint32_t count = 0;

        for (std::uint32_t i = 0; i < m_node_count; ++i)
            count += m_nodes[i].selected ? 1u : 0u;

        return count;
    }

    std::uint32_t graph::selected_links() const noexcept
    {
        std::uint32_t count = 0;

        for (std::uint32_t i = 0; i < m_link_count; ++i)
            count += m_links[i].selected ? 1u : 0u;

        return count;
    }

    std::uint32_t graph::remove_selected() noexcept
    {
        std::uint32_t removed = 0;

        for (std::uint32_t i = m_link_count; i > 0; --i)
        {
            if (m_links[i - 1].selected && remove_link(m_links[i - 1].id))
                ++removed;
        }

        for (std::uint32_t i = m_node_count; i > 0; --i)
        {
            if (m_nodes[i - 1].selected && remove_node(m_nodes[i - 1].id))
                ++removed;
        }

        return removed;
    }

    std::uint32_t graph::duplicate_selected(float dx, float dy) noexcept
    {
        std::uint32_t from_id[k_max_nodes];
        std::uint32_t to_id[k_max_nodes];
        std::uint32_t made = 0;
        const std::uint32_t original_count = m_node_count;

        for (std::uint32_t i = 0; i < original_count && m_node_count < k_max_nodes; ++i)
        {
            if (!m_nodes[i].selected)
                continue;

            const graph_node copy = m_nodes[i];
            const std::uint32_t id = add_node(copy, copy.x + dx, copy.y + dy);

            if (id == 0)
                break;

            from_id[made] = copy.id;
            to_id[made] = id;
            ++made;
        }

        if (made == 0)
            return 0;

        const std::uint32_t original_links = m_link_count;

        for (std::uint32_t i = 0; i < original_links && m_link_count < k_max_links; ++i)
        {
            const graph_link link = m_links[i];
            std::uint32_t a = 0;
            std::uint32_t b = 0;

            for (std::uint32_t n = 0; n < made; ++n)
            {
                if (from_id[n] == link.from_node)
                    a = to_id[n];

                if (from_id[n] == link.to_node)
                    b = to_id[n];
            }

            if (a != 0 && b != 0)
            {
                const std::uint32_t id = add_link(a, link.from_pin, b, link.to_pin);

                if (id != 0)
                {
                    graph_link* added = find_link(id);
                    added->knob = link.knob;
                    std::memcpy(added->label, link.label, sizeof(added->label));
                }
            }
        }

        // The copies are the selection now.
        clear_selection();

        for (std::uint32_t n = 0; n < made; ++n)
            find_node(to_id[n])->selected = true;

        return made;
    }

    std::uint32_t graph::evaluation_order(std::uint32_t* ids, std::uint32_t capacity) const noexcept
    {
        std::uint32_t waiting[k_max_nodes]{};

        for (std::uint32_t i = 0; i < m_link_count; ++i)
        {
            const std::int32_t to = index_of(m_links[i].to_node);

            if (to >= 0)
                ++waiting[to];
        }

        std::uint32_t queue[k_max_nodes];
        std::uint32_t head = 0;
        std::uint32_t tail = 0;

        for (std::uint32_t i = 0; i < m_node_count; ++i)
        {
            if (waiting[i] == 0)
                queue[tail++] = i;
        }

        std::uint32_t written = 0;

        while (head < tail)
        {
            const std::uint32_t at = queue[head++];

            if (written < capacity)
                ids[written] = m_nodes[at].id;

            ++written;

            for (std::uint32_t i = 0; i < m_link_count; ++i)
            {
                if (m_links[i].from_node != m_nodes[at].id)
                    continue;

                const std::int32_t to = index_of(m_links[i].to_node);

                if (to >= 0 && --waiting[to] == 0)
                    queue[tail++] = static_cast<std::uint32_t>(to);
            }
        }

        return written == m_node_count ? std::min(written, capacity) : k_none;
    }
}
