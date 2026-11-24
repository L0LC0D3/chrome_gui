#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/data/curve.hpp>

#include <cstdint>

namespace chrome
{
    // Draws and edits up to six curves at once on a grid of time across and value up, in a sunken well with a ruler on the top and
    // on the left. The curves keep their keys; the editor only holds pointers to them, and to a selection that other views of the
    // same curves can share.
    //
    //   Wheel: zoom both ways round the pointer. Ctrl and the wheel: time only. Shift and the wheel: value only.
    //   Press a key: select it (Ctrl or Shift adds), drag to move the selected keys (Shift holds them to one axis), and drag the
    //     handles of a selected smooth or free key to bend the curve there.
    //   Press empty space and drag: select the keys in the box. Double click: add a key on the nearest curve.
    //   Top ruler: drag to move the time cursor. With Ctrl, drag to pan in time. Left ruler: drag to pan in value.
    //   Keys: Delete removes the selected keys, F frames them (or all), Ctrl+A selects all, S smooth, L linear, T stepped, B free.
    //
    //   curve_editor editor;
    //   editor.set_name("Curves").set_height(260);
    //   editor.add_curve(position_x, "X", 0xE5534B).add_curve(position_y, "Y", 0x3FA66B);
    //   editor.frame_all().set_on_change(&edited, this);
    //   set.add(editor);
    class curve_editor final : public control
    {
    public:
        static constexpr std::uint32_t k_max_curves = 6;

        // Called while keys or handles are dragged (final false) and when the drag ends or a key is added, removed or changed
        // (final true).
        using change_fn = void (*)(void* user, curve_editor& source, bool final) noexcept;
        // Called after the user changed which keys are selected.
        using select_fn = void (*)(void* user, curve_editor& source) noexcept;
        // Called while the time cursor is dragged on the top ruler.
        using time_fn = void (*)(void* user, curve_editor& source, float time) noexcept;

        // Shows a curve, in the colour 0xRRGGBB. The curve and the name must outlive the editor. Curves are numbered in the order
        // they are added, and that number is what a shared selection uses.
        curve_editor& add_curve(curve& data, const char* name, std::uint32_t colour) noexcept;
        curve_editor& clear_curves() noexcept;
        curve_editor& set_curve_visible(std::uint32_t index, bool visible) noexcept;
        // Takes the selection from another place, which several views can share. It has to outlive the editor. Null goes back to
        // the editor's own.
        curve_editor& set_selection(key_selection* selection) noexcept;
        [[nodiscard]] key_selection& selection() const noexcept { return *m_selection; }

        // The part of the grid that shows.
        curve_editor& set_view(float time_from, float time_to, float value_from, float value_to) noexcept;
        // Fits the view to every visible key, or to the selected keys.
        curve_editor& frame_all() noexcept;
        curve_editor& frame_selected() noexcept;
        // The time cursor, a vertical line that a timeline can share. NaN, or a negative time, shows none.
        curve_editor& set_time(float time) noexcept;
        [[nodiscard]] float time() const noexcept { return m_time; }
        [[nodiscard]] float time_from() const noexcept { return m_t0; }
        [[nodiscard]] float time_to() const noexcept { return m_t1; }

        // The height at 96 dpi, and the width, where 0 fills the row.
        curve_editor& set_height(std::int32_t height) noexcept { m_height = height; return *this; }
        curve_editor& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        curve_editor& set_name(const char* name) noexcept { m_name = name; return *this; }
        curve_editor& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        curve_editor& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        curve_editor& set_on_select(select_fn handler, void* user) noexcept { m_select = handler; m_select_user = user; return *this; }
        curve_editor& set_on_time(time_fn handler, void* user) noexcept { m_time_fn = handler; m_time_user = user; return *this; }

        // Actions on the selected keys, the ones the keys of the keyboard do. They call the change handler.
        void set_mode_of_selected(key_mode mode) noexcept;
        void remove_selected() noexcept;
        void select_all() noexcept;
        void clear_selection() noexcept;

        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] std::uint32_t curve_count() const noexcept { return m_count; }

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
            pan_time,
            pan_value,
            // A press on a key that has not moved far enough to start moving.
            lift,
            move,
            handle,
            box
        };

        struct hit
        {
            std::int32_t curve{-1};
            std::int32_t key{-1};
            // 0 the key, 1 its in handle, 2 its out handle.
            std::int32_t handle{0};
        };

        struct point_f
        {
            float x{0.0f};
            float y{0.0f};
        };

        [[nodiscard]] dock_rect inner() const noexcept;
        // The grid itself, inside the rulers.
        [[nodiscard]] dock_rect plot() const noexcept;
        [[nodiscard]] float x_of(float time) const noexcept;
        [[nodiscard]] float y_of(float value) const noexcept;
        [[nodiscard]] float time_at(float x) const noexcept;
        [[nodiscard]] float value_at(float y) const noexcept;
        [[nodiscard]] point_f key_point(std::uint32_t curve_index, std::uint32_t key) const noexcept;
        // Where a handle of a key is drawn.
        [[nodiscard]] point_f handle_point(std::uint32_t curve_index, std::uint32_t key, bool out) const noexcept;
        [[nodiscard]] bool has_handles(std::uint32_t curve_index, std::uint32_t key, bool out) const noexcept;
        [[nodiscard]] hit hit_test(std::int32_t x, std::int32_t y) const noexcept;
        void zoom(float factor_time, float factor_value, float anchor_x, float anchor_y) noexcept;
        void notify_change(bool final) noexcept;
        void notify_select() noexcept;
        void fix_view() noexcept;
        void box_select() noexcept;
        void add_key_at(std::int32_t x, std::int32_t y) noexcept;
        void apply_move(std::int32_t x, std::int32_t y) noexcept;
        void apply_handle(std::int32_t x, std::int32_t y) noexcept;

        struct entry
        {
            curve* data{nullptr};
            const char* name{nullptr};
            std::uint32_t colour{0xFFFFFF};
            bool visible{true};
        };

        entry m_curves[k_max_curves]{};
        std::uint32_t m_count{0};
        key_selection m_own_selection{};
        key_selection* m_selection{&m_own_selection};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        select_fn m_select{nullptr};
        void* m_select_user{nullptr};
        time_fn m_time_fn{nullptr};
        void* m_time_user{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_width{0};
        std::int32_t m_height{260};

        float m_t0{-0.1f};
        float m_t1{1.1f};
        float m_v0{-0.1f};
        float m_v1{1.1f};
        float m_time{-1.0f};

        drag_kind m_drag{drag_kind::none};
        // The selection a box select adds to.
        key_selection m_base{};
        hit m_hover{};
        hit m_press{};
        std::int32_t m_press_x{0};
        std::int32_t m_press_y{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_changed{false};
        // The keys as they were when a move began.
        float m_orig_time[k_max_curves][curve::k_max_keys]{};
        float m_orig_value[k_max_curves][curve::k_max_keys]{};
    };
}
