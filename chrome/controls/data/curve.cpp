#include <chrome/controls/data/curve.hpp>

#include <algorithm>

namespace chrome
{
    std::uint32_t curve::add_key(float time, float value, key_mode mode) noexcept
    {
        if (m_count >= k_max_keys)
            return k_none;

        std::uint32_t at = 0;

        while (at < m_count && m_keys[at].time < time)
            ++at;

        if ((at < m_count && m_keys[at].time - time < k_min_gap) || (at > 0 && time - m_keys[at - 1].time < k_min_gap))
            return k_none;

        for (std::uint32_t i = m_count; i > at; --i)
            m_keys[i] = m_keys[i - 1];

        m_keys[at] = {time, value, 0.0f, 0.0f, mode};
        ++m_count;
        refresh();
        return at;
    }

    void curve::remove_key(std::uint32_t index) noexcept
    {
        if (index >= m_count)
            return;

        for (std::uint32_t i = index; i + 1 < m_count; ++i)
            m_keys[i] = m_keys[i + 1];

        --m_count;
        refresh();
    }

    void curve::move_key(std::uint32_t index, float time, float value) noexcept
    {
        if (index >= m_count)
            return;

        const float low = index > 0 ? m_keys[index - 1].time + k_min_gap : time;
        const float high = index + 1 < m_count ? m_keys[index + 1].time - k_min_gap : time;
        m_keys[index].time = std::clamp(time, std::min(low, high), std::max(low, high));
        m_keys[index].value = value;
        refresh();
    }

    void curve::set_mode(std::uint32_t index, key_mode mode) noexcept
    {
        if (index >= m_count)
            return;

        m_keys[index].mode = mode;
        refresh();
    }

    void curve::set_slopes(std::uint32_t index, float in_slope, float out_slope) noexcept
    {
        if (index >= m_count)
            return;

        m_keys[index].in_slope = in_slope;
        m_keys[index].out_slope = out_slope;
        m_keys[index].mode = key_mode::free;
        refresh();
    }

    void curve::refresh() noexcept
    {
        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            curve_key& key = m_keys[i];
            const bool has_prev = i > 0;
            const bool has_next = i + 1 < m_count;
            const float prev_slope = has_prev ? (key.value - m_keys[i - 1].value) / (key.time - m_keys[i - 1].time) : 0.0f;
            const float next_slope = has_next ? (m_keys[i + 1].value - key.value) / (m_keys[i + 1].time - key.time) : 0.0f;

            switch (key.mode)
            {
            case key_mode::smooth:
            {
                float slope = 0.0f;

                if (has_prev && has_next)
                    slope = (m_keys[i + 1].value - m_keys[i - 1].value) / (m_keys[i + 1].time - m_keys[i - 1].time);
                else if (has_next)
                    slope = next_slope;
                else if (has_prev)
                    slope = prev_slope;

                key.in_slope = slope;
                key.out_slope = slope;
                break;
            }

            case key_mode::linear:
                key.out_slope = has_next ? next_slope : prev_slope;
                key.in_slope = has_prev ? prev_slope : key.out_slope;
                break;

            case key_mode::stepped:
                key.out_slope = 0.0f;
                key.in_slope = has_prev ? prev_slope : 0.0f;
                break;

            case key_mode::free:
                break;
            }
        }
    }

    std::uint32_t curve::key_before(float time) const noexcept
    {
        std::uint32_t low = 0;
        std::uint32_t high = m_count;

        // The first key after the time.
        while (low < high)
        {
            const std::uint32_t middle = (low + high) / 2;

            if (m_keys[middle].time <= time)
                low = middle + 1;
            else
                high = middle;
        }

        return low == 0 ? k_none : low - 1;
    }

    float curve::evaluate_segment(std::uint32_t index, float time) const noexcept
    {
        if (index + 1 >= m_count)
            return index < m_count ? m_keys[index].value : 0.0f;

        const curve_key& a = m_keys[index];
        const curve_key& b = m_keys[index + 1];

        if (a.mode == key_mode::stepped)
            return a.value;

        const float span = b.time - a.time;
        const float u = std::clamp((time - a.time) / span, 0.0f, 1.0f);

        if (a.mode == key_mode::linear)
            return a.value + (b.value - a.value) * u;

        // Cubic Hermite through the two keys with their slopes.
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float m0 = a.out_slope * span;
        const float m1 = b.in_slope * span;
        return (2.0f * u3 - 3.0f * u2 + 1.0f) * a.value + (u3 - 2.0f * u2 + u) * m0 + (-2.0f * u3 + 3.0f * u2) * b.value + (u3 - u2) * m1;
    }

    float curve::evaluate(float time) const noexcept
    {
        if (m_count == 0)
            return 0.0f;

        const std::uint32_t before = key_before(time);

        if (before == k_none)
            return m_keys[0].value;

        return evaluate_segment(before, time);
    }

    void key_selection::set(std::uint32_t curve_index, std::uint32_t key, bool on) noexcept
    {
        if (curve_index >= k_max_curves || key >= curve::k_max_keys)
            return;

        const std::uint64_t mask = std::uint64_t{1} << (key & 63);

        if (on)
            m_bits[curve_index][key >> 6] |= mask;
        else
            m_bits[curve_index][key >> 6] &= ~mask;
    }

    void key_selection::clear() noexcept
    {
        for (auto& row : m_bits)
            row[0] = row[1] = 0;
    }

    std::uint32_t key_selection::count() const noexcept
    {
        std::uint32_t total = 0;

        for (const auto& row : m_bits)
        {
            for (const std::uint64_t word : row)
            {
                std::uint64_t rest = word;

                while (rest != 0)
                {
                    rest &= rest - 1;
                    ++total;
                }
            }
        }

        return total;
    }

    void key_selection::key_removed(std::uint32_t curve_index, std::uint32_t key) noexcept
    {
        if (curve_index >= k_max_curves)
            return;

        for (std::uint32_t i = key; i + 1 < curve::k_max_keys; ++i)
            set(curve_index, i, is_selected(curve_index, i + 1));

        set(curve_index, curve::k_max_keys - 1, false);
    }

    void key_selection::key_inserted(std::uint32_t curve_index, std::uint32_t key) noexcept
    {
        if (curve_index >= k_max_curves)
            return;

        for (std::uint32_t i = curve::k_max_keys - 1; i > key; --i)
            set(curve_index, i, is_selected(curve_index, i - 1));

        set(curve_index, key, false);
    }
}
