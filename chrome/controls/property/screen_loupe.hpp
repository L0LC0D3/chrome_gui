#pragma once

#include <cstdint>

namespace chrome
{
    // A small window that floats beside the pointer and shows the screen under it magnified, with the pixel in the middle
    // marked and its colour written under it, for picking a colour from anywhere on the screen. It sits over every window
    // without taking the focus or the mouse, so it does not disturb what is being pointed at. Positions are in screen pixels.
    //
    //   loupe.show(cursor_x, cursor_y, rgb);       // each time the pointer moved
    //   loupe.hide();
    class screen_loupe
    {
    public:
        screen_loupe() noexcept = default;
        ~screen_loupe();
        screen_loupe(const screen_loupe&) = delete;
        screen_loupe& operator=(const screen_loupe&) = delete;

        // Shows the loupe by a point of the screen, on the side of it that has room, with the colour of the pixel at that point
        // as 0xRRGGBB. The window is made when it is first needed.
        void show(std::int32_t x, std::int32_t y, std::uint32_t rgb) noexcept;
        void hide() noexcept;
        [[nodiscard]] bool visible() const noexcept { return m_visible; }

        // How many pixels of the screen show across the loupe.
        static constexpr std::int32_t k_span = 11;

        // Paints the loupe into a device context. The window calls this for its own paint message.
        void paint(void* dc) const noexcept;

    private:
        [[nodiscard]] bool create() noexcept;

        void* m_window{nullptr};
        bool m_visible{false};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        std::uint32_t m_rgb{0};
        // Where the window is, so it is only moved when the pointer has taken it somewhere else.
        std::int32_t m_left{0};
        std::int32_t m_top{0};
        std::int32_t m_width{0};
        std::int32_t m_height{0};
    };
}
