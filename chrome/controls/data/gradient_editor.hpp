#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/data/gradient.hpp>

#include <cstdint>

namespace chrome
{
    // Shows and edits a gradient as Unity's gradient field does: a bar with the colours running along it, over squares where the
    // alpha lets them show through, with markers for the alpha stops above the bar and markers for the colour stops below it. The
    // gradient is held by pointer, so the same one can be shown by a preview or saved elsewhere. Colours and alphas of the selected
    // stop are set from outside with a colour swatch and number fields, which is what the handlers are for.
    //
    //   Press empty space above or below the bar: add an alpha stop or a colour stop there, with the value the gradient has at that
    //     place. Press a marker: select it. Drag a marker along the bar to move it, or far above or below to remove it.
    //   Keys: Delete removes the selected stop, Left and Right move it (Shift in tenths, Ctrl in thousandths), Up and Down change
    //     the alpha of a selected alpha stop.
    //
    //   gradient ramp;
    //   gradient_editor editor;
    //   editor.set_name("Ramp").set_gradient(&ramp).set_on_change(&edited, this);
    //   set.add(editor);
    class gradient_editor final : public control
    {
    public:
        enum class stop_kind : std::uint8_t
        {
            none,
            color,
            alpha
        };

        // Called while a stop is dragged (final false), and when a drag ends or a stop is added, removed or changed (final true).
        using change_fn = void (*)(void* user, gradient_editor& source, bool final) noexcept;
        // Called after the user changed which stop is selected.
        using select_fn = void (*)(void* user, gradient_editor& source) noexcept;

        // The gradient to show, not owned. Without one nothing is drawn.
        gradient_editor& set_gradient(gradient* data) noexcept;
        [[nodiscard]] gradient* data() const noexcept { return m_data; }

        // The selected stop, if any.
        gradient_editor& select(stop_kind kind, std::uint32_t index) noexcept;
        gradient_editor& clear_selection() noexcept;
        [[nodiscard]] stop_kind selected_kind() const noexcept { return m_kind; }
        [[nodiscard]] std::uint32_t selected_index() const noexcept { return m_index; }
        // What the selected stop holds. The setters call the change handler, the others do not.
        [[nodiscard]] float selected_position() const noexcept;
        [[nodiscard]] float selected_alpha() const noexcept;
        [[nodiscard]] color_rgba selected_color() const noexcept;
        void set_selected_position(float position) noexcept;
        void set_selected_alpha(float alpha) noexcept;
        void set_selected_color(const color_rgba& color) noexcept;
        void remove_selected() noexcept;

        // The width at 96 dpi, where 0 fills the row.
        gradient_editor& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        gradient_editor& set_name(const char* name) noexcept { m_name = name; return *this; }
        gradient_editor& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        gradient_editor& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        gradient_editor& set_on_select(select_fn handler, void* user) noexcept { m_select = handler; m_select_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        struct hit
        {
            stop_kind kind{stop_kind::none};
            std::uint32_t index{0};

            [[nodiscard]] friend constexpr bool operator==(const hit&, const hit&) noexcept = default;
        };

        // The bar itself, which the stops are placed along.
        [[nodiscard]] dock_rect bar() const noexcept;
        [[nodiscard]] float x_of(float position) const noexcept;
        [[nodiscard]] float position_at(std::int32_t x) const noexcept;
        [[nodiscard]] hit hit_test(std::int32_t x, std::int32_t y) const noexcept;
        void paint_bar(void* dc, const dock_rect& inside) const noexcept;
        void paint_marker(void* dc, stop_kind kind, std::uint32_t index, bool selected, bool hovered, bool fading) const noexcept;
        void set_selected(stop_kind kind, std::uint32_t index) noexcept;
        void notify_change(bool final) noexcept;
        void notify_select() noexcept;
        void move_selected_to(float position) noexcept;

        gradient* m_data{nullptr};
        stop_kind m_kind{stop_kind::none};
        std::uint32_t m_index{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        select_fn m_select{nullptr};
        void* m_select_user{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_width{0};

        bool m_dragging{false};
        // The pointer is far enough from the row that letting go removes the stop.
        bool m_removing{false};
        bool m_changed{false};
        std::int32_t m_grab{0};
        hit m_hover{};
    };
}
