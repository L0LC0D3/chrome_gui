#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class list_select_mode : std::uint8_t
    {
        // One row at a time.
        single,
        // Ctrl adds and removes rows, Shift takes a range, and a drag sweeps over rows.
        multi
    };

    // Where a drop lands in a list.
    enum class list_drop_mode : std::uint8_t
    {
        // Between two rows: the slot is the place a new row would take, 0 before the first row and count after the last. A line shows there.
        between,
        // On a row, which is outlined. The slot is that row, or k_none over the empty part.
        on_row
    };

    // A list of text rows in a frame, of which only the rows that show are asked for and drawn, so it holds thousands of
    // them at no more cost than ten. The rows come from an array of strings or from a function. A click selects a row,
    // a double click or Enter activates it, and the wheel, the keys and a thin bar on the right scroll it. With the
    // keyboard focus, the arrows, Home, End and the paging keys move the selection (with Shift they extend it), Ctrl+A
    // selects all and Ctrl+Space toggles the current row.
    //
    //   list_view assets;
    //   assets.set_name("Assets").set_width(240).set_rows(8).set_select_mode(list_select_mode::multi);
    //   assets.set_items(names, count).set_on_change(&picked, this).set_on_activate(&opened, this);
    //   set.add(assets);
    class list_view final : public control
    {
    public:
        static constexpr std::uint32_t k_max_items = 4096;
        static constexpr std::int32_t k_none = -1;

        // The text of a row, UTF-8, which has to stay valid while the list shows. Called for the rows on show only.
        using text_fn = const char* (*)(void* user, std::uint32_t index) noexcept;
        // Called after the user changed the selection. Choosing from code does not call it.
        using change_fn = void (*)(void* user, list_view& source) noexcept;
        // Called for a double click on a row, or Enter on the current one.
        using activate_fn = void (*)(void* user, list_view& source, std::uint32_t index) noexcept;
        // Fills in what a drag of the selected rows carries. false to refuse the drag.
        using drag_fn = bool (*)(void* user, list_view& source, drag_payload& payload) noexcept;
        // Asked about a drag over the list. slot is where it would land, as list_drop_mode says. With commit false it only answers
        // with the effect a drop here would have, or none. With commit true the drop happens, and the answer is the effect performed.
        using drop_fn = drop_effect (*)(void* user, list_view& target, const drag_event& event, std::int32_t slot, bool commit) noexcept;

        // Rows from an array of UTF-8 strings. Neither the array nor the strings are copied. The selection is cleared.
        list_view& set_items(const char* const* items, std::uint32_t count) noexcept;
        // Rows from a function, for a list that is made as it is shown. The selection is cleared.
        list_view& set_source(text_fn source, void* user, std::uint32_t count) noexcept;
        // The list got longer or shorter, with the same source. A selection past the new end is dropped.
        list_view& set_count(std::uint32_t count) noexcept;

        list_view& set_select_mode(list_select_mode mode) noexcept;
        // The width of the list at 96 dpi. 0 fills the row.
        list_view& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How many rows show at once, 1 to 64.
        list_view& set_rows(std::uint32_t rows) noexcept;
        // Every other row is a little lighter.
        list_view& set_striped(bool striped) noexcept { m_striped = striped; invalidate(); return *this; }
        // Shown in the middle of the list while it has no rows. Null for nothing.
        list_view& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        // What it is called, such as in a status line.
        list_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        list_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        list_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        list_view& set_on_activate(activate_fn handler, void* user) noexcept { m_activate = handler; m_activate_user = user; return *this; }
        // Makes the rows something to drag out of the list. A press on a selected row keeps the selection, so several rows can be
        // taken together, and moving the pointer a few pixels starts the drag. A press with Ctrl or Shift still selects, and sweeps.
        list_view& set_drag_source(drag_fn handler, void* user) noexcept { m_drag_fn = handler; m_drag_user = user; return *this; }
        // Makes the list take drops. The wheel and the edges scroll it while a drag is held there.
        list_view& set_drop_target(drop_fn handler, void* user, list_drop_mode mode = list_drop_mode::between) noexcept
        {
            m_drop_fn = handler;
            m_drop_user = user;
            m_drop_mode = mode;
            return *this;
        }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] const char* text(std::uint32_t index) const noexcept;
        [[nodiscard]] list_select_mode select_mode() const noexcept { return m_mode; }

        // Selection from code. None of these call the change handler.
        // Selects one row and nothing else, and brings it into view. k_none, or a row past the end, selects nothing.
        list_view& select(std::int32_t index) noexcept;
        // Adds a row to the selection of a multi list.
        list_view& add_to_selection(std::uint32_t index) noexcept;
        list_view& select_all() noexcept;
        list_view& clear_selection() noexcept;
        [[nodiscard]] bool is_selected(std::uint32_t index) const noexcept;
        [[nodiscard]] std::uint32_t selected_count() const noexcept { return m_selected_count; }
        // The first selected row, or k_none.
        [[nodiscard]] std::int32_t first_selected() const noexcept;
        // The row the keyboard is on, or k_none.
        [[nodiscard]] std::int32_t current() const noexcept { return m_current; }
        // The first row that shows.
        [[nodiscard]] std::int32_t first_row() const noexcept { return m_first; }
        // Scrolls the least that brings a row into view.
        void reveal(std::uint32_t index) noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }
        [[nodiscard]] bool is_drop_target() const noexcept override { return m_drop_fn != nullptr; }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        static constexpr std::uint32_t k_words = k_max_items / 64;

        enum class drag_kind : std::uint8_t
        {
            none,
            rows,
            bar,
            // A press on a row that may be dragged out, before it has moved far enough to start.
            lift
        };

        // Where the thumb of the bar is, in the coordinates of the panel.
        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        // The inside of the frame, which holds the rows and the bar.
        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t max_first() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        // The row at a height, or k_none above, below or past the last.
        [[nodiscard]] std::int32_t row_at(std::int32_t y) const noexcept;
        // The row at a height, kept to the first and last row. k_none when the list is empty.
        [[nodiscard]] std::int32_t row_near(std::int32_t y) const noexcept;
        void set_first(std::int32_t first) noexcept;
        // Where a drag at a height would land, by the drop mode.
        [[nodiscard]] std::int32_t slot_at(std::int32_t y) const noexcept;

        void set_bit(std::uint32_t index, bool on) noexcept;
        void recount() noexcept;
        // Selects the rows between two, and with keep also leaves the selection as it is.
        void select_range(std::int32_t from, std::int32_t to, bool keep) noexcept;
        // A click on a row, with the modifiers it came with. true when the selection changed.
        bool click_row(std::int32_t row) noexcept;
        // Moves the current row, extending the selection when asked. true when the selection changed.
        bool move_current(std::int32_t row, bool extend) noexcept;
        void notify_change() noexcept;
        void activate(std::int32_t row) noexcept;

        const char* const* m_items{nullptr};
        text_fn m_source{nullptr};
        void* m_source_user{nullptr};
        std::uint32_t m_count{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        activate_fn m_activate{nullptr};
        void* m_activate_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"No items"};
        std::int32_t m_width{240};
        std::uint32_t m_rows{8};
        list_select_mode m_mode{list_select_mode::single};
        bool m_striped{false};

        std::uint64_t m_bits[k_words]{};
        std::uint32_t m_selected_count{0};
        // The row the keyboard is on, the end of a range that stays put, and the first row that shows.
        std::int32_t m_current{k_none};
        std::int32_t m_anchor{k_none};
        std::int32_t m_first{0};
        std::int32_t m_hover{k_none};
        drag_kind m_drag{drag_kind::none};
        // Where in the thumb it was grabbed.
        std::int32_t m_grab{0};
        drag_fn m_drag_fn{nullptr};
        void* m_drag_user{nullptr};
        drop_fn m_drop_fn{nullptr};
        void* m_drop_user{nullptr};
        list_drop_mode m_drop_mode{list_drop_mode::between};
        // Where the drag over the list would land, or k_none.
        std::int32_t m_drop_slot{k_none};
        // The row a lift started on, and where the pointer was.
        std::int32_t m_lift_row{k_none};
        std::int32_t m_lift_x{0};
        std::int32_t m_lift_y{0};
    };
}
