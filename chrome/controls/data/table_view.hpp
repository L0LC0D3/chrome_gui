#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/data/list_view.hpp>

#include <cstdint>

namespace chrome
{
    enum class column_align : std::uint8_t
    {
        left,
        right,
        center
    };

    enum class sort_direction : std::uint8_t
    {
        // The rows are in the order the source has them.
        none,
        ascending,
        descending
    };

    // A grid of text cells under a header of columns. Like the list view it asks only for the rows on show, so thousands of
    // rows cost nothing extra. A click on a column's title sorts by it: ascending, then descending, then back to the
    // source's order. The edge between two titles is dragged to change a column's width, and the last column takes any
    // room that is left over. Rows are selected as in a list view, and are told apart by the row of the source, so a
    // selection stays with its row when the sort moves it. The keys, the wheel and the bar work as in a list view.
    //
    //   table_view files;
    //   files.set_name("Files").set_width(420).set_rows(10);
    //   files.add_column("Name", 200).add_column("Size", 80, column_align::right).add_column("Kind", 100);
    //   files.set_source(&cell_text, this, count).set_on_change(&picked, this);
    //   set.add(files);
    class table_view final : public control
    {
    public:
        static constexpr std::uint32_t k_max_rows = 4096;
        static constexpr std::uint32_t k_max_columns = 12;
        static constexpr std::int32_t k_none = -1;

        // The text of one cell, UTF-8. It has to stay valid until the next call, and a sort asks for two cells at a time, so
        // a source that makes its text on the fly keeps at least a few buffers in turn.
        using cell_fn = const char* (*)(void* user, std::uint32_t row, std::uint32_t column) noexcept;
        // Orders two rows of the source by a column: below 0 when a comes first, above 0 when b does. The default
        // compares the text, with numbers by value and other text without regard to case.
        using compare_fn = std::int32_t (*)(void* user, std::uint32_t column, std::uint32_t row_a, std::uint32_t row_b) noexcept;
        // Called after the user changed the selection. Choosing from code does not call it.
        using change_fn = void (*)(void* user, table_view& source) noexcept;
        // Called for a double click on a row, or Enter on the current one, with the row of the source.
        using activate_fn = void (*)(void* user, table_view& source, std::uint32_t row) noexcept;
        // Called after a click on a title sorted the rows.
        using sort_fn = void (*)(void* user, table_view& source, std::uint32_t column, sort_direction direction) noexcept;
        // Fills in what a drag of the selected rows carries. false to refuse the drag.
        using drag_fn = bool (*)(void* user, table_view& source, drag_payload& payload) noexcept;
        // Asked while something is dragged over the table (commit false) and when it is let go (commit true). row is the row of the
        // source under the pointer, or k_none for the empty part. Returns what a drop there would do, none for nothing; a row is
        // outlined while it is not none.
        using drop_fn = drop_effect (*)(void* user, table_view& target, const drag_event& event, std::int32_t row, bool commit) noexcept;

        // Adds a column at the right. false when the table has all it can hold. The title is UTF-8 and has to outlive the
        // table, and the width is at 96 dpi.
        bool add_column(const char* title, std::int32_t width, column_align align = column_align::left, bool sortable = true) noexcept;
        table_view& clear_columns() noexcept;

        // The rows come from a function. The selection is cleared, and the sort is kept.
        table_view& set_source(cell_fn source, void* user, std::uint32_t count) noexcept;
        // The table got more or fewer rows, with the same source. A selection past the new end is dropped.
        table_view& set_count(std::uint32_t count) noexcept;
        table_view& set_compare(compare_fn compare, void* user) noexcept { m_compare = compare; m_compare_user = user; return *this; }

