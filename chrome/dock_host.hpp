#pragma once

#include <chrome/dock.hpp>
#include <chrome/panel.hpp>
#include <chrome/theme.hpp>
#include <chrome/controls/core/context_menu.hpp>

#include <cstddef>

namespace chrome
{
    // How a tab strip looks while a tab or a window is dragged over it. The tabs slide apart to leave a gap where the
    // drop would land, and the tab being dragged out of this strip folds away. Widths are in pixels and chase their
    // targets a little every tick.
    struct tab_strip_anim
    {
        bool live{false};
        // The dragged tab when it belongs to this strip. It is not drawn, and its slot shrinks to nothing.
        panel_id skip{k_no_panel};
        float skip_w{0};
        // The gap wanted before the visible tab with this number, counting without skip. -1 for none.
        std::int32_t slot{-1};
        float want{0};
        // The width of the gap before each visible tab, and one more for the end of the strip.
        float gap[dock_model::k_max_panels + 1]{};
        const char* title{nullptr};
    };

    // The Win32 side of the dock model. It keeps one window per live root and one per tab group, and moves
    // panel windows between them as the model changes.
    //
    //   main window
    //     root host (root 0)        draws the splitter bars
    //       group window            draws the tab strip, parents the panel content
    //         panel window
    //   floating window (root N)    a tool window owned by the main one
    //     root host
    //       group window ...
    //
    // Closed panels live under a hidden holder window. Call sync after any change to the model.
    //
    // Sizes follow the DPI of the main window. A floating window dragged to a monitor with another scale keeps
    // the main window's scale.
    // One row of a popup menu. Text and shortcut are UTF-8. The shortcut is shown right aligned.
    struct popup_row
    {
        const char* text{nullptr};
        const char* shortcut{nullptr};
        bool separator{false};
        bool enabled{true};
        bool checked{false};
    };

    // One title of the main menu bar and the menu under it. An ampersand before a letter of the title makes that letter
    // the mnemonic, so Alt plus it opens the menu. Both have to outlive the host.
    struct menu_bar_entry
    {
        const char* title{nullptr};
        context_menu* menu{nullptr};
    };

    // Where a strip sits in the main window: under the menu bar, or along the bottom edge.
    enum class strip_edge : std::uint8_t
    {
        top,
        bottom
    };

    class dock_host
    {
    public:
        // Bands of the main window outside the dock, such as a toolbar and a status bar. Their panel ids come after the
        // ones of the docked panels.
        static constexpr std::uint32_t k_max_strips = 3;
        static constexpr std::uint32_t k_max_bar_menus = 8;
        // Rows with an action that one open menu, submenus included, can hold.
        static constexpr std::uint32_t k_max_commands = 256;

        // Registers the window classes. Call once, before create.

        // Text added after the panel name in the title of a floating window, such as L" - My Editor". Empty by default.
        static void set_window_title_suffix(const wchar_t* suffix) noexcept;
        [[nodiscard]] static bool register_classes(void* instance) noexcept;

        // main_window is the HWND of the editor's main window. Root 0 fills its client area.
        [[nodiscard]] bool create(void* main_window, void* instance) noexcept;

        [[nodiscard]] dock_model& model() noexcept { return m_model; }

        // Makes a panel: its window, and its entry in the model, closed. content may be null for a panel that only
        // needs a window, and has to outlive the host. k_no_panel when the table is full. Titles must be unique,
        // because a saved layout names panels by title.
        [[nodiscard]] panel_id add_panel(const char* title, panel_content* content, bool permanent,
            panel_kind kind = panel_kind::standard) noexcept;
        // A band across the main window, outside the dock: under the menu bar for top, along the bottom edge for bottom.
        // Several on one edge stack in the order they were added. height is at 96 dpi. The content paints into it and gets
        // its mouse like a panel's, but it is never docked, listed, saved or given the keyboard. The id works with
        // invalidate_panel, set_tooltip, set_cursor and popup_menu. k_no_panel when there is no room. Add strips before
        // the first layout.
        [[nodiscard]] panel_id add_strip(panel_content* content, strip_edge edge, std::int32_t height) noexcept;
        // Windows that were not made by add_panel can be asked for the HWND of a panel or strip.
        [[nodiscard]] void* window_of(panel_id panel) const noexcept;
        // Repaints a standard panel.
        void invalidate_panel(panel_id panel) noexcept;
        // Starts calling the panel's on_frame, for animation. It stops when on_frame returns false.
        void request_frames(panel_id panel) noexcept;

