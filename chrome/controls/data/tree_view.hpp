#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/icon_kind.hpp>
#include <chrome/controls/data/list_view.hpp>
#include <chrome/controls/data/table_view.hpp>

#include <cstdint>

namespace chrome
{
    // A hierarchy of text rows in a frame, with a chevron on each row that has children. Only the rows that show are
    // drawn. A click on a chevron opens or closes the row, a click elsewhere selects it, and a double click opens or closes
    // it and activates it. The wheel, the keys and a thin bar on the right scroll. With the keyboard focus the arrows move
    // along the rows (with Shift they extend a multi selection), Right opens a row or goes into it, Left closes it or goes
    // to its parent, Home and End go to the ends, Enter activates and Ctrl+A selects all. Closing a row that hides
    // selected rows selects the row itself.
    //
    // It can be a tree table: add_column puts fixed columns at the right of the tree, which is the first column and takes the
    // rest of the width, and a strip of titles shows over them. A cell holds text, a picture or both, and is asked for when it is
    // drawn. A column that is clickable gives a click on its cells to a handler, and the click does not select the row.
    //
    //   tree_view outline;
    //   outline.set_name("Outline").set_width(260).set_rows(10);
    //   const auto world = outline.add(tree_view::k_root, "World", true);
    //   outline.add(world, "Terrain");
    //   outline.set_on_change(&picked, this);
    //   set.add(outline);
    class tree_view final : public control
    {
    public:
        // A row of the tree, from add. The parent of the top rows is k_root.
        using node_id = std::int32_t;
        static constexpr node_id k_root = 0;
        static constexpr node_id k_none = -1;
        static constexpr std::uint32_t k_max_nodes = 1024;

        // The picture in front of a row's text: none for no picture. The colour is 0xRRGGBB, and a row that is selected draws it
        // in the colour of its text instead.
        struct row_icon
        {
            icon_kind kind{icon_kind::none};
            std::uint32_t colour{0};
        };
        // Asked for each row that is drawn, with whether it is open.
        using icon_fn = row_icon (*)(void* user, node_id node, bool expanded) noexcept;

        // What a cell of a column shows: text (UTF-8, valid until the next call), a picture, or both with the picture first.
        struct cell_content
        {
            const char* text{nullptr};
            row_icon icon{};
        };
        // Asked for each cell that is drawn.
        using cell_fn = void (*)(void* user, node_id node, std::uint32_t column, cell_content& out) noexcept;
        // Called after the user clicked a cell of a clickable column.
        using cell_click_fn = void (*)(void* user, tree_view& source, node_id node, std::uint32_t column) noexcept;

        // Asked for each row that is drawn: true to draw it dimmed, as for something that is hidden. The chevron and the cells dim too.
        using dim_fn = bool (*)(void* user, node_id node) noexcept;

        static constexpr std::uint32_t k_max_columns = 4;

        // Called after the user changed the selection. Choosing from code does not call it.
        using change_fn = void (*)(void* user, tree_view& source) noexcept;
        // Called for a double click on a row, or Enter on the current one.
        using activate_fn = void (*)(void* user, tree_view& source, node_id node) noexcept;
        // Called after the user opened or closed a row.
        using toggle_fn = void (*)(void* user, tree_view& source, node_id node) noexcept;
        // Fills in what a drag of the selected rows carries. false to refuse the drag.
        using drag_fn = bool (*)(void* user, tree_view& source, drag_payload& payload) noexcept;

        // Asked about a drag over the tree. over is the row under the pointer, or k_root for the empty part below the rows. With commit
        // false it only answers with the effect a drop there would have, or none, and the row is outlined when it is not none. With
        // commit true the drop happens, and the answer is the effect performed.
        using drop_fn = drop_effect (*)(void* user, tree_view& target, const drag_event& event, node_id over, bool commit) noexcept;

        // Adds a row as the last child of a parent, and returns it, or k_none when the tree is full or the parent is not
        // one. The text is UTF-8 and is not copied, so it has to outlive the tree.
        node_id add(node_id parent, const char* text, bool expanded = false) noexcept;
        // Takes every row away.
        tree_view& clear() noexcept;

