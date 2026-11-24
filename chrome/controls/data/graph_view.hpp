#pragma once

#include <chrome/controls/core/context_menu.hpp>
#include <chrome/controls/core/control.hpp>
#include <chrome/controls/data/graph.hpp>

#include <cstdint>

namespace chrome
{
    // Draws and edits a graph: nodes as raised cards with a coloured header and pins down the sides, joined by smooth links whose colour
    // runs from the type of the output to the type of the input. The view holds a pointer to the graph and keeps only the pan, the zoom
    // and what the mouse is doing, so several views and an inspector can look at one graph, and a material or a state machine only has
    // to read the graph.
    //
    //   Wheel: zoom round the pointer. Right button and drag, or Space or Alt and drag: pan.
    //   Right click without moving: a menu for what is under the pointer, a node, a pin, a link or empty space. Arrow keys nudge the selected nodes, or pan with none selected.
    //   Press a node: select it (Ctrl or Shift toggles) and drag to move the selected nodes. Press empty space and drag: box select.
    //   Press a pin and drag to another pin to link them; a link taken by its input end is picked up again. Let go over empty space
    //     and the create handler hears where. Green on a pin means the link can be made, red that it cannot.
    //   Press a link to select it. Double click a link to give it a knob, a round handle that carries a label, or to take it away.
    //   Drag the bar of an input that has a value, Shift for fine steps, double click for its first value.
    //   Double click empty space: the create handler is asked for a node there. Keys: Delete, F frames the selection or all,
    //     Ctrl+A selects all, Ctrl+D duplicates the selected nodes.
    //
    //   graph_view view;
    //   view.set_name("Material").set_graph(&nodes).set_height(360).set_snap(8.0f);
    //   view.set_on_change(&edited, this).set_on_select(&picked, this).set_on_create(&create_node, this);
    //   set.add(view);
    class graph_view final : public control
    {
    public:
        // Called while nodes or values are dragged (final false) and once an edit is done (final true): a node moved or removed, a link
        // made or cut, a value set.
        using change_fn = void (*)(void* user, graph_view& source, bool final) noexcept;
        // Called after the user changed which nodes or links are selected.
        using select_fn = void (*)(void* user, graph_view& source) noexcept;
        // Called when the user asks for a new node at a point of the graph (double click on empty space, with from_node 0), or lets a
        // link go over empty space (from_node and from_pin say where it came from). The handler adds the node and, if it likes, the link.
        using create_fn = void (*)(void* user, graph_view& source, float x, float y, std::uint32_t from_node, std::uint32_t from_pin) noexcept;

        // The graph shown. It has to outlive the view.
        graph_view& set_graph(graph* data) noexcept;
        [[nodiscard]] graph* data() const noexcept { return m_graph; }

        // Fits the view to every node, or to the selected ones. Before the view has a size it waits for the first paint.
        graph_view& frame_all() noexcept;
        graph_view& frame_selected() noexcept;
        [[nodiscard]] float zoom() const noexcept { return m_zoom; }

        // Nodes dropped after a drag land on a grid of this many graph units. 0 for none.
        graph_view& set_snap(float units) noexcept { m_snap = units; return *this; }
        // The height at 96 dpi, and the width, where 0 fills the row.
        graph_view& set_height(std::int32_t height) noexcept { m_height = height; return *this; }
        graph_view& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        graph_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        graph_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        graph_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        graph_view& set_on_select(select_fn handler, void* user) noexcept { m_select = handler; m_select_user = user; return *this; }
        graph_view& set_on_create(create_fn handler, void* user) noexcept { m_create = handler; m_create_user = user; return *this; }

