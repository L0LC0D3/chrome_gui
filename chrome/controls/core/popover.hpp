#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/layout.hpp>

#include <cstdint>

namespace chrome
{
    // Which side of the anchor the popover opens on.
    enum class popover_side : std::uint8_t
    {
        // Below if there is room, otherwise above, otherwise below anyway.
        automatic,
        below,
        above,
        right,
        left
    };

    // A small window of controls that opens over the others beside something, such as a button, and goes away on its own.
    // It is for the things too big for a dropdown and too small for a dialog: a few settings, a filter, a quick form.
    //
    // It opens next to an anchor, on the side that has room, inside the bounds, with a small arrow pointing at the anchor.
    // It takes the keyboard focus while it is open, so Tab goes round the controls in it, and Escape or a click anywhere
    // else closes it and gives the focus back to what had it. A popover that is pinned ignores the click, and stays until
    // it is closed in code or with Escape.
    //
    // The controls in it are added to the popover, which lays them out as a container does (column, row, grid), and not to
    // the panel's set. A control in it that opens something of its own, such as a dropdown, has to have that inside the
    // popover. The popover is added to the panel's set, with attach, after the controls it opens from.
    //
    //   popover filter;
    //   filter.attach(set).set_size(240, 0);
    //   filter.add(show_hidden);
    //   filter.add(opacity);
    //   // in the click handler of the button that opens it:
    //   filter.toggle(button.area(), bounds);
    class popover final : public control
    {
    public:
        // Called after the popover closed, whatever the reason.
        using close_fn = void (*)(void* user, popover& source) noexcept;

        popover() noexcept;
        popover(const popover&) = delete;
        popover& operator=(const popover&) = delete;

        // Puts the popover in a panel's set, which it takes the focus through. Do it once, before anything else.
        popover& attach(control_set& set) noexcept;

        // A control inside, which the popover owns the place of. The layout slot says how it sits, as in a container.
        layout_slot& add(control& item) noexcept;
        // The container the controls are laid out by, for its kind, gap and padding.
        [[nodiscard]] layout_container& body() noexcept { return m_body; }

        // The width and height of the popover at 96 dpi. 0 for either fits the controls in it, up to the most width.
        popover& set_size(std::int32_t width, std::int32_t height) noexcept { m_width = width; m_height = height; return *this; }
        popover& set_max_width(std::int32_t width) noexcept { m_max_width = width; return *this; }
        popover& set_side(popover_side side) noexcept { m_side = side; return *this; }
        // The small arrow at the edge nearest the anchor. On by default.
        popover& set_arrow(bool arrow) noexcept { m_arrow = arrow; return *this; }
        // Whether a click outside closes it. On by default. Escape closes it either way.
        popover& set_dismissable(bool dismissable) noexcept { m_dismissable = dismissable; return *this; }
        popover& set_on_close(close_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        // Opens beside an area, inside the bounds, both in the coordinates of the controls. An empty bounds area, the
        // default, means there is no limit.
        void open(const dock_rect& anchor, const dock_rect& bounds = {}) noexcept;
        // Closes it if it is open, and opens it if not. Right after a click outside has closed it, that same click on the
        // anchor does not open it again, so the anchor works as a switch.
        void toggle(const dock_rect& anchor, const dock_rect& bounds = {}) noexcept;
        // Closes it, and gives the focus back to the control that had it when it opened.
        void close() noexcept { close_now(true); }
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        // Moves it when its anchor moved, such as in a scrolled panel, or the bounds changed.
        void set_anchor(const dock_rect& anchor, const dock_rect& bounds = {}) noexcept;

        // Where it is, in the coordinates of the controls. Empty when it is closed.
        [[nodiscard]] dock_rect frame() const noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        // The popover draws only as an overlay.
        void paint(void* dc) const noexcept override { (void)dc; }
        [[nodiscard]] bool overlay_open() const noexcept override { return m_open; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override { return frame(); }
        void paint_overlay(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_open && m_inner.animating(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        // The area of the popover and the side it ended up on, from the anchor and the bounds.
        [[nodiscard]] dock_rect place(popover_side& side) const noexcept;
        [[nodiscard]] control_size content_size() const noexcept;
        void close_now(bool give_back) noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;

        control_set* m_set{nullptr};
        mutable layout_container m_body{layout_kind::column};
        mutable control_set m_inner;
        std::int32_t m_width{240};
        std::int32_t m_height{0};
        std::int32_t m_max_width{360};
        popover_side m_side{popover_side::automatic};
        bool m_arrow{true};
        bool m_dismissable{true};
        bool m_open{false};
        dock_rect m_anchor{};
        dock_rect m_bounds{};
        // What the controls need when the size is left to fit them, as of the last paint.
        mutable control_size m_fit{0, 0};
        control* m_return{nullptr};
        std::uint64_t m_closed_at{0};
        bool m_closed_by_focus{false};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        close_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
