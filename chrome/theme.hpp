#pragma once

#include <cstdint>

namespace chrome
{
    enum class theme_mode : std::uint8_t
    {
        dark,
        light
    };

    // Colours as 0xRRGGBB. Every window of the editor takes its colours from here, so this is the only place
    // the look is set.
    struct theme_colours
    {
        std::uint32_t window;
        std::uint32_t panel;
        std::uint32_t splitter;
        // Tab strips, the menu bar and the window frame.
        std::uint32_t title;
        std::uint32_t hover;
        std::uint32_t text;
        std::uint32_t text_dim;
        std::uint32_t accent;
        // The areas a dragged panel can dock into.
        std::uint32_t hint;
    };

    inline constexpr theme_colours k_theme_dark{0x1E1E1E, 0x252526, 0x111111, 0x2D2D30, 0x3E3E42, 0xD4D4D4, 0x9D9D9D, 0x007ACC, 0xD98324};
    inline constexpr theme_colours k_theme_light{0xF3F3F3, 0xFAFAFA, 0xC8C8C8, 0xE6E6E8, 0xD4D4D8, 0x1E1E1E, 0x5A5A5A, 0x007ACC, 0xD98324};

    // Mixes two colours: t is how much of b there is, from 0 to 1.
    [[nodiscard]] inline std::uint32_t blend_colour(std::uint32_t a, std::uint32_t b, float t) noexcept
    {
        const auto channel = [&](int shift) {
            const float value = static_cast<float>((a >> shift) & 0xFF) * (1.0f - t) + static_cast<float>((b >> shift) & 0xFF) * t;
            return static_cast<std::uint32_t>(value + 0.5f);
        };

        return (channel(16) << 16) | (channel(8) << 8) | channel(0);
    }

    inline theme_mode g_theme_mode = theme_mode::dark;

    [[nodiscard]] inline const theme_colours& theme() noexcept
    {
        return g_theme_mode == theme_mode::dark ? k_theme_dark : k_theme_light;
    }
}
