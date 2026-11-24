#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/status_bar.hpp>
#include <chrome/controls/core/toolbar.hpp>
#include <chrome/dock.hpp>
#include <chrome/panel.hpp>

#include <cstdint>

namespace chrome
{
    class dock_host;

    // What a strip of the main window has in common: a control set that gets the mouse and the paint, with a tooltip
    // for the control under the mouse. A strip is a band at the top or bottom of the main window (dock_host::add_strip),
    // not a tab, so it never takes the keyboard focus.
    class strip_panel : public panel_content
    {
    public:
        // The strip needs its host to ask for a repaint. Call after dock_host::add_strip.
        void bind(dock_host& host, panel_id id) noexcept;
        // Asks for a repaint.
        void refresh() noexcept;

        void on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept override;
        void on_mouse(const panel_mouse& mouse) noexcept override;

    protected:
        // Places the controls for the size of the strip.
        virtual void lay_out(void* dc, std::int32_t width, std::int32_t height) noexcept = 0;
        // Draws what lies under the controls.
        virtual void paint_under(void* dc, std::int32_t width, std::int32_t height) const noexcept { (void)dc; (void)width; (void)height; }

        control_set m_set;

    private:
        dock_host* m_host{nullptr};
        panel_id m_id{k_no_panel};
    };

    // The editor's toolbar, under the menu bar: flat icon buttons that act on the editor as a whole.
    class main_toolbar final : public strip_panel
    {
    public:
        main_toolbar() noexcept;
        [[nodiscard]] toolbar& bar() noexcept { return m_bar; }
        // The control set takes the buttons once the bar has them. Call after filling the bar.
        void build() noexcept;

    protected:
        void lay_out(void* dc, std::int32_t width, std::int32_t height) noexcept override;
        void paint_under(void* dc, std::int32_t width, std::int32_t height) const noexcept override;

    private:
        toolbar m_bar;
    };

    // The editor's status bar, at the bottom of the main window.
    class main_status final : public strip_panel
    {
    public:
        main_status() noexcept;
        [[nodiscard]] status_bar& bar() noexcept { return m_bar; }

    protected:
        void lay_out(void* dc, std::int32_t width, std::int32_t height) noexcept override;

    private:
        status_bar m_bar;
    };
}
