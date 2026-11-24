#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // What happened in a tab bar.
    enum class tab_event : std::uint8_t
    {
        // The user chose a tab. index is the tab now selected.
        select,
        // The user asked for a tab to be closed, by its cross, Delete or Ctrl+W. The bar does not remove it, so the owner
        // can ask about unsaved work first and then call remove_tab. index is the tab.
        close,
        // The user pressed the plus button, and the owner is to add a tab. index is the number of tabs there are.
        add,
        // The user dragged a tab to another place, or moved it with Ctrl+Shift and an arrow. index is its new place.
        move
    };

    // A row of tabs, such as the open scenes of an editor or the pages of a panel. Each tab has a title, perhaps a small
    // picture, a cross to close it, and a dot while it holds changes that are not saved. Tabs shrink to share the width
    // and, when they cannot shrink further, the row scrolls with the wheel and keeps the chosen tab in view. The tabs can be
    // dragged into another order, and a plus button at their end asks for a new one.
    //
    //   tab_bar tabs;
    //   tabs.add_tab("Main.scene", icon_kind::folder);
    //   tabs.add_tab("Forest.scene");
    //   tabs.set_modified(1, true);
    //   tabs.set_show_add(true).set_on_event(&on_tab, this);
    //
    //   static void on_tab(void* user, tab_bar& source, tab_event what, std::int32_t index) noexcept;
    class tab_bar final : public control
    {
    public:
        static constexpr std::int32_t k_max_tabs = 32;
        static constexpr std::int32_t k_title_length = 48;

        using event_fn = void (*)(void* user, tab_bar& source, tab_event what, std::int32_t index) noexcept;

        // Adds a tab at the end, and returns its place, or -1 when the bar is full. The first tab added is selected. The title
        // is UTF-8 and is copied.
        std::int32_t add_tab(const char* title, icon_kind icon = icon_kind::none) noexcept;
        // Takes a tab out. When it was the selected one, the tab that takes its place is selected, with no event.
        void remove_tab(std::int32_t index) noexcept;
        void clear_tabs() noexcept;

        // Chooses a tab, with no event, and brings it into view.
        void select(std::int32_t index) noexcept;
        void set_title(std::int32_t index, const char* title) noexcept;
        void set_icon(std::int32_t index, icon_kind icon) noexcept;
        // A tab with changes that are not saved shows a dot where its cross is, until the mouse is over it.
        void set_modified(std::int32_t index, bool modified) noexcept;
        // A pinned tab has no cross, cannot be closed from the bar, and cannot be dragged. Other tabs cannot be dragged before
        // it, so pinned tabs are meant to be the first ones added.
        void set_pinned(std::int32_t index, bool pinned) noexcept;

        [[nodiscard]] std::int32_t count() const noexcept { return m_count; }
        [[nodiscard]] bool pinned(std::int32_t index) const noexcept { return index >= 0 && index < m_count && m_tabs[index].pinned; }
        // The tab under a point of the panel, or -1.
        [[nodiscard]] std::int32_t tab_at(std::int32_t x, std::int32_t y) const noexcept { return find(x, y).tab; }
        // Where a tab was last painted, in the coordinates of the panel.
        [[nodiscard]] dock_rect tab_area(std::int32_t index) const noexcept { return index >= 0 && index < m_count ? tab_rect(index) : dock_rect{}; }
        // The selected tab, or -1 when there are none.
        [[nodiscard]] std::int32_t selected() const noexcept { return m_selected; }
        [[nodiscard]] const char* title(std::int32_t index) const noexcept;
        [[nodiscard]] bool modified(std::int32_t index) const noexcept;

        // Tabs have a cross. On by default.
        tab_bar& set_closable(bool closable) noexcept { m_closable = closable; m_stale = true; return *this; }
        // A plus button after the last tab. Off by default.
        tab_bar& set_show_add(bool show) noexcept { m_show_add = show; m_stale = true; return *this; }
        // Tabs can be dragged into another order. On by default.
        tab_bar& set_reorderable(bool reorderable) noexcept { m_reorderable = reorderable; return *this; }
        // The width the bar asks for, at 96 dpi. 0, the default, asks for the width its tabs need.
        tab_bar& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        tab_bar& set_name(const char* name) noexcept { m_name = name; return *this; }
        tab_bar& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        tab_bar& set_on_event(event_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return m_overflow; }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        void on_click() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return m_count > 0; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        void on_focus(bool gained) noexcept override;

    private:
        struct tab_item
        {
            char title[k_title_length]{};
            icon_kind icon{icon_kind::none};
            bool modified{false};
            bool pinned{false};
        };

        // What is under a point.
        struct place
        {
            std::int32_t tab{-1};
            bool close{false};
            bool add{false};
        };

        void measure(void* dc) const noexcept;
        [[nodiscard]] dock_rect tab_rect(std::int32_t index) const noexcept;
        [[nodiscard]] dock_rect close_rect(const dock_rect& tab) const noexcept;
        [[nodiscard]] dock_rect add_rect() const noexcept;
        [[nodiscard]] dock_rect strip_area() const noexcept;
        [[nodiscard]] bool close_shown(std::int32_t index) const noexcept;
        // Whether a tab has room for a cross or a dot at its end.
        [[nodiscard]] bool has_slot(std::int32_t index) const noexcept;
        // The number of pinned tabs at the start, which is the first place another tab can be dragged to.
        [[nodiscard]] std::int32_t first_free() const noexcept;
        [[nodiscard]] place find(std::int32_t x, std::int32_t y) const noexcept;
        void move_tab(std::int32_t from, std::int32_t to) noexcept;
        void raise(tab_event what, std::int32_t index) noexcept;

        tab_item m_tabs[k_max_tabs]{};
        std::int32_t m_count{0};
        std::int32_t m_selected{-1};
        bool m_closable{true};
        bool m_show_add{false};
        bool m_reorderable{true};
        std::int32_t m_width{0};
        const char* m_name{nullptr};
        event_fn m_handler{nullptr};
        void* m_user{nullptr};

        // Mouse state.
        std::int32_t m_hot_tab{-1};
        bool m_hot_close{false};
        bool m_hot_add{false};
        place m_press{};
        std::int32_t m_press_x{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_dragging{false};

        // Measured in paint, which is const, and so mutable.
        mutable std::int32_t m_widths[k_max_tabs]{};
        mutable std::int32_t m_total{0};
        mutable std::int32_t m_measured_width{-1};
        mutable std::int32_t m_scroll{0};
        mutable bool m_overflow{false};
        mutable bool m_stale{true};
        mutable bool m_reveal{false};
    };
}
