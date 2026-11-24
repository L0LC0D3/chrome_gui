#pragma once

#include <chrome/controls/core/icon_kind.hpp>
#include <chrome/controls/core/view_range.hpp>
#include <chrome/dock.hpp>
#include <chrome/panel.hpp>

#include <cstdint>

namespace chrome
{
    class context_menu;

    struct control_size
    {
        std::int32_t width{0};
        std::int32_t height{0};
    };

    // One thing a panel draws and the user can act on, such as a button. A control knows its place in the panel, how it
    // looks and what a click does, and nothing about windows: the panel gives it a device context to paint into and the
    // mouse events through a control_set.
    class control
    {
    public:
        virtual ~control() = default;

        // The size the control wants, measured with the UI font selected in dc.
        [[nodiscard]] virtual control_size preferred_size(void* dc) const noexcept = 0;
        // The size at a most width, for a control that breaks its text into lines. The default ignores the width.
        [[nodiscard]] virtual control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept
        {
            (void)max_width;
            return preferred_size(dc);
        }

        // The control takes a row of its own, as wide as the row.
        [[nodiscard]] virtual bool fills_row() const noexcept { return false; }
        // Draws the control in its area. dc is a Win32 HDC with the UI font selected and a transparent text background.
        virtual void paint(void* dc) const noexcept = 0;

        void set_area(const dock_rect& area) noexcept { m_area = area; }
        [[nodiscard]] const dock_rect& area() const noexcept { return m_area; }
        void set_enabled(bool enabled) noexcept { m_enabled = enabled; }
        [[nodiscard]] bool enabled() const noexcept { return m_enabled; }
        // A control that is not visible is not painted, hit or given the focus, and the panel leaves it out of the layout.
        void set_visible(bool visible) noexcept { m_visible = visible; }
        [[nodiscard]] bool visible() const noexcept { return m_visible; }
        // Limits where the control shows and can be hit, such as the revealed part of a group that is folding. In the
        // coordinates of the panel, like the area.
        void set_clip(const dock_rect& clip) noexcept { m_clip = clip; m_clipped = true; }
        void clear_clip() noexcept { m_clipped = false; }
        [[nodiscard]] bool clipped() const noexcept { return m_clipped; }
        [[nodiscard]] const dock_rect& clip() const noexcept { return m_clip; }
        // Nothing of the control can show: it is hidden, has no area, or lies wholly outside its clip. A culled control is
        // not painted, hit, given the focus or animated, so a folded group costs next to nothing.
        [[nodiscard]] bool culled() const noexcept
        {
            if (!m_visible || m_area.width <= 0 || m_area.height <= 0)
                return true;

            if (!m_clipped)
                return false;

            return m_area.x >= m_clip.x + m_clip.width || m_area.x + m_area.width <= m_clip.x ||
                m_area.y >= m_clip.y + m_clip.height || m_area.y + m_area.height <= m_clip.y;
        }
        // The control lies wholly outside a band of the content, so there is no point in drawing or animating it.
        [[nodiscard]] bool outside(const view_range& band) const noexcept { return !band.touches(m_area.y, m_area.height); }
        // Text shown next to the control after the mouse rests on it. UTF-8, and has to outlive the control. Null for none.
        // The menu a right click on this control asks for. Not owned. Null for none.
        void set_context_menu(context_menu* menu) noexcept { m_context_menu = menu; }
        [[nodiscard]] context_menu* context_menu_of() const noexcept { return m_context_menu; }
        // The menu a right click at a point asks for, when the button comes up. The default is the one given to set_context_menu. A
        // control whose menu depends on what is under the pointer rebuilds its own here, and may answer null for none.
        [[nodiscard]] virtual context_menu* context_menu_at(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; return m_context_menu; }
        void set_tooltip(const char* text) noexcept { m_tooltip = text; }
        // Where the tooltip points: the whole control unless a part of it, such as one segment of a bar, has a tooltip of its own.
        [[nodiscard]] virtual dock_rect tooltip_area() const noexcept { return m_area; }
        [[nodiscard]] const char* tooltip() const noexcept { return m_tooltip; }
        // The pointer over a point in the control, in the coordinates of the panel, as for the mouse. While a button is held on the control
        // it is asked wherever the mouse is.
        [[nodiscard]] virtual mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept { (void)x; (void)y; return mouse_cursor::arrow; }
        // A control can have something open over the others, such as the list of a dropdown. The set paints it after
        // every control, and hands it the mouse over its area before the controls under it.
        [[nodiscard]] virtual bool overlay_open() const noexcept { return false; }
        [[nodiscard]] virtual dock_rect overlay_area() const noexcept { return {}; }
        virtual void paint_overlay(void* dc) const noexcept { (void)dc; }
        // The control takes drops. The set asks it on_drag_over, on_drag_leave and on_drop for the pointer over it.
        [[nodiscard]] virtual bool is_drop_target() const noexcept { return false; }
        // The control scrolls by itself, such as a list, so the wheel over it goes to on_wheel before the panel gets it.
        [[nodiscard]] virtual bool takes_wheel() const noexcept { return false; }
        // True while the control is moving by itself, such as a switch sliding. The panel then asks for frames.
        [[nodiscard]] virtual bool animating() const noexcept { return false; }
        // Moves the animation to the time now, in milliseconds. true while it still has further to go.
        virtual bool animate(std::uint64_t now_ms) noexcept { (void)now_ms; return false; }
        // The control has the keyboard focus.
        [[nodiscard]] bool focused() const noexcept { return m_focused; }
        // The focus came by the keyboard, by Tab, a shortcut or code, and not by a click, so it is worth showing.
        [[nodiscard]] bool focus_visible() const noexcept { return m_focused && m_focus_visible; }
        // Where Tab goes. 0, the default, follows the order the controls were added to the set. A positive number comes
        // before all of those, the lowest first, and a negative one is left out of Tab, though a click or code can still
        // give it the focus.
        void set_tab_index(std::int32_t index) noexcept { m_tab_index = index; }
        [[nodiscard]] std::int32_t tab_index() const noexcept { return m_tab_index; }
        // The mouse is over the control.
        [[nodiscard]] bool hot() const noexcept { return m_hot && m_enabled; }
        // A mouse button is held down on the control, and the mouse is still over it.
        [[nodiscard]] bool pressed() const noexcept { return m_down && m_hot && m_enabled; }

