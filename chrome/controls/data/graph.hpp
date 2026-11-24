#pragma once

#include <cstdint>

namespace chrome
{
    enum class pin_dir : std::uint8_t
    {
        input,
        output
    };

    // One socket of a node. Outputs take any number of links. An input takes one, which a new link replaces, unless it is multi.
    // An input can also have a value of its own, a knob the node shows as a bar until a link takes the input over.
    struct graph_pin
    {
        char name[20]{};
        // An index into the graph's types, which give the colour and say what can join; graph::k_any joins with anything.
        std::uint8_t type{0};
        pin_dir dir{pin_dir::input};
        bool multi{false};
        bool has_value{false};
        float value{0.0f};
        float reset_value{0.0f};
        float low{0.0f};
        float high{1.0f};
    };

    // What a node is made of, before it is in a graph: a title, a colour and its pins. A palette is a list of these.
    //
    //   graph_node_desc math;
    //   math.set_title("Multiply").set_colour(0x4F8FDB).input("A", 0).value(1.0f, 0.0f, 4.0f).input("B", 0).output("Result", 0);
    struct graph_node_desc
    {
        static constexpr std::uint32_t k_max_pins = 12;

        char title[32]{};
        std::uint32_t colour{0x4F8FDB};
        // At 96 dpi and zoom 1, in graph units.
        float width{176.0f};
        std::uint32_t pin_count{0};
        graph_pin pins[k_max_pins]{};

        graph_node_desc& set_title(const char* text) noexcept;
        graph_node_desc& set_colour(std::uint32_t rgb) noexcept { colour = rgb; return *this; }
        graph_node_desc& set_width(float units) noexcept { width = units; return *this; }
        // Add a pin at the end. Pins past k_max_pins are dropped.
        graph_node_desc& input(const char* name, std::uint8_t type, bool multi = false) noexcept;
        graph_node_desc& output(const char* name, std::uint8_t type) noexcept;
        // Gives the pin added last a value of its own, shown as a bar while nothing is linked to it.
        graph_node_desc& value(float initial, float low, float high) noexcept;
    };

    struct graph_node : graph_node_desc
    {
        std::uint32_t id{0};
        float x{0.0f};
        float y{0.0f};
        bool selected{false};
    };

    // A link runs from an output pin to an input pin. With a knob it shows a round handle at its middle, which carries a short label,
    // so a link can stand for something of its own, such as the condition of a transition.
    struct graph_link
    {
        std::uint32_t id{0};
        std::uint32_t from_node{0};
        std::uint32_t from_pin{0};
        std::uint32_t to_node{0};
        std::uint32_t to_pin{0};
        bool knob{false};
        bool selected{false};
        char label[24]{};
    };

    // How a view draws a graph. flow is data flow: pins down the sides of a node, linked by curves. states is a state machine: compact
    // nodes joined by arrows from node to node, where a link between two nodes in both directions keeps to its own side.
    enum class graph_style : std::uint8_t
    {
        flow,
        states
    };

    // The nodes and links, with no drawing in it, so a material or a state machine can be built, checked and walked without a view.
    // Nodes keep their order of drawing, the last on top. Ids are never reused.
    class graph
    {
    public:
        static constexpr std::uint32_t k_max_nodes = 128;
        static constexpr std::uint32_t k_max_links = 512;
        static constexpr std::uint32_t k_max_types = 16;
        static constexpr std::uint32_t k_none = 0xFFFFFFFFu;
        static constexpr std::uint8_t k_any = 0xFF;

        enum class link_check : std::uint8_t
        {
            ok,
            missing,
            same_node,
            same_direction,
            type_mismatch,
            cycle,
            full
        };

        // Whether an output of the first type may feed an input of the second. By default the types must be equal, or either be k_any.
        using compatible_fn = bool (*)(void* user, std::uint8_t from, std::uint8_t to) noexcept;

        // A graph of data flow does not allow cycles, one of states does.
        graph& set_style(graph_style style) noexcept { m_style = style; ++m_revision; return *this; }
        [[nodiscard]] graph_style style() const noexcept { return m_style; }
        graph& set_allow_cycles(bool allow) noexcept { m_allow_cycles = allow; return *this; }
        [[nodiscard]] bool allow_cycles() const noexcept { return m_allow_cycles; }
        graph& set_compatible(compatible_fn handler, void* user) noexcept { m_compatible = handler; m_compatible_user = user; return *this; }
        // Names a type of pin and gives it a colour 0xRRGGBB. The name has to outlive the graph.
        graph& set_type(std::uint8_t index, const char* name, std::uint32_t colour) noexcept;
        [[nodiscard]] bool has_type(std::uint8_t type) const noexcept { return type < k_max_types && m_type_name[type] != nullptr; }
        [[nodiscard]] std::uint32_t type_colour(std::uint8_t type) const noexcept;
        [[nodiscard]] const char* type_name(std::uint8_t type) const noexcept;

