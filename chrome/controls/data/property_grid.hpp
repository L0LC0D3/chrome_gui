#pragma once

#include <chrome/controls/data/field_desc.hpp>
#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>

#include <cstdint>

namespace chrome
{
    // Name and value rows for the fields of components, driven by the editor's reflection: a group is a component's name
    // over one row for each of its field_desc, and the row shows the field in a form that fits its type. A float or an
    // integer is one cell, a boolean is a box, and a vec3, a quaternion (as euler degrees) and a colour are three cells,
    // the colour with a swatch. A row's name takes the left of the grid and its value the right, and the line between them
    // can be dragged. The values are read from memory on every paint, so the grid shows what is there.
    //
    // A cell is edited by a click, which opens a number field over it, or by dragging sideways on it, which changes the
    // value by one step for each pixel (Shift ten times as much, Ctrl a tenth). A box is toggled by a click. Up and Down
    // move between rows, Left and Right between cells, Enter or F2 edits or toggles, and a group is folded with Enter, Left
    // and Right. Tab in an edit goes on to the next cell.
    //
    //   property_grid inspector;
    //   inspector.set_name("Inspector").set_width(380).set_rows(14).set_on_edit(&edited, this);
    //   inspector.add_group("pose", pose_fields, 3, &pose);
    //   set.add(inspector);
    //
    // Without an edit handler the grid writes the new value into the memory itself. With one it only reports, and the owner
    // writes it, such as through the document so the edit can be undone, and may call apply for the plain write.
    class property_grid final : public control
    {
    public:
        property_grid() noexcept;

        static constexpr std::uint32_t k_max_groups = 16;
        static constexpr std::uint32_t k_max_rows = 512;

        // A field changed. value points at field_size(type) bytes in the field's own layout. final is false while a drag is
        // still going, so the owner can fold those edits into one, and true for the last or for an edit that is a single step.
        using edit_fn = void (*)(void* user, property_grid& source, std::uint32_t group, std::uint32_t field, const void* value, bool final) noexcept;

        // Adds a group at the end, and returns its place, or k_none when the grid is full. The name and the fields have to
        // outlive the grid. base is the component's memory, and may be null for a group that shows no values.
        std::int32_t add_group(const char* name, const field_desc* fields, std::uint32_t count, void* base, bool expanded = true) noexcept;
        property_grid& clear() noexcept;
        // Shows another object's memory in a group, such as when the selection changed.
        property_grid& set_base(std::uint32_t group, void* base) noexcept;

        // Writes a value into the memory of a field, with no handler and no check. For the owner of an edit handler.
        void apply(std::uint32_t group, std::uint32_t field, const void* value) noexcept;

        // The width of the grid at 96 dpi. 0 fills the row.
        property_grid& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How many rows show at once, 1 to 64.
        property_grid& set_rows(std::uint32_t rows) noexcept;
        // Where the line between names and values is, as a part of the width from 0.2 to 0.8.
        property_grid& set_split(float split) noexcept;
        property_grid& set_read_only(bool read_only) noexcept;
        // Draws the line between names and values, which is on by default. The line can be dragged either way.
        property_grid& set_show_rule(bool show) noexcept { m_show_rule = show; return *this; }
        // Shown in the middle of the grid while it has no rows. Null for nothing.
        property_grid& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        // What it is called, such as in a status line.
        property_grid& set_name(const char* name) noexcept { m_name = name; return *this; }
        property_grid& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        property_grid& set_on_edit(edit_fn handler, void* user) noexcept { m_edit = handler; m_edit_user = user; return *this; }

