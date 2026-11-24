#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

// The look of a row of tabs, shared by the tab bar control and the headers of the dock, so the two cannot drift apart.
// Sizes are at 96 dpi inside, and what is returned is in pixels. dc is a Win32 HDC with the UI font selected.
namespace chrome::tab_style
{
    // What one tab looks like.
    struct tab_look
    {
        // UTF-8, drawn with an ellipsis when it does not fit.
        const char* title{""};
        icon_kind icon{icon_kind::none};
        // The tab on show: the colour of the content, with a line on top.
        bool chosen{false};
        // The mouse is over the tab.
        bool hot{false};
        // The tab is being dragged, which turns its line white.
        bool lifted{false};
        // A place held open for a tab that is about to arrive, with the title of that tab.
        bool ghost{false};
        bool enabled{true};
        // The tab has room at its right end for a cross or a dot.
        bool slot{false};
        bool close_shown{false};
        bool close_hot{false};
        bool modified{false};
        // The colour of the line over the chosen tab. 0 is the accent colour.
        std::uint32_t line{0};
    };

    // Metrics, in pixels.
    [[nodiscard]] std::int32_t height() noexcept;
    // The gap above the tabs, where a floating window can be grabbed.
    [[nodiscard]] std::int32_t top() noexcept;
    [[nodiscard]] std::int32_t least_width() noexcept;
    [[nodiscard]] std::int32_t most_width() noexcept;
    [[nodiscard]] std::int32_t add_width() noexcept;
    // The width a tab asks for, given the width of its title.
    [[nodiscard]] std::int32_t natural_width(std::int32_t title_width, bool has_icon, bool slot) noexcept;
    // Where the cross of a tab sits, in the rectangle the tab was painted in.
    [[nodiscard]] dock_rect close_rect(const dock_rect& tab) noexcept;

    // The colours of the row.
    [[nodiscard]] std::uint32_t bar_colour() noexcept;
    [[nodiscard]] std::uint32_t line_colour() noexcept;

    // The background of the row and the line along its foot, over area.
    void paint_strip(void* dc, const dock_rect& area) noexcept;
    // One tab in tab, which reaches from the gap at the top down to the foot of the row. The chosen tab covers the line of the
    // foot, and so joins the content below it.
    void paint_tab(void* dc, const dock_rect& tab, const tab_look& look) noexcept;
}
