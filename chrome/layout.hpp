#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/dock.hpp>

#include <cstdint>

namespace chrome
{
    enum class layout_kind : std::uint8_t
    {
        // Children side by side, left to right, in one line.
        row,
        // Children one under the other.
        column,
        // Children side by side that break into further lines at the width, like text.
        wrap,
        // Children in cells, filled left to right and then down, in a set number of columns.
        grid
    };

    enum class layout_align : std::uint8_t
    {
        // The container decides: its own alignment, except that a container or a control that fills the row stretches
        // across a column or a grid cell.
        automatic,
        start,
        center,
        end,
        // Takes all the room there is on that axis.
        stretch
    };

    class layout_container;

    // How one child sits in its container. add returns it, and the setters chain.
    //
    //   row.add(name_field).set_grow(1);
    //   row.add(ok_button).set_align(layout_align::end);
    class layout_slot
    {
    public:
        // A share of the room left over along the container's main axis (row width, column height, grid column width
        // is set on the container). 0 keeps the preferred size.
        layout_slot& set_grow(float weight) noexcept { m_grow = weight > 0.0f ? weight : 0.0f; return *this; }
        // Across the main axis: vertical in a row or a wrap, horizontal in a column or a grid.
        layout_slot& set_align(layout_align align) noexcept { m_align = align; return *this; }
        // Vertical in a grid cell.
        layout_slot& set_valign(layout_align align) noexcept { m_valign = align; return *this; }
        // The columns a child takes in a grid.
        layout_slot& set_span(std::uint32_t columns) noexcept { m_span = columns > 0 ? columns : 1; return *this; }
        // Replaces the preferred width and height, in pixels. 0 keeps the preferred one.
        layout_slot& set_size(std::int32_t width, std::int32_t height) noexcept { m_width = width; m_height = height; return *this; }

    private:
        friend class layout_container;

        control* m_item{nullptr};
        layout_container* m_box{nullptr};
        float m_grow{0.0f};
        layout_align m_align{layout_align::automatic};
        layout_align m_valign{layout_align::automatic};
        std::uint32_t m_span{1};
        std::int32_t m_width{0};
        std::int32_t m_height{0};
    };

    // Places controls, and further containers, so a panel does not give each one a position. A container only sets the
    // area of its controls: it does not own them, and they have to outlive it. Sizes are in pixels, so the caller scales
    // them for the dpi. A hidden control takes no room, and neither does a container that has nothing visible in it, so
    // no gap is left for it.
    //
    //   layout_container form(layout_kind::grid);
    //   form.set_columns(2).set_gap(8).set_column_grow(1, 1);
    //   form.add(name_label);  form.add(name_field);
    //   form.add(size_label);  form.add(size_field);
    //   form.arrange(dc, {0, 0, panel_width, 0});
    class layout_container
    {
    public:
        static constexpr std::uint32_t k_max_children = 32;
        static constexpr std::uint32_t k_max_columns = 8;

        explicit layout_container(layout_kind kind = layout_kind::column) noexcept : m_kind(kind) {}

        layout_container& set_kind(layout_kind kind) noexcept { m_kind = kind; return *this; }
        // Room between children. A row spaces only across, a column only down, a wrap and a grid both ways.
        layout_container& set_gap(std::int32_t gap) noexcept { m_gap_x = gap; m_gap_y = gap; return *this; }
        layout_container& set_gaps(std::int32_t across, std::int32_t down) noexcept { m_gap_x = across; m_gap_y = down; return *this; }
        // Room inside the edges, around all the children.
        layout_container& set_padding(std::int32_t all) noexcept { return set_padding(all, all, all, all); }
        layout_container& set_padding(std::int32_t left, std::int32_t top, std::int32_t right, std::int32_t bottom) noexcept
        {
            m_pad_l = left;
            m_pad_t = top;
            m_pad_r = right;
            m_pad_b = bottom;
            return *this;
        }
        // The alignment of children whose slot says automatic, across the main axis. start when automatic.
        layout_container& set_align(layout_align align) noexcept { m_align = align; return *this; }
        // Vertical alignment in grid cells.
        layout_container& set_valign(layout_align align) noexcept { m_valign = align; return *this; }
        // Grid only.
        layout_container& set_columns(std::uint32_t columns) noexcept
        {
            m_columns = columns < 1 ? 1 : (columns > k_max_columns ? k_max_columns : columns);
            return *this;
        }
        // Grid only: a share of the width left over, for one column. 0 keeps it as wide as its widest cell.
        layout_container& set_column_grow(std::uint32_t column, float weight) noexcept
        {
            if (column < k_max_columns)
                m_column_grow[column] = weight > 0.0f ? weight : 0.0f;

            return *this;
        }
        // Grid only: every column as wide as the widest.
        layout_container& set_uniform_columns(bool uniform) noexcept { m_uniform = uniform; return *this; }
        // How much of the container shows, 0 to 1. Its children keep their places and are clipped to the part that shows,
        // and the container is that much of its height, as a folding group needs.
        layout_container& set_reveal(float reveal) noexcept { m_reveal = reveal < 0.0f ? 0.0f : (reveal > 1.0f ? 1.0f : reveal); return *this; }
        layout_container& set_visible(bool visible) noexcept { m_visible = visible; return *this; }

