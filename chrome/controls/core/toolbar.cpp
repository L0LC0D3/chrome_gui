#include <chrome/controls/core/toolbar.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_margin = 6;
        constexpr std::int32_t k_gap = 2;
        constexpr std::int32_t k_separator = 9;
        // The room above and below the buttons.
        constexpr std::int32_t k_pad = 3;
    }

    toolbar::item& toolbar::next_item() noexcept
    {
        item& slot = m_items[m_count < k_max_items ? m_count : k_max_items - 1];

        if (m_count < k_max_items)
        {
            slot = item{};
            ++m_count;
        }

        slot.owner = this;
        return slot;
    }

    button& toolbar::add_button(icon_kind icon, std::int32_t id, const char* tooltip, const char* label) noexcept
    {
        item& slot = next_item();
        slot.id = id;
        slot.face.set_style(button_style::flat).set_icon(icon).set_label(label).set_tooltip(tooltip).set_on_click(&toolbar::clicked, &slot);
        slot.face.set_name(tooltip);
        return slot.face;
    }

    button& toolbar::add_toggle(icon_kind icon, std::int32_t id, const char* tooltip, const char* label) noexcept
    {
        return add_button(icon, id, tooltip, label).set_toggle(true);
    }

    void toolbar::add_separator() noexcept
    {
        next_item().what = kind::separator;
    }

    void toolbar::add_spacer() noexcept
    {
        next_item().what = kind::spacer;
    }

    button* toolbar::find(std::int32_t id) noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i].what == kind::action && m_items[i].id == id)
                return &m_items[i].face;
        }

        return nullptr;
    }

    void toolbar::clear() noexcept
    {
        for (item& it : m_items)
            it = item{};

        m_count = 0;
    }

    bool toolbar::attach_all(control_set& set) noexcept
    {
        bool all = true;

        for (item& it : m_items)
            all = set.add(it.face) && all;

        return all;
    }

    bool toolbar::attach(control_set& set) noexcept
    {
        bool all = true;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i].what == kind::action)
                all = set.add(m_items[i].face) && all;
        }

        return all;
    }

    void toolbar::arrange(void* dc, const dock_rect& area) noexcept
    {
        const std::int32_t pad = draw::px(k_pad);
        const std::int32_t gap = draw::px(k_gap);
        const std::int32_t height = std::max(area.height - 2 * pad, 1);
        std::int32_t sizes[k_max_items]{};
        std::int32_t left_total = 0;
        std::int32_t right_total = 0;
        bool right = false;

        // The width of each item, and of the left and right groups.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const item& it = m_items[i];

            if (it.what == kind::spacer)
            {
                right = true;
                continue;
            }

            sizes[i] = it.what == kind::separator ? draw::px(k_separator) : it.face.preferred_size(dc).width + gap;
            (right ? right_total : left_total) += sizes[i];
        }

        const std::int32_t margin = draw::px(k_margin);
        std::int32_t x = area.x + margin;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            item& it = m_items[i];

            if (it.what == kind::spacer)
            {
                x =std::max(area.x + area.width - margin - right_total, area.x + margin + left_total);
                continue;
            }

            if (it.what == kind::separator)
            {
                it.rule = {x + sizes[i] / 2, area.y + 2 * pad, 1, std::max(area.height - 4 * pad, 1)};
            }
            else
            {
                const control_size wanted = it.face.preferred_size(dc);
                const std::int32_t h = std::min(wanted.height, height);
                it.face.set_area({x, area.y + (area.height - h) / 2, wanted.width, h});
            }

            x += sizes[i];
        }
    }

    void toolbar::paint_separators(void* dc) const noexcept
    {
        const std::uint32_t colour = blend_colour(theme().panel, 0x000000, 0.3f);

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i].what == kind::separator)
                draw::fill(dc, m_items[i].rule, colour);
        }
    }

    void toolbar::clicked(void* user, button& source) noexcept
    {
        (void)source;
        const item& it = *static_cast<const item*>(user);
        toolbar& self = *it.owner;

        if (self.m_handler != nullptr)
            self.m_handler(self.m_user, self, it.id);
    }
}
