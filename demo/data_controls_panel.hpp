#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/scroll_view.hpp>
#include <chrome/controls/data/list_view.hpp>
#include <chrome/controls/property/color_swatch.hpp>
#include <chrome/controls/core/breadcrumb_bar.hpp>
#include <chrome/controls/core/button.hpp>
#include <chrome/controls/core/checkbox.hpp>
#include <chrome/controls/core/drop_box.hpp>
#include <chrome/controls/core/label.hpp>
#include <chrome/controls/core/popover.hpp>
#include <chrome/controls/core/slider.hpp>
#include <chrome/controls/core/search_box.hpp>
#include <chrome/controls/core/splitter.hpp>
#include <chrome/controls/core/tab_bar.hpp>
#include <chrome/controls/core/text_area.hpp>
#include <chrome/controls/core/text_field.hpp>
#include <chrome/controls/property/angle_dial.hpp>
#include <chrome/controls/property/asset_field.hpp>
#include <chrome/controls/property/option_field.hpp>
#include <chrome/controls/property/range_slider.hpp>
#include <chrome/controls/property/quat_field.hpp>
#include <chrome/controls/property/vec_field.hpp>
#include <chrome/controls/data/property_grid.hpp>
#include <chrome/controls/data/proportion_bar.hpp>
#include <chrome/controls/data/table_view.hpp>
#include <chrome/controls/data/curve_editor.hpp>
#include <chrome/controls/core/number_field.hpp>
#include <chrome/controls/data/gradient_editor.hpp>
#include <chrome/controls/data/graph.hpp>
#include <chrome/controls/data/graph_view.hpp>
#include <chrome/controls/data/thumbnail_grid.hpp>
#include <chrome/controls/data/timeline_view.hpp>
#include <chrome/controls/data/tree_view.hpp>
#include <chrome/dock.hpp>
#include <chrome/layout.hpp>
#include <chrome/panel.hpp>

#include <cstdint>

namespace chrome
{
    class dock_host;

    // The memory the property grids of the demo show. The field tables that describe it are in the source file.
    struct demo_pose
    {
        float translation[3]{};
        quaternion rotation{};
        float scale[3]{1.0f, 1.0f, 1.0f};
    };

    struct demo_light
    {
        float color[3]{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        bool enabled{true};
    };

    struct demo_camera
    {
        float fov_y{1.0472f};
        float near_plane{0.1f};
    };

    // A drafting board for the editor's data controls, the ones that show many rows of something. It is the Controls panel
    // for lists, trees and tables: each is put here in its states first, and shows what was chosen in a line at the bottom.
    class data_controls_panel final : public panel_content
    {
    public:
        data_controls_panel() noexcept;

        // The panel needs its host to ask for a repaint when something changes.
        void bind(dock_host& host, panel_id id) noexcept;

        void on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept override;
        void on_mouse(const panel_mouse& mouse) noexcept override;
        [[nodiscard]] bool on_frame(std::uint64_t now_ms) noexcept override;
        [[nodiscard]] bool frame_dirty() const noexcept override { return m_frame_dirty; }
        [[nodiscard]] bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;
        [[nodiscard]] drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        [[nodiscard]] drop_effect on_drop(const drag_event& event) noexcept override;

    private:
        static constexpr std::uint32_t k_max_sections = 21;
        static constexpr std::uint32_t k_max_lists = 8;
        // The rows of the long list are named as they are asked for, in a ring of buffers, so none are kept for the rest.
        static constexpr std::uint32_t k_name_ring = 128;
        static constexpr std::uint32_t k_name_length = 32;
        static constexpr std::uint32_t k_long_rows = 4096;
        static constexpr std::uint32_t k_max_trees = 4;
        static constexpr std::uint32_t k_max_tables = 4;
        // The cells of the tables are made as they are asked for, in a ring of buffers, since a sort holds two at a time.
        static constexpr std::uint32_t k_cell_ring = 256;
        static constexpr std::uint32_t k_cell_length = 48;
        // The names of the rows of the large tree, which the tree does not copy.
        static constexpr std::uint32_t k_tree_names = 280;

