#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class scroll_axis : std::uint8_t
    {
        vertical,
        horizontal
    };

    // A thin bar that shows how much of something is on screen and moves it. The thumb is dragged, a click on the track
    // moves a page, and it widens while the mouse is near. It draws nothing when everything fits.
    //
    //   scrollbar bar;
    //   bar.set_range(content_height, view_height).set_on_change(&moved, this);
    //   bar.set_area({x, y, bar_thickness, view_height});
    class scrollbar final : public control
    {
    public:
        // Called after the user moved the position. Setting it from code does not call it.
        using change_fn = void (*)(void* user, scrollbar& source) noexcept;

        scrollbar& set_axis(scroll_axis axis) noexcept { m_axis = axis; return *this; }
        // content is the whole size and page the part that shows, both in pixels. The position stays in range.
        scrollbar& set_range(std::int32_t content, std::int32_t page) noexcept;
        scrollbar& set_position(std::int32_t position) noexcept;
        scrollbar& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] std::int32_t position() const noexcept { return m_position; }
        [[nodiscard]] std::int32_t max_position() const noexcept;
        // Something is out of view.
        [[nodiscard]] bool needed() const noexcept { return m_content > m_page && m_page > 0; }
        // The thumb is being dragged.
        [[nodiscard]] bool dragging() const noexcept { return m_dragging; }
        // How wide the bar wants to be, at the current scale.
        [[nodiscard]] static std::int32_t thickness() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;

    private:
        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            // How far the thumb can travel along the track.
            std::int32_t travel{0};
        };

        [[nodiscard]] thumb_span thumb() const noexcept;
        [[nodiscard]] std::int32_t along(std::int32_t x, std::int32_t y) const noexcept;
        void moved_to(std::int32_t position) noexcept;

        scroll_axis m_axis{scroll_axis::vertical};
        std::int32_t m_content{0};
        std::int32_t m_page{0};
        std::int32_t m_position{0};
        // Where in the thumb it was grabbed.
        std::int32_t m_grab{0};
        bool m_dragging{false};
        bool m_changed{false};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
