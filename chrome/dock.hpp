#pragma once

#include <cstddef>
#include <cstdint>

namespace chrome
{
    struct dock_rect
    {
        std::int32_t x{0};
        std::int32_t y{0};
        std::int32_t width{0};
        std::int32_t height{0};

        [[nodiscard]] friend constexpr bool operator==(dock_rect, dock_rect) noexcept = default;
    };

    enum class dock_axis : std::uint8_t
    {
        // The two children sit side by side. The splitter is a vertical bar.
        horizontal,
        // The two children are stacked. The splitter is a horizontal bar.
        vertical
    };

    using panel_id = std::uint32_t;
    using node_id = std::uint32_t;
    using root_id = std::uint32_t;

    inline constexpr panel_id k_no_panel = 0xFFFFFFFFu;
    inline constexpr node_id k_no_node = 0xFFFFFFFFu;
    inline constexpr root_id k_no_root = 0xFFFFFFFFu;
    // Insert at the end of a tab strip.
    inline constexpr std::uint32_t k_tab_end = 0xFFFFFFFFu;

    struct panel_desc
    {
        // UTF-8. Must outlive the model.
        const char* title{nullptr};
        // The window system's handle for the panel's content, such as an HWND. The model never touches it.
        void* handle{nullptr};
        // A permanent panel cannot be closed. It can still be moved, tabbed and floated.
        bool permanent{false};
    };

    enum class drop_zone : std::uint8_t
    {
        none,
        // Join the tab strip of the group, or fill an empty root.
        tabs,
        // Split the group and put the panel on that side.
        left,
        right,
        top,
        bottom,
        // Split the whole root and put the panel along that edge of it.
        root_left,
        root_right,
        root_top,
        root_bottom
    };

    struct drop_target
    {
        drop_zone zone{drop_zone::none};
        root_id root{k_no_root};
        // k_no_node when the root is empty.
        node_id group{k_no_node};
        // Screen rectangle that shows where the panel would land.
        dock_rect preview{};
        // The point is over a dock root even when it is not a valid target.
        bool inside{false};
        // The drop does nothing on purpose, such as a lone panel put back on its own tab strip. Without this a drop
        // that is not on a docking zone floats the panel.
        bool keep{false};
        // The group under the point shows a compass of docking tiles, and compass is its bounding square on screen.
        bool hint{false};
        dock_rect compass{};
        // The root under the point shows a tile on each of its edges, and root_area is that root on screen.
        bool root_hint{false};
        dock_rect root_area{};
    };

    // Moves one tab group to its rectangle, in the client coordinates of its root.
    using dock_place_fn = void (*)(void* user, node_id group, const dock_rect& rect) noexcept;

    // The layout of the editor's panels, with no window-system code in it.
    //
    // A root is one window's worth of layout: root 0 is the main window, the others are floating windows.
    // A root holds a tree. Its leaves are tab groups, a stack of panels with one active, and its inner nodes
    // split the space in two. Fixed capacity and no allocation.
    //
    // Panels start closed. A closed panel is in no group. Moving a panel takes it out of its old group first,
    // and a group that ends up empty goes away, so its split collapses into the sibling.
    class dock_model
    {
    public:
        static constexpr std::uint32_t k_max_panels = 16;
        static constexpr std::uint32_t k_max_nodes = 64;
        static constexpr std::uint32_t k_max_roots = 8;
        static constexpr root_id k_main_root = 0;
        // Sizes in pixels. They follow the display scale, so ask for them instead of keeping the numbers.
        // Width of the bar between two children.
        [[nodiscard]] static std::int32_t splitter_width() noexcept;
        // A child never gets smaller than this while there is room for it.
        [[nodiscard]] static std::int32_t min_panel_size() noexcept;
        [[nodiscard]] static std::int32_t tab_height() noexcept;
        // The compass is a cross of five tiles in the middle of a group: tabs in the centre and a side tile around it.
        [[nodiscard]] static std::int32_t compass_cell() noexcept;
        [[nodiscard]] static std::int32_t compass_gap() noexcept;
        [[nodiscard]] static std::int32_t compass_extent() noexcept;
        // How far the edge tiles of a root sit in from its edge.
        [[nodiscard]] static std::int32_t root_inset() noexcept;
        // 1 is 96 dpi. Every size above is multiplied by it.
        static void set_scale(float scale) noexcept;
        [[nodiscard]] static float scale() noexcept;
        // Scales a size given at 96 dpi.
        [[nodiscard]] static std::int32_t scaled(std::int32_t pixels) noexcept;

        // How much of the root a panel docked to its edge takes.
        static constexpr float k_root_share = 0.25f;

