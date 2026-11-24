#include <chrome/controls/core/context_menu.hpp>

namespace chrome
{
    menu_item& context_menu::add_item(const char* text, std::int32_t id) noexcept
    {
        menu_item& item = m_items[m_count < k_max_items ? m_count : k_max_items - 1];

        if (m_count < k_max_items)
        {
            item = menu_item{};
            ++m_count;
        }

        item.m_text = text;
        item.m_id = id;
        return item;
    }

    menu_item& context_menu::add_check(const char* text, std::int32_t id) noexcept
    {
        menu_item& item = add_item(text, id);
        item.m_checkable = true;
        return item;
    }

    menu_item& context_menu::add_submenu(const char* text, context_menu& submenu) noexcept
    {
        menu_item& item = add_item(text, 0);
        item.m_submenu = &submenu;
        return item;
    }

    void context_menu::notify_open() noexcept
    {
        if (m_open != nullptr)
            m_open(m_open_user, *this);
    }

    void context_menu::add_separator() noexcept
    {
        if (m_count >= k_max_items)
            return;

        m_items[m_count] = menu_item{};
        m_items[m_count].m_separator = true;
        ++m_count;
    }

    menu_item* context_menu::find(std::int32_t id) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!m_items[i].m_separator && m_items[i].m_id == id)
                return &m_items[i];
        }

        return nullptr;
    }

    void context_menu::choose(std::int32_t index) noexcept
    {
        if (index < 0 || index >= static_cast<std::int32_t>(m_count))
            return;

        menu_item& item = m_items[index];

        if (item.m_separator || !item.m_enabled || item.m_submenu != nullptr)
            return;

        if (item.m_checkable)
            item.m_checked = !item.m_checked;

        if (m_handler != nullptr)
            m_handler(m_user, *this, item);
    }
}
