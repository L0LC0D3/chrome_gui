#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // Empty room between controls. With no width it takes a row of its own and is only as tall as asked, which puts space
    // between two rows. With a width it is a gap inside a row.
    //
    //   spacer gap;
    //   gap.set_height(12);              // 12 px between rows
    //   set.add(gap);
    class spacer final : public control
    {
    public:
        // Sizes are at 96 dpi.
        spacer& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        spacer& set_height(std::int32_t height) noexcept { m_height = height; return *this; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override { (void)dc; }

    private:
        std::int32_t m_width{0};
        std::int32_t m_height{8};
    };
}