        [[nodiscard]] list_view& add_list(std::int32_t section) noexcept;
        [[nodiscard]] tree_view& add_tree(std::int32_t section) noexcept;
        void fill_trees() noexcept;
        [[nodiscard]] table_view& add_table(std::int32_t section) noexcept;
        void fill_tables() noexcept;
        void fill_grids() noexcept;
        void fill_vectors() noexcept;
        void fill_colours() noexcept;
        void fill_options() noexcept;
        void fill_ranges() noexcept;
        void fill_overlays() noexcept;
        void fill_text() noexcept;
        void fill_tabs() noexcept;
        void fill_crumbs() noexcept;
        void fill_dnd() noexcept;
        void fill_thumbs() noexcept;
        void fill_animation() noexcept;
        void fill_gradient() noexcept;
        void fill_asset_fields() noexcept;
        // The node graph demo: loads a material network or a state machine, and builds the nodes of its palette.
        void fill_graph() noexcept;
        // The proportion bars demo: three bars with the parts of a frame, which the buttons shuffle, push over the budget and drill into.
        void fill_proportion() noexcept;
        void load_proportion() noexcept;
        static void proportion_picked(void* user, proportion_bar& source, std::uint32_t id) noexcept;
        static void proportion_activated(void* user, proportion_bar& source, std::uint32_t id) noexcept;
        static void proportion_clicked(void* user, button& source) noexcept;
        void show_menu(context_menu& menu, std::int32_t x, std::int32_t y) noexcept;
        void load_graph_demo(std::uint32_t demo) noexcept;
        void sync_graph_title() noexcept;
        static void graph_palette(std::uint32_t demo, std::uint32_t kind, graph_node_desc& out) noexcept;
        static void graph_edited(void* user, graph_view& source, bool final) noexcept;
        static void graph_selected(void* user, graph_view& source) noexcept;
        static void graph_create(void* user, graph_view& source, float x, float y, std::uint32_t from_node, std::uint32_t from_pin) noexcept;
        static void graph_clicked(void* user, button& source) noexcept;
        static bool graph_title_action(void* user, text_field& source, text_action action) noexcept;
        static void asset_changed(void* user, asset_field& source) noexcept;
        static void asset_browse(void* user, asset_field& source) noexcept;
        // Shows the selected stop of the gradient in the fields beside it, and turns off the ones that do not apply.
        void sync_gradient_fields() noexcept;
        static void gradient_edited(void* user, gradient_editor& source, bool final) noexcept;
        static void gradient_selected(void* user, gradient_editor& source) noexcept;
        static void gradient_color_changed(void* user, color_swatch& source) noexcept;
        static void gradient_number_changed(void* user, number_field& source) noexcept;
        static void gradient_clicked(void* user, button& source) noexcept;
        static void anim_clicked(void* user, button& source) noexcept;
        static void curve_edited(void* user, curve_editor& source, bool final) noexcept;
        static void curve_selected(void* user, curve_editor& source) noexcept;
        static void curve_time(void* user, curve_editor& source, float time) noexcept;
        static void timeline_edited(void* user, timeline_view& source, bool final) noexcept;
        static void timeline_selected(void* user, timeline_view& source) noexcept;
        static void timeline_time(void* user, timeline_view& source, float time) noexcept;
        static const char* thumb_many(void* user, std::uint32_t index) noexcept;
        static const char* thumb_label(void* user, std::uint32_t index) noexcept;
        static void thumb_picture(void* user, void* dc, std::uint32_t index, const dock_rect& box) noexcept;
        static bool thumb_drag(void* user, thumbnail_grid& source, drag_payload& payload) noexcept;
        static void thumb_changed(void* user, thumbnail_grid& source) noexcept;
        static void thumb_activated(void* user, thumbnail_grid& source, std::uint32_t index) noexcept;
        // What a drag of rows carries: which list they came from and which rows.
        struct rows_ref
        {
            std::uint8_t source;
            std::uint8_t count;
            std::uint16_t rows[24];
        };

