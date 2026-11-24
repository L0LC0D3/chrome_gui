#include <chrome/controls/core/control.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/drag_drop.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdlib>

namespace chrome
{
    namespace
    {
        [[nodiscard]] bool contains(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }
    }

    bool control::begin_drag(const drag_payload& payload) noexcept
    {
        if (!drag_drop::begin(payload))
            return false;

        m_drag_started = true;
        return true;
    }

    control_set::item_list::~item_list()
    {
        std::free(m_data);
    }

    control_set::item_list::item_list(item_list&& other) noexcept : m_data(other.m_data), m_capacity(other.m_capacity)
    {
        other.m_data = nullptr;
        other.m_capacity = 0;
    }

    control_set::item_list& control_set::item_list::operator=(item_list&& other) noexcept
    {
        if (this != &other)
        {
            std::free(m_data);
            m_data = other.m_data;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_capacity = 0;
        }

        return *this;
    }

    bool control_set::item_list::grow(std::uint32_t needed) noexcept
    {
        if (needed <= m_capacity)
            return true;

        const std::uint32_t capacity = std::max(needed, m_capacity == 0 ? 32u : m_capacity * 2);
        auto** grown = static_cast<control**>(std::realloc(m_data, static_cast<std::size_t>(capacity) * sizeof(control*)));

        if (grown == nullptr)
            return false;

        m_data = grown;
        m_capacity = capacity;
        return true;
    }

    bool control_set::add(control& item) noexcept
    {
        if (!m_items.grow(m_count + 1))
            return false;

        m_items[m_count++] = &item;
        return true;
    }

    void control_set::paint(void* dc) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            control& item = *m_items[i];

            if (skipped(item))
                continue;

            if (item.m_clipped)
                draw::clip(dc, item.m_clip);

            item.paint(dc);

            // A ring for the keyboard focus, on the controls that have no look of their own for it.
            if (item.m_focused && item.m_focus_visible && item.wants_focus_ring())
            {
                dock_rect ring = item.focus_ring_area();
                ring.x -= 2;
                ring.y -= 2;
                ring.width += 4;
                ring.height += 4;
                draw::outline(dc, ring, 4, theme().accent, 2.0f);
            }

