#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class grid_select_mode : std::uint8_t
    {
        // One tile at a time.
        single,
        // Ctrl adds and removes tiles, and Shift takes a range.
        multi
    };

    // A grid of tiles, each a picture with a name under it, in a well that scrolls. The tiles are raised cards with a soft shadow,
    // the picture sits in a recess in the card, and a tile under the pointer lifts a little. Only the tiles that show are asked
    // for and drawn, so thousands of them cost the same as a screenful. The tiles fill the width row after row, and the wheel,
    // the keys and a thin bar on the right scroll them. Ctrl and the wheel change the size of the tiles. With the keyboard
    // focus, the arrows, Home, End and the paging keys move the selection (with Shift they extend it), Ctrl+A selects all,
    // Ctrl+Space toggles the current tile and Enter activates it.
    //
    //   thumbnail_grid assets;
    //   assets.set_name("Assets").set_rows(3).set_select_mode(grid_select_mode::multi);
    //   assets.set_source(&asset_name, &asset_picture, this, count).set_on_activate(&opened, this);
    //   set.add(assets);
    class thumbnail_grid final : public control
    {
    public:
        static constexpr std::uint32_t k_max_items = 4096;
        static constexpr std::int32_t k_none = -1;

        // The name of a tile, UTF-8, which has to stay valid while the grid shows. Called for the tiles on show only.
        using label_fn = const char* (*)(void* user, std::uint32_t index) noexcept;
        // Draws the picture of a tile into the box, and must stay inside it. Called for the tiles on show only, at every
        // paint, so a picture that costs much should be kept by the caller. Without one, a tile shows a coloured card with the
        // first letter of its name.
        using picture_fn = void (*)(void* user, void* dc, std::uint32_t index, const dock_rect& box) noexcept;
        // Called after the user changed the selection. Choosing from code does not call it.
        using change_fn = void (*)(void* user, thumbnail_grid& source) noexcept;
        // Called for a double click on a tile, or Enter on the current one.
        using activate_fn = void (*)(void* user, thumbnail_grid& source, std::uint32_t index) noexcept;
        // Fills in what a drag of the selected tiles carries. false to refuse the drag.
        // Called after the user zoomed the tiles with Ctrl and the wheel. Choosing a size from code does not call it.
        using size_fn = void (*)(void* user, thumbnail_grid& source) noexcept;
        using drag_fn = bool (*)(void* user, thumbnail_grid& source, drag_payload& payload) noexcept;
        // Asked while something is dragged over the grid (commit false) and when it is let go (commit true). item is the tile under the
        // pointer, or k_none for the empty well. Returns what a drop there would do, none for nothing; a tile is lit while it is not none.
        using drop_fn = drop_effect (*)(void* user, thumbnail_grid& target, const drag_event& event, std::int32_t item, bool commit) noexcept;

        // Tiles from functions. picture may be null. The selection is cleared.
        thumbnail_grid& set_source(label_fn label, picture_fn picture, void* user, std::uint32_t count) noexcept;
        // The grid got more or fewer tiles, with the same source. A selection past the new end is dropped.
        thumbnail_grid& set_count(std::uint32_t count) noexcept;

        thumbnail_grid& set_select_mode(grid_select_mode mode) noexcept;
        // The width of the grid at 96 dpi. 0 fills the row, which is what a grid should do.
        thumbnail_grid& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How tall the grid is, in rows of tiles at the present size, 1 to 12.
        thumbnail_grid& set_rows(std::uint32_t rows) noexcept;
        // The width of a tile at 96 dpi, 56 to 200. Ctrl and the wheel do this too.
        thumbnail_grid& set_tile_size(std::int32_t size) noexcept;
        // Whether Ctrl and the wheel change the size of the tiles. On by default.
        thumbnail_grid& set_zoomable(bool zoomable) noexcept { m_zoomable = zoomable; return *this; }
        // Shown in the middle of the grid while it has no tiles. Null for nothing.
        thumbnail_grid& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        // What it is called, such as in a status line.
        thumbnail_grid& set_name(const char* name) noexcept { m_name = name; return *this; }
        thumbnail_grid& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        thumbnail_grid& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        thumbnail_grid& set_on_size(size_fn handler, void* user) noexcept { m_size_handler = handler; m_size_user = user; return *this; }
        thumbnail_grid& set_on_activate(activate_fn handler, void* user) noexcept { m_activate = handler; m_activate_user = user; return *this; }
        // Makes the tiles something to drag out of the grid. A press on a selected tile keeps the selection, so several can be
        // taken together, and moving the pointer a few pixels starts the drag.
        thumbnail_grid& set_drag_source(drag_fn handler, void* user) noexcept { m_drag_fn = handler; m_drag_user = user; return *this; }
        // Makes the grid take drops. The tile under the pointer is outlined while a drop on it would do something.
        thumbnail_grid& set_drop_target(drop_fn handler, void* user) noexcept { m_drop_fn = handler; m_drop_user = user; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] const char* label(std::uint32_t index) const noexcept;
        [[nodiscard]] grid_select_mode select_mode() const noexcept { return m_mode; }
        [[nodiscard]] std::int32_t tile_size() const noexcept { return m_tile; }

        // Selection from code. None of these call the change handler.
        // Selects one tile and nothing else, and brings it into view. k_none, or a tile past the end, selects nothing.
        thumbnail_grid& select(std::int32_t index) noexcept;
        thumbnail_grid& select_all() noexcept;
        thumbnail_grid& clear_selection() noexcept;
        [[nodiscard]] bool is_selected(std::uint32_t index) const noexcept;
        [[nodiscard]] std::uint32_t selected_count() const noexcept { return m_selected_count; }
        // The first selected tile, or k_none.
        [[nodiscard]] std::int32_t first_selected() const noexcept;
        // The tile the keyboard is on, or k_none.
        [[nodiscard]] std::int32_t current() const noexcept { return m_current; }
        // Scrolls the least that brings a tile into view.
        void reveal(std::uint32_t index) noexcept;
        // The tile at a point of the panel, or k_none.
        [[nodiscard]] std::int32_t tile_at(std::int32_t x, std::int32_t y) const noexcept { return item_at(x, y); }
        // Where the name of a tile is written, in the coordinates of the panel, for something to take its place such as a text field.
        // Empty while the tile is out of view.
        [[nodiscard]] dock_rect label_area(std::uint32_t index) const noexcept;

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
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        [[nodiscard]] bool is_drop_target() const noexcept override { return m_drop_fn != nullptr; }
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;

    private:
        static constexpr std::uint32_t k_words = k_max_items / 64;

        enum class drag_kind : std::uint8_t
        {
            none,
            bar,
            // A press on a tile that may be dragged out, before it has moved far enough to start.
            lift
        };

        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        // The inside of the frame, which holds the tiles and the bar.
        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] std::int32_t columns() const noexcept;
        [[nodiscard]] std::int32_t card_height() const noexcept;
        // The step from one row of tiles to the next.
        [[nodiscard]] std::int32_t row_step() const noexcept;
        [[nodiscard]] std::int32_t content_height() const noexcept;
        [[nodiscard]] std::int32_t max_scroll() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        // Where a tile is, in the coordinates of the panel, with the scroll taken into account.
        [[nodiscard]] dock_rect card_area(std::int32_t index) const noexcept;
        // The tile at a point, or k_none between tiles and past the last.
        [[nodiscard]] std::int32_t item_at(std::int32_t x, std::int32_t y) const noexcept;
        void set_scroll(std::int32_t scroll) noexcept;

        void set_bit(std::uint32_t index, bool on) noexcept;
        void recount() noexcept;
        void select_range(std::int32_t from, std::int32_t to, bool keep) noexcept;
        // A click on a tile, with the modifiers it came with. true when the selection changed.
        bool click_item(std::int32_t item) noexcept;
        // Moves the current tile, extending the selection when asked. true when the selection changed.
        bool move_current(std::int32_t item, bool extend) noexcept;
        void notify_change() noexcept;
        void activate(std::int32_t item) noexcept;
        void paint_card(void* dc, std::int32_t index, const dock_rect& card, bool lit) const noexcept;

        label_fn m_label{nullptr};
        picture_fn m_picture{nullptr};
        void* m_source_user{nullptr};
        std::uint32_t m_count{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        activate_fn m_activate{nullptr};
        void* m_activate_user{nullptr};
        size_fn m_size_handler{nullptr};
        void* m_size_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"Nothing here"};
        std::int32_t m_width{0};
        std::uint32_t m_rows{3};
        std::int32_t m_tile{96};
        bool m_zoomable{true};
        grid_select_mode m_mode{grid_select_mode::single};

        std::uint64_t m_bits[k_words]{};
        std::uint32_t m_selected_count{0};
        std::int32_t m_current{k_none};
        std::int32_t m_anchor{k_none};
        std::int32_t m_hover{k_none};
        // How far the tiles are scrolled up, in pixels.
        std::int32_t m_scroll{0};
        drag_kind m_drag{drag_kind::none};
        std::int32_t m_grab{0};
        drop_fn m_drop_fn{nullptr};
        void* m_drop_user{nullptr};
        std::int32_t m_drop_item{k_none};
        drag_fn m_drag_fn{nullptr};
        void* m_drag_user{nullptr};
        std::int32_t m_lift_item{k_none};
        std::int32_t m_lift_x{0};
        std::int32_t m_lift_y{0};
    };
}