        static constexpr std::int32_t k_none = -1;

        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] std::uint32_t group_count() const noexcept { return m_group_count; }
        [[nodiscard]] bool read_only() const noexcept { return m_read_only; }
        [[nodiscard]] bool is_expanded(std::uint32_t group) const noexcept { return group < m_group_count && m_groups[group].expanded; }
        property_grid& set_expanded(std::uint32_t group, bool expanded) noexcept;
        // A cell is being edited with the keyboard.
        [[nodiscard]] bool editing() const noexcept { return m_editing; }
        // The field the grid is on, or k_none, and its group.
        [[nodiscard]] std::int32_t current_group() const noexcept;
        [[nodiscard]] std::int32_t current_field() const noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        // The caret of an edit blinks.
        [[nodiscard]] bool animating() const noexcept override { return m_editing; }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            bar,
            split,
            // A press on a cell that has not moved yet. It edits on release, or scrubs once the mouse has moved.
            cell,
            scrub,
            // A press inside the open edit, which belongs to the number field.
            editor
        };

        struct group
        {
            const char* name{nullptr};
            const field_desc* fields{nullptr};
            std::uint32_t count{0};
            void* base{nullptr};
            bool expanded{true};
        };

        // One row of what shows: a group's bar, or one of its fields.
        struct row
        {
            std::uint16_t group{0};
            std::int16_t field{-1};
        };

        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        void rebuild() noexcept;
        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t max_first() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        [[nodiscard]] std::int32_t row_at(std::int32_t y) const noexcept;
        // The x of the line between names and values.
        [[nodiscard]] std::int32_t split_x() const noexcept;
        // The rectangle of a row on show, and of one of its cells. A colour has its swatch first, which is not a cell.
        [[nodiscard]] dock_rect row_rect(std::int32_t row_index) const noexcept;
        [[nodiscard]] dock_rect cell_rect(std::int32_t row_index, std::uint32_t cell) const noexcept;
        [[nodiscard]] dock_rect swatch_rect(std::int32_t row_index) const noexcept;
        [[nodiscard]] std::int32_t cell_at(std::int32_t row_index, std::int32_t x, std::int32_t y) const noexcept;
        void set_first(std::int32_t first) noexcept;
        void reveal_row(std::int32_t row_index) noexcept;
        // Puts the number field of an edit over its cell, which moves with the scroll and the split.
        void place_editor() noexcept;

        [[nodiscard]] const field_desc* field_of(const row& item) const noexcept;
        [[nodiscard]] static std::uint32_t cell_count(field_type type) noexcept;
        [[nodiscard]] static bool is_number(field_type type) noexcept;
        // The value of one cell, in the units the grid shows it in.
        [[nodiscard]] bool read_cell(const row& item, std::uint32_t cell, double& value) const noexcept;
        void write_cell(const row& item, std::uint32_t cell, double value, bool final) noexcept;
        void send(const row& item, const unsigned char* bytes, bool final) noexcept;
        [[nodiscard]] static double step_of(field_type type) noexcept;
        [[nodiscard]] static std::int32_t decimals_of(field_type type) noexcept;

        void start_edit(std::int32_t row_index, std::uint32_t cell) noexcept;
        void end_edit() noexcept;
        // Ends the edit when the number field has given up the focus.
        void settle_edit() noexcept;
        void toggle_group(std::uint32_t group) noexcept;
        void toggle_box(const row& item) noexcept;
        // The next cell that can be edited, in the direction, or false at the end.
        [[nodiscard]] bool next_cell(std::int32_t direction, std::int32_t& row_index, std::uint32_t& cell) const noexcept;
        static void editor_changed(void* user, number_field& source) noexcept;

        group m_groups[k_max_groups]{};
        std::uint32_t m_group_count{0};
        row m_rows[k_max_rows]{};
        std::uint32_t m_row_count{0};

        edit_fn m_edit{nullptr};
        void* m_edit_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"Nothing to show"};
        std::int32_t m_width{380};
        std::uint32_t m_rows_shown{10};
        float m_split{0.42f};
        bool m_read_only{false};
        bool m_show_rule{true};

        std::int32_t m_first{0};
        std::int32_t m_hover_row{k_none};
        std::int32_t m_hover_cell{k_none};
        // The row and the cell the keyboard is on.
        std::int32_t m_current{k_none};
        std::uint32_t m_cell{0};
        drag_kind m_drag{drag_kind::none};
        std::int32_t m_grab{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        // Where a press on a cell began, and the cell's value then, for a drag that scrubs.
        std::int32_t m_press_x{0};
        std::int32_t m_press_row{k_none};
        std::uint32_t m_press_cell{0};
        double m_press_value{0.0};
        double m_scrub_from{0.0};

        // The number field of an edit, in a set of its own so it gets the mouse and the keys through the usual route.
        control_set m_inner;
        number_field m_editor;
        bool m_editing{false};
        std::int32_t m_edit_row{k_none};
        std::uint32_t m_edit_cell{0};
    };
}
