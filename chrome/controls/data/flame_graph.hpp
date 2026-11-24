#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // One box of a flame_graph: a zone, at a depth of the call stack.
    struct flame_box
    {
        // Names the box from one update to the next, so a selection follows it.
        std::uint32_t id{0};
        // 0 for the outermost zone. A box sits under the box before it that is one level up.
        std::uint32_t depth{0};
        // Where the box starts and how long it is, in the unit of the graph.
        float start{0.0f};
        float length{0.0f};
        const char* label{nullptr};
        std::uint32_t colour{0x5EA6FFu};
        // Waiting, not working: drawn hatched and dim.
        bool idle{false};
        // Lines under the time in the tooltip, such as the stack that led here. May be null.
        const char* detail{nullptr};
    };

    // The call stack of a frame laid out as rows: the outermost zones on top, each row the zones called by the box above it,
    // every box as wide as its time. Boxes come in the order a depth-first walk visits them (a box, then what it calls), which
    // is also how the keys move: Left and Right to the neighbour at the same depth, Up to the caller, Down to the first callee.
    //
    // A click selects a box and a double click, or Enter, activates it. The tooltip names the box, its time and share, and the
    // stack of zones that led to it.
    //
    //   flame_graph graph;
    //   const flame_box boxes[] = {{1, 0, 0.0f, 8.0f, "frame"}, {2, 1, 0.0f, 3.0f, "update"}, {3, 1, 3.0f, 5.0f, "draw"}};
    //   graph.set_unit("ms").set_boxes(boxes, 3, 8.0f);
    //   set.add(graph);
    class flame_graph final : public control
    {
    public:
        static constexpr std::uint32_t k_max_boxes = 256;
        static constexpr std::uint32_t k_max_rows = 16;

        using pick_fn = void (*)(void* user, flame_graph& source, std::uint32_t id) noexcept;

        flame_graph& set_name(const char* name) noexcept { m_name = name; return *this; }
        // What the lengths are in, shown after them in the tooltip.
        flame_graph& set_unit(const char* unit) noexcept;
        // The height of a row at 96 dpi.
        flame_graph& set_row_height(std::int32_t height) noexcept { m_row_height = height; return *this; }
        // Rows to keep room for even when the boxes use fewer, so the height does not jump as zones come and go.
        flame_graph& set_min_rows(std::uint32_t rows) noexcept { m_min_rows = rows; return *this; }
        // What the tooltip says when the mouse is not on a box. Has to outlive the graph.
        flame_graph& set_help(const char* text) noexcept;
        flame_graph& set_on_select(pick_fn fn, void* user) noexcept { m_on_select = fn; m_on_select_user = user; return *this; }
        flame_graph& set_on_activate(pick_fn fn, void* user) noexcept { m_on_activate = fn; m_on_activate_user = user; return *this; }

        // Replaces the boxes. whole is the length the full width stands for, or 0 for the end of the last box. Boxes past
        // k_max_boxes or k_max_rows deep are dropped, and the texts are copied.
        void set_boxes(const flame_box* boxes, std::uint32_t count, float whole) noexcept;
        [[nodiscard]] std::uint32_t box_count() const noexcept { return m_count; }
        // How many rows the boxes use, at least 1, so a panel can give the graph the height it needs.
        [[nodiscard]] std::uint32_t rows() const noexcept { return m_rows; }
        // The height the graph wants for its rows, in pixels.
        [[nodiscard]] std::int32_t height_for_rows() const noexcept;

        // The selected box, which may be none. Setting it does not call the handler.
        void set_selected(std::uint32_t id) noexcept;
        void clear_selection() noexcept;
        [[nodiscard]] bool has_selection() const noexcept { return m_has_selection; }
        [[nodiscard]] std::uint32_t selected() const noexcept { return m_selected; }

        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        [[nodiscard]] dock_rect tooltip_area() const noexcept override;
        void paint(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        struct item
        {
            std::uint32_t id{0};
            std::uint32_t depth{0};
            float start{0.0f};
            float length{0.0f};
            char label[40]{};
            char detail[160]{};
            std::uint32_t colour{0};
            bool idle{false};
        };

        [[nodiscard]] dock_rect well_area() const noexcept;
        [[nodiscard]] dock_rect box_area(const item& it) const noexcept;
        [[nodiscard]] std::int32_t box_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t index_of(std::uint32_t id) const noexcept;
        void show_tooltip(std::int32_t index) noexcept;
        void select_index(std::int32_t index, bool by_user) noexcept;

        const char* m_name{nullptr};
        const char* m_help{nullptr};
        char m_unit[8]{'m', 's', '\0'};
        std::int32_t m_row_height{20};
        std::uint32_t m_min_rows{1};

        item m_items[k_max_boxes]{};
        std::uint32_t m_count{0};
        std::uint32_t m_rows{1};
        float m_whole{0.0f};

        bool m_has_selection{false};
        std::uint32_t m_selected{0};
        std::int32_t m_hover{-1};

        // The tooltip texts take turns, so a new one differs in address from the one on show and the host shows it.
        char m_tip[2][288]{};
        std::uint32_t m_tip_next{0};

        pick_fn m_on_select{nullptr};
        void* m_on_select_user{nullptr};
        pick_fn m_on_activate{nullptr};
        void* m_on_activate_user{nullptr};
    };
}
