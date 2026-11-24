#include <chrome/controls/data/gradient.hpp>

#include <algorithm>

namespace chrome
{
    namespace
    {
        [[nodiscard]] float unit(float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        // Puts the stop at index where its position says, shifting the ones between, and returns the new place.
        template <typename stop>
        std::uint32_t settle(stop* stops, std::uint32_t count, std::uint32_t index) noexcept
        {
            const stop value = stops[index];
            std::uint32_t at = index;

            while (at > 0 && stops[at - 1].position > value.position)
            {
                stops[at] = stops[at - 1];
                --at;
            }

            while (at + 1 < count && stops[at + 1].position < value.position)
            {
                stops[at] = stops[at + 1];
                ++at;
            }

            stops[at] = value;
            return at;
        }

        template <typename stop>
        std::int32_t insert(stop* stops, std::uint32_t& count, const stop& value) noexcept
        {
            if (count >= gradient::k_max_stops)
                return -1;

            stops[count] = value;
            ++count;
            return static_cast<std::int32_t>(settle(stops, count, count - 1));
        }

        template <typename stop>
        bool erase(stop* stops, std::uint32_t& count, std::uint32_t index) noexcept
        {
            if (index >= count || count <= gradient::k_min_stops)
                return false;

            for (std::uint32_t i = index; i + 1 < count; ++i)
                stops[i] = stops[i + 1];

            --count;
            return true;
        }
    }

    gradient::gradient() noexcept
    {
        reset();
    }

    void gradient::reset() noexcept
    {
        m_colors[0] = {0.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
        m_colors[1] = {1.0f, {0.0f, 0.0f, 0.0f, 1.0f}};
        m_color_count = 2;
        m_alphas[0] = {0.0f, 1.0f};
        m_alphas[1] = {1.0f, 1.0f};
        m_alpha_count = 2;
        m_mode = gradient_mode::blend;
    }

    std::int32_t gradient::add_color(float position, const color_rgba& color) noexcept
    {
        return insert(m_colors, m_color_count, color_stop{unit(position), {color.r, color.g, color.b, 1.0f}});
    }

    std::int32_t gradient::add_alpha(float position, float alpha) noexcept
    {
        return insert(m_alphas, m_alpha_count, alpha_stop{unit(position), unit(alpha)});
    }

    bool gradient::remove_color(std::uint32_t index) noexcept
    {
        return erase(m_colors, m_color_count, index);
    }

    bool gradient::remove_alpha(std::uint32_t index) noexcept
    {
        return erase(m_alphas, m_alpha_count, index);
    }

    std::uint32_t gradient::move_color(std::uint32_t index, float position) noexcept
    {
        if (index >= m_color_count)
            return index;

        m_colors[index].position = unit(position);
        return settle(m_colors, m_color_count, index);
    }

    std::uint32_t gradient::move_alpha(std::uint32_t index, float position) noexcept
    {
        if (index >= m_alpha_count)
            return index;

        m_alphas[index].position = unit(position);
        return settle(m_alphas, m_alpha_count, index);
    }

    void gradient::set_color(std::uint32_t index, const color_rgba& color) noexcept
    {
        if (index < m_color_count)
            m_colors[index].color = {unit(color.r), unit(color.g), unit(color.b), 1.0f};
    }

    void gradient::set_alpha(std::uint32_t index, float alpha) noexcept
    {
        if (index < m_alpha_count)
            m_alphas[index].alpha = unit(alpha);
    }

    color_rgba gradient::color_at_position(float position) const noexcept
    {
        position = unit(position);

        if (position <= m_colors[0].position)
            return m_colors[0].color;

        for (std::uint32_t i = 1; i < m_color_count; ++i)
        {
            if (position <= m_colors[i].position)
            {
                const color_stop& from = m_colors[i - 1];
                const color_stop& to = m_colors[i];

                if (m_mode == gradient_mode::fixed)
                    return to.color;

                const float span = to.position - from.position;
                const float t = span > 1.0e-6f ? (position - from.position) / span : 1.0f;
                return {from.color.r + (to.color.r - from.color.r) * t, from.color.g + (to.color.g - from.color.g) * t,
                    from.color.b + (to.color.b - from.color.b) * t, 1.0f};
            }
        }

        return m_colors[m_color_count - 1].color;
    }

    float gradient::alpha_at_position(float position) const noexcept
    {
        position = unit(position);

        if (position <= m_alphas[0].position)
            return m_alphas[0].alpha;

        for (std::uint32_t i = 1; i < m_alpha_count; ++i)
        {
            if (position <= m_alphas[i].position)
            {
                const alpha_stop& from = m_alphas[i - 1];
                const alpha_stop& to = m_alphas[i];

                if (m_mode == gradient_mode::fixed)
                    return to.alpha;

                const float span = to.position - from.position;
                const float t = span > 1.0e-6f ? (position - from.position) / span : 1.0f;
                return from.alpha + (to.alpha - from.alpha) * t;
            }
        }

        return m_alphas[m_alpha_count - 1].alpha;
    }

    color_rgba gradient::evaluate(float position) const noexcept
    {
        color_rgba out = color_at_position(position);
        out.a = alpha_at_position(position);
        return out;
    }
}