        // What the keys do, for buttons that do the same.
        void remove_selected() noexcept;
        void duplicate_selected() noexcept;
        void select_all() noexcept;
        // Lays the nodes out in columns by how far they are from the start of the graph, in an order that keeps links short, and
        // frames the result. Links that run back (a cycle) are left out of the ordering. Calls the change handler.
        void arrange() noexcept;
        // The middle of what is on show, in graph units, for placing a new node.
        [[nodiscard]] float centre_x() const noexcept;
        [[nodiscard]] float centre_y() const noexcept;

        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] context_menu* context_menu_at(std::int32_t x, std::int32_t y) noexcept override;
        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_right_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_right_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_right_release() noexcept override;
        void on_leave() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            pan,
            // A press on a node that has not moved far enough to start moving.
            lift,
            move,
            // A press on a linked input that has not moved far enough to pick the link up.
            detach,
            link,
            value,
            box
        };

        enum class hit_kind : std::uint8_t
        {
            none,
            node,
            pin,
            value,
            link
        };

        struct hit
        {
            hit_kind kind{hit_kind::none};
            std::uint32_t node{0};
            std::uint32_t pin{0};
            std::uint32_t link{0};
        };

        struct point_f
        {
            float x{0.0f};
            float y{0.0f};
        };

        // The rows of a node: an input and an output share one, and an input with a value has a row to itself.
        struct row_plan
        {
            std::uint8_t row[graph_node_desc::k_max_pins]{};
            std::uint32_t rows{0};
        };

        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] float scale() const noexcept;
        [[nodiscard]] float sx(float graph_x) const noexcept;
        [[nodiscard]] float sy(float graph_y) const noexcept;
        [[nodiscard]] float gx(float screen_x) const noexcept;
        [[nodiscard]] float gy(float screen_y) const noexcept;
        [[nodiscard]] static row_plan plan(const graph_node& node) noexcept;
        [[nodiscard]] float node_height(const graph_node& node) const noexcept;
        // The node's box and a pin's centre, in graph units.
        [[nodiscard]] point_f pin_at(const graph_node& node, std::uint32_t pin) const noexcept;
        // The bar of a pin that has a value, in pixels. Empty when the pin has none or is linked.
        [[nodiscard]] dock_rect bar_of(const graph_node& node, std::uint32_t pin) const noexcept;
        // The two ends of a link in pixels. false when a node or pin it names is gone.
        [[nodiscard]] bool link_ends(const graph_link& link, point_f& from, point_f& to) const noexcept;
        // The curve between two points sampled into points, at most capacity of them. Returns how many.
        [[nodiscard]] std::uint32_t link_curve(const point_f& from, const point_f& to, point_f* out, std::uint32_t capacity) const noexcept;
        [[nodiscard]] float tangent_of(const point_f& from, const point_f& to) const noexcept;
        [[nodiscard]] hit hit_test(std::int32_t x, std::int32_t y) const noexcept;
        void draw_link(void* dc, const point_f& from, const point_f& to, std::uint32_t colour_from, std::uint32_t colour_to, float width, bool shadowed) const noexcept;
        void paint_node(void* dc, const graph_node& node) const noexcept;
        void paint_grid(void* dc, const dock_rect& plot, std::uint32_t base) const noexcept;
        void frame_now(bool selected_only) const noexcept;
        void zoom_at(float factor, float anchor_x, float anchor_y) noexcept;
        void notify_change(bool final) noexcept;
        void notify_select() noexcept;
        void box_select() noexcept;
        void nudge(float dx, float dy) noexcept;
        void select_link(std::uint32_t id, bool add) noexcept;
        // Selects what the selected nodes lead to, or what leads to them, all the way along the links.
        void select_reach(bool downstream) noexcept;
        void disconnect_selected() noexcept;
        void menu_chosen(std::int32_t id) noexcept;
        static void menu_chosen_static(void* user, context_menu& source, menu_item& item) noexcept;

        graph* m_graph{nullptr};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        select_fn m_select{nullptr};
        void* m_select_user{nullptr};
        create_fn m_create{nullptr};
        void* m_create_user{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_width{0};
        std::int32_t m_height{320};
        float m_snap{0.0f};

        // The graph point at the top left corner of the view, and the zoom. They change while painting, to frame what was asked for.
        mutable float m_pan_x{0.0f};
        mutable float m_pan_y{0.0f};
        mutable float m_zoom{1.0f};
        mutable bool m_frame_pending{true};
        mutable bool m_frame_selected{false};
        // While the user has not panned or zoomed, the view keeps what was framed fitted when its size changes.
        mutable bool m_auto_frame{true};
        mutable std::int32_t m_framed_width{0};
        mutable std::int32_t m_framed_height{0};

        drag_kind m_drag{drag_kind::none};
        hit m_hover{};
        hit m_press{};
        std::int32_t m_press_x{0};
        std::int32_t m_press_y{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_changed{false};
        // The link being made: where it started, and whether that was an output.
        std::uint32_t m_link_node{0};
        std::uint32_t m_link_pin{0};
        bool m_link_from_output{true};
        std::uint32_t m_detach{0};
        float m_value_origin{0.0f};
        std::uint32_t m_base_nodes[graph::k_max_nodes]{};
        std::uint32_t m_base_count{0};
        // The nodes as they were when a move began, by id.
        std::uint32_t m_orig_id[graph::k_max_nodes]{};
        float m_orig_x[graph::k_max_nodes]{};
        float m_orig_y[graph::k_max_nodes]{};
        std::uint32_t m_orig_count{0};

        // The right button: where it went down, where the pointer was last, and whether it panned, in which case no menu follows.
        hit m_right_hit{};
        std::int32_t m_right_x{0};
        std::int32_t m_right_y{0};
        bool m_right_panned{false};
        context_menu m_menu{};
        float m_menu_x{0.0f};
        float m_menu_y{0.0f};
    };
}