        // The edge tile of a root_* zone inside a root's area. Empty for any other zone.
        [[nodiscard]] static dock_rect root_tile(const dock_rect& area, drop_zone zone) noexcept;
        // The strip of the root a root_* zone would fill.
        [[nodiscard]] static dock_rect root_strip(const dock_rect& area, drop_zone zone) noexcept;
        // The tile of one zone inside a compass square. Empty for none.
        [[nodiscard]] static dock_rect compass_tile(const dock_rect& compass, drop_zone zone) noexcept;

        dock_model() noexcept;

        // k_no_panel when the table is full or the description is not usable.
        [[nodiscard]] panel_id add_panel(const panel_desc& desc) noexcept;
        [[nodiscard]] std::uint32_t panel_count() const noexcept { return m_panel_count; }
        [[nodiscard]] bool is_open(panel_id panel) const noexcept;
        // True for a panel that cannot be closed: one made permanent, or any panel while the model is locked.
        [[nodiscard]] bool is_permanent(panel_id panel) const noexcept;
        // A locked model keeps the set of panels as it is: none can be closed, and none that is closed can be opened. The panels
        // can still be resized. It is for an arrangement that is a fixed set of panels, such as a code editor's.
        void set_locked(bool locked) noexcept { m_locked = locked; }
        [[nodiscard]] bool locked() const noexcept { return m_locked; }
        // A panel that is the only tab of its group shows no header. The group has the header again when another tab joins it.
        void set_headerless(panel_id panel, bool headerless) noexcept;
        [[nodiscard]] bool is_headerless(panel_id panel) const noexcept;
        // The height of the header of a group: tab_height(), or 0 when its only tab is headerless.
        [[nodiscard]] std::int32_t header_height(node_id group) const noexcept;
        [[nodiscard]] const char* title_of(panel_id panel) const noexcept;
        [[nodiscard]] void* handle_of(panel_id panel) const noexcept;
        // k_no_node when the panel is closed.
        [[nodiscard]] node_id group_of(panel_id panel) const noexcept;

        // Placement. Each one opens a closed panel or moves an open one, and returns false when it cannot.
        // Into an existing group at a tab position. With group k_no_node the panel becomes the root of an empty root.
        [[nodiscard]] bool dock_tabs(panel_id panel, root_id root, node_id group, std::uint32_t index = k_tab_end) noexcept;
        // Next to a group. share is how much of the group's space the new panel takes.
        [[nodiscard]] bool dock_split(panel_id panel, node_id group, drop_zone side, float share = 0.3f) noexcept;
        // Along an edge of a whole root, so it spans the root's full height or width. side is a group side (left, right,
        // top or bottom). share is how much of the root the new panel takes.
        [[nodiscard]] bool dock_root_split(panel_id panel, root_id root, drop_zone side, float share = k_root_share) noexcept;
        // In a new floating root. screen is its client rectangle on screen. k_no_root when there is no room.
        [[nodiscard]] root_id float_panel(panel_id panel, const dock_rect& screen) noexcept;
        // false for a permanent or a closed panel. The place it had is remembered, as a tab next to the panel it shared a
        // group with, as a split beside the group it sat next to, or as a floating window, so restore_home can put it back.
        [[nodiscard]] bool close_panel(panel_id panel) noexcept;
        // Opens a closed panel where it was docked when it was closed, as a layout saved it, or as close_panel remembered
        // it. false, with the panel still closed, when it has no remembered place or the panel it was next to is not open
        // and has nowhere to follow it to, and then the caller picks a place.
        [[nodiscard]] bool restore_home(panel_id panel) noexcept;
        // Closes every panel and removes every floating root. The panels stay registered.
        void reset() noexcept;

        // The panel that gets keyboard input. k_no_panel when none does. Only an open panel can have it, and closing
        // the panel takes it away.
        [[nodiscard]] panel_id focused() const noexcept { return m_focus; }
        void set_focused(panel_id panel) noexcept;

        // The layout as text: roots, splits, tab groups and the tab order, with panels named by their title.
        // Returns the length written, or 0 when capacity is too small. Not null terminated.
        [[nodiscard]] std::size_t save(char* out, std::size_t capacity) const noexcept;
        // Replaces the layout with a saved one. Titles that match no panel are skipped. false, with the layout left
        // as it was, when the text is damaged or leaves a permanent panel closed.
        [[nodiscard]] bool load(const char* text, std::size_t size) noexcept;
        // Places a panel where find_drop pointed. index is the tab position when the zone is tabs.
        [[nodiscard]] bool apply_drop(panel_id panel, const drop_target& target, std::uint32_t index = k_tab_end) noexcept;

        // Roots
        [[nodiscard]] bool root_used(root_id root) const noexcept;
        [[nodiscard]] bool root_floating(root_id root) const noexcept;
        // Counts up whenever a root is created, so a window can tell a new root from the old one in the same slot.
        [[nodiscard]] std::uint32_t root_serial(root_id root) const noexcept;
        [[nodiscard]] node_id root_node(root_id root) const noexcept;
        // The client rectangle of the root's window, in screen coordinates.
        void set_root_area(root_id root, const dock_rect& screen) noexcept;
        [[nodiscard]] dock_rect root_area(root_id root) const noexcept;
        // Computes every rectangle of the root and calls place for each group.
        void layout(root_id root, dock_place_fn place, void* user) noexcept;