    protected:
        // The mouse button was pressed and let go on this control.
        virtual void on_click() noexcept {}
        // The left button went down on this control, at a point in panel coordinates.
        virtual void on_press(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; }
        // The mouse moved while the button was held down on this control, wherever it is now. true when the control
        // changed and needs a repaint.
        virtual bool on_drag(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; return false; }
        // The mouse moved over the control with no button held, to a point in panel coordinates. true when the control
        // changed and needs a repaint.
        virtual bool on_move(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; return false; }
        // The wheel turned over the control's open overlay. true when it was used.
        virtual bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept { (void)x; (void)y; (void)notches; return false; }
        // The button came up, before the click is decided.
        virtual void on_release() noexcept {}
        // The right button, for a control that uses it, such as to pan. The press is at a point in panel coordinates, the drag is every
        // move while the button is held, wherever the mouse is, and true from it asks for a repaint. The menu is asked for after the release.
        virtual void on_right_press(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; }
        virtual bool on_right_drag(std::int32_t x, std::int32_t y) noexcept { (void)x; (void)y; return false; }
        virtual void on_right_release() noexcept {}
        // The mouse went from this control to another, or off the controls.
        virtual void on_leave() noexcept {}
        // A second press close after the first. It starts as a press again, unless the control does more.
        virtual void on_double_click(std::int32_t x, std::int32_t y) noexcept { on_press(x, y); }

