#pragma once

#include <cmath>
#include <cstdint>

namespace chrome
{
    // How the curve leaves a key.
    enum class key_mode : std::uint8_t
    {
        // Rounded: the slopes at the key are worked out from its neighbours, so the curve passes through it smoothly.
        smooth,
        // A straight line to the next key.
        linear,
        // The value holds until the next key, then jumps.
        stepped,
        // The two slopes at the key are set by hand, and need not match.
        free
    };

    struct curve_key
    {
        // The slopes are in value per unit of time. For a smooth or linear key they are kept up to date by the curve.
        float time{0.0f};
        float value{0.0f};
        float in_slope{0.0f};
        float out_slope{0.0f};
        key_mode mode{key_mode::smooth};
    };

    // A value that changes over time, as a list of keys in order of time, with a curve through them. Before the first key and
    // after the last the value holds. Editing keeps the keys in order and the slopes of smooth and linear keys current.
    class curve
    {
    public:
        static constexpr std::uint32_t k_max_keys = 128;
        static constexpr std::uint32_t k_none = 0xFFFFFFFFu;
        // Two keys never lie closer in time than this.
        static constexpr float k_min_gap = 1.0e-4f;

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const curve_key& key(std::uint32_t index) const noexcept { return m_keys[index]; }

        // Adds a key in its place in the order. Returns its index, or k_none when the curve is full or a key is already there.
        std::uint32_t add_key(float time, float value, key_mode mode = key_mode::smooth) noexcept;
        void remove_key(std::uint32_t index) noexcept;
        // Moves a key. Its time is held between its neighbours, so no key changes its index.
        void move_key(std::uint32_t index, float time, float value) noexcept;
        void set_mode(std::uint32_t index, key_mode mode) noexcept;
        // Sets both slopes of a key by hand, which makes it free.
        void set_slopes(std::uint32_t index, float in_slope, float out_slope) noexcept;
        void clear() noexcept { m_count = 0; }

        // The value at a time.
        [[nodiscard]] float evaluate(float time) const noexcept;
        // The value at a time inside the segment that starts at a key, which is the one after the key before the time.
        [[nodiscard]] float evaluate_segment(std::uint32_t index, float time) const noexcept;
        // The index of the last key at or before a time, or k_none when it is before the first.
        [[nodiscard]] std::uint32_t key_before(float time) const noexcept;

    private:
        // Brings the slopes of smooth and linear keys up to date.
        void refresh() noexcept;

        curve_key m_keys[k_max_keys]{};
        std::uint32_t m_count{0};
    };

    // Which keys of which curves are chosen, shared by the views that show the same curves so a choice in one shows in the other.
    // Curves are numbered as the views number them, the same in every view that shares it.
    class key_selection
    {
    public:
        static constexpr std::uint32_t k_max_curves = 16;

        [[nodiscard]] bool is_selected(std::uint32_t curve_index, std::uint32_t key) const noexcept
        {
            return curve_index < k_max_curves && key < curve::k_max_keys && (m_bits[curve_index][key >> 6] >> (key & 63) & 1) != 0;
        }

        void set(std::uint32_t curve_index, std::uint32_t key, bool on) noexcept;
        void clear() noexcept;
        [[nodiscard]] std::uint32_t count() const noexcept;
        // A key was taken out or put in at an index: the choices after it move along.
        void key_removed(std::uint32_t curve_index, std::uint32_t key) noexcept;
        void key_inserted(std::uint32_t curve_index, std::uint32_t key) noexcept;

    private:
        std::uint64_t m_bits[k_max_curves][2]{};
    };

    // A round step for a grid: 1, 2 or 5 times a power of ten, at least as large as the value asked for.
    [[nodiscard]] inline float nice_step(float raw) noexcept
    {
        if (!(raw > 0.0f))
            return 1.0f;

        const float magnitude = std::pow(10.0f, std::floor(std::log10(raw)));
        const float fraction = raw / magnitude;
        return (fraction <= 1.0f ? 1.0f : fraction <= 2.0f ? 2.0f : fraction <= 5.0f ? 5.0f : 10.0f) * magnitude;
    }
}
