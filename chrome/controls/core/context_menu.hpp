#pragma once

#include <cstdint>

namespace chrome
{
    class context_menu;

    // One row of a menu: an action, a check that turns on and off, a row that opens a submenu, or a line between groups of
    // rows. Text and shortcut are UTF-8 and have to outlive the menu. The shortcut is only shown, right aligned. Making
    // the key work is up to the owner.
    //
    //   menu.add_item("Copy", k_copy).set_shortcut("Ctrl+C");
    //   menu.add_check("Word wrap", k_wrap).set_checked(true);
    //   menu.add_submenu("Recent", recent);
    //   menu.add_separator();
    class menu_item
    {
    public:
        menu_item& set_shortcut(const char* shortcut) noexcept { m_shortcut = shortcut; return *this; }
        menu_item& set_enabled(bool enabled) noexcept { m_enabled = enabled; return *this; }
        // Only for a check item.
        menu_item& set_checked(bool checked) noexcept { m_checked = checked; return *this; }

        [[nodiscard]] const char* text() const noexcept { return m_text; }
        [[nodiscard]] const char* shortcut() const noexcept { return m_shortcut; }
        // What the owner gave it, to tell the rows apart in the handler.
        [[nodiscard]] std::int32_t id() const noexcept { return m_id; }
        [[nodiscard]] bool enabled() const noexcept { return m_enabled; }
        [[nodiscard]] bool checkable() const noexcept { return m_checkable; }
        [[nodiscard]] bool checked() const noexcept { return m_checked; }
        [[nodiscard]] bool separator() const noexcept { return m_separator; }
        // The menu this row opens, or null for a row that is an action. A submenu is a menu of its own, with its own
        // handler, and has to outlive this one. Its rows are chosen through it.
        [[nodiscard]] context_menu* submenu() const noexcept { return m_submenu; }

    private:
        friend class context_menu;

        const char* m_text{nullptr};
        const char* m_shortcut{nullptr};
        context_menu* m_submenu{nullptr};
        std::int32_t m_id{0};
        bool m_enabled{true};
        bool m_checkable{false};
        bool m_checked{false};
        bool m_separator{false};
    };

    // The rows of a menu that opens at the mouse when the right button is released on a control that has it
    // (control::set_context_menu). The menu itself is the system's popup menu, in the editor's dark or light mode, which
    // the panel shows through its dock host. This only holds the rows and tells the owner which was chosen. A menu can
    // serve several controls.
    //
    //   context_menu edit;
    //   edit.set_on_select(&picked, this);
    //   edit.add_item("Copy", k_copy).set_shortcut("Ctrl+C");
    //   canvas.set_context_menu(&edit);
    class context_menu
    {
    public:
        static constexpr std::uint32_t k_max_items = 32;

        // Called after a row was chosen. A check row has already changed its state.
        using select_fn = void (*)(void* user, context_menu& source, menu_item& item) noexcept;
        // Called just before the menu is shown, so the owner can rebuild the rows or bring their states up to date.
        using open_fn = void (*)(void* user, context_menu& source) noexcept;

        // The new row, or the last one again when the menu is full.
        menu_item& add_item(const char* text, std::int32_t id) noexcept;
        menu_item& add_check(const char* text, std::int32_t id) noexcept;
        // A row that opens another menu to its right. The submenu is shown as it is when it opens.
        menu_item& add_submenu(const char* text, context_menu& submenu) noexcept;
        void add_separator() noexcept;
        context_menu& clear_items() noexcept { m_count = 0; return *this; }
        context_menu& set_on_select(select_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }
        context_menu& set_on_open(open_fn handler, void* user) noexcept { m_open = handler; m_open_user = user; return *this; }

        // What the host calls before it shows the menu, and again for each submenu as it is built.
        void notify_open() noexcept;

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] menu_item* item(std::uint32_t index) noexcept { return index < m_count ? &m_items[index] : nullptr; }
        [[nodiscard]] const menu_item* item(std::uint32_t index) const noexcept { return index < m_count ? &m_items[index] : nullptr; }
        // The row with an id, or null.
        [[nodiscard]] menu_item* find(std::int32_t id) noexcept;

        // Chooses a row, as the user does: a check row changes, then the handler is called. Does nothing for a line, a row
        // that is off or an index that is not a row.
        void choose(std::int32_t index) noexcept;

    private:
        menu_item m_items[k_max_items]{};
        std::uint32_t m_count{0};
        select_fn m_handler{nullptr};
        void* m_user{nullptr};
        open_fn m_open{nullptr};
        void* m_open_user{nullptr};
    };
}
