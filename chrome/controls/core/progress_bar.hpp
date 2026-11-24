#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A bar that fills from the left as work gets done, with an optional label before it and the percentage after it. With
    // no known end it is indeterminate instead, and a segment runs along the track. It shows progress and takes no input.
    //
    //   progress_bar load;
    //   load.set_label("Loading").set_value(0.25f);
    //   set.add(load);
    //   load.ease_to(0.5f);   // The bar slides there, and the panel gets frames while it does.
    class progress_bar final : public control
    {
    public:
        progress_bar& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The width of the label's column, at 96 dpi.
        progress_bar& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The whole control at 96 dpi. 0 makes it take a row of its own, as wide as the row.
        progress_bar& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // The percentage after the bar.
        progress_bar& set_show_percent(bool show) noexcept { m_show_percent = show; return *this; }
        progress_bar& set_indeterminate(bool indeterminate) noexcept { m_indeterminate = indeterminate; invalidate(); return *this; }
        progress_bar& set_name(const char* name) noexcept { m_name = name; return *this; }
        progress_bar& set_tooltip(const char* tip) noexcept { control::set_tooltip(tip); return *this; }
        // From 0 to 1. The bar jumps to it.
        progress_bar& set_value(float value) noexcept;
        // The bar slides to the value.
        progress_bar& ease_to(float value) noexcept;

        [[nodiscard]] float value() const noexcept { return m_value; }
        [[nodiscard]] bool indeterminate() const noexcept { return m_indeterminate; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return (m_indeterminate && enabled()) || m_shown != m_value; }
        bool animate(std::uint64_t now_ms) noexcept override;

    private:
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_label_width{90};
        std::int32_t m_width{300};
        bool m_show_percent{true};
        bool m_indeterminate{false};
        // What was asked for, and what is drawn while the bar slides there.
        float m_value{0.0f};
        float m_shown{0.0f};
        // How far the indeterminate segment has run, from 0 to 1.
        float m_phase{0.0f};
        std::uint64_t m_stamp{0};
    };
}