        // Brings every window in line with the model. With animate, groups slide to their new rectangles and new
        // ones grow in. Pass false for the first layout, and for anything that is not a change of layout.
        void sync(bool animate = true) noexcept;
        // The main window changed size.
        void layout_main() noexcept;
        // Shows or hides the menu bar and the strips, so the dock alone fills the window. The window is laid out again.
        void set_chrome_shown(bool shown) noexcept;
        [[nodiscard]] bool chrome_shown() const noexcept { return m_chrome_shown; }
        // The main window moved to a monitor with another scale, or the user changed the scale. dpi is the new one
        // and suggested the new window rectangle the system proposes (a RECT), or null.
        void on_dpi_changed(std::uint32_t dpi, const void* suggested) noexcept;

        // The editor's own menu bar, drawn in the theme. It starts empty. Call after the panels exist.
        void install_menu_bar() noexcept;
        // The titles of the bar and their menus, which can have submenus. A menu is rebuilt through its on_open handler
        // each time it opens. While one is open, moving over another title or pressing Left or Right goes there. F10 opens
        // the first one. At most k_max_bar_menus.
        void set_menu_bar(const menu_bar_entry* entries, std::uint32_t count) noexcept;
        // A button at the right end of the bar, before the theme button, that opens this menu when clicked, hanging from
        // the button's right edge. The menu is rebuilt through its on_open handler each time. Null removes the button.
        void set_layouts_menu(context_menu* menu) noexcept;
        // A button with a word on it at the right end of the bar, before the layouts button, that calls the handler when it is clicked.
        // For a command that is not a menu, such as exporting. A null label removes it.
        using bar_action_fn = void (*)(void* user) noexcept;
        void set_bar_action(const char* label, bar_action_fn handler, void* user) noexcept;
        // Opens a menu of the bar, as a click on its title does.
        void open_bar_menu(std::uint32_t index) noexcept { open_menu(index); }
        // Height the menu bar takes at the top of the main window.
        [[nodiscard]] static std::int32_t menu_bar_height() noexcept;

        // Switches the colours of every window, the window frames and the menus.
        void set_theme_mode(theme_mode mode) noexcept;

        // Closes a panel when it is open and not permanent, and floats it when it is closed.
        void toggle_panel(panel_id panel) noexcept;

        // Tooltips. A panel calls this when the mouse moves, with the tooltip of the control under it and that control's
        // area in the panel, or null text when there is none. The tooltip appears after the mouse has rested there, and goes
        // when the mouse leaves, is pressed or the control changes. Calling again with the same control does nothing.
        void set_tooltip(panel_id panel, const char* text, const dock_rect& area) noexcept;

        // Shows the system's popup menu, in the theme's dark or light mode, at a point of a panel, and returns the index of
        // the row chosen, or -1 when it was dismissed. It does not return until the menu is closed.
        [[nodiscard]] std::int32_t popup_menu(panel_id panel, const popup_row* rows, std::uint32_t count, std::int32_t x, std::int32_t y) noexcept;
        // The same for a menu with submenus. The row chosen is passed to the menu that owns it, and its handler runs
        // before this returns.
        void popup_menu(panel_id panel, context_menu& menu, std::int32_t x, std::int32_t y) noexcept;

        // The pointer over a panel. A panel calls this as the mouse moves, with what the pointer should look like where it is
        // now. It takes effect at once, and stays until the panel says otherwise.
        void set_cursor(panel_id panel, mouse_cursor cursor) noexcept;

        // Drag and drop between the panels of this host. A control that is held down starts a drag with control::begin_drag, which
        // ends up here. The ghost that follows the pointer is a window of its own, so it can leave the panel. Over each panel the
        // pointer is turned into panel_content::on_drag_over calls, and the release into on_drop. Escape, a right click, or losing
        // the mouse capture cancels. A drag may only start while the mouse is held down on a panel. false when one is going.
        bool begin_drag(const drag_payload& payload) noexcept;
        // Ends the drag in progress with nothing dropped.
        void cancel_drag() noexcept;
        [[nodiscard]] bool dragging() const noexcept { return m_dnd.active; }
        // Files dragged in from outside the program, such as from Explorer. The window's drop target calls these. files is the list of
        // full paths, each ended by a zero and the list by one more, and keys are the MK_ flags of the mouse message. The first
        // call starts a drag of kind k_files_kind that may be copied or linked, and every call is answered with the effect the
        // panel under the screen point gives it. file_drag_drop is the release.
        drop_effect file_drag_over(const wchar_t* files, std::int32_t x, std::int32_t y, std::uint32_t keys) noexcept;
        void file_drag_leave() noexcept;
        drop_effect file_drag_drop(std::int32_t x, std::int32_t y, std::uint32_t keys) noexcept;