        static const char* playlist_row(void* user, std::uint32_t index) noexcept;
        // Trees and tables give their selected rows to drag, which only the box that takes anything accepts.
        static bool tree_drag(void* user, tree_view& source, drag_payload& payload) noexcept;
        static bool table_drag(void* user, table_view& source, drag_payload& payload) noexcept;
        static bool assets_drag(void* user, list_view& source, drag_payload& payload) noexcept;
        static bool playlist_drag(void* user, list_view& source, drag_payload& payload) noexcept;
        static drop_effect playlist_drop(void* user, list_view& target, const drag_event& event, std::int32_t slot, bool commit) noexcept;
        static void zone_dropped(void* user, drop_box& source, const drag_event& event, drop_effect effect) noexcept;
        // The names of the rows a drag carries, copied. Returns how many.
        [[nodiscard]] std::uint32_t rows_named(const rows_ref& ref, char (*names)[32]) const noexcept;
        static void crumb_event_raised(void* user, breadcrumb_bar& source, crumb_event what, std::int32_t segment, std::int32_t child) noexcept;
        static std::uint32_t crumb_children(void* user, breadcrumb_bar& source, std::int32_t segment, const char** names, std::uint32_t capacity) noexcept;
        static void tab_event_raised(void* user, tab_bar& source, tab_event what, std::int32_t index) noexcept;
        // Shows the assets that pass the search box, and counts how many each chip would let through.
        void apply_search() noexcept;
        static bool area_acted(void* user, text_area& source, text_action action) noexcept;
        static void search_changed(void* user, search_box& source) noexcept;
        static void search_submitted(void* user, search_box& source) noexcept;
        static const char* asset_row(void* user, std::uint32_t index) noexcept;
        [[nodiscard]] char* next_cell() noexcept;
        std::int32_t layout(void* dc, std::int32_t width) noexcept;
        static void paint_content(void* user, void* dc, const view_range& band) noexcept;
        static const char* long_row(void* user, std::uint32_t index) noexcept;
        static void list_changed(void* user, list_view& source) noexcept;
        static void list_activated(void* user, list_view& source, std::uint32_t index) noexcept;
        static const char* files_cell(void* user, std::uint32_t row, std::uint32_t column) noexcept;
        static const char* scores_cell(void* user, std::uint32_t row, std::uint32_t column) noexcept;
        static void table_changed(void* user, table_view& source) noexcept;
        static void table_activated(void* user, table_view& source, std::uint32_t row) noexcept;
        static void table_sorted(void* user, table_view& source, std::uint32_t column, sort_direction direction) noexcept;
        static void property_edited(void* user, property_grid& source, std::uint32_t group, std::uint32_t field, const void* value, bool final) noexcept;
        static void swatch_changed(void* user, color_swatch& source) noexcept;
        static void option_changed(void* user, option_field& source) noexcept;
        static void range_changed(void* user, range_slider& source) noexcept;
        static void pop_clicked(void* user, button& source) noexcept;
        static void pop_closed(void* user, popover& source) noexcept;
        static void pane_changed(void* user, splitter& source) noexcept;
        static void shortcut_filters(void* user) noexcept;
        static void shortcut_reset(void* user) noexcept;
        static void dial_changed(void* user, angle_dial& source) noexcept;
        static void quat_changed(void* user, quat_field& source) noexcept;
        static void vec_changed(void* user, vec_field& source, std::uint32_t index) noexcept;
        static void tree_changed(void* user, tree_view& source) noexcept;
        static void tree_activated(void* user, tree_view& source, tree_view::node_id node) noexcept;
        static void tree_toggled(void* user, tree_view& source, tree_view::node_id node) noexcept;