        // A control that takes keys, such as a text field, can have the focus. A click on it gives it the focus, and
        // so does Tab.
        [[nodiscard]] virtual bool focusable() const noexcept { return false; }
        // A key with the focus. Virtual key codes. true when it was used, and false lets the next one have it, such as Tab.
        virtual bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept { (void)key; (void)ctrl; (void)shift; (void)alt; return false; }
        // The set draws a ring round the control while it has the focus by the keyboard, for a control that has no look of
        // its own for it. The ring goes round focus_ring_area(), which is the whole control unless it says otherwise.
        [[nodiscard]] virtual bool wants_focus_ring() const noexcept { return false; }
        [[nodiscard]] virtual dock_rect focus_ring_area() const noexcept { return m_area; }
        // A typed character, as a UTF-16 unit. Control characters arrive as keys.
        virtual void on_char(std::uint32_t code) noexcept { (void)code; }
        virtual void on_focus(bool gained) noexcept { (void)gained; }
        // Ctrl and Shift as the last mouse event had them, for a press that selects.
        [[nodiscard]] bool ctrl_down() const noexcept { return m_ctrl; }
        [[nodiscard]] bool shift_down() const noexcept { return m_shift; }
        // A drag is over the control, which takes drops: the effect a drop here would have, or none. x and y are in the coordinates
        // of the panel, as for the mouse. Called as the pointer moves and about every 40 ms while it rests.
        virtual drop_effect on_drag_over(const drag_event& event) noexcept { (void)event; return drop_effect::none; }
        // The drag went to another control, or ended without a drop here.
        virtual void on_drag_leave() noexcept {}
        // The drag was released over the control. Returns the effect it performed, or none when it took nothing.
        virtual drop_effect on_drop(const drag_event& event) noexcept { (void)event; return drop_effect::none; }
        // Starts a drag with the payload, from a control that is held down. It follows the pointer until the button comes up, and
        // the control stops being held: it gets no more drags or clicks from this press. false when a drag is already going.
        bool begin_drag(const drag_payload& payload) noexcept;
        // Asks for the focus to be taken away once the key is dealt with.
        void request_blur() noexcept { m_blur = true; }
        // Something about how the control looks changed, such as a blinking caret. The panel redraws it on its next frame.
        void invalidate() noexcept { m_dirty = true; }

    private:
        friend class control_set;
        // A slider holds a number field and passes the keyboard and mouse to it while it is being edited.
        friend class slider;

        void set_modifiers(const panel_mouse& mouse) noexcept { m_ctrl = mouse.ctrl; m_shift = mouse.shift; }

        dock_rect m_area{};
        const char* m_tooltip{nullptr};
        context_menu* m_context_menu{nullptr};
        bool m_enabled{true};
        bool m_visible{true};
        bool m_clipped{false};
        dock_rect m_clip{};
        bool m_hot{false};
        bool m_down{false};
        bool m_focused{false};
        bool m_focus_visible{false};
        std::int32_t m_tab_index{0};
        bool m_blur{false};
        bool m_dirty{false};
        bool m_drag_started{false};
        bool m_ctrl{false};
        bool m_shift{false};
    };

    // A way to move the focus to the control nearest in a direction.
    enum class focus_direction : std::uint8_t
    {
        left,
        right,
        up,
        down
    };

    // The controls of one panel. It finds the one under the mouse, keeps their hover and pressed state, and calls
    // on_click for a press and release on the same control. The controls belong to the panel and have to outlive the set.
    class control_set
    {
    public:
        static constexpr std::uint32_t k_max_shortcuts = 32;

        // Called when the keys of a shortcut are pressed and the focused control did not use them.
        using shortcut_fn = void (*)(void* user) noexcept;

        // Adds a control. The set grows as needed, so it has no limit of its own; false only when memory ran out.
        [[nodiscard]] bool add(control& item) noexcept;
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] control* at(std::uint32_t index) const noexcept { return index < m_count ? m_items[index] : nullptr; }

        // The control the mouse is over, or null.
        [[nodiscard]] control* hovered() const noexcept { return m_hot; }
        // The control a mouse button is held down on, or null.
        [[nodiscard]] control* pressed() const noexcept { return m_down; }

