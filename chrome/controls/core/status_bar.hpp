#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // The colour of the dot in front of the text of a segment.
    enum class status_tone : std::uint8_t
    {
        // No dot.
        none,
        ok,
        info,
        warning,
        error
    };

    // A strip of segments, side by side, each with a line of text and perhaps a coloured dot, divided by thin lines.
    // A segment is as wide as its text, or a fixed minimum so it does not jump while a number changes, and a flexible one
    // takes the room that is left, with its text cut short by "..." when it does not fit. A segment can be clickable. The
    // text is copied into the segment, so it can come from a buffer that changes. The set functions return true when
    // something changed, so the panel only repaints then.
    //
    //   status_bar bar;
    //   const std::uint32_t message = bar.add_segment(0, true);
    //   const std::uint32_t fps = bar.add_segment(80);
    //   bar.set_on_click(&clicked, this);
    //   bar.set_text(message, "Ready");
    class status_bar final : public control
    {
    public:
        static constexpr std::uint32_t k_max_segments = 8;
        static constexpr std::uint32_t k_text = 160;

        // Called after a click on a clickable segment.
        using click_fn = void (*)(void* user, status_bar& source, std::uint32_t index) noexcept;

        // Adds a segment at the right end and returns its index. min_width is at 96 dpi. When the bar is full this gives
        // the last one again.
        std::uint32_t add_segment(std::int32_t min_width, bool flexible = false) noexcept;
        bool set_text(std::uint32_t index, const char* text) noexcept;
        bool set_tone(std::uint32_t index, status_tone tone) noexcept;
        // A clickable segment lights up under the mouse and calls the click handler.
        status_bar& set_clickable(std::uint32_t index, bool clickable) noexcept;
        status_bar& set_on_click(click_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* text(std::uint32_t index) const noexcept { return index < m_count ? m_segments[index].text : ""; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        void paint(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override;
        void on_click() noexcept override;

    private:
        struct segment
        {
            char text[k_text]{};
            std::int32_t min_width{0};
            bool flexible{false};
            bool clickable{false};
            status_tone tone{status_tone::none};
        };

        // Works out where the segments are for the width of the bar.
        void layout(void* dc) const noexcept;
        // The segment under a point, or -1. Uses the places of the last paint.
        [[nodiscard]] std::int32_t segment_at(std::int32_t x, std::int32_t y) const noexcept;

        segment m_segments[k_max_segments]{};
        mutable dock_rect m_rect[k_max_segments]{};
        std::uint32_t m_count{0};
        std::int32_t m_hot_segment{-1};
        std::int32_t m_press_segment{-1};
        click_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