        // Shows the strip of titles over the rows, with this title over the tree. Null takes the strip away, unless there are columns.
        tree_view& set_title(const char* title) noexcept;
        // Adds a column at the right of the ones there are, width at 96 dpi. false when the tree has all it can hold. The title is UTF-8
        // and has to outlive the tree. A column with a header icon shows it in place of the title. Clickable columns report clicks.
        bool add_column(const char* title, std::int32_t width, column_align align = column_align::left, icon_kind header_icon = icon_kind::none,
            bool clickable = false) noexcept;
        tree_view& clear_columns() noexcept;
        tree_view& set_cell_source(cell_fn source, void* user) noexcept { m_cell = source; m_cell_user = user; invalidate(); return *this; }
        tree_view& set_dim_source(dim_fn source, void* user) noexcept { m_dim = source; m_dim_user = user; invalidate(); return *this; }
        tree_view& set_on_cell_click(cell_click_fn handler, void* user) noexcept { m_cell_click = handler; m_cell_click_user = user; return *this; }
        [[nodiscard]] std::uint32_t column_count() const noexcept { return m_columns; }

        tree_view& set_select_mode(list_select_mode mode) noexcept;
        // The width of the tree at 96 dpi. 0 fills the row.
        tree_view& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How many rows show at once, 1 to 64.
        tree_view& set_rows(std::uint32_t rows) noexcept;
        // Shown in the middle of the tree while it has no rows. Null for nothing.
        tree_view& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        // What it is called, such as in a status line.
        tree_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        tree_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        tree_view& set_icon_source(icon_fn source, void* user) noexcept { m_icon = source; m_icon_user = user; invalidate(); return *this; }
        tree_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        tree_view& set_on_activate(activate_fn handler, void* user) noexcept { m_activate = handler; m_activate_user = user; return *this; }
        tree_view& set_on_toggle(toggle_fn handler, void* user) noexcept { m_toggle = handler; m_toggle_user = user; return *this; }
        // Makes the rows something to drag out of the tree. A press on a selected row keeps the selection, so several rows can be
        // taken together, and moving the pointer a few pixels starts the drag. A press with Ctrl or Shift still selects, and sweeps.
        tree_view& set_drag_source(drag_fn handler, void* user) noexcept { m_drag_fn = handler; m_drag_user = user; return *this; }

        // Makes the tree take drops. The edges scroll it while a drag is held there.
        tree_view& set_drop_target(drop_fn handler, void* user) noexcept { m_drop_fn = handler; m_drop_user = user; return *this; }
        // The row at a height in the coordinates of the control, or k_none when there is none there.
        [[nodiscard]] node_id node_at(std::int32_t y) const noexcept;
        // Where the text of a row is, from its start to the end of the row, in the coordinates of the control. Empty while the row
        // is not on show: scrolled out, or under a closed row. For something that edits the name in place.
        [[nodiscard]] dock_rect label_area(node_id node) const noexcept;

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] list_select_mode select_mode() const noexcept { return m_mode; }
        [[nodiscard]] bool valid(node_id node) const noexcept { return node > k_root && node <= static_cast<node_id>(m_count); }
        [[nodiscard]] const char* text(node_id node) const noexcept { return valid(node) ? m_nodes[node].text : nullptr; }
        [[nodiscard]] node_id parent_of(node_id node) const noexcept { return valid(node) && m_nodes[node].parent != k_root ? m_nodes[node].parent : k_none; }
        [[nodiscard]] bool has_children(node_id node) const noexcept { return valid(node) && m_nodes[node].first_child != k_none; }
        [[nodiscard]] bool is_expanded(node_id node) const noexcept { return valid(node) && m_nodes[node].expanded; }
        // How many rows show, which is those whose parents are all open.
        [[nodiscard]] std::uint32_t visible_count() const noexcept { return m_visible; }

        // Opening and closing from code. They do not call the toggle handler, and closing keeps the selection as it is.
        tree_view& set_expanded(node_id node, bool expanded) noexcept;
        tree_view& expand_all() noexcept;
        tree_view& collapse_all() noexcept;

