#include <chrome/controls/property/color.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr std::uint32_t k_checker_light = 0xB4B4B4;
        constexpr std::uint32_t k_checker_dark = 0x808080;

        [[nodiscard]] float saturate(float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }
    }

    std::int32_t to_byte(float channel) noexcept
    {
        return static_cast<std::int32_t>(saturate(channel) * 255.0f + 0.5f);
    }

    std::uint32_t to_rgb24(const color_rgba& color) noexcept
    {
        return (static_cast<std::uint32_t>(to_byte(color.r)) << 16) | (static_cast<std::uint32_t>(to_byte(color.g)) << 8) | static_cast<std::uint32_t>(to_byte(color.b));
    }

    color_rgba from_rgb24(std::uint32_t rgb, float alpha) noexcept
    {
        return {static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f, alpha};
    }

    color_hsv to_hsv(const color_rgba& color, float fallback_hue) noexcept
    {
        const float r = saturate(color.r);
        const float g = saturate(color.g);
        const float b = saturate(color.b);
        const float high = std::max({r, g, b});
        const float low = std::min({r, g, b});
        const float span = high - low;
        color_hsv out;
        out.v = high;
        out.s = high > 0.0f ? span / high : 0.0f;

        if (span <= 0.0f)
        {
            out.h = fallback_hue;
            return out;
        }

        float hue = 0.0f;

        if (high == r)
            hue = (g - b) / span + (g < b ? 6.0f : 0.0f);
        else if (high == g)
            hue = (b - r) / span + 2.0f;
        else
            hue = (r - g) / span + 4.0f;

        out.h = hue / 6.0f;
        return out;
    }

    color_rgba from_hsv(const color_hsv& hsv, float alpha) noexcept
    {
        const float h = hsv.h - std::floor(hsv.h);
        const float s = saturate(hsv.s);
        const float v = saturate(hsv.v);
        const float sector = h * 6.0f;
        const float whole = std::floor(sector);
        const float fraction = sector - whole;
        const float p = v * (1.0f - s);
        const float q = v * (1.0f - s * fraction);
        const float t = v * (1.0f - s * (1.0f - fraction));

        switch (static_cast<std::int32_t>(whole) % 6)
        {
        case 0:
            return {v, t, p, alpha};

        case 1:
            return {q, v, p, alpha};

        case 2:
            return {p, v, t, alpha};

        case 3:
            return {p, q, v, alpha};

        case 4:
            return {t, p, v, alpha};

        default:
            return {v, p, q, alpha};
        }
    }

    void paint_checker(void* dc, const dock_rect& area, std::int32_t cell, const color_rgba* over) noexcept
    {
        if (area.width <= 0 || area.height <= 0 || cell <= 0)
            return;

        const auto shown = [&](std::uint32_t ground) {
            if (over == nullptr)
                return ground;

            const color_rgba back = from_rgb24(ground);
            const float a = saturate(over->a);
            return to_rgb24({back.r + (over->r - back.r) * a, back.g + (over->g - back.g) * a, back.b + (over->b - back.b) * a, 1.0f});
        };

        const std::uint32_t light = shown(k_checker_light);
        const std::uint32_t dark = shown(k_checker_dark);

        for (std::int32_t y = 0, row = 0; y < area.height; y += cell, ++row)
        {
            for (std::int32_t x = 0, column = 0; x < area.width; x += cell, ++column)
                draw::fill(dc, {area.x + x, area.y + y, std::min(cell, area.width - x), std::min(cell, area.height - y)}, ((row + column) & 1) != 0 ? dark : light);
        }
    }
}