        // The control with the keyboard focus, or null. Setting it gives and takes the focus, which the controls hear.
        [[nodiscard]] control* focused() const noexcept { return m_focus; }
        void set_focus(control* item) noexcept;
        void blur() noexcept { set_focus(nullptr); }
        // Moves the focus to the next control in the tab order, or the one before, going round at the ends. false when no
        // control can take it.
        bool focus_next(bool backwards) noexcept;
        // Moves it to the control nearest in a direction from the one that has it, by where they are. false when none is there.
        bool focus_toward(focus_direction direction) noexcept;
        // Escape, when the focused control did not use it, takes the focus away. Off by default.
        void set_escape_blurs(bool blurs) noexcept { m_escape_blurs = blurs; }
        // Keys that do something whatever control has the focus, as long as it does not use them first. key is a virtual
        // key code, such as 'K' or 0x70 for F1. false when the table is full.
        [[nodiscard]] bool add_shortcut(std::uint32_t key, bool ctrl, bool shift, bool alt, shortcut_fn handler, void* user) noexcept;
        // A key, for the focused control, then the shortcuts. Tab moves the focus between the controls that can have it,
        // and Alt with an arrow moves it toward the next control in that direction. true when used.
        [[nodiscard]] bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept;
        void on_char(std::uint32_t code) noexcept;
        // The wheel, for the control whose open overlay is under the point, or that scrolls by itself. true when used, so the panel does not scroll.
        [[nodiscard]] bool on_wheel(const panel_mouse& mouse) noexcept;
        // The menu of the control the right button was released on, or null. Clears it. The panel shows the menu, since a
        // control knows nothing about windows.
        [[nodiscard]] context_menu* take_context_menu() noexcept { context_menu* menu = m_context; m_context = nullptr; return menu; }
        // true when a control asked to be redrawn since the last call. Clears it.
        [[nodiscard]] bool take_dirty() noexcept;

        // true when any control is animating.
        [[nodiscard]] bool animating() const noexcept;
        // Steps every control's animation. true while any has further to go.
        bool animate(std::uint64_t now_ms) noexcept;

        // Paints every control, in the order they were added.
        void paint(void* dc) const noexcept;
        // Limits drawing and animation to the controls that touch a band of the content, such as what a scroll view keeps
        // painted. Controls outside it still take part in hits, focus and layout, and catch up when the band reaches them.
        void set_view(const view_range& band) noexcept { m_view = band; m_has_view = true; }
        void clear_view() noexcept { m_has_view = false; }
        // The enabled control at a point, or null.
        [[nodiscard]] control* hit(std::int32_t x, std::int32_t y) const noexcept;
        // Feeds a mouse event. true when something changed that needs a repaint.
        [[nodiscard]] bool on_mouse(const panel_mouse& mouse) noexcept;

        // A drag over the controls, in their coordinates. The first control that takes drops at the point gets the events, and the
        // one before it is told the drag left. Returns the effect it answered, or none when no control takes it.
        [[nodiscard]] drop_effect drag_over(const drag_event& event) noexcept;
        // The drag left the controls, or ended.
        void drag_leave() noexcept;
        // The drag was released at a point. The control under it performs the drop. Returns its effect, or none.
        [[nodiscard]] drop_effect drop(const drag_event& event) noexcept;
        // The control the drag is over, or null.
        [[nodiscard]] control* drop_target() const noexcept { return m_drop; }

    private:
        void set_hot(control* item) noexcept;
        void focus_with(control* item, bool by_keyboard) noexcept;

        [[nodiscard]] control* drop_hit(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] bool skipped(const control& item) const noexcept { return item.culled() || (m_has_view && item.outside(m_view)); }

        // The controls, in a block that grows as controls are added. It moves with the set and is freed with it.
        class item_list
        {
        public:
            item_list() noexcept = default;
            ~item_list();
            item_list(item_list&& other) noexcept;
            item_list& operator=(item_list&& other) noexcept;
            item_list(const item_list&) = delete;
            item_list& operator=(const item_list&) = delete;

            [[nodiscard]] control*& operator[](std::uint32_t index) const noexcept { return m_data[index]; }
            // Room for one more control. false when memory ran out.
            [[nodiscard]] bool grow(std::uint32_t needed) noexcept;

        private:
            control** m_data{nullptr};
            std::uint32_t m_capacity{0};
        };

        item_list m_items;
        view_range m_view{};
        bool m_has_view{false};
        std::uint32_t m_count{0};
        control* m_hot{nullptr};
        control* m_down{nullptr};
        // The control the right button is held on.
        control* m_right{nullptr};
        control* m_focus{nullptr};
        control* m_drop{nullptr};
        bool m_escape_blurs{false};
        struct shortcut
        {
            std::uint32_t key{0};
            bool ctrl{false};
            bool shift{false};
            bool alt{false};
            shortcut_fn handler{nullptr};
            void* user{nullptr};
        };
        shortcut m_shortcuts[k_max_shortcuts]{};
        std::uint32_t m_shortcut_count{0};
        context_menu* m_context{nullptr};
    };
}
