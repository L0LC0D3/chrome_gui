#pragma once

#include <chrome/drag_drop.hpp>

#include <cstdint>

namespace chrome
{
    enum class mouse_event : std::uint8_t
    {
        move,
        down,
        up,
        double_click,
        wheel,
        // The mouse left the panel without a button held.
        leave
    };

    // The pointer a panel asks for over a place in it.
    enum class mouse_cursor : std::uint8_t
    {
        arrow,
        // Arrows left and right, for what is dragged sideways.
        resize_horizontal,
        // Arrows up and down, for what is dragged up or down.
        resize_vertical
    };

    struct panel_mouse
    {
        mouse_event event{mouse_event::move};
        // In the client coordinates of the panel.
        std::int32_t x{0};
        std::int32_t y{0};
        // 0 left, 1 right, 2 middle. Only meaningful for down, up and double_click.
        std::int32_t button{0};
        // Notches for wheel, positive away from the user.
        std::int32_t wheel{0};
        bool ctrl{false};
        bool shift{false};
    };

    // What a panel is made of. standard panels are painted by their content into a double-buffered surface.
    // A viewport panel is a bare window that something else draws into, such as a swapchain.
    enum class panel_kind : std::uint8_t
    {
        standard,
        viewport
    };

    // The behaviour of one panel. The dock host forwards the window's messages to it, on the UI thread.
    // Every function has a default that does nothing, so a panel overrides what it needs.
    class panel_content
    {
    public:
        virtual ~panel_content() = default;

        // dc is a Win32 HDC. The background is already the theme's panel colour, the UI font is selected and the
        // text background is transparent. Ask the host to repaint with dock_host::invalidate_panel.
        virtual void on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept { (void)dc; (void)width; (void)height; }
        virtual void on_size(std::int32_t width, std::int32_t height) noexcept { (void)width; (void)height; }
        // A press captures the mouse until the button comes up, so a drag keeps arriving outside the panel.
        virtual void on_mouse(const panel_mouse& mouse) noexcept { (void)mouse; }
        // Virtual key codes. true when the key was used, and false lets the editor's own key handler have it.
        [[nodiscard]] virtual bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
        {
            (void)key;
            (void)ctrl;
            (void)shift;
            (void)alt;
            return false;
        }

        virtual void on_char(std::uint32_t code) noexcept { (void)code; }
        // The panel got or lost keyboard focus.
        virtual void on_focus(bool gained) noexcept { (void)gained; }
        // The panel's tab is now the one on show, or it stopped being: another tab took over, or it was closed.
        virtual void on_visible(bool shown) noexcept { (void)shown; }
        // Called about every 10 ms while the panel has asked for frames with dock_host::request_frames, with a
        // millisecond clock. The panel is repainted after each call. Return true to keep getting frames.
        [[nodiscard]] virtual bool on_frame(std::uint64_t now_ms) noexcept { (void)now_ms; return false; }
        // Asked right after on_frame: did that frame change what the panel shows? If not, it is not repainted.
        [[nodiscard]] virtual bool frame_dirty() const noexcept { return true; }

        // Drag and drop. While something is dragged over the panel, on_drag_over is called as the pointer moves and about every
        // 40 ms while it rests, in the client coordinates of the panel. It answers with the effect a drop here would have, or none.
        // on_drag_leave comes when the pointer goes to another panel or the drag ends without a drop. on_drop comes on release over
        // a panel that answered with an effect, and returns the effect it performed. The defaults take nothing.
        virtual drop_effect on_drag_over(const drag_event& event) noexcept { (void)event; return drop_effect::none; }
        virtual void on_drag_leave() noexcept {}
        virtual drop_effect on_drop(const drag_event& event) noexcept { (void)event; return drop_effect::none; }
    };
}
