#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/data/curve.hpp>

#include <cstdint>

namespace chrome
{
    // How the ruler counts.
    enum class time_display : std::uint8_t
    {
        seconds,
        frames
    };

    // A dope sheet: a row for each animated value, with its keys as diamonds along a time ruler, and a cursor that marks the moment
    // being looked at. It holds pointers to the curves, which the curve editor can show too, and to a selection the two can share.
    // Rows are numbered in the order they are added, and the curve editor must number its curves the same way to share a selection.
    //
    //   Ruler: drag to move the time cursor, which snaps to frames. With Ctrl, drag to pan.
    //   Press a diamond: select it (Ctrl or Shift adds), drag to move the selected keys in time, snapped to frames.
    //   Press empty space and drag: select the keys in the box. Double click: add a key to that row. A name selects its row's keys.
    //   Wheel: scroll the rows. Ctrl and the wheel: zoom time round the pointer. Shift and the wheel: pan.
    //   Keys: Delete removes the selected keys, F frames them all, Ctrl+A selects all, Left and Right step the cursor a frame
    //     (with Ctrl, to the previous or next key).
    //
    //   timeline_view timeline;
    //   timeline.set_name("Timeline").set_rows(4).set_fps(30.0f);
    //   timeline.add_track(position_x, "Position X", 0xE5534B).add_track(position_y, "Position Y", 0x3FA66B);
    //   timeline.set_range(0.0f, 2.0f).frame_all().set_on_time(&scrubbed, this);
    //   set.add(timeline);
    class timeline_view final : public control
    {
    public:
        static constexpr std::uint32_t k_max_tracks = 16;

        // Called while keys are dragged (final false) and when the drag ends or a key is added or removed (final true).
        using change_fn = void (*)(void* user, timeline_view& source, bool final) noexcept;
        using select_fn = void (*)(void* user, timeline_view& source) noexcept;
        // Called while the time cursor is dragged on the ruler, or stepped with the keys.
        using time_fn = void (*)(void* user, timeline_view& source, float time) noexcept;

        // Adds a row for a curve. The curve and the name must outlive the view.
        timeline_view& add_track(curve& data, const char* name, std::uint32_t colour) noexcept;
        timeline_view& clear_tracks() noexcept;
        timeline_view& set_selection(key_selection* selection) noexcept;
        [[nodiscard]] key_selection& selection() const noexcept { return *m_selection; }

        // Frames per second, for the ruler and for snapping. 1 to 240.
        timeline_view& set_fps(float fps) noexcept;
        // Whether moved keys and the cursor land on whole frames. On by default.
        timeline_view& set_snap(bool snap) noexcept { m_snap = snap; return *this; }
        timeline_view& set_display(time_display display) noexcept { m_display = display; invalidate(); return *this; }
        timeline_view& set_time(float time) noexcept;
        [[nodiscard]] float time() const noexcept { return m_time; }
        // The span that plays, shown as a lighter band. An end not past the start shows none.
        timeline_view& set_range(float start, float end) noexcept;
        // Which time shows at the left edge of the rows, and how many logical pixels one second takes.
        timeline_view& set_view(float time_from, float pixels_per_second) noexcept;
        timeline_view& frame_all() noexcept;
        [[nodiscard]] float time_from() const noexcept { return m_t0; }
        [[nodiscard]] float pixels_per_second() const noexcept { return m_pps; }

        // How many rows show at once, 1 to 16.
        timeline_view& set_rows(std::uint32_t rows) noexcept;
        timeline_view& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // The width of the column of names at 96 dpi.
        timeline_view& set_label_width(std::int32_t width) noexcept { m_label_width = width; invalidate(); return *this; }
        timeline_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        timeline_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        timeline_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        timeline_view& set_on_select(select_fn handler, void* user) noexcept { m_select = handler; m_select_user = user; return *this; }
        timeline_view& set_on_time(time_fn handler, void* user) noexcept { m_time_fn = handler; m_time_user = user; return *this; }

        void remove_selected() noexcept;
        void select_all() noexcept;
        void clear_selection() noexcept;

        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] std::uint32_t track_count() const noexcept { return m_count; }
        [[nodiscard]] float fps() const noexcept { return m_fps; }

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
        void on_leave() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            scrub,
            pan,
            bar,
            // A press on a key that has not moved far enough to start moving.
            lift,
            move,
            box
        };

        struct hit
        {
            std::int32_t track{-1};
            std::int32_t key{-1};
        };

        [[nodiscard]] dock_rect inner() const noexcept;
        // The rows, right of the names and below the ruler, less the bar.
        [[nodiscard]] dock_rect rows_area() const noexcept;
        [[nodiscard]] std::int32_t row_height() const noexcept;
        [[nodiscard]] std::int32_t max_scroll() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] float scale() const noexcept;
        [[nodiscard]] float per_second() const noexcept;
        [[nodiscard]] float x_of(float time) const noexcept;
        [[nodiscard]] float time_at(float x) const noexcept;
        [[nodiscard]] float snapped(float time) const noexcept;
        [[nodiscard]] std::int32_t row_top(std::uint32_t row) const noexcept;
        [[nodiscard]] hit hit_test(std::int32_t x, std::int32_t y) const noexcept;
        void set_scroll(std::int32_t scroll) noexcept;
        void move_time(float time) noexcept;
        void resolve_frame() const noexcept;
        void notify_change(bool final) noexcept;
        void notify_select() noexcept;
        void box_select() noexcept;
        void apply_move(std::int32_t x) noexcept;

        struct entry
        {
            curve* data{nullptr};
            const char* name{nullptr};
            std::uint32_t colour{0xFFFFFF};
        };

        entry m_tracks[k_max_tracks]{};
        std::uint32_t m_count{0};
        key_selection m_own_selection{};
        key_selection* m_selection{&m_own_selection};
        key_selection m_base{};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        select_fn m_select{nullptr};
        void* m_select_user{nullptr};
        time_fn m_time_fn{nullptr};
        void* m_time_user{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_width{0};
        std::int32_t m_label_width{132};
        std::uint32_t m_rows{5};
        float m_fps{30.0f};
        bool m_snap{true};
        time_display m_display{time_display::seconds};
        float m_time{0.0f};
        float m_range_start{0.0f};
        float m_range_end{0.0f};
        // The view is set by framing before the control knows its size, so framing waits for the first paint and shows then.
        mutable float m_t0{-0.1f};
        mutable float m_pps{140.0f};
        mutable bool m_frame_pending{false};
        std::int32_t m_scroll{0};

        drag_kind m_drag{drag_kind::none};
        hit m_hover{};
        hit m_press{};
        std::int32_t m_press_x{0};
        std::int32_t m_press_y{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        std::int32_t m_grab{0};
        bool m_changed{false};
        float m_orig_time[k_max_tracks][curve::k_max_keys]{};
    };
}
