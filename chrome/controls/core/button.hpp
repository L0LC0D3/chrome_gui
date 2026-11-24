#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class button_style : std::uint8_t
    {
        // A raised button.
        standard,
        // The accent colour, for the one thing a panel mostly wants done.
        primary,
        // No face until the mouse is over it, for toolbars.
        flat
    };

    // A push button, or a toggle that stays down. It can have a label, an icon, both, or neither.
    //
    //   button play;
    //   play.set_icon(icon_kind::play).set_label("Play").set_style(button_style::primary).set_on_click(&played, this);
    //   set.add(play);
    class button final : public control
    {
    public:
        // Called after a click. A toggle has already changed by then.
        using click_fn = void (*)(void* user, button& source) noexcept;

        // Text and names are UTF-8 and have to outlive the button.
        button& set_label(const char* label) noexcept { m_label = label; return *this; }
        button& set_icon(icon_kind icon) noexcept { m_icon = icon; return *this; }
        button& set_style(button_style style) noexcept { m_style = style; return *this; }
        // A toggle flips between on and off with every click.
        button& set_toggle(bool toggle) noexcept { m_toggle = toggle; return *this; }
        button& set_checked(bool checked) noexcept { m_checked = checked; return *this; }
        // What the button is called when it has no label, such as in a status line.
        button& set_name(const char* name) noexcept { m_name = name; return *this; }
        // Shown next to the button when the mouse rests on it. Optional.
        button& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        button& set_on_click(click_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] icon_kind icon() const noexcept { return m_icon; }
        [[nodiscard]] button_style style() const noexcept { return m_style; }
        [[nodiscard]] bool is_toggle() const noexcept { return m_toggle; }
        [[nodiscard]] bool checked() const noexcept { return m_checked; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;

    protected:
        void on_click() noexcept override;

    private:
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        icon_kind m_icon{icon_kind::none};
        button_style m_style{button_style::standard};
        bool m_toggle{false};
        bool m_checked{false};
        click_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