        void clear() noexcept;

        // Nodes. add_node returns the id, or 0 when full.
        [[nodiscard]] std::uint32_t add_node(const graph_node_desc& desc, float x, float y) noexcept;
        bool remove_node(std::uint32_t id) noexcept;
        [[nodiscard]] std::uint32_t node_count() const noexcept { return m_node_count; }
        [[nodiscard]] graph_node& node_at(std::uint32_t index) noexcept { return m_nodes[index]; }
        [[nodiscard]] const graph_node& node_at(std::uint32_t index) const noexcept { return m_nodes[index]; }
        [[nodiscard]] graph_node* find_node(std::uint32_t id) noexcept;
        [[nodiscard]] const graph_node* find_node(std::uint32_t id) const noexcept;
        // Where the node is in the order, or -1.
        [[nodiscard]] std::int32_t index_of(std::uint32_t id) const noexcept;
        // Moves a node to the top.
        void raise(std::uint32_t id) noexcept;
        // Sets the title; the text is cut to fit.
        void set_title(std::uint32_t id, const char* text) noexcept;

        // Links. The ends may be given in either order. A new link into an input that takes one replaces the old.
        [[nodiscard]] link_check check_link(std::uint32_t node_a, std::uint32_t pin_a, std::uint32_t node_b, std::uint32_t pin_b) const noexcept;
        // The id of the new link, or 0 when check_link refuses it.
        [[nodiscard]] std::uint32_t add_link(std::uint32_t node_a, std::uint32_t pin_a, std::uint32_t node_b, std::uint32_t pin_b) noexcept;
        bool remove_link(std::uint32_t id) noexcept;
        [[nodiscard]] std::uint32_t link_count() const noexcept { return m_link_count; }
        [[nodiscard]] graph_link& link_at(std::uint32_t index) noexcept { return m_links[index]; }
        [[nodiscard]] const graph_link& link_at(std::uint32_t index) const noexcept { return m_links[index]; }
        [[nodiscard]] graph_link* find_link(std::uint32_t id) noexcept;
        [[nodiscard]] const graph_link* find_link(std::uint32_t id) const noexcept;
        // The first link that ends at, or starts at, a pin. Null when none.
        [[nodiscard]] const graph_link* link_into(std::uint32_t node, std::uint32_t pin) const noexcept;
        [[nodiscard]] const graph_link* link_from(std::uint32_t node, std::uint32_t pin) const noexcept;
        [[nodiscard]] bool pin_linked(std::uint32_t node, std::uint32_t pin) const noexcept;
        void set_link_knob(std::uint32_t id, bool knob, const char* label = nullptr) noexcept;

        // Selection, kept in the nodes and links, so views of one graph agree.
        void select_all() noexcept;
        void clear_selection() noexcept;
        [[nodiscard]] std::uint32_t selected_nodes() const noexcept;
        [[nodiscard]] std::uint32_t selected_links() const noexcept;
        // Removes what is selected, and the links of the nodes. Returns how many nodes and links went.
        std::uint32_t remove_selected() noexcept;
        // Copies the selected nodes, and the links between them, a little aside. The copies become the selection. Returns how many nodes.
        std::uint32_t duplicate_selected(float dx, float dy) noexcept;

        // The ids of the nodes in an order that puts every node after the ones that feed it, for evaluating a graph of data flow. Returns
        // how many were written (up to capacity), or k_none when there is a cycle.
        [[nodiscard]] std::uint32_t evaluation_order(std::uint32_t* ids, std::uint32_t capacity) const noexcept;

        // Changes with every edit of the structure, a value or a title, so a cache can tell it is stale.
        [[nodiscard]] std::uint32_t revision() const noexcept { return m_revision; }
        void touch() noexcept { ++m_revision; }

    private:
        [[nodiscard]] bool types_match(std::uint8_t from, std::uint8_t to) const noexcept;
        // true when a path of links leads from one node to another.
        [[nodiscard]] bool reaches(std::uint32_t from, std::uint32_t to) const noexcept;

        graph_node m_nodes[k_max_nodes]{};
        graph_link m_links[k_max_links]{};
        std::uint32_t m_node_count{0};
        std::uint32_t m_link_count{0};
        std::uint32_t m_next_id{1};
        std::uint32_t m_revision{0};
        bool m_allow_cycles{false};
        graph_style m_style{graph_style::flow};
        compatible_fn m_compatible{nullptr};
        void* m_compatible_user{nullptr};
        const char* m_type_name[k_max_types]{};
        std::uint32_t m_type_colour[k_max_types]{};
    };
}