        dock_host* m_host{nullptr};
        panel_id m_id{k_no_panel};
        list_view m_lists[k_max_lists]{};
        std::uint32_t m_list_count{0};
        std::int32_t m_section_of[k_max_lists]{};
        // The memory the property grids show.
        demo_pose m_pose{};
        demo_light m_light{};
        demo_camera m_camera{};
        property_grid m_grids[2]{};
        vec2_field m_vec2[2]{};
        vec3_field m_vec3[4]{};
        vec4_field m_vec4[3]{};
        quat_field m_quats[3]{};
        // One picker for all the swatches, since only one is open at a time.
        color_picker m_picker;
        color_swatch m_swatches[5]{};
        enum_field m_enums[3]{};
        flags_field m_flag_fields[3]{};
        range_slider m_ranges[3]{};
        // Popovers, with the buttons they open from and what is in them.
        button m_pop_buttons[3]{};
        button m_pop_close[2]{};
        popover m_pops[2]{};
        checkbox m_pop_checks[2]{};
        float_slider m_pop_slider{};
        label m_pop_note{};
        // Two splitters, the second inside the first, and the controls in their four panes.
        splitter m_split_outer{};
        splitter m_split_inner{};
        layout_container m_pane_a1{layout_kind::column};
        layout_container m_pane_a2{layout_kind::column};
        layout_container m_pane_b1{layout_kind::column};
        layout_container m_pane_b2{layout_kind::column};
        button m_pane_buttons[3]{};
        checkbox m_pane_checks[2]{};
        float_slider m_pane_slider{};
        dock_rect m_bounds{};
        text_area m_areas[4]{};
        tab_bar m_tab_bars[3]{};
        breadcrumb_bar m_crumbs[3]{};
        // The drag and drop demo: a library to take assets from, a playlist that takes them and reorders, and three places to drop.
        list_view m_dnd_assets{};
        list_view m_dnd_playlist{};
        drop_box m_dnd_zones[3]{};
        char m_playlist[24][32]{};
        std::uint32_t m_playlist_count{0};
        char m_favourite[32]{};
        std::uint32_t m_tab_serial{1};
        thumbnail_grid m_thumbs[3]{};
        // The animation demo: three curves, a curve editor and a timeline showing them, and one selection the two share.
        curve m_anim_curves[3]{};
        key_selection m_anim_selection{};
        curve_editor m_curve_editor{};
        timeline_view m_timeline{};
        button m_anim_buttons[5]{};
        // The gradient demo: a ramp, its editor, and fields for the selected stop. The swatch uses the panel's picker.
        gradient m_gradient{};
        gradient_editor m_gradient_editor{};
        color_swatch m_stop_color{};
        number_field m_stop_position{};
        number_field m_stop_alpha{};
        button m_gradient_buttons[2]{};
        asset_field m_asset_fields[5]{};
        // The node graph demo: the graph, its view, buttons that add nodes of the palette and a field for the title of the selected node.
        graph m_graph{};
        graph_view m_graph_view{};
        button m_graph_buttons[7]{};
        text_field m_graph_title{};
        proportion_bar m_proportion[3]{};
        button m_proportion_buttons[3]{};
        std::uint32_t m_proportion_seed{1};
        bool m_proportion_over{false};
        bool m_proportion_drilled{false};
        std::uint32_t m_graph_demo{0};
        std::uint32_t m_graph_kind{0};
        std::uint32_t m_browse_count{0};
        search_box m_search{};
        // The assets that pass the search, as numbers in the list of all of them, and the list that shows them.
        std::uint32_t m_found[64]{};
        std::uint32_t m_found_count{0};
        std::int32_t m_results{-1};
        angle_dial m_dials[5]{};
        table_view m_tables[k_max_tables]{};
        std::uint32_t m_table_count{0};
        std::int32_t m_table_section[k_max_tables]{};
        char m_cells[k_cell_ring][k_cell_length]{};
        std::uint32_t m_cell_next{0};
        tree_view m_trees[k_max_trees]{};
        std::uint32_t m_tree_count{0};
        std::int32_t m_tree_section[k_max_trees]{};
        char m_tree_text[k_tree_names][24]{};
        std::uint32_t m_tree_text_count{0};
        layout_container m_root;
        layout_container m_sections[k_max_sections];
        const char* m_heading[k_max_sections]{};
        std::int32_t m_heading_y[k_max_sections]{};
        control_set m_set;
        scroll_view m_view;
        std::int32_t m_width{0};
        char m_names[k_name_ring][k_name_length]{};
        std::uint32_t m_name_next{0};
        char m_status[128]{};
        bool m_has_status{false};
        bool m_frame_dirty{true};
    };
}