        table_view& set_select_mode(list_select_mode mode) noexcept;
        // The width of the table at 96 dpi. 0 fills the row.
        table_view& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How many rows show at once under the header, 1 to 64.
        table_view& set_rows(std::uint32_t rows) noexcept;
        // Every other row is a little lighter.
        table_view& set_striped(bool striped) noexcept { m_striped = striped; invalidate(); return *this; }
        // Shown in the middle of the rows while there are none. Null for nothing.
        table_view& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        // What it is called, such as in a status line.
        table_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        table_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        table_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        table_view& set_on_activate(activate_fn handler, void* user) noexcept { m_activate = handler; m_activate_user = user; return *this; }
        table_view& set_on_sort(sort_fn handler, void* user) noexcept { m_sort = handler; m_sort_user = user; return *this; }
        // Makes the rows something to drag out of the table. A press on a selected row keeps the selection, so several rows can be
        // taken together, and moving the pointer a few pixels starts the drag. A press with Ctrl or Shift still selects, and sweeps.
        table_view& set_drag_source(drag_fn handler, void* user) noexcept { m_drag_fn = handler; m_drag_user = user; return *this; }
        // Makes the table take drops over its rows.
        table_view& set_drop_target(drop_fn handler, void* user) noexcept { m_drop_fn = handler; m_drop_user = user; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] std::uint32_t column_count() const noexcept { return m_columns; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] const char* cell(std::uint32_t row, std::uint32_t column) const noexcept;
        [[nodiscard]] list_select_mode select_mode() const noexcept { return m_mode; }
        // The width of a column at 96 dpi, as the user left it.
        [[nodiscard]] std::int32_t column_width(std::uint32_t column) const noexcept { return column < m_columns ? m_cols[column].width : 0; }
        table_view& set_column_width(std::uint32_t column, std::int32_t width) noexcept;

        // Sorts by a column from code, which does not call the sort handler. A column that cannot be sorted is ignored.
        table_view& sort_by(std::uint32_t column, sort_direction direction) noexcept;
        // The column the rows are sorted by, or k_none.
        [[nodiscard]] std::int32_t sort_column() const noexcept { return m_sort_column; }
        [[nodiscard]] sort_direction direction() const noexcept { return m_sort_dir; }
        // The row of the source that shows at a place of the sorted rows, and where a row of the source shows.
        [[nodiscard]] std::uint32_t row_at_position(std::uint32_t position) const noexcept { return position < m_count ? m_order[position] : 0; }
        [[nodiscard]] std::uint32_t position_of(std::uint32_t row) const noexcept { return row < m_count ? m_position[row] : 0; }

        // Selection from code, by the row of the source. None of these call the change handler.
        // Selects one row and nothing else, and brings it into view. k_none, or a row past the end, selects nothing.
        table_view& select(std::int32_t row) noexcept;
        table_view& add_to_selection(std::uint32_t row) noexcept;
        table_view& select_all() noexcept;
        table_view& clear_selection() noexcept;
        [[nodiscard]] bool is_selected(std::uint32_t row) const noexcept;
        [[nodiscard]] std::uint32_t selected_count() const noexcept { return m_selected_count; }
        // The selected row that shows first, or k_none.
        [[nodiscard]] std::int32_t first_selected() const noexcept;
        // The row the keyboard is on, or k_none.
        [[nodiscard]] std::int32_t current() const noexcept { return m_current; }
        // Scrolls the least that brings a row into view.
        void reveal(std::uint32_t row) noexcept;
        // The row of the source at a point of the panel, or k_none.
        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        // Where a cell is, in the coordinates of the panel, for something to take its place such as a text field. Empty while the row
        // is out of view or the column is not there.
        [[nodiscard]] dock_rect cell_area(std::uint32_t row, std::uint32_t column) const noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_click() noexcept override;
        void on_release() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        [[nodiscard]] bool is_drop_target() const noexcept override { return m_drop_fn != nullptr; }
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;

    private:
        static constexpr std::uint32_t k_words = k_max_rows / 64;

