#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class separator_align : std::uint8_t
    {
        left,
        center,
        right
    };

    // A thin line that divides a panel into groups, optionally with a caption inside it. A horizontal one takes a row of
    // its own, as wide as the row. A vertical one stands between controls in a row, such as groups of buttons.
    //
    //   separator view;
    //   view.set_label("View").set_align(separator_align::center);
    //   set.add(view);
    class separator final : public control
    {
    public:
        // Text is UTF-8 and has to outlive the separator.
        separator& set_label(const char* label) noexcept { m_label = label; return *this; }
        separator& set_align(separator_align align) noexcept { m_align = align; return *this; }
        // A vertical separator has no label.
        separator& set_vertical(bool vertical) noexcept { m_vertical = vertical; return *this; }
        // The height of a vertical one, at 96 dpi. 0 is the height of a button.
        separator& set_length(std::int32_t length) noexcept { m_length = length; return *this; }

        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] bool vertical() const noexcept { return m_vertical; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return !m_vertical; }
        void paint(void* dc) const noexcept override;

    private:
        const char* m_label{nullptr};
        separator_align m_align{separator_align::left};
        bool m_vertical{false};
        std::int32_t m_length{0};
    };
}