            if (item.m_clipped)
                draw::unclip(dc);
        }

        // What is open goes over everything.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!m_items[i]->culled() && m_items[i]->overlay_open())
                m_items[i]->paint_overlay(dc);
        }
    }

    control* control_set::drop_hit(std::int32_t x, std::int32_t y) const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const control& item = *m_items[i];

            if (item.is_drop_target() && !item.culled() && item.enabled() && contains(item.area(), x, y) && (!item.m_clipped || contains(item.m_clip, x, y)))
                return m_items[i];
        }

        return nullptr;
    }

    drop_effect control_set::drag_over(const drag_event& event) noexcept
    {
        control* target = drop_hit(event.x, event.y);

        if (target != m_drop)
        {
            if (m_drop != nullptr)
                m_drop->on_drag_leave();

            m_drop = target;
        }

        return target != nullptr ? target->on_drag_over(event) : drop_effect::none;
    }

    void control_set::drag_leave() noexcept
    {
        if (m_drop != nullptr)
            m_drop->on_drag_leave();

        m_drop = nullptr;
    }

    drop_effect control_set::drop(const drag_event& event) noexcept
    {
        control* target = drop_hit(event.x, event.y);

        if (target != m_drop)
        {
            if (m_drop != nullptr)
                m_drop->on_drag_leave();

            m_drop = nullptr;
        }

        const drop_effect effect = target != nullptr ? target->on_drop(event) : drop_effect::none;
        m_drop = nullptr;
        return effect;
    }

    control* control_set::hit(std::int32_t x, std::int32_t y) const noexcept
    {
        // An open overlay hides what is under it.
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i]->m_visible && m_items[i]->enabled() && m_items[i]->overlay_open() && contains(m_items[i]->overlay_area(), x, y))
                return m_items[i];
        }

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const control& item = *m_items[i];

            if (!item.culled() && item.enabled() && contains(item.area(), x, y) && (!item.m_clipped || contains(item.m_clip, x, y)))
                return m_items[i];
        }

        return nullptr;
    }

    void control_set::set_hot(control* item) noexcept
    {
        control* previous = m_hot;

        if (m_hot != nullptr)
            m_hot->m_hot = false;

        m_hot = item;

        if (previous != nullptr && previous != item)
            previous->on_leave();

        if (m_hot != nullptr)
            m_hot->m_hot = true;
    }

    bool control_set::on_mouse(const panel_mouse& mouse) noexcept
    {
        switch (mouse.event)
        {
        case mouse_event::move:
        {
            // A control that was pressed keeps getting the movement, such as a thumb being dragged.
            if (m_down != nullptr)
                m_down->set_modifiers(mouse);

            bool changed = m_down != nullptr && m_down->on_drag(mouse.x, mouse.y);

            // The right button is held on a control: it gets the movement, and nothing else does.
            if (m_right != nullptr)
            {
                m_right->set_modifiers(mouse);
                return m_right->on_right_drag(mouse.x, mouse.y);
            }

            // A control that started a drag is let go: the pointer belongs to the drag now, and no click will follow.
            if (m_down != nullptr && m_down->m_drag_started)
            {
                m_down->m_drag_started = false;
                m_down->m_down = false;
                m_down->on_release();
                m_down = nullptr;
                changed = true;
            }

            control* under = hit(mouse.x, mouse.y);

            if (m_down == nullptr && under != nullptr)
                under->set_modifiers(mouse);

            if (m_down == nullptr && under != nullptr)
                changed = under->on_move(mouse.x, mouse.y) || changed;

            if (under == m_hot)
                return changed;

            set_hot(under);
            return true;
        }

        case mouse_event::leave:
        {
            const bool changed = m_hot != nullptr;
            set_hot(nullptr);
            return changed;
        }

        case mouse_event::down:
        case mouse_event::double_click:
        {
            if (mouse.button == 1)
            {
                m_right = hit(mouse.x, mouse.y);

                if (m_right != nullptr)
                {
                    m_right->set_modifiers(mouse);
                    m_right->on_right_press(mouse.x, mouse.y);
                }

                return m_right != nullptr;
            }

            if (mouse.button != 0)
                return false;

            control* under = hit(mouse.x, mouse.y);

            if (m_down != nullptr)
                m_down->m_down = false;

            m_down = under;

            // A click gives the focus to the control that takes it, and anything else takes it away.
            focus_with(under != nullptr && under->focusable() ? under : nullptr, false);

            if (m_down != nullptr)
            {
                m_down->m_down = true;
                m_down->set_modifiers(mouse);

                if (mouse.event == mouse_event::double_click)
                    m_down->on_double_click(mouse.x, mouse.y);
                else
                    m_down->on_press(mouse.x, mouse.y);
            }

            set_hot(under);
            return true;
        }

        case mouse_event::up:
        {
            if (mouse.button == 1)
            {
                // The control the button went down on has it, even if the pointer has moved off it.
                control* target = m_right != nullptr ? m_right : hit(mouse.x, mouse.y);
                const bool held = m_right != nullptr;
                m_right = nullptr;

                if (held)
                {
                    target->set_modifiers(mouse);
                    target->on_right_release();
                }

                m_context = target != nullptr ? target->context_menu_at(mouse.x, mouse.y) : nullptr;
                return held;
            }

            if (mouse.button != 0)
                return false;

            // A click is a press and a release on the same control.
            control* under = hit(mouse.x, mouse.y);
            control* pressed = m_down;

            if (m_down != nullptr)
            {
                m_down->m_down = false;
                m_down->set_modifiers(mouse);
                m_down->on_release();
            }

            m_down = nullptr;
            set_hot(under);

            if (pressed != nullptr && pressed == under)
                pressed->on_click();

            return true;
        }

        case mouse_event::wheel:
            break;
        }

        return false;
    }

    void control_set::set_focus(control* item) noexcept
    {
        focus_with(item, true);
    }

    void control_set::focus_with(control* item, bool by_keyboard) noexcept
    {
        if (item == m_focus)
        {
            if (m_focus != nullptr)
                m_focus->m_focus_visible = by_keyboard;

            return;
        }

        control* previous = m_focus;
        m_focus = item;

        if (previous != nullptr)
        {
            previous->m_focused = false;
            previous->m_focus_visible = false;
            previous->on_focus(false);
        }

        if (m_focus != nullptr)
        {
            m_focus->m_focused = true;
            m_focus->m_focus_visible = by_keyboard;
            m_focus->on_focus(true);
        }
    }

    bool control_set::add_shortcut(std::uint32_t key, bool ctrl, bool shift, bool alt, shortcut_fn handler, void* user) noexcept
    {
        if (m_shortcut_count >= k_max_shortcuts || handler == nullptr)
            return false;

        m_shortcuts[m_shortcut_count++] = {key, ctrl, shift, alt, handler, user};
        return true;
    }

    bool control_set::focus_next(bool backwards) noexcept
    {
        if (m_count == 0)
            return false;

        auto** order = static_cast<control**>(std::malloc(static_cast<std::size_t>(m_count) * sizeof(control*)));

        if (order == nullptr)
            return false;

        std::uint32_t count = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            control* item = m_items[i];

            if (item->focusable() && item->enabled() && !item->culled() && item->m_tab_index >= 0)
                order[count++] = item;
        }

        if (count == 0)
        {
            std::free(order);
            return false;
        }

        // The explicit numbers first, lowest first, then the rest as they were added. The sort keeps the added order of equals.
        std::stable_sort(order, order + count, [](const control* a, const control* b) {
            const bool a_set = a->m_tab_index > 0;
            const bool b_set = b->m_tab_index > 0;

            if (a_set != b_set)
                return a_set;

            return a_set && a->m_tab_index < b->m_tab_index;
        });

        std::uint32_t at = count;

        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (order[i] == m_focus)
            {
                at = i;
                break;
            }
        }

        std::uint32_t target = 0;

        if (at == count)
            target = backwards ? count - 1 : 0;
        else
            target = backwards ? (at + count - 1) % count : (at + 1) % count;

        control* chosen = order[target];
        std::free(order);
        focus_with(chosen, true);
        return true;
    }

    bool control_set::focus_toward(focus_direction direction) noexcept
    {
        if (m_focus == nullptr)
            return focus_next(false);

        const dock_rect from = m_focus->area();
        const std::int32_t from_x = from.x + from.width / 2;
        const std::int32_t from_y = from.y + from.height / 2;
        control* best = nullptr;
        std::int64_t best_score = 0;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            control* item = m_items[i];

            if (item == m_focus || !item->focusable() || !item->enabled() || item->culled())
                continue;

            const std::int32_t dx = item->area().x + item->area().width / 2 - from_x;
            const std::int32_t dy = item->area().y + item->area().height / 2 - from_y;
            std::int32_t along = 0;
            std::int32_t across = 0;

            switch (direction)
            {
            case focus_direction::left: along = -dx; across = dy; break;
            case focus_direction::right: along = dx; across = dy; break;
            case focus_direction::up: along = -dy; across = dx; break;
            case focus_direction::down: along = dy; across = dx; break;
            }

            // Only what lies the way the arrow points. Being off to the side costs twice as much as being further along.
            if (along <= 0)
                continue;

            const std::int64_t score = static_cast<std::int64_t>(along) * along + 4 * static_cast<std::int64_t>(across) * across;

            if (best == nullptr || score < best_score)
            {
                best = item;
                best_score = score;
            }
        }

        if (best == nullptr)
            return false;

        focus_with(best, true);
        return true;
    }

    bool control_set::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        control* handler = m_focus;

        if (handler != nullptr && handler->on_key(key, ctrl, shift, alt))
        {
            // The key may have moved the focus, such as a popover closing and giving it back, so it is the handler that is asked.
            if (handler->m_blur)
            {
                handler->m_blur = false;

                if (m_focus == handler)
                    set_focus(nullptr);
            }

            return true;
        }

        for (std::uint32_t i = 0; i < m_shortcut_count; ++i)
        {
            const shortcut& entry = m_shortcuts[i];

            if (entry.key == key && entry.ctrl == ctrl && entry.shift == shift && entry.alt == alt)
            {
                entry.handler(entry.user);
                return true;
            }
        }

        // Tab goes to the next control that can have the focus, and Shift+Tab to the one before.
        if (key == 9 && !ctrl && !alt)
            return focus_next(shift);

        // Alt with an arrow goes toward the next control that way.
        if (alt && !ctrl && !shift)
        {
            switch (key)
            {
            case 0x25: return focus_toward(focus_direction::left);
            case 0x26: return focus_toward(focus_direction::up);
            case 0x27: return focus_toward(focus_direction::right);
            case 0x28: return focus_toward(focus_direction::down);
            default: break;
            }
        }

        if (key == 0x1B && m_escape_blurs && m_focus != nullptr)
        {
            set_focus(nullptr);
            return true;
        }

        return false;
    }

    void control_set::on_char(std::uint32_t code) noexcept
    {
        if (m_focus != nullptr)
            m_focus->on_char(code);
    }

    bool control_set::on_wheel(const panel_mouse& mouse) noexcept
    {
        control* under = hit(mouse.x, mouse.y);

        if (under == nullptr)
            return false;

        if (under->overlay_open() && contains(under->overlay_area(), mouse.x, mouse.y))
            return under->on_wheel(mouse.x, mouse.y, mouse.wheel);

        return under->takes_wheel() && under->on_wheel(mouse.x, mouse.y, mouse.wheel);
    }

    bool control_set::take_dirty() noexcept
    {
        bool dirty = false;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_items[i]->m_dirty)
            {
                m_items[i]->m_dirty = false;
                dirty = true;
            }
        }

        return dirty;
    }

    bool control_set::animating() const noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (!skipped(*m_items[i]) && m_items[i]->animating())
                return true;
        }

        return false;
    }

    bool control_set::animate(std::uint64_t now_ms) noexcept
    {
        bool more = false;

        for (std::uint32_t i = 0; i < m_count; ++i)
            if (!skipped(*m_items[i]))
                more = m_items[i]->animate(now_ms) || more;

        return more;
    }
}
