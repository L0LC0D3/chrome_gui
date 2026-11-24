#pragma once

#include <cstdint>

namespace chrome
{
    // Pictures a control can carry. They are drawn as lines, so they take the colour of the text.
    enum class icon_kind : std::uint8_t
    {
        none,
        plus,
        play,
        cross,
        menu,
        check,
        // A bin, two sheets, an arrow down onto a line, a clock, a window with a pane under it, and a funnel.
        trash,
        stack,
        arrow_down,
        clock,
        panel,
        filter,
        // A turning arrow to the left and to the right, a square, two bars, a floppy disk and a folder.
        undo,
        redo,
        stop,
        pause,
        save,
        folder,
        // A sun with rays, and a crescent moon, for the light and dark theme.
        sun,
        moon,
        // A window split into three panes, for a layout.
        layout,
        // A house, for the home page, and a pair of angle brackets, for code.
        home,
        code,
        // Four squares in a grid, and rows of a list, for the two ways to show files.
        grid,
        list,
        // An eye, open and struck through, for what is shown and what is hidden.
        eye,
        eye_off
    };
}
