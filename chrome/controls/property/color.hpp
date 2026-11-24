#pragma once

#include <chrome/dock.hpp>

#include <cstdint>

namespace chrome
{
    // A colour as the editor edits it: red, green, blue and alpha, each from 0 to 1.
    struct color_rgba
    {
        float r{1.0f};
        float g{1.0f};
        float b{1.0f};
        float a{1.0f};

        [[nodiscard]] friend constexpr bool operator==(const color_rgba&, const color_rgba&) noexcept = default;
    };

    // Hue, saturation and value, each from 0 to 1. The hue goes round: 0 and 1 are both red.
    struct color_hsv
    {
        float h{0.0f};
        float s{0.0f};
        float v{1.0f};
    };

    // The hue of a grey or a black is not defined, so it is kept from fallback_hue, which keeps a picker's hue where the user
    // left it while the saturation or the value is at 0.
    [[nodiscard]] color_hsv to_hsv(const color_rgba& color, float fallback_hue) noexcept;
    [[nodiscard]] color_rgba from_hsv(const color_hsv& hsv, float alpha) noexcept;

    // 0xRRGGBB, the colour without its alpha, each channel rounded and kept inside 0 to 255.
    [[nodiscard]] std::uint32_t to_rgb24(const color_rgba& color) noexcept;
    [[nodiscard]] color_rgba from_rgb24(std::uint32_t rgb, float alpha = 1.0f) noexcept;
    // A channel from 0 to 1 as a whole number from 0 to 255.
    [[nodiscard]] std::int32_t to_byte(float channel) noexcept;

    // The squares that show through a colour that is not opaque. dc is a Win32 HDC and the sizes are in pixels. With over set,
    // each square shows through that colour at its alpha, which is what a colour with alpha looks like on a plain ground.
    void paint_checker(void* dc, const dock_rect& area, std::int32_t cell, const color_rgba* over = nullptr) noexcept;
}
