#include <chrome/strip_panel.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/dock_host.hpp>
#include <chrome/theme.hpp>

namespace chrome
{
    void strip_panel::bind(dock_host& host, panel_id id) noexcept
    {
        m_host = &host;
        m_id = id;
    }

    void strip_panel::refresh() noexcept
    {
        if (m_host != nullptr)
            m_host->invalidate_panel(m_id);
    }

    void strip_panel::on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept
    {
        lay_out(dc, width, height);
        paint_under(dc, width, height);
        m_set.paint(dc);
    }

    void strip_panel::on_mouse(const panel_mouse& mouse) noexcept
    {
        if (m_host == nullptr)
            return;

        bool repaint = m_set.on_mouse(mouse);

        if (m_set.take_dirty())
            repaint = true;

        // A strip has no menus of its own, so a right click is only dropped.
        (void)m_set.take_context_menu();

        if (repaint)
            refresh();

        // The control under the mouse says what its tooltip is. A control without one hides the previous.
        const control* hot = m_set.hovered();
        m_host->set_tooltip(m_id, hot != nullptr && mouse.event != mouse_event::leave ? hot->tooltip() : nullptr, hot != nullptr ? hot->area() : dock_rect{});
    }

    main_toolbar::main_toolbar() noexcept = default;

    void main_toolbar::build() noexcept
    {
        (void)m_bar.attach(m_set);
    }

    void main_toolbar::lay_out(void* dc, std::int32_t width, std::int32_t height) noexcept
    {
        m_bar.arrange(dc, {0, 0, width, height});
    }

    void main_toolbar::paint_under(void* dc, std::int32_t width, std::int32_t height) const noexcept
    {
        // A line under the toolbar, where the panels begin.
        draw::fill(dc, {0, height - 1, width, 1}, blend_colour(theme().panel, 0x000000, 0.4f));
        m_bar.paint_separators(dc);
    }

    main_status::main_status() noexcept
    {
        (void)m_set.add(m_bar);
    }

    void main_status::lay_out(void* dc, std::int32_t width, std::int32_t height) noexcept
    {
        (void)dc;
        m_bar.set_area({0, 0, width, height});
    }
}