        // Selection from code. None of these call the change handler.
        // Selects one row and nothing else, opens the rows above it, and brings it into view. k_none selects nothing.
        tree_view& select(node_id node) noexcept;
        tree_view& add_to_selection(node_id node) noexcept;
        tree_view& select_all() noexcept;
        tree_view& clear_selection() noexcept;
        [[nodiscard]] bool is_selected(node_id node) const noexcept { return valid(node) && m_nodes[node].selected; }
        [[nodiscard]] std::uint32_t selected_count() const noexcept { return m_selected_count; }
        // The first selected row in the order they show, or k_none.
        [[nodiscard]] node_id first_selected() const noexcept;
        // The row the keyboard is on, or k_none.
        [[nodiscard]] node_id current() const noexcept { return m_current; }
        // Scrolls the least that brings a row into view. A row that is hidden under a closed one stays hidden.
        void reveal(node_id node) noexcept;
        // The row that shows first, and a way to go back to one after the tree was made again.
        [[nodiscard]] std::int32_t scroll_position() const noexcept { return m_first; }
        void set_scroll_position(std::int32_t row) noexcept { set_first(row); }

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
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool is_drop_target() const noexcept override { return m_drop_fn != nullptr; }
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            rows,
            bar,
            // A press on a row that may be dragged out, before it has moved far enough to start.
            lift
        };

        struct node
        {
            const char* text{nullptr};
            node_id parent{k_none};
            node_id first_child{k_none};
            node_id last_child{k_none};
            node_id next{k_none};
            // How deep it is below the top rows, set when the visible rows are made.
            std::int32_t depth{0};
            // Where it shows, or k_none while a closed row above it hides it.
            std::int32_t row{k_none};
            bool expanded{false};
            bool selected{false};
        };

        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        struct column
        {
            const char* title{nullptr};
            std::int32_t width{60};
            column_align align{column_align::left};
            icon_kind header_icon{icon_kind::none};
            bool clickable{false};
        };

        // Makes the list of visible rows again, after a row was added, opened or closed.
        void rebuild() noexcept;
        // The inside of the frame under the strip of titles, which is where the rows are.
        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] bool has_header() const noexcept { return m_title != nullptr || m_columns != 0; }
        [[nodiscard]] std::int32_t header_height() const noexcept;
        // The width the columns take at the right of the rows, and where a column starts, in the coordinates of the panel.
        [[nodiscard]] std::int32_t columns_width() const noexcept;
        [[nodiscard]] std::int32_t column_left(std::uint32_t column) const noexcept;
        // The column at a pointer position over the rows, or k_none when it is over the tree.
        [[nodiscard]] std::int32_t column_at(std::int32_t x) const noexcept;
        void paint_header(void* dc, const dock_rect& frame) const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t max_first() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        // The row at a height, or k_none above, below or past the last. The rows are in the order they show.
        [[nodiscard]] std::int32_t row_at(std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t row_near(std::int32_t y) const noexcept;
        // The chevron's square at the left of a row, where the row is the place of a node among the visible ones.
        [[nodiscard]] dock_rect chevron_of(std::int32_t row) const noexcept;
        void set_first(std::int32_t first) noexcept;

        void set_selected(node_id node, bool on) noexcept;
        void clear_all() noexcept;
        void select_rows(std::int32_t from, std::int32_t to, bool keep) noexcept;
        bool click_row(std::int32_t row) noexcept;
        bool move_current(std::int32_t row, bool extend) noexcept;
        // Opens or closes a row for the user. Closing a row selects it when it hid a selected one.
        void toggle(node_id node) noexcept;
        void notify_change() noexcept;
        void activate(node_id node) noexcept;

        node m_nodes[k_max_nodes + 1]{};
        // The visible rows, in order, by node.
        node_id m_rows[k_max_nodes]{};
        std::uint32_t m_count{0};
        std::uint32_t m_visible{0};
        icon_fn m_icon{nullptr};
        void* m_icon_user{nullptr};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        activate_fn m_activate{nullptr};
        void* m_activate_user{nullptr};
        toggle_fn m_toggle{nullptr};
        void* m_toggle_user{nullptr};
        const char* m_title{nullptr};
        column m_cols[k_max_columns]{};
        std::uint32_t m_columns{0};
        cell_fn m_cell{nullptr};
        void* m_cell_user{nullptr};
        dim_fn m_dim{nullptr};
        void* m_dim_user{nullptr};
        cell_click_fn m_cell_click{nullptr};
        void* m_cell_click_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"No items"};
        std::int32_t m_width{260};
        std::uint32_t m_rows_shown{8};
        list_select_mode m_mode{list_select_mode::single};

        std::uint32_t m_selected_count{0};
        node_id m_current{k_none};
        node_id m_anchor{k_none};
        std::int32_t m_first{0};
        std::int32_t m_hover{k_none};
        drag_kind m_drag{drag_kind::none};
        std::int32_t m_grab{0};
        drag_fn m_drag_fn{nullptr};
        void* m_drag_user{nullptr};
        // The row a lift started on, and where the pointer was.
        std::int32_t m_lift_row{k_none};
        std::int32_t m_lift_x{0};
        std::int32_t m_lift_y{0};
        drop_fn m_drop_fn{nullptr};
        void* m_drop_user{nullptr};
        // The row a drag is over and would drop on, or k_none.
        node_id m_drop_node{k_none};
        // Set by the selection changes, so the caller can tell whether anything moved.
        bool m_changed{false};
    };
}
