#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class label_style : std::uint8_t
    {
        // The text colour of the theme.
        body,
        // Quieter, for hints and captions.
        dim,
        // Semibold, for what names a block of controls.
        heading,
        // The accent colour, underlined while the mouse is over it. It is the only style that can be clicked.
        link,
        success,
        warning,
        error
    };

    enum class label_align : std::uint8_t
    {
        left,
        center,
        right
    };

    // Text in a panel. It is one line unless it is told to wrap. Given a width it can align, and it ends with "..." when
    // the text does not fit.
    //
    //   label hint;
    //   hint.set_text("Drag to reorder").set_style(label_style::dim);
    //   set.add(hint);
    class label final : public control
    {
    public:
        // Called after a link was clicked.
        using click_fn = void (*)(void* user, label& source) noexcept;

        // The text is UTF-8 and has to outlive the label.
        label& set_text(const char* text) noexcept { m_text = text; return *this; }
        label& set_style(label_style style) noexcept { m_style = style; return *this; }
        label& set_align(label_align align) noexcept { m_align = align; return *this; }
        // Breaks lines at the width of the row, so the label takes a row of its own.
        label& set_wrap(bool wrap) noexcept { m_wrap = wrap; return *this; }
        // A fixed width at 96 dpi, instead of the width of the text. 0 goes back to the text's own.
        label& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        label& set_tooltip(const char* tip) noexcept { control::set_tooltip(tip); return *this; }
        label& set_name(const char* name) noexcept { m_name = name; return *this; }
        label& set_on_click(click_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* text() const noexcept { return m_text; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_text; }
        [[nodiscard]] label_style style() const noexcept { return m_style; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_wrap; }
        void paint(void* dc) const noexcept override;

    protected:
        void on_click() noexcept override;

    private:
        [[nodiscard]] std::uint32_t flags() const noexcept;

        const char* m_text{nullptr};
        const char* m_name{nullptr};
        label_style m_style{label_style::body};
        label_align m_align{label_align::left};
        bool m_wrap{false};
        std::int32_t m_width{0};
        click_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
