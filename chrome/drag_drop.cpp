#include <chrome/drag_drop.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/dock_host.hpp>

#include <cstdlib>

namespace chrome::drag_drop
{
    namespace
    {
        constexpr std::int32_t k_threshold = 5;
        dock_host* g_host = nullptr;
    }

    drop_effect wanted_effect(const drag_payload& payload, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt && payload.allows(drop_effect::link))
            return drop_effect::link;

        if (ctrl && payload.allows(drop_effect::copy))
            return drop_effect::copy;

        if (shift && payload.allows(drop_effect::move))
            return drop_effect::move;

        // Without a key the source's first choice: moving, then copying, then linking.
        if (payload.allows(drop_effect::move))
            return drop_effect::move;

        if (payload.allows(drop_effect::copy))
            return drop_effect::copy;

        return payload.allows(drop_effect::link) ? drop_effect::link : drop_effect::none;
    }

    std::int32_t threshold() noexcept
    {
        return draw::px(k_threshold);
    }

    bool exceeds_threshold(std::int32_t dx, std::int32_t dy) noexcept
    {
        const std::int32_t limit = threshold();
        return std::abs(dx) >= limit || std::abs(dy) >= limit;
    }

    void install(dock_host* host) noexcept
    {
        g_host = host;
    }

    bool active() noexcept
    {
        return g_host != nullptr && g_host->dragging();
    }

    bool begin(const drag_payload& payload) noexcept
    {
        return g_host != nullptr && g_host->begin_drag(payload);
    }

    void cancel() noexcept
    {
        if (g_host != nullptr)
            g_host->cancel_drag();
    }
}
