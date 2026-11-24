#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // What happened in a breadcrumb bar. The bar never changes its own path: the owner does, usually in answer to these.
    enum class crumb_event : std::uint8_t
    {
        // A segment before the last was clicked, or chosen from the list of hidden ones. segment is its place, so the path
        // the user asked for is the segments up to and including it. child is -1.
        navigate,
        // A name was chosen from the list under the arrow after a segment. segment is that segment and child is the place
        // of the name in the list, as the children handler gave it.
        pick,
        // The user clicked the empty part of the bar, pressed F2 or Ctrl+L: they want to type a path. segment and child are -1.
        edit
    };

    // A path as a row of clickable segments with an arrow after each, such as the folder an asset browser shows:
    //
    //   Assets > Models > Props > Barrels
    //
    // A click on a segment goes there. The arrow after a segment opens the list of what is inside it, when the owner gives
    // the bar a way to ask for that. When the path is too long for the bar, the first segments fold into one "..." that opens
    // a list of them. A click on the empty part asks for the path to be typed, which a path field answers.
    //
    //   breadcrumb_bar crumbs;
    //   crumbs.set_path("Assets/Models/Props").set_on_event(&on_crumb, this).set_children(&children_of, this);
    //
    //   static void on_crumb(void* user, breadcrumb_bar& source, crumb_event what, std::int32_t segment, std::int32_t child) noexcept;
    //   static std::uint32_t children_of(void* user, breadcrumb_bar& source, std::int32_t segment, const char** names, std::uint32_t capacity) noexcept;
    class breadcrumb_bar final : public control
    {
    public:
        static constexpr std::int32_t k_max_segments = 24;
        static constexpr std::int32_t k_title_length = 48;
        static constexpr std::uint32_t k_max_names = 32;
        // The rows the list shows at once. A longer list scrolls with the wheel and the keys.
        static constexpr std::int32_t k_max_rows = 8;

        using event_fn = void (*)(void* user, breadcrumb_bar& source, crumb_event what, std::int32_t segment, std::int32_t child) noexcept;
        // Writes the names of what is inside the folder that segment ends, up to capacity of them, and returns how many. The
        // text is copied at once, so it only has to live until the handler returns. Return 0 for none, and no arrow list opens.
        using children_fn = std::uint32_t (*)(void* user, breadcrumb_bar& source, std::int32_t segment, const char** names, std::uint32_t capacity) noexcept;

        // Sets the path from text with / or \ between the segments. Empty parts are left out. Returns how many segments it has.
        breadcrumb_bar& set_path(const char* path) noexcept;
        // Adds one segment at the end, and returns false when the bar is full. Titles are UTF-8 and copied.
        bool add_segment(const char* title) noexcept;
        // Keeps only the first count segments.
        breadcrumb_bar& truncate(std::int32_t count) noexcept;
        breadcrumb_bar& clear() noexcept;
        // A small picture before the first segment.
        breadcrumb_bar& set_root_icon(icon_kind icon) noexcept { m_root_icon = icon; m_stale = true; return *this; }
        // Writes the path, with / between the segments, and returns its length. Always ends with a zero when capacity is not 0.
        std::uint32_t path(char* out, std::uint32_t capacity, std::int32_t count = -1) const noexcept;

        [[nodiscard]] std::int32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* segment(std::int32_t index) const noexcept { return index >= 0 && index < m_count ? m_titles[index] : ""; }

        // Without a handler there are no arrows after the segments, except the one that stands between two of them.
        breadcrumb_bar& set_children(children_fn handler, void* user) noexcept { m_children = handler; m_children_user = user; m_stale = true; return *this; }
        breadcrumb_bar& set_on_event(event_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }
        // The width the bar asks for, at 96 dpi. 0, the default, asks for the width its segments need.
        breadcrumb_bar& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // The area the list has to stay inside, in the coordinates of the controls. It opens upward when it does not fit
        // below. An empty area, the default, means it always opens downward.
        breadcrumb_bar& set_bounds(const dock_rect& bounds) noexcept { m_bounds = bounds; return *this; }
        breadcrumb_bar& set_name(const char* name) noexcept { m_name = name; return *this; }
        breadcrumb_bar& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        void close() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool overlay_open() const noexcept override { return m_open; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override;
        void on_click() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        void on_focus(bool gained) noexcept override;

    private:
        enum class part_kind : std::uint8_t
        {
            none,
            segment,
            arrow,
            fold
        };

        // What is under a point.
        struct part
        {
            part_kind kind{part_kind::none};
            std::int32_t index{-1};

            [[nodiscard]] bool operator==(const part& other) const noexcept { return kind == other.kind && index == other.index; }
        };

        void measure(void* dc) const noexcept;
        [[nodiscard]] part find(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] dock_rect arrow_rect(std::int32_t segment) const noexcept;
        [[nodiscard]] dock_rect segment_rect(std::int32_t segment) const noexcept;
        [[nodiscard]] dock_rect fold_rect() const noexcept;
        // Opens the list of what is in a segment, or with -1 the list of the segments that are folded away.
        void open_list(std::int32_t segment) noexcept;
        [[nodiscard]] dock_rect full_area() const noexcept;
        [[nodiscard]] std::int32_t anchor_x() const noexcept;
        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        void show_row(std::int32_t index) noexcept;
        bool point_at(std::int32_t x, std::int32_t y) noexcept;
        void choose(std::int32_t row) noexcept;
        void raise(crumb_event what, std::int32_t segment, std::int32_t child) noexcept;

        char m_titles[k_max_segments][k_title_length]{};
        std::int32_t m_count{0};
        icon_kind m_root_icon{icon_kind::none};
        std::int32_t m_width{0};
        const char* m_name{nullptr};
        event_fn m_handler{nullptr};
        void* m_user{nullptr};
        children_fn m_children{nullptr};
        void* m_children_user{nullptr};
        dock_rect m_bounds{};

        // The open list. m_open_for is the segment whose contents it shows, or -1 for the folded segments.
        bool m_open{false};
        std::int32_t m_open_for{-1};
        char m_names[k_max_names][k_title_length]{};
        std::uint32_t m_name_count{0};
        std::int32_t m_highlight{-1};
        std::int32_t m_first{0};

        part m_hot{};
        part m_press{};
        std::int32_t m_x{0};
        std::int32_t m_y{0};

        // Where things lie, measured in paint, which is const, and so mutable.
        mutable std::int32_t m_seg_x[k_max_segments]{};
        mutable std::int32_t m_seg_w[k_max_segments]{};
        mutable std::int32_t m_arrow_x[k_max_segments]{};
        mutable std::int32_t m_fold_x{0};
        // The first segment that shows. The ones before it are folded.
        mutable std::int32_t m_first_shown{0};
        mutable std::int32_t m_measured_width{-1};
        mutable bool m_stale{true};
    };
}