        // Groups and splits
        [[nodiscard]] bool node_used(node_id node) const noexcept;
        [[nodiscard]] bool is_group(node_id node) const noexcept;
        [[nodiscard]] std::uint32_t node_serial(node_id node) const noexcept;
        [[nodiscard]] root_id root_of(node_id node) const noexcept;
        [[nodiscard]] dock_rect rect_of(node_id node) const noexcept;
        [[nodiscard]] std::uint32_t tab_count(node_id group) const noexcept;
        [[nodiscard]] panel_id tab_at(node_id group, std::uint32_t index) const noexcept;
        [[nodiscard]] panel_id active_tab(node_id group) const noexcept;
        void set_active(node_id group, panel_id panel) noexcept;

        // Splitter bars. The point is in the root's client coordinates.
        [[nodiscard]] node_id hit_splitter(root_id root, std::int32_t x, std::int32_t y) const noexcept;
        // The bar looks thin, but it is grabbed from this many pixels each side of it too (the panels let the mouse through there).
        [[nodiscard]] static std::int32_t splitter_grab() noexcept;
        [[nodiscard]] dock_axis axis_of(node_id split) const noexcept;
        [[nodiscard]] float ratio_of(node_id split) const noexcept;
        // Moves the bar of a split so it is centred on the point, then clamps. The caller lays out again.
        void drag_splitter(node_id split, std::int32_t x, std::int32_t y) noexcept;

        // Where a dragged panel would land if dropped at a screen point. Floating roots are tried before the main one.
        // skip_root is a root that is itself being dragged, which is no place to drop onto.
        [[nodiscard]] drop_target find_drop(std::int32_t screen_x, std::int32_t screen_y, panel_id dragged,
            root_id skip_root = k_no_root) const noexcept;
        // Docks everything in a floating root at the place find_drop pointed to, as one group of tabs, with the tab that
        // was on show still on show. The root goes away. index is the tab position when the zone is tabs.
        [[nodiscard]] bool apply_root_drop(root_id source, const drop_target& target, std::uint32_t index = k_tab_end) noexcept;

    private:
        // Where a closed panel goes when it opens again.
        enum class home_kind : std::uint8_t
        {
            none,
            // A tab in the group of ref, at an index.
            tabs,
            // Beside the group of ref, on the side given by zone, taking share of it.
            split,
            // A floating window in area.
            floating
        };

        struct panel
        {
            bool used{false};
            bool permanent{false};
            // A panel alone in its group needs no header: its view is the whole group.
            bool headerless{false};
            const char* title{nullptr};
            void* handle{nullptr};
            node_id group{k_no_node};
            home_kind home{home_kind::none};
            panel_id home_ref{k_no_panel};
            drop_zone home_zone{drop_zone::none};
            float home_share{0.3f};
            std::uint32_t home_index{k_tab_end};
            dock_rect home_area{};
        };

        struct node
        {
            bool used{false};
            bool group{true};
            root_id root{k_no_root};
            node_id parent{k_no_node};
            std::uint32_t serial{0};
            // Group
            panel_id tabs[k_max_panels]{};
            std::uint32_t tab_count{0};
            panel_id active{k_no_panel};
            // Split
            dock_axis axis{dock_axis::horizontal};
            float ratio{0.5f};
            node_id first{k_no_node};
            node_id second{k_no_node};
            // Computed by layout
            dock_rect rect{};
            dock_rect bar{};
        };

        struct root
        {
            bool used{false};
            bool floating{false};
            std::uint32_t serial{0};
            node_id top{k_no_node};
            dock_rect area{};
        };

        [[nodiscard]] node_id alloc_node(root_id owner) noexcept;
        [[nodiscard]] std::uint32_t free_nodes() const noexcept;
        void replace_child(node_id parent, node_id old_child, node_id new_child, root_id owner) noexcept;
        void detach(panel_id panel) noexcept;
        // Works out where an open panel sits and keeps it as its home.
        void record_home(panel_id panel) noexcept;
        void remove_group(node_id group) noexcept;
        void attach(panel_id panel, node_id group, std::uint32_t index) noexcept;
        void place(node_id node, const dock_rect& area, dock_place_fn fn, void* user) noexcept;

        struct loader;
        friend struct loader;

        panel m_panels[k_max_panels]{};
        node m_nodes[k_max_nodes]{};
        root m_roots[k_max_roots]{};
        std::uint32_t m_panel_count{0};
        bool m_locked{false};
        std::uint32_t m_serial{0};
        panel_id m_focus{k_no_panel};
    };
}