        // Keyboard focus. The focused panel is the one whose tab is lit, and the one that gets key presses.
        void focus_panel(panel_id panel) noexcept;
        // Gives the keyboard to a panel in a root: the focused one when it is in the root, else the first group's.
        // For a window that got the focus itself, which is the main window when it is activated.
        void focus_root(root_id root) noexcept;
        // Keys that no panel used go here, with the Win32 virtual key code. true when it was used.
        using key_handler_fn = bool (*)(void* user, std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept;
        void set_key_handler(key_handler_fn handler, void* user) noexcept;
        // Offers a key to the focused panel, then to the key handler. For windows that are not panels.
        [[nodiscard]] bool dispatch_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept;

        // The arrangement a fresh start gets, and what Reset Layout goes back to. It opens panels in the model it is
        // given, which has every panel closed.
        using default_layout_fn = void (*)(void* user, dock_model& model) noexcept;
        void set_default_layout(default_layout_fn build, void* user) noexcept;
        // A panel that is alone in its group shows no header, so its view is the whole group.
        void set_headerless(panel_id panel, bool headerless) noexcept { m_model.set_headerless(panel, headerless); }
        void reset_layout() noexcept;

        // The layout, the theme and the main window's place as text. Returns the length written, or 0 when capacity is
        // too small. Not null terminated.
        [[nodiscard]] std::size_t save_layout(char* out, std::size_t capacity) noexcept;
        // Applies saved text. false, with nothing changed, when it does not fit the panels. Call before the first sync.
        // With live, for an editor that is running, only the arrangement of the panels changes, the windows are brought in
        // line at once, and it can be called at any time.
        [[nodiscard]] bool restore_layout(const char* text, std::size_t size, bool live = false) noexcept;
        // The saved window was maximized, so show it that way.
        [[nodiscard]] bool start_maximized() const noexcept { return m_start_maximized; }

    private:
        friend struct dock_host_access;

        struct root_context
        {
            dock_host* host{nullptr};
            root_id root{k_no_root};
        };

        struct group_context
        {
            dock_host* host{nullptr};
            node_id node{k_no_node};
        };

        struct panel_slot
        {
            dock_host* host{nullptr};
            panel_id panel{k_no_panel};
        };

        void refresh_area(root_id root) noexcept;
        void refresh_all_areas() noexcept;
        void layout_root(root_id root) noexcept;
        void fit_group(node_id group) noexcept;
        void apply_theme() noexcept;
        void apply_dpi(std::uint32_t dpi) noexcept;
        void open_menu(std::uint32_t index) noexcept;
        // Appends the rows of a menu, and of its submenus, to a native popup, and records which row each command is.
        void fill_menu(void* popup, context_menu& menu) noexcept;
        void open_layouts_menu() noexcept;
        // F10 and the mnemonics of the bar. true when the key opened a menu.
        bool bar_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept;
        // Runs the row a command stands for. false when it is not one.
        bool run_command(std::uint32_t command) noexcept;
        void open_tab_list(node_id group) noexcept;
        void invalidate_strips() noexcept;
        // forget also drops the target, so the same control can ask again. Without it the tooltip stays dismissed until the
        // mouse moves to another control.
        void hide_tooltip(bool forget) noexcept;
        void show_tooltip() noexcept;
        // Tab drag feedback. begin hides the dragged tab of its strip, set_tab_drop opens a gap in one strip (group
        // k_no_node closes them all), end puts everything back.
        void begin_tab_drag(panel_id panel, node_id group) noexcept;
        void set_tab_drop(node_id group, std::int32_t slot, std::int32_t width, const char* title) noexcept;
        void end_tab_drag() noexcept;
        [[nodiscard]] bool step_strips() noexcept;
        [[nodiscard]] std::int32_t tab_width_of(panel_id panel) noexcept;
        // The tabs of everything in a floating window side by side, and the title of the first.
        [[nodiscard]] std::int32_t root_tabs_width(root_id root, const char** first_title) noexcept;
        // The buttons at the end of a floating window's top tab strip: 0 minimize, 1 maximize or restore, 2 close.
        void window_control(root_id root, std::int32_t which) noexcept;
        void move_root_window(root_id root, const dock_rect& screen) noexcept;
        // Preview modes: 0 a docked panel, 1 the window a panel would float as, 2 a tab joining a group.
        void show_preview(const dock_rect& screen, const char* title, std::uint32_t mode) noexcept;
        // A drag in progress. over is the panel the pointer is on, effect what a drop there would do, shown what the ghost shows.
        struct drag_session
        {
            bool active{false};
            drag_payload payload{};
            panel_id over{k_no_panel};
            drop_effect effect{drop_effect::none};
            drop_effect shown{drop_effect::none};
            std::int32_t x{0};
            std::int32_t y{0};
            std::uint64_t stamp{0};
            std::int32_t ghost_w{0};
            std::int32_t ghost_h{0};
            void* capture{nullptr};
            // Files from outside: no ghost and no capture, and the keys come with each call.
            // The tab of a panel that is not showing which the pointer rests on, and since when. A moment later it comes to the front.
            panel_id spring_panel{k_no_panel};
            std::uint64_t spring_since{0};
            bool external{false};
            bool ctrl{false};
            bool shift{false};
            bool alt{false};
        };

        // The pointer is at a screen point, with the keys as they are now: moves the ghost and asks the panel under it.
        void drag_update(std::int32_t x, std::int32_t y) noexcept;
        // Ends the drag. With drop, the panel under the pointer is asked to take it.
        drop_effect drag_finish(bool drop) noexcept;
        // The panel under a screen point, with the point in its coordinates, or k_no_panel.
        [[nodiscard]] panel_id panel_at(std::int32_t x, std::int32_t y, std::int32_t& local_x, std::int32_t& local_y) const noexcept;
        // Brings forward the panel whose tab a drag rests on, so the drop can reach what is behind it.
        void spring_tabs(std::int32_t x, std::int32_t y) noexcept;
        void tick() noexcept;
        void start_timer() noexcept;
        void hide_preview() noexcept;
        void show_hint(const drop_target& target) noexcept;
        void close_root(root_id root) noexcept;
        void notify_visibility() noexcept;
        [[nodiscard]] bool create_root_windows(root_id root) noexcept;
        [[nodiscard]] bool create_group_window(node_id group) noexcept;
        [[nodiscard]] std::uint32_t tab_index_at(node_id group, std::int32_t screen_x) noexcept;

        dock_model m_model;
        void* m_instance{nullptr};
        void* m_main{nullptr};
        void* m_holder{nullptr};
        void* m_preview{nullptr};
        void* m_hint{nullptr};
        void* m_root_hint{nullptr};
        drop_target m_root_hint_target{};
        drop_target m_hint_target{};
        void* m_menubar{nullptr};
        bool m_chrome_shown{true};
        void* m_tip_window{nullptr};
        panel_id m_tip_panel{k_no_panel};
        const char* m_tip_text{nullptr};
        dock_rect m_tip_local{};
        std::int32_t m_menu_hot{-1};
        // The mouse is over the theme button at the right end of the bar.
        bool m_theme_hot{false};
        // The layouts button: the menu it opens, whether the mouse is over it and whether the menu is open.
        context_menu* m_layouts_menu{nullptr};
        bool m_layouts_hot{false};
        bool m_layouts_open{false};
        // The action button of the bar: its word and handler, and whether the mouse is over it.
        char m_action_label[32]{};
        bar_action_fn m_action{nullptr};
        void* m_action_user{nullptr};
        bool m_action_hot{false};
        // The menu of the bar that is open, or -1.
        std::int32_t m_menu_open{-1};
        menu_bar_entry m_bar[k_max_bar_menus]{};
        std::uint32_t m_bar_count{0};

        struct menu_command
        {
            context_menu* menu{nullptr};
            std::uint32_t index{0};
        };

        menu_command m_command[k_max_commands]{};
        std::uint32_t m_command_count{0};
        std::uint32_t m_dpi{96};

        void* m_root_top[dock_model::k_max_roots]{};
        void* m_root_host[dock_model::k_max_roots]{};
        std::uint32_t m_root_serial[dock_model::k_max_roots]{};
        root_context m_root_context[dock_model::k_max_roots]{};

        void* m_group[dock_model::k_max_nodes]{};
        std::uint32_t m_group_serial[dock_model::k_max_nodes]{};
        group_context m_group_context[dock_model::k_max_nodes]{};
        // Tab strips that hold more tabs than fit scroll. The offset is in pixels, and follow is the active tab the
        // strip last brought into view, so a new active tab scrolls into view once and the wheel is not fought.
        std::int32_t m_tab_scroll[dock_model::k_max_nodes]{};
        panel_id m_tab_follow[dock_model::k_max_nodes]{};

        // The window button under the mouse, and the group it is in.
        node_id m_ctl_group{k_no_node};
        std::int32_t m_ctl_hot{-1};
        // The tab the mouse is over, and whether it is over its cross.
        node_id m_tab_hot_group{k_no_node};
        panel_id m_tab_hot{k_no_panel};
        bool m_tab_hot_close{false};

        tab_strip_anim m_strip[dock_model::k_max_nodes]{};

        // Docked panels, then the strips.
        static constexpr std::uint32_t k_slots = dock_model::k_max_panels + k_max_strips;

        [[nodiscard]] static bool is_strip(panel_id panel) noexcept { return panel >= dock_model::k_max_panels && panel < k_slots; }

        panel_slot m_slot[k_slots]{};
        panel_content* m_content[k_slots]{};
        panel_kind m_kind[k_slots]{};
        bool m_shown[dock_model::k_max_panels]{};
        void* m_strip_window[k_max_strips]{};
        strip_edge m_strip_edge[k_max_strips]{};
        std::int32_t m_strip_height[k_max_strips]{};
        std::uint32_t m_strip_count{0};

        key_handler_fn m_key_handler{nullptr};
        void* m_key_user{nullptr};
        default_layout_fn m_default_layout{nullptr};
        void* m_default_user{nullptr};
        bool m_start_maximized{false};

        // A group sliding to a new rectangle. Rectangles are in the client coordinates of the group's root.
        struct tween
        {
            bool active{false};
            dock_rect from{};
            dock_rect to{};
            std::uint64_t start{0};
        };

        tween m_tween[dock_model::k_max_nodes]{};
        dock_rect m_group_cur[dock_model::k_max_nodes]{};
        bool m_group_valid[dock_model::k_max_nodes]{};
        bool m_animate{false};
        bool m_timer_on{false};
        bool m_frames[dock_model::k_max_panels]{};

        // A viewport is a swapchain, and every size it gets costs a new one. While the layout is moving, its window
        // keeps its old size, and it takes the new one once nothing has changed for a moment.
        std::uint64_t m_resize_stamp{0};
        bool m_resize_pending{false};

        // The preview window chases its target while it shows a docked panel, so it morphs from zone to zone.
        dock_rect m_preview_cur{};
        dock_rect m_preview_target{};
        bool m_preview_smooth{false};
        const char* m_preview_title{nullptr};
        dock_rect m_preview_drawn{};
        // A drag overlay appeared, so the three have to be stacked again: preview, then edge tiles, then compass.
        bool m_restack{false};

        // A floating window being dragged by its title bar, and where the cursor and the window were when it started.
        root_id m_wdrag_root{k_no_root};
        std::int32_t m_wdrag_cx{0};
        std::int32_t m_wdrag_cy{0};
        std::int32_t m_wdrag_wx{0};
        std::int32_t m_wdrag_wy{0};

        mouse_cursor m_cursor[k_slots]{};
        drag_session m_dnd{};
        static constexpr std::size_t k_file_capacity = 8192;
        wchar_t m_files[k_file_capacity]{};
        void* m_ghost{nullptr};

        // A splitter being dragged.
        node_id m_split_drag{k_no_node};
        // A tab being pressed or dragged.
        panel_id m_press_panel{k_no_panel};
        node_id m_press_group{k_no_node};
        std::int32_t m_press_x{0};
        std::int32_t m_press_y{0};
        bool m_press_close{false};
        bool m_dragging{false};
    };
}
