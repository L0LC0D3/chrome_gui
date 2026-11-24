#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A ring with an arc running round it, for work that has no end to show, with an optional label after it. It shows
    // activity and takes no input. Stopped, it keeps the bare ring.
    //
    //   spinner busy;
    //   busy.set_label("Compiling shaders...");
    //   set.add(busy);
    class spinner final : public control
    {
    public:
        // Text is UTF-8 and has to outlive the spinner.
        spinner& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The diameter at 96 dpi.
        spinner& set_size(std::int32_t size) noexcept { m_size = size; return *this; }
        // The label goes before the ring instead of after it.
        spinner& set_label_left(bool left) noexcept { m_label_left = left; return *this; }
        spinner& set_spinning(bool spinning) noexcept { m_spinning = spinning; invalidate(); return *this; }
        spinner& set_name(const char* name) noexcept { m_name = name; return *this; }
        spinner& set_tooltip(const char* tip) noexcept { control::set_tooltip(tip); return *this; }

        [[nodiscard]] bool spinning() const noexcept { return m_spinning; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_spinning && enabled(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    private:
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_size{20};
        bool m_label_left{false};
        bool m_spinning{true};
        // Where the arc starts, in degrees.
        float m_angle{0.0f};
    };
}
