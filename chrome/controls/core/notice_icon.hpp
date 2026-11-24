#pragma once

#include <chrome/dock.hpp>

#include <cstdint>

namespace chrome
{
    // What a message is about. The dialog and the toast show it as a round badge in the colour of the kind.
    enum class notice_kind : std::uint8_t
    {
        none,
        info,
        success,
        warning,
        error,
        question
    };

    namespace notice
    {
        // The colour of a kind. none is the accent colour.
        [[nodiscard]] std::uint32_t colour(notice_kind kind) noexcept;
        // A round badge with the mark of the kind, filling a square box. dc is a Win32 HDC.
        void paint_badge(void* dc, notice_kind kind, const dock_rect& box) noexcept;
    }
}