        [[nodiscard]] layout_kind kind() const noexcept { return m_kind; }
        [[nodiscard]] float reveal() const noexcept { return m_reveal; }
        [[nodiscard]] bool visible() const noexcept { return m_visible; }

        // Adds a control or a container at the end. Past k_max_children nothing is added and a slot that is ignored is
        // returned, so a caller never gets a bad reference. Check full() when it matters.
        layout_slot& add(control& item) noexcept;
        layout_slot& add(layout_container& box) noexcept;
        void clear() noexcept { m_count = 0; }
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] bool full() const noexcept { return m_count >= k_max_children; }

        // True when nothing in it takes room: it is hidden, or all its children are.
        [[nodiscard]] bool empty() const noexcept;

        // The size it would take at a most width, for a parent to place it. Takes the reveal into account.
        [[nodiscard]] control_size measure(void* dc, std::int32_t max_width) const noexcept;
        // Places the children in an area. The width is the width it has. The height is 0 to be as tall as the children
        // need, or more to give the room that is left to the children that grow in a column or stretch in a row.
        // Returns the size it used. Controls that are hidden get an empty area.
        control_size arrange(void* dc, const dock_rect& area) noexcept { return arrange_in(dc, area, nullptr); }
        // The same, with the controls clipped to the area, for a pane whose contents must not spill out of it.
        control_size arrange_clipped(void* dc, const dock_rect& area) noexcept { return arrange_in(dc, area, &area); }
        // Where the last arrange put it.
        [[nodiscard]] const dock_rect& area() const noexcept { return m_area; }

    private:
        struct plan;

        control_size arrange_in(void* dc, const dock_rect& area, const dock_rect* clip) noexcept;
        void blank() noexcept;
        void solve(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept;
        void solve_row(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept;
        void solve_column(void* dc, std::int32_t width, std::int32_t height, bool fit, plan& out) const noexcept;
        void solve_wrap(void* dc, std::int32_t width, bool fit, plan& out) const noexcept;
        void solve_grid(void* dc, std::int32_t width, bool fit, plan& out) const noexcept;
        [[nodiscard]] static bool is_live(const layout_slot& slot) noexcept;
        [[nodiscard]] static control_size slot_size(void* dc, const layout_slot& slot, std::int32_t max_width) noexcept;
        [[nodiscard]] layout_align across_align(const layout_slot& slot) const noexcept;
        [[nodiscard]] layout_align horizontal_align(const layout_slot& slot) const noexcept;
        [[nodiscard]] layout_align vertical_align(const layout_slot& slot) const noexcept;

        layout_kind m_kind;
        layout_align m_align{layout_align::automatic};
        layout_align m_valign{layout_align::automatic};
        std::int32_t m_gap_x{0};
        std::int32_t m_gap_y{0};
        std::int32_t m_pad_l{0};
        std::int32_t m_pad_t{0};
        std::int32_t m_pad_r{0};
        std::int32_t m_pad_b{0};
        std::uint32_t m_columns{1};
        float m_column_grow[k_max_columns]{};
        bool m_uniform{false};
        float m_reveal{1.0f};
        bool m_visible{true};
        layout_slot m_slots[k_max_children]{};
        layout_slot m_spare{};
        std::uint32_t m_count{0};
        dock_rect m_area{};
    };
}