        enum class drag_kind : std::uint8_t
        {
            none,
            rows,
            bar,
            resize,
            header,
            // A press on a row that may be dragged out, before it has moved far enough to start.
            lift
        };

        struct column
        {
            const char* title{nullptr};
            std::int32_t width{100};
            column_align align{column_align::left};
            bool sortable{true};
        };

        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        [[nodiscard]] dock_rect inner() const noexcept;
        // The strip of titles on top, and the part under it that holds the rows.
        [[nodiscard]] dock_rect header_area() const noexcept;
        [[nodiscard]] dock_rect body_area() const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t max_first() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        // Where a column starts and how wide it is, in pixels, in the coordinates of the panel. The last one takes the
        // room that is left.
        [[nodiscard]] std::int32_t column_left(std::uint32_t column) const noexcept;
        [[nodiscard]] std::int32_t column_pixels(std::uint32_t column) const noexcept;
        // The column whose title is at a point of the header, or k_none. edge is true near its right edge.
        [[nodiscard]] std::int32_t header_at(std::int32_t x, std::int32_t y, bool& edge) const noexcept;
        // The place in the sorted rows of the row at a height, or k_none.
        [[nodiscard]] std::int32_t position_at(std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t position_near(std::int32_t y) const noexcept;
        void set_first(std::int32_t first) noexcept;

        [[nodiscard]] std::int32_t compare_rows(std::uint32_t column, std::uint32_t a, std::uint32_t b) const noexcept;
        void sort_rows() noexcept;
        void cycle_sort(std::uint32_t column) noexcept;

        void set_bit(std::uint32_t row, bool on) noexcept;
        void recount() noexcept;
        void select_positions(std::int32_t from, std::int32_t to, bool keep) noexcept;
        bool click_position(std::int32_t position) noexcept;
        bool move_current(std::int32_t position, bool extend) noexcept;
        void notify_change() noexcept;
        void activate(std::int32_t row) noexcept;

        column m_cols[k_max_columns]{};
        std::uint32_t m_columns{0};
        cell_fn m_source{nullptr};
        void* m_source_user{nullptr};
        compare_fn m_compare{nullptr};
        void* m_compare_user{nullptr};
        std::uint32_t m_count{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        activate_fn m_activate{nullptr};
        void* m_activate_user{nullptr};
        sort_fn m_sort{nullptr};
        void* m_sort_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"No rows"};
        std::int32_t m_width{420};
        std::uint32_t m_rows{8};
        list_select_mode m_mode{list_select_mode::single};
        bool m_striped{true};

        // The rows of the source in the order they show, and where each of them is in it.
        std::uint32_t m_order[k_max_rows]{};
        std::uint32_t m_position[k_max_rows]{};
        std::uint32_t m_scratch[k_max_rows]{};
        std::int32_t m_sort_column{k_none};
        sort_direction m_sort_dir{sort_direction::none};

        std::uint64_t m_bits[k_words]{};
        std::uint32_t m_selected_count{0};
        std::int32_t m_current{k_none};
        std::int32_t m_anchor{k_none};
        std::int32_t m_first{0};
        std::int32_t m_hover{k_none};
        std::int32_t m_hover_header{k_none};
        drag_kind m_drag{drag_kind::none};
        // The column that is pressed or resized, where the mouse was, and what was grabbed.
        std::int32_t m_column{k_none};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        std::int32_t m_grab{0};
        // A title was pressed, and a release on it sorts.
        bool m_press_header{false};
        drop_fn m_drop_fn{nullptr};
        void* m_drop_user{nullptr};
        std::int32_t m_drop_row{k_none};
        drag_fn m_drag_fn{nullptr};
        void* m_drag_user{nullptr};
        // The place a lift started on, and where the pointer was.
        std::int32_t m_lift_position{k_none};
        std::int32_t m_lift_x{0};
        std::int32_t m_lift_y{0};
        bool m_changed{false};
    };
}
