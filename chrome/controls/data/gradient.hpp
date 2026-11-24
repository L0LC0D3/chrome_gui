#pragma once

#include <chrome/controls/property/color.hpp>

#include <cstdint>

namespace chrome
{
    // How a gradient runs between two stops.
    enum class gradient_mode : std::uint8_t
    {
        // Fades from one stop to the next.
        blend,
        // Holds each stop's value up to the next one.
        fixed
    };

    // A colour ramp, as Unity's Gradient is: colour stops and alpha stops, each at a position from 0 to 1, kept in order of
    // position. The colours and the alphas are separate, so a stop of one kind does not need a stop of the other at its place.
    // There are always at least two of each kind.
    class gradient
    {
    public:
        static constexpr std::uint32_t k_max_stops = 16;
        static constexpr std::uint32_t k_min_stops = 2;

        struct color_stop
        {
            float position{0.0f};
            // Red, green and blue; the alpha of the colour is not used.
            color_rgba color{};
        };

        struct alpha_stop
        {
            float position{0.0f};
            float alpha{1.0f};
        };

        // Starts as white to black, opaque.
        gradient() noexcept;

        [[nodiscard]] std::uint32_t color_count() const noexcept { return m_color_count; }
        [[nodiscard]] std::uint32_t alpha_count() const noexcept { return m_alpha_count; }
        [[nodiscard]] const color_stop& color_at(std::uint32_t index) const noexcept { return m_colors[index]; }
        [[nodiscard]] const alpha_stop& alpha_at(std::uint32_t index) const noexcept { return m_alphas[index]; }
        [[nodiscard]] gradient_mode mode() const noexcept { return m_mode; }
        void set_mode(gradient_mode mode) noexcept { m_mode = mode; }

        // Adds a stop and returns where it went in the order, or -1 when there is no room.
        std::int32_t add_color(float position, const color_rgba& color) noexcept;
        std::int32_t add_alpha(float position, float alpha) noexcept;
        // Removes a stop. false when only the least number is left.
        bool remove_color(std::uint32_t index) noexcept;
        bool remove_alpha(std::uint32_t index) noexcept;
        // Moves a stop and returns where it is in the order after that.
        std::uint32_t move_color(std::uint32_t index, float position) noexcept;
        std::uint32_t move_alpha(std::uint32_t index, float position) noexcept;
        void set_color(std::uint32_t index, const color_rgba& color) noexcept;
        void set_alpha(std::uint32_t index, float alpha) noexcept;
        // Back to white to black.
        void reset() noexcept;

        // The colour and the alpha at a position, from 0 to 1.
        [[nodiscard]] color_rgba evaluate(float position) const noexcept;
        [[nodiscard]] color_rgba color_at_position(float position) const noexcept;
        [[nodiscard]] float alpha_at_position(float position) const noexcept;

    private:
        color_stop m_colors[k_max_stops]{};
        alpha_stop m_alphas[k_max_stops]{};
        std::uint32_t m_color_count{0};
        std::uint32_t m_alpha_count{0};
        gradient_mode m_mode{gradient_mode::blend};
    };
}
