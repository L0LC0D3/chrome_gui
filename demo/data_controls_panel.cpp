#include <demo/data_controls_panel.hpp>

#include <chrome/controls/core/draw.hpp>
#include <cstddef>
#include <chrome/dock_host.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace chrome
{
    namespace
    {
        // The spacing of the panel, at 96 dpi.
        constexpr std::int32_t k_margin = 16;
        constexpr std::int32_t k_gap = 16;
        constexpr std::int32_t k_section_gap = 22;
        constexpr std::int32_t k_heading_height = 20;
        constexpr std::int32_t k_heading_to_controls = 10;
        constexpr std::int32_t k_footer_height = 28;

        const char* const k_scenes[] = {
            "Main", "Forest", "Desert", "Harbour", "Cave", "Castle", "Arena", "Tutorial", "Credits", "Sandbox", "Test lab", "Menu",
        };

        const char* const k_few[] = {"Position", "Rotation", "Scale"};

        // What the search box looks through. The kind is the number of the chip that lets it through.
        struct asset_entry
        {
            const char* name;
            std::uint32_t kind;
        };

        const asset_entry k_assets[] = {
            {"rock_large.fbx", 0}, {"rock_small.fbx", 0}, {"tree_pine.fbx", 0}, {"tree_oak.fbx", 0}, {"crate.fbx", 0}, {"barrel.fbx", 0},
            {"house_wall.fbx", 0}, {"bridge.fbx", 0}, {"lantern.fbx", 0},
            {"rock_albedo.png", 1}, {"rock_normal.png", 1}, {"grass_albedo.png", 1}, {"bark_albedo.png", 1}, {"sand_albedo.dds", 1},
            {"wall_albedo.png", 1}, {"sky_cubemap.dds", 1}, {"noise_blue.png", 1}, {"lantern_emissive.png", 1},
            {"rock.mat", 2}, {"grass.mat", 2}, {"bark.mat", 2}, {"sand.mat", 2}, {"metal_rust.mat", 2}, {"glass.mat", 2}, {"water.mat", 2},
            {"lantern.mat", 2},
            {"wind_loop.wav", 3}, {"footstep_grass.wav", 3}, {"footstep_rock.wav", 3}, {"door_creak.wav", 3}, {"water_stream.wav", 3},
            {"music_forest.ogg", 3}, {"rock_crumble.wav", 3},
        };

        const char* const k_chip_names[] = {"Meshes", "Textures", "Materials", "Audio"};
        const std::uint32_t k_chip_colours[] = {0x4F8FDB, 0xC9803A, 0x6BAA5A, 0xA66BC4};

        const char* const k_code_sample =
            "struct transform\n"
            "{\n"
            "    vec3 position;\n"
            "    quat rotation;\n"
            "    vec3 scale;\n"
            "};\n"
            "\n"
            "float length_squared(vec3 v)\n"
            "{\n"
            "    // A line that is much longer than the box, so it scrolls sideways when wrapping is off.\n"
            "    return v.x * v.x + v.y * v.y + v.z * v.z;\n"
            "}\n";

        const char* const k_prose_sample =
            "A text area breaks long lines at its width, so a paragraph like this one stays readable without scrolling sideways. "
            "It keeps the caret in view, and the wheel and the thin bar on the right move through the lines.\n"
            "\n"
            "This one is read only: it can be selected and copied, not changed.";
    }

    data_controls_panel::data_controls_panel() noexcept
    {
        m_heading[0] = "List view";
        m_heading[1] = "List view, many rows";
        m_heading[2] = "List view, empty and disabled";
        m_heading[3] = "Tree view";
        m_heading[4] = "Table view";
        m_heading[5] = "Property grid";
        m_heading[6] = "Vector fields";
        m_heading[7] = "Colour swatch and picker";
        m_heading[8] = "Enum and flags fields";
        m_heading[9] = "Range slider and angle dial";
        m_heading[10] = "Popover, splitter and focus";
        m_heading[11] = "Text area and search box";
        m_heading[12] = "Tab bar";
        m_heading[13] = "Breadcrumb bar";
        m_heading[14] = "Drag and drop";
        m_heading[15] = "Thumbnail grid";
        m_heading[16] = "Curve editor and timeline";
        m_heading[17] = "Gradient editor";
        m_heading[18] = "Asset reference and path field";
        m_heading[19] = "Node graph";
        m_heading[20] = "Proportion bars";

        add_list(0).set_name("Scenes").set_items(k_scenes, static_cast<std::uint32_t>(std::size(k_scenes))).set_rows(6).set_tooltip("A single list: click selects, double click or Enter activates");
        add_list(0).set_name("Scenes (multi)").set_items(k_scenes, static_cast<std::uint32_t>(std::size(k_scenes))).set_rows(6)
            .set_select_mode(list_select_mode::multi).set_striped(true).set_tooltip("Ctrl adds, Shift takes a range, drag sweeps, Ctrl+A selects all");
        add_list(0).set_name("Few rows").set_items(k_few, static_cast<std::uint32_t>(std::size(k_few))).set_rows(5).set_width(160);

        add_list(1).set_name("4096 rows").set_source(&data_controls_panel::long_row, this, k_long_rows).set_rows(10).set_width(300)
            .set_select_mode(list_select_mode::multi).set_striped(true).set_tooltip("Only the rows on show are asked for and drawn");

        add_list(2).set_name("Empty").set_rows(4).set_width(200);
        add_list(2).set_name("Empty, no text").set_rows(4).set_width(200).set_empty_text(nullptr);
        add_list(2).set_name("Disabled").set_items(k_scenes, static_cast<std::uint32_t>(std::size(k_scenes))).set_rows(4).set_width(200).set_enabled(false);

        // The disabled list shows a choice, which it keeps without taking clicks.
        m_lists[m_list_count - 1].select(2);

        fill_trees();
        fill_tables();
        fill_grids();
        fill_vectors();
        fill_colours();
        fill_options();
        fill_ranges();
        fill_overlays();
        fill_text();
        fill_tabs();
        fill_crumbs();
        fill_dnd();
        fill_thumbs();
        fill_animation();
        fill_gradient();
        fill_asset_fields();
        fill_graph();
        fill_proportion();

        for (std::uint32_t s = 0; s < k_max_sections; ++s)
            m_root.add(m_sections[s]);

        // The text areas and the search box come before the list of what it found.
        for (text_area& item : m_areas)
            m_sections[11].add(item);

        m_sections[11].add(m_search);

        for (tab_bar& item : m_tab_bars)
            m_sections[12].add(item);

        for (breadcrumb_bar& item : m_crumbs)
            m_sections[13].add(item);

        m_sections[14].add(m_dnd_assets);
        m_sections[14].add(m_dnd_playlist);

        for (drop_box& item : m_dnd_zones)
            m_sections[14].add(item);

        for (thumbnail_grid& item : m_thumbs)
            m_sections[15].add(item);

        for (button& item : m_anim_buttons)
            m_sections[16].add(item);

        m_sections[16].add(m_curve_editor);
        m_sections[16].add(m_timeline);

        m_sections[17].add(m_gradient_editor);
        m_sections[17].add(m_stop_color);
        m_sections[17].add(m_stop_position);
        m_sections[17].add(m_stop_alpha);

        for (button& item : m_gradient_buttons)
            m_sections[17].add(item);

        for (asset_field& item : m_asset_fields)
            m_sections[18].add(item);

        for (button& item : m_graph_buttons)
            m_sections[19].add(item);

        m_sections[19].add(m_graph_title);
        m_sections[19].add(m_graph_view);

        for (button& item : m_proportion_buttons)
            m_sections[20].add(item);

        for (proportion_bar& item : m_proportion)
            m_sections[20].add(item);

        for (std::uint32_t i = 0; i < m_list_count; ++i)
            m_sections[m_section_of[i]].add(m_lists[i]);

        for (std::uint32_t i = 0; i < m_tree_count; ++i)
            m_sections[m_tree_section[i]].add(m_trees[i]);

        for (std::uint32_t i = 0; i < m_table_count; ++i)
            m_sections[m_table_section[i]].add(m_tables[i]);

        for (property_grid& grid : m_grids)
            m_sections[5].add(grid);

        for (vec2_field& item : m_vec2)
            m_sections[6].add(item);

        for (vec3_field& item : m_vec3)
            m_sections[6].add(item);

        for (vec4_field& item : m_vec4)
            m_sections[6].add(item);

        for (quat_field& item : m_quats)
            m_sections[6].add(item);

        for (color_swatch& item : m_swatches)
            m_sections[7].add(item);

        for (enum_field& item : m_enums)
            m_sections[8].add(item);

        for (flags_field& item : m_flag_fields)
            m_sections[8].add(item);

        for (range_slider& item : m_ranges)
            m_sections[9].add(item);

        for (angle_dial& item : m_dials)
            m_sections[9].add(item);

        for (button& item : m_pop_buttons)
            m_sections[10].add(item);

        m_sections[10].add(m_split_outer);

        for (std::uint32_t s = 0; s < k_max_sections; ++s)
            m_sections[s].set_kind(layout_kind::wrap);

        m_root.set_kind(layout_kind::column);
    }

    list_view& data_controls_panel::add_list(std::int32_t section) noexcept
    {
        list_view& item = m_lists[m_list_count < k_max_lists ? m_list_count : k_max_lists - 1];

        if (m_list_count < k_max_lists)
        {
            item.set_on_change(&data_controls_panel::list_changed, this);
            item.set_on_activate(&data_controls_panel::list_activated, this);
            m_section_of[m_list_count++] = section;
            const bool added = m_set.add(item);
            (void)added;
        }

        return item;
    }

    void data_controls_panel::fill_colours() noexcept
    {
        for (color_swatch& item : m_swatches)
        {
            item.set_picker(&m_picker).set_on_change(&data_controls_panel::swatch_changed, this).set_width(160).set_label_width(90);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_swatches[0].set_label("Albedo").set_tooltip("Click to open the picker: the square, the hue bar, sliders, a hex field and presets");
        m_swatches[0].set_color({0.85f, 0.42f, 0.12f, 1.0f});
        m_swatches[1].set_label("Tint").set_alpha(false);
        m_swatches[1].set_color({0.2f, 0.55f, 0.9f, 1.0f});
        m_swatches[2].set_label("Glass");
        m_swatches[2].set_color({0.3f, 0.8f, 0.7f, 0.4f});
        m_swatches[3].set_label("Shadow");
        m_swatches[3].set_color({0.0f, 0.0f, 0.0f, 0.65f});
        m_swatches[4].set_label("Disabled");
        m_swatches[4].set_color({0.7f, 0.2f, 0.5f, 1.0f});
        m_swatches[4].set_enabled(false);
    }

    void data_controls_panel::fill_options() noexcept
    {
        const auto hook = [this](option_field& item) {
            item.set_on_change(&data_controls_panel::option_changed, this).set_width(200).set_label_width(90);
            const bool added = m_set.add(item);
            (void)added;
        };

        for (enum_field& item : m_enums)
            hook(item);

        for (flags_field& item : m_flag_fields)
            hook(item);

        enum_field& blend = m_enums[0];
        blend.set_label("Blend mode").set_tooltip("One choice out of a list. Up and Down step through it while it is closed");
        blend.add("Opaque", 0);
        blend.add("Alpha blend", 1);
        blend.add("Additive", 2);
        blend.add("Multiply", 3);
        blend.set_value(1);

        // Values that are not 0, 1, 2, so the number a row stands for is shown to be its own.
        enum_field& shadows = m_enums[1];
        shadows.set_label("Shadows");
        shadows.add("Off", 0);
        shadows.add("Hard", 10);
        shadows.add("Soft", 20);
        shadows.add("Very soft", 30);
        shadows.set_value(20);

        // A list longer than the rows it shows, which scrolls.
        enum_field& scene = m_enums[2];
        scene.set_label("Scene");

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(std::size(k_scenes)); ++i)
            scene.add(k_scenes[i], i);

        scene.set_value(4);

        flags_field& layers = m_flag_fields[0];
        layers.set_label("Culling mask").set_tooltip("Tick any number of layers. The list stays open while ticking");
        layers.add("Default", 1u << 0);
        layers.add("Terrain", 1u << 1);
        layers.add("Water", 1u << 2);
        layers.add("Characters", 1u << 3);
        layers.add("Effects", 1u << 4);
        layers.add("UI", 1u << 5);
        layers.set_flags(0x0B);

        // A choice that is a group of bits, ticked when all of them are set.
        flags_field& axes = m_flag_fields[1];
        axes.set_label("Constraints");
        axes.add("Move X", 1u << 0);
        axes.add("Move Y", 1u << 1);
        axes.add("Move Z", 1u << 2);
        axes.add("Move all", 0x7);
        axes.add("Rotate X", 1u << 3);
        axes.add("Rotate Y", 1u << 4);
        axes.add("Rotate Z", 1u << 5);
        axes.set_flags(0);

        flags_field& frozen = m_flag_fields[2];
        frozen.set_label("Disabled");
        frozen.add("Red", 1u << 0);
        frozen.add("Green", 1u << 1);
        frozen.add("Blue", 1u << 2);
        frozen.set_flags(0x5);
        frozen.set_enabled(false);
    }

    void data_controls_panel::fill_ranges() noexcept
    {
        for (range_slider& item : m_ranges)
        {
            item.set_on_change(&data_controls_panel::range_changed, this).set_width(380).set_label_width(90);
            const bool added = m_set.add(item);
            (void)added;
        }

        for (angle_dial& item : m_dials)
        {
            item.set_on_change(&data_controls_panel::dial_changed, this).set_label_width(90);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_ranges[0].set_label("Reach").set_limits(0.0, 100.0).set_decimals(1).set_values(15.0, 60.0);
        m_ranges[0].set_tooltip("Drag a thumb, or the filled part to move both. Space picks the other thumb for the keys");
        m_ranges[1].set_label("Lifetime").set_limits(0.0, 10.0).set_step(0.25).set_min_gap(0.5).set_values(1.0, 4.0);
        m_ranges[1].set_tooltip("Steps of a quarter, and the thumbs stay half a unit apart");
        m_ranges[2].set_label("Disabled").set_limits(0.0, 1.0).set_values(0.2, 0.8);
        m_ranges[2].set_enabled(false);

        m_dials[0].set_label("Yaw").set_zero(angle_zero::north).set_angle(45.0);
        m_dials[0].set_tooltip("Drag round the dial. Shift snaps to 15 degrees, a double click puts it back to zero");
        m_dials[1].set_label("Hue").set_signed(false).set_zero(angle_zero::east).set_clockwise(false).set_angle(210.0);
        m_dials[2].set_label("Hinge").set_zero(angle_zero::south).set_limits(-60.0, 90.0).set_angle(30.0);
        m_dials[2].set_tooltip("Stops at its limits instead of wrapping");
        m_dials[3].set_label("Snapped").set_step(5.0).set_zero(angle_zero::north).set_angle(100.0);
        m_dials[4].set_label("Disabled").set_angle(-120.0);
        m_dials[4].set_enabled(false);
    }

    void data_controls_panel::fill_overlays() noexcept
    {
        m_pop_buttons[0].set_label("Filters").set_style(button_style::primary).set_on_click(&data_controls_panel::pop_clicked, this);
        m_pop_buttons[0].set_tooltip("Opens a popover under the button. Ctrl+K does the same. Escape or a click outside closes it");
        m_pop_buttons[1].set_label("Pinned note").set_on_click(&data_controls_panel::pop_clicked, this);
        m_pop_buttons[1].set_tooltip("A popover that opens to the right and ignores clicks outside. Escape or Close closes it");
        m_pop_buttons[2].set_label("Skipped by Tab").set_style(button_style::flat);
        m_pop_buttons[2].set_tab_index(-1);
        m_pop_buttons[2].set_tooltip("Tab goes past this one, but a click still takes it");

        for (button& item : m_pop_buttons)
        {
            const bool added = m_set.add(item);
            (void)added;
        }

        // The close buttons are in the popovers, so they are not added to the panel.
        for (button& item : m_pop_close)
            item.set_label("Close").set_on_click(&data_controls_panel::pop_clicked, this);

        // What is inside the popovers is added to them, and not to the panel.
        m_pop_checks[0].set_label("Show hidden").set_checked(true);
        m_pop_checks[1].set_label("Only selected");
        m_pop_slider.set_range(0.0f, 1.0f).set_value(0.6f).set_label("Opacity").set_label_width(60).set_width(230);
        m_pop_note.set_text("This one stays open until it is closed.").set_style(label_style::dim).set_wrap(true);

        m_pops[0].attach(m_set).set_size(260, 0).set_on_close(&data_controls_panel::pop_closed, this);
        m_pops[0].add(m_pop_checks[0]);
        m_pops[0].add(m_pop_checks[1]);
        m_pops[0].add(m_pop_slider);
        m_pops[0].add(m_pop_close[0]);

        m_pops[1].attach(m_set).set_size(220, 0).set_side(popover_side::right).set_dismissable(false).set_on_close(&data_controls_panel::pop_closed, this);
        m_pops[1].add(m_pop_note);
        m_pops[1].add(m_pop_close[1]);

        // The panes of the splitters: the buttons and the checkboxes and the slider are in the set, the panes only place them.
        m_pane_buttons[0].set_label("One");
        m_pane_buttons[1].set_label("Two");
        m_pane_buttons[2].set_label("Three");
        m_pane_checks[0].set_label("Wrap");
        m_pane_checks[1].set_label("Clip");
        m_pane_slider.set_range(0.0f, 10.0f).set_value(4.0f).set_label("Zoom").set_label_width(40).set_width(160);

        for (button& item : m_pane_buttons)
        {
            m_pane_a1.add(item).set_align(layout_align::stretch);
            const bool added = m_set.add(item);
            (void)added;
        }

        for (checkbox& item : m_pane_checks)
        {
            m_pane_b1.add(item);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_pane_b2.add(m_pane_slider);
        const bool slider_added = m_set.add(m_pane_slider);
        (void)slider_added;

        m_pane_a2.add(m_split_inner).set_grow(1);
        m_split_inner.set_axis(split_axis::vertical).set_panes(m_pane_b1, m_pane_b2).set_ratio(0.45f).set_first_min(40).set_second_min(40);
        m_split_inner.set_name("Inner").set_tooltip("Drag the bar, or focus it and use the arrows. A double click puts it back").set_on_change(&data_controls_panel::pane_changed, this);
        m_split_outer.set_axis(split_axis::horizontal).set_panes(m_pane_a1, m_pane_a2).set_ratio(0.3f).set_first_min(90).set_second_min(120).set_height(190);
        m_split_outer.set_name("Outer").set_tooltip("A splitter with another inside it").set_on_change(&data_controls_panel::pane_changed, this);

        // A splitter goes after the controls in its panes, and an inner one before the outer.
        const bool inner_added = m_set.add(m_split_inner);
        const bool outer_added = m_set.add(m_split_outer);
        (void)inner_added;
        (void)outer_added;

        m_set.set_escape_blurs(true);
        const bool k_added = m_set.add_shortcut('K', true, false, false, &data_controls_panel::shortcut_filters, this);
        const bool r_added = m_set.add_shortcut('R', true, true, false, &data_controls_panel::shortcut_reset, this);
        (void)k_added;
        (void)r_added;
    }

    void data_controls_panel::fill_text() noexcept
    {
        m_areas[0].set_name("Notes").set_placeholder("Notes: type here. Enter breaks the line, Ctrl+Enter submits").set_rows(6).set_width(360)
            .set_on_action(&data_controls_panel::area_acted, this).set_tooltip("Several lines. Up and Down keep their column, a triple click takes a line");
        m_areas[1].set_name("Code").set_rows(7).set_width(420).set_wrap(false).set_line_numbers(true).set_tab_inserts(true)
            .set_on_action(&data_controls_panel::area_acted, this).set_tooltip("No wrapping, line numbers, and Tab puts spaces in");
        m_areas[1].set_text(k_code_sample);
        m_areas[2].set_name("Read only").set_rows(5).set_width(300).set_read_only(true).set_on_action(&data_controls_panel::area_acted, this);
        m_areas[2].set_text(k_prose_sample);
        m_areas[3].set_name("Disabled").set_rows(5).set_width(300).set_enabled(false);
        m_areas[3].set_text("Not for editing right now.");

        for (std::uint32_t i = 0; i < std::size(k_chip_names); ++i)
        {
            const std::int32_t chip = m_search.add_chip(k_chip_names[i]);

            if (chip >= 0)
                m_search.set_chip_colour(static_cast<std::uint32_t>(chip), k_chip_colours[i]);
        }

        m_search.set_placeholder("Search assets").set_name("Search").set_width(460).set_on_change(&data_controls_panel::search_changed, this)
            .set_on_submit(&data_controls_panel::search_submitted, this)
            .set_tooltip("Enter makes the text a pill, Backspace takes the last one, Ctrl+1 to Ctrl+4 flip the chips");

        for (text_area& item : m_areas)
        {
            const bool added = m_set.add(item);
            (void)added;
        }

        const bool search_added = m_set.add(m_search);
        (void)search_added;

        m_results = static_cast<std::int32_t>(m_list_count);
        add_list(11).set_name("Results").set_source(&data_controls_panel::asset_row, this, 0).set_rows(8).set_width(300).set_striped(true)
            .set_tooltip("What the search box lets through");
        apply_search();
        m_has_status = false;
    }

    void data_controls_panel::fill_crumbs() noexcept
    {
        m_crumbs[0].set_path("Assets/Models/Props/Barrels").set_root_icon(icon_kind::folder).set_name("Assets path").set_width(560)
            .set_children(&data_controls_panel::crumb_children, this).set_on_event(&data_controls_panel::crumb_event_raised, this)
            .set_tooltip("Click a segment to go there, an arrow for what is inside it, the empty part to type. Alt+Up goes up, F2 types");
        m_crumbs[1].set_path("Projects/Kaumdron/Content/Assets/Models/Characters/Hero/Armour/Plate").set_root_icon(icon_kind::folder).set_name("Long path")
            .set_width(360).set_children(&data_controls_panel::crumb_children, this).set_on_event(&data_controls_panel::crumb_event_raised, this)
            .set_tooltip("Too long for the width: the first segments fold into the dots, which open a list of them");
        m_crumbs[2].set_path("Scenes/Forest/Camp").set_name("No arrows").set_width(560).set_on_event(&data_controls_panel::crumb_event_raised, this)
            .set_tooltip("Without a children handler there is no list under the arrows");

        for (breadcrumb_bar& item : m_crumbs)
        {
            const bool added = m_set.add(item);
            (void)added;
        }

        m_has_status = false;
    }

    std::uint32_t data_controls_panel::crumb_children(void* user, breadcrumb_bar& source, std::int32_t segment, const char** names, std::uint32_t capacity) noexcept
    {
        (void)user;
        (void)source;
        static const char* const k_level_0[] = {"Models", "Textures", "Materials", "Audio", "Scenes", "Shaders"};
        static const char* const k_level_1[] = {"Props", "Characters", "Environment", "Vehicles", "Weapons"};
        static const char* const k_level_2[] = {"Barrels", "Crates", "Lanterns", "Barrows", "Fences", "Signs", "Benches", "Wells", "Carts", "Troughs", "Ladders"};
        static const char* const k_level_n[] = {"Low", "Medium", "High"};
        const char* const* source_list = k_level_n;
        std::uint32_t total = static_cast<std::uint32_t>(std::size(k_level_n));

        if (segment == 0)
        {
            source_list = k_level_0;
            total = static_cast<std::uint32_t>(std::size(k_level_0));
        }
        else if (segment == 1)
        {
            source_list = k_level_1;
            total = static_cast<std::uint32_t>(std::size(k_level_1));
        }
        else if (segment == 2)
        {
            source_list = k_level_2;
            total = static_cast<std::uint32_t>(std::size(k_level_2));
        }

        total = std::min(total, capacity);

        for (std::uint32_t i = 0; i < total; ++i)
            names[i] = source_list[i];

        return total;
    }

    void data_controls_panel::crumb_event_raised(void* user, breadcrumb_bar& source, crumb_event what, std::int32_t segment, std::int32_t child) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_has_status = true;
        char path[160];

        switch (what)
        {
        case crumb_event::navigate:
            source.truncate(segment + 1);
            source.path(path, sizeof(path));
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: went to %s", source.name(), path);
            break;
        case crumb_event::pick:
        {
            char picked[breadcrumb_bar::k_title_length];
            const char* names[breadcrumb_bar::k_max_names]{};
            const std::uint32_t found = crumb_children(user, source, segment, names, breadcrumb_bar::k_max_names);
            std::snprintf(picked, sizeof(picked), "%s", child >= 0 && static_cast<std::uint32_t>(child) < found ? names[child] : "");
            source.truncate(segment + 1);
            source.add_segment(picked);
            source.path(path, sizeof(path));
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: chose %s, now %s", source.name(), picked, path);
            break;
        }
        case crumb_event::edit:
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: asked to type a path", source.name());
            break;
        }

        if (self.m_host != nullptr)
            self.m_host->invalidate_panel(self.m_id);
    }

    namespace
    {
        const char* const k_dnd_assets[] = {
            "rock_large.fbx", "tree_pine.fbx", "crate.fbx", "barrel.fbx", "lantern.fbx", "bridge.fbx", "grass.mat", "metal_rust.mat",
            "wind_loop.wav", "door_creak.wav", "music_forest.ogg", "sky_cubemap.dds",
        };

        // The names of the files of a drag from outside, narrowed. Returns how many.
        std::uint32_t file_names(const wchar_t* files, char (*names)[32]) noexcept
        {
            std::uint32_t found = 0;

            for (const wchar_t* path = files; path != nullptr && *path != L'\0' && found < 24; path += std::wcslen(path) + 1)
            {
                const wchar_t* name = path;

                for (const wchar_t* at = path; *at != L'\0'; ++at)
                {
                    if (*at == L'\\' || *at == L'/')
                        name = at + 1;
                }

                std::uint32_t length = 0;

                for (; name[length] != L'\0' && length < 31; ++length)
                    names[found][length] = name[length] < 128 ? static_cast<char>(name[length]) : '?';

                names[found][length] = '\0';
                ++found;
            }

            return found;
        }

        constexpr std::uint32_t k_asset_kind = kind_id("ASST");
        constexpr std::uint32_t k_row_kind = kind_id("PLST");
    }

    void data_controls_panel::fill_dnd() noexcept
    {
        m_dnd_assets.set_name("Library").set_items(k_dnd_assets, static_cast<std::uint32_t>(std::size(k_dnd_assets))).set_rows(8).set_width(220)
            .set_select_mode(list_select_mode::multi).set_drag_source(&data_controls_panel::assets_drag, this)
            .set_tooltip("Drag rows out: Ctrl copies, Alt links. Ctrl or Shift selects, a plain press on a selected row keeps all of them");

        const char* const start[] = {"barrel.fbx", "lantern.fbx", "wind_loop.wav"};

        for (const char* name : start)
            std::snprintf(m_playlist[m_playlist_count++], sizeof(m_playlist[0]), "%s", name);

        m_dnd_playlist.set_name("Playlist").set_source(&data_controls_panel::playlist_row, this, m_playlist_count).set_rows(8).set_width(260)
            .set_select_mode(list_select_mode::multi).set_drag_source(&data_controls_panel::playlist_drag, this)
            .set_drop_target(&data_controls_panel::playlist_drop, this, list_drop_mode::between)
            .set_tooltip("Takes rows from the library and from itself, and files dragged in from Explorer. Moving reorders, Ctrl copies. The edges scroll while held");

        m_dnd_zones[0].set_name("Favourite").set_prompt("Favourite: drop an asset").set_icon(icon_kind::stack).set_width(240).add_kind(k_asset_kind);
        m_dnd_zones[0].set_effects(effects::copy | effects::link).set_on_drop(&data_controls_panel::zone_dropped, this)
            .set_tooltip("Takes one kind, assets, and only copies or links");
        m_dnd_zones[1].set_name("Trash").set_prompt("Trash: drop playlist rows").set_icon(icon_kind::trash).set_width(240).add_kind(k_row_kind);
        m_dnd_zones[1].set_effects(effects::move).set_on_drop(&data_controls_panel::zone_dropped, this)
            .set_tooltip("Takes playlist rows, and only moving them out");
        m_dnd_zones[2].set_name("Anything").set_prompt("Anything: takes every kind").set_icon(icon_kind::folder).set_width(240)
            .set_on_drop(&data_controls_panel::zone_dropped, this).set_tooltip("Takes every kind and every effect");

        const bool lists_added = m_set.add(m_dnd_assets) && m_set.add(m_dnd_playlist);
        (void)lists_added;

        for (drop_box& item : m_dnd_zones)
        {
            const bool added = m_set.add(item);
            (void)added;
        }

        m_has_status = false;
    }

    namespace
    {
        constexpr std::uint32_t k_thumb_kind = kind_id("THMB");
    }

    void data_controls_panel::fill_thumbs() noexcept
    {
        m_thumbs[0].set_name("Assets").set_rows(3).set_select_mode(grid_select_mode::multi)
            .set_source(&data_controls_panel::thumb_label, &data_controls_panel::thumb_picture, this, static_cast<std::uint32_t>(std::size(k_assets)))
            .set_drag_source(&data_controls_panel::thumb_drag, this).set_on_change(&data_controls_panel::thumb_changed, this)
            .set_on_activate(&data_controls_panel::thumb_activated, this)
            .set_tooltip("Ctrl and the wheel change the size. Ctrl or Shift selects, the arrows move, Enter opens. Drag tiles into the boxes above");
        m_thumbs[1].set_name("Many").set_rows(2).set_tile_size(72).set_select_mode(grid_select_mode::multi)
            .set_source(&data_controls_panel::thumb_many, nullptr, this, 1500).set_on_change(&data_controls_panel::thumb_changed, this)
            .set_on_activate(&data_controls_panel::thumb_activated, this)
            .set_tooltip("1500 tiles with no pictures of their own: only the ones in view are made");
        m_thumbs[2].set_name("Empty").set_rows(1).set_width(300);
        m_thumbs[2].set_empty_text("No assets in this folder");

        for (thumbnail_grid& item : m_thumbs)
        {
            const bool added = m_set.add(item);
            (void)added;
        }
    }

    void data_controls_panel::fill_animation() noexcept
    {
        m_anim_curves[0].add_key(0.0f, 0.0f);
        m_anim_curves[0].add_key(0.5f, 1.2f);
        m_anim_curves[0].add_key(1.0f, 0.4f);
        m_anim_curves[0].add_key(1.6f, 1.0f);
        m_anim_curves[0].add_key(2.0f, 0.2f);
        m_anim_curves[1].add_key(0.0f, 0.5f);
        m_anim_curves[1].add_key(0.7f, -0.3f, key_mode::stepped);
        m_anim_curves[1].add_key(1.3f, 0.9f);
        m_anim_curves[1].add_key(2.0f, 0.6f);
        m_anim_curves[2].add_key(0.0f, 1.0f, key_mode::linear);
        m_anim_curves[2].add_key(1.0f, 1.8f, key_mode::linear);
        m_anim_curves[2].add_key(2.0f, 1.0f);

        const char* const names[3] = {"Position X", "Position Y", "Scale"};
        const std::uint32_t tints[3] = {0xE5534B, 0x3FA66B, 0x4F8FDB};

        m_curve_editor.set_name("Curves").set_height(280).set_selection(&m_anim_selection).set_on_change(&data_controls_panel::curve_edited, this)
            .set_on_select(&data_controls_panel::curve_selected, this).set_on_time(&data_controls_panel::curve_time, this)
            .set_tooltip("Wheel zooms (Ctrl time, Shift value). Drag keys and handles, drag empty space to box select, double click adds a key. F frames, S L T B set the key mode, Delete removes");
        m_timeline.set_name("Timeline").set_rows(3).set_fps(30.0f).set_range(0.0f, 2.0f).set_selection(&m_anim_selection)
            .set_on_change(&data_controls_panel::timeline_edited, this).set_on_select(&data_controls_panel::timeline_selected, this)
            .set_on_time(&data_controls_panel::timeline_time, this)
            .set_tooltip("Drag the ruler to move the cursor. Drag diamonds to move keys, snapped to frames. Ctrl+wheel zooms, Shift+wheel pans, Left and Right step a frame");

        for (std::uint32_t i = 0; i < 3; ++i)
        {
            m_curve_editor.add_curve(m_anim_curves[i], names[i], tints[i]);
            m_timeline.add_track(m_anim_curves[i], names[i], tints[i]);
        }

        m_curve_editor.set_time(0.6f).frame_all();
        m_timeline.set_time(0.6f).frame_all();

        const char* const labels[5] = {"Smooth", "Linear", "Stepped", "Free", "Frame all"};
        const char* const tips[5] = {"Selected keys become smooth (S)", "Selected keys become linear (L)", "Selected keys hold their value (T)",
            "Selected keys keep their handles as they are, each side on its own (B)", "Fit the view to every key (F)"};

        for (std::uint32_t i = 0; i < 5; ++i)
        {
            m_anim_buttons[i].set_label(labels[i]).set_tooltip(tips[i]).set_on_click(&data_controls_panel::anim_clicked, this);

            if (i == 4)
                m_anim_buttons[i].set_style(button_style::flat);

            const bool added = m_set.add(m_anim_buttons[i]);
            (void)added;
        }

        const bool added = m_set.add(m_curve_editor) && m_set.add(m_timeline);
        (void)added;
    }

    void data_controls_panel::fill_gradient() noexcept
    {
        m_gradient.reset();
        m_gradient.set_color(0, {0.95f, 0.45f, 0.12f, 1.0f});
        m_gradient.set_color(1, {0.2f, 0.45f, 0.95f, 1.0f});
        m_gradient.add_color(0.35f, {0.98f, 0.85f, 0.2f, 1.0f});
        m_gradient.add_color(0.7f, {0.2f, 0.8f, 0.65f, 1.0f});
        m_gradient.set_alpha(0, 0.0f);
        m_gradient.set_alpha(1, 0.35f);
        m_gradient.add_alpha(0.2f, 1.0f);
        m_gradient.add_alpha(0.85f, 1.0f);

        m_gradient_editor.set_name("Gradient").set_gradient(&m_gradient).set_on_change(&data_controls_panel::gradient_edited, this)
            .set_on_select(&data_controls_panel::gradient_selected, this)
            .set_tooltip("Click above the bar to add an alpha stop, below it for a colour stop. Drag a marker to move it, or far away to remove it. Delete removes, arrows nudge");

        m_stop_color.set_label("Stop colour").set_alpha(false).set_picker(&m_picker).set_on_change(&data_controls_panel::gradient_color_changed, this).set_width(160)
            .set_label_width(90).set_tooltip("The colour of the selected colour stop");
        m_stop_position.set_label("Location %").set_label_width(90).set_width(110).set_range(0.0, 100.0).set_step(1.0).set_decimals(1)
            .set_on_change(&data_controls_panel::gradient_number_changed, this).set_tooltip("Where the selected stop is along the bar");
        m_stop_alpha.set_label("Alpha %").set_label_width(90).set_width(110).set_range(0.0, 100.0).set_step(1.0).set_decimals(1)
            .set_on_change(&data_controls_panel::gradient_number_changed, this).set_tooltip("The alpha of the selected alpha stop");

        m_gradient_buttons[0].set_label("Blend").set_tooltip("Switch between blending between stops and holding each stop's value");
        m_gradient_buttons[1].set_label("Reset").set_style(button_style::flat).set_tooltip("White to black again");

        for (button& item : m_gradient_buttons)
        {
            item.set_on_click(&data_controls_panel::gradient_clicked, this);
            const bool added = m_set.add(item);
            (void)added;
        }

        const bool added = m_set.add(m_gradient_editor) && m_set.add(m_stop_color) && m_set.add(m_stop_position) && m_set.add(m_stop_alpha);
        (void)added;
        sync_gradient_fields();
    }

    void data_controls_panel::fill_asset_fields() noexcept
    {
        for (asset_field& item : m_asset_fields)
        {
            item.set_on_change(&data_controls_panel::asset_changed, this).set_on_browse(&data_controls_panel::asset_browse, this).set_label_width(110).set_width(320);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_asset_fields[0].set_label("Albedo").set_prompt("None (Texture)").set_extensions("png;jpg;tga").set_icon(icon_kind::stack)
            .set_tooltip("Takes tiles dragged from a thumbnail grid or the assets list, and png, jpg or tga files dragged from outside. Browse cycles sample paths. Ctrl+C and Ctrl+V copy and paste");
        m_asset_fields[0].add_kind(k_thumb_kind);
        m_asset_fields[0].add_kind(k_asset_kind);
        m_asset_fields[1].set_label("Mesh").set_prompt("None (Mesh)").set_extensions("fbx;obj;gltf").set_icon(icon_kind::layout).set_tooltip("Takes fbx, obj or gltf files");
        m_asset_fields[1].add_kind(k_asset_kind);
        m_asset_fields[1].set_path("Assets/Models/rock_large.fbx");
        m_asset_fields[2].set_label("Output folder").set_prompt("No folder chosen").set_icon(icon_kind::folder).set_tooltip("Any dropped file or pasted path");
        m_asset_fields[3].set_label("Missing").set_icon(icon_kind::stack).set_path("Assets/Old/removed_texture.png");
        m_asset_fields[3].set_missing(true);
        m_asset_fields[3].set_tooltip("The path is of a file that is not there, so it shows in red");
        m_asset_fields[4].set_label("Disabled").set_path("Assets/Textures/locked.png");
        m_asset_fields[4].set_enabled(false);
    }

    void data_controls_panel::fill_proportion() noexcept
    {
        const char* captions[3] = {"SIM", "RENDER", "GPU"};
        const char* helps[3] = {"Where a sim frame went. Click a part to select it, double click or Enter to go into it, Left and Right step through the parts",
            "Waiting is hatched and does not count as work", "Parts under half a percent that sit together are drawn as one"};

        for (std::uint32_t i = 0; i < 3; ++i)
        {
            m_proportion[i].set_name(captions[i]).set_caption(captions[i], "").set_budget(16.6f).set_scale(16.6f).set_help(helps[i]);
            m_proportion[i].set_on_select(&data_controls_panel::proportion_picked, this).set_on_activate(&data_controls_panel::proportion_activated, this);
            const bool added = m_set.add(m_proportion[i]);
            (void)added;
        }

        m_proportion_buttons[0].set_label("Shuffle").set_tooltip("New times for every part. The widths ease to them");
        m_proportion_buttons[1].set_label("Over budget").set_style(button_style::flat).set_tooltip("Make the sim frame longer than the budget: the bar keeps every part, and the line moves in");
        m_proportion_buttons[2].set_label("Reset").set_style(button_style::flat).set_tooltip("Back to the first times, and out of the part that was gone into");

        for (button& item : m_proportion_buttons)
        {
            item.set_on_click(&data_controls_panel::proportion_clicked, this);
            const bool added = m_set.add(item);
            (void)added;
        }

        load_proportion();
    }

    // Parts of the three demo bars. A shuffle scales each one by a factor from a small generator.
    void data_controls_panel::load_proportion() noexcept
    {
        const auto jitter = [this]() noexcept
        {
            m_proportion_seed = m_proportion_seed * 1664525u + 1013904223u;
            return 0.6f + static_cast<float>((m_proportion_seed >> 16) & 0xFF) / 255.0f * 0.9f;
        };

        const bool shuffled = m_proportion_seed != 1;
        const auto scale = [&](float value) noexcept { return shuffled ? value * jitter() : value; };
        constexpr std::uint32_t automatic = proportion_bar::k_auto_colour;

        bar_segment sim[24]{};
        std::uint32_t count = 0;
        char tiny_names[8][12]{};

        if (m_proportion_drilled)
        {
            const char* names[] = {"physics", "animation", "scripts", "ai", "audio", "world update"};
            const float values[] = {2.1f, 1.2f, 0.7f, 0.4f, 0.3f, 1.4f};

            for (std::uint32_t i = 0; i < 6; ++i)
                sim[count++] = {100 + i, names[i], scale(values[i]), automatic, false, "inside sim"};
        }
        else
        {
            sim[count++] = {1, "pump", scale(0.4f), automatic, false, "window events and the bus"};
            sim[count++] = {2, "sim", scale(6.1f + (m_proportion_over ? 9.0f : 0.0f)), automatic, false, "Double click to go into it"};
            sim[count++] = {3, "modules", scale(2.3f), automatic, false, "the editor and every module"};
            sim[count++] = {4, "flush", scale(0.5f), automatic, false, nullptr};

            for (std::uint32_t i = 0; i < 8; ++i)
            {
                std::snprintf(tiny_names[i], sizeof(tiny_names[i]), "tiny %u", i);
                sim[count++] = {10 + i, tiny_names[i], 0.02f, automatic, false, nullptr};
            }

            sim[count++] = {5, "sleep", scale(4.0f), automatic, true, "the host waits for the next frame"};
        }

        m_proportion[0].set_segments(sim, count);

        const bar_segment render[] = {
            {20, "wait frame", scale(1.2f), automatic, true, nullptr},
            {21, "acquire", scale(0.1f), automatic, true, nullptr},
            {22, "record", scale(3.4f), automatic, false, "build graph, compile and execute"},
            {23, "submit", scale(0.3f), automatic, false, nullptr},
            {24, "present", scale(5.0f), automatic, false, nullptr},
        };
        m_proportion[1].set_segments(render, 5);

        const bar_segment gpu[] = {
            {30, "shadow", scale(2.1f), automatic, false, nullptr},
            {31, "gbuffer", scale(4.4f), automatic, false, nullptr},
            {32, "lighting", scale(3.0f), automatic, false, nullptr},
            {33, "post", scale(0.9f), automatic, false, nullptr},
            {34, "idle", scale(3.5f), automatic, true, nullptr},
        };
        m_proportion[2].set_segments(gpu, 5);

        // The values on the right of the captions are the sums of the parts that work.
        const bar_segment* all[3] = {sim, render, gpu};
        const std::uint32_t sizes[3] = {count, 5, 5};
        const char* names[3] = {"SIM", "RENDER", "GPU"};

        for (std::uint32_t i = 0; i < 3; ++i)
        {
            float total = 0.0f;

            for (std::uint32_t k = 0; k < sizes[i]; ++k)
                total += all[i][k].idle ? 0.0f : all[i][k].value;

            char text[24]{};
            std::snprintf(text, sizeof(text), "%.1f ms", static_cast<double>(total));
            m_proportion[i].set_caption(names[i], text);
        }
    }

    void data_controls_panel::proportion_picked(void* user, proportion_bar& source, std::uint32_t id) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: selected part %u", source.name(), id);
        self.m_has_status = true;
    }

    void data_controls_panel::proportion_activated(void* user, proportion_bar& source, std::uint32_t id) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: went into part %u", source.name(), id);
        self.m_has_status = true;

        // Going into the sim part of the first bar swaps its parts for the ones inside it.
        if (&source == &self.m_proportion[0] && id == 2 && !self.m_proportion_drilled)
        {
            self.m_proportion_drilled = true;
            self.m_proportion[0].clear_selection();
            self.load_proportion();
        }
    }

    void data_controls_panel::proportion_clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (&source == &self.m_proportion_buttons[0])
        {
            self.m_proportion_seed = self.m_proportion_seed * 2891336453u + 12345u;
        }
        else if (&source == &self.m_proportion_buttons[1])
        {
            self.m_proportion_over = !self.m_proportion_over;
        }
        else
        {
            self.m_proportion_seed = 1;
            self.m_proportion_over = false;
            self.m_proportion_drilled = false;
            self.m_proportion[0].clear_selection();
        }

        self.load_proportion();
    }

    void data_controls_panel::fill_graph() noexcept
    {
        m_graph_view.set_name("Node graph").set_graph(&m_graph).set_height(380).set_snap(8.0f).set_on_change(&data_controls_panel::graph_edited, this)
            .set_on_select(&data_controls_panel::graph_selected, this).set_on_create(&data_controls_panel::graph_create, this)
            .set_tooltip("Wheel zooms, Space or Alt and drag pans. Drag from a pin to link, or let go on empty space to add a node there. "
                         "Double click a link for a knob, empty space for a node. Drag the bars to set values. F frames, Ctrl+D duplicates, Delete removes");

        m_graph_title.set_placeholder("Title of the selected node").set_width(220).set_on_action(&data_controls_panel::graph_title_action, this);
        m_graph_title.set_enabled(false);

        for (button& item : m_graph_buttons)
        {
            item.set_on_click(&data_controls_panel::graph_clicked, this);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_graph_buttons[3].set_label("Arrange").set_tooltip("Lay the nodes out in columns, left to right along the links");
        m_graph_buttons[4].set_label("Frame all").set_style(button_style::flat).set_tooltip("Fit the view to every node (F frames the selection)");
        m_graph_buttons[5].set_label("Delete").set_style(button_style::flat).set_tooltip("Remove the selected nodes and links (Delete)");
        m_graph_buttons[6].set_style(button_style::flat);

        const bool added = m_set.add(m_graph_view) && m_set.add(m_graph_title);
        (void)added;
        load_graph_demo(0);
    }

    // The palettes of the two demos. Kind 0 to 2 are the nodes the buttons add.
    void data_controls_panel::graph_palette(std::uint32_t demo, std::uint32_t kind, graph_node_desc& out) noexcept
    {
        out = {};

        if (demo == 0)
        {
            switch (kind % 3)
            {
            case 0:
                out.set_title("Texture").set_colour(0xB5527F).input("Tiling", 0).value(1.0f, 0.0f, 8.0f).output("Colour", 1).output("Alpha", 0);
                break;

            case 1:
                out.set_title("Multiply").set_colour(0x4F8FDB).input("A", 1).input("B", 0).value(1.0f, 0.0f, 4.0f).output("Result", 1);
                break;

            default:
                out.set_title("Surface").set_colour(0xC4503F).set_width(190.0f).input("Base colour", 1).input("Roughness", 0).value(0.5f, 0.0f, 1.0f)
                    .input("Metallic", 0).value(0.0f, 0.0f, 1.0f).input("Emission", 1);
                break;
            }

            return;
        }

        switch (kind % 3)
        {
        case 0:
            out.set_title("State").set_colour(0x3F8F5E).set_width(130.0f).input("In", 0, true).output("Out", 0);
            break;

        case 1:
            out.set_title("Blend tree").set_colour(0x4F8FDB).set_width(130.0f).input("In", 0, true).output("Out", 0);
            break;

        default:
            out.set_title("Entry").set_colour(0xC4503F).set_width(130.0f).output("Out", 0);
            break;
        }
    }

    void data_controls_panel::load_graph_demo(std::uint32_t demo) noexcept
    {
        m_graph.clear();
        m_graph_demo = demo;
        m_graph_kind = 0;
        graph_node_desc desc;

        const auto make = [&](std::uint32_t kind, float x, float y, const char* title) {
            graph_palette(demo, kind, desc);
            const std::uint32_t id = m_graph.add_node(desc, x, y);

            if (title != nullptr)
                m_graph.set_title(id, title);

            return id;
        };

        if (demo == 0)
        {
            m_graph.set_style(graph_style::flow).set_allow_cycles(false);
            m_graph.set_type(0, "Float", 0x8FC9A0).set_type(1, "Colour", 0xE8B04B);

            const std::uint32_t texture = make(0, 0.0f, 24.0f, "Albedo texture");
            const std::uint32_t multiply = make(1, 248.0f, 40.0f, nullptr);
            const std::uint32_t surface = make(2, 496.0f, 16.0f, nullptr);
            make(0, 0.0f, 168.0f, "Glow texture");
            const std::uint32_t first = m_graph.add_link(texture, 1, multiply, 0);
            (void)m_graph.add_link(multiply, 2, surface, 0);
            m_graph.set_link_knob(first, true, "albedo");

            m_graph_buttons[0].set_label("Add texture");
            m_graph_buttons[1].set_label("Add multiply");
            m_graph_buttons[2].set_label("Add surface");
            m_graph_buttons[6].set_label("Show states").set_tooltip("Switch to a state machine: cycles are allowed and the knobs carry the conditions");
        }
        else
        {
            m_graph.set_style(graph_style::states).set_allow_cycles(true);
            m_graph.set_type(0, "Flow", 0xB9C2D0);

            const std::uint32_t entry = make(2, 0.0f, 112.0f, nullptr);
            const std::uint32_t idle = make(0, 200.0f, 112.0f, "Idle");
            const std::uint32_t walk = make(1, 424.0f, 24.0f, "Walk");
            const std::uint32_t run = make(1, 648.0f, 112.0f, "Run");
            const std::uint32_t jump = make(0, 424.0f, 208.0f, "Jump");
            (void)m_graph.add_link(entry, 0, idle, 0);

            const auto transition = [&](std::uint32_t from, std::uint32_t to, const char* condition) {
                const std::uint32_t id = m_graph.add_link(from, from == entry ? 0u : 1u, to, 0);
                m_graph.set_link_knob(id, true, condition);
            };

            transition(idle, walk, "speed > 0.1");
            transition(walk, idle, "speed < 0.1");
            transition(walk, run, "speed > 4");
            transition(run, walk, "speed < 4");
            transition(idle, jump, "jump");
            transition(jump, idle, "landed");

            m_graph_buttons[0].set_label("Add state");
            m_graph_buttons[1].set_label("Add blend tree");
            m_graph_buttons[2].set_label("Add entry");
            m_graph_buttons[6].set_label("Show material").set_tooltip("Switch to a material network: data flows one way and inputs have values");
        }

        m_graph.clear_selection();
        m_graph_view.frame_all();
        sync_graph_title();
    }

    void data_controls_panel::sync_graph_title() noexcept
    {
        const graph_node* chosen = nullptr;

        if (m_graph.selected_nodes() == 1)
        {
            for (std::uint32_t i = 0; i < m_graph.node_count(); ++i)
            {
                if (m_graph.node_at(i).selected)
                    chosen = &m_graph.node_at(i);
            }
        }

        m_graph_title.set_enabled(chosen != nullptr);

        // The field is only filled in while no title is being typed, so a keystroke is not undone by its own echo.
        if (!m_graph_title.focused())
            m_graph_title.set_text(chosen != nullptr ? chosen->title : "");
    }

    void data_controls_panel::graph_edited(void* user, graph_view& source, bool final) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (final)
        {
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u nodes, %u links", source.name(), self.m_graph.node_count(), self.m_graph.link_count());
            self.m_has_status = true;
        }
    }

    void data_controls_panel::graph_selected(void* user, graph_view& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u nodes and %u links selected", source.name(), self.m_graph.selected_nodes(), self.m_graph.selected_links());
        self.m_has_status = true;
        self.sync_graph_title();
    }

    void data_controls_panel::graph_create(void* user, graph_view& source, float x, float y, std::uint32_t from_node, std::uint32_t from_pin) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        graph_node_desc desc;
        graph_palette(self.m_graph_demo, self.m_graph_kind++, desc);
        const std::uint32_t id = self.m_graph.add_node(desc, std::floor(x / 8.0f) * 8.0f, std::floor(y / 8.0f) * 8.0f);

        if (id == 0)
            return;

        // A link that was let go on empty space joins the first pin of the new node that takes it.
        if (from_node != 0)
        {
            for (std::uint32_t p = 0; p < desc.pin_count; ++p)
            {
                if (self.m_graph.add_link(from_node, from_pin, id, p) != 0)
                    break;
            }
        }

        self.m_graph.clear_selection();
        self.m_graph.find_node(id)->selected = true;
        self.sync_graph_title();
        graph_edited(user, source, true);
    }

    void data_controls_panel::graph_clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        for (std::uint32_t kind = 0; kind < 3; ++kind)
        {
            if (&source != &self.m_graph_buttons[kind])
                continue;

            // New nodes land in the middle of the view, a step further each time so they do not hide one another.
            graph_node_desc desc;
            graph_palette(self.m_graph_demo, kind, desc);
            const float shift = static_cast<float>(self.m_graph.node_count() % 6) * 16.0f;
            const std::uint32_t id = self.m_graph.add_node(desc, std::floor((self.m_graph_view.centre_x() - desc.width * 0.5f + shift) / 8.0f) * 8.0f,
                std::floor((self.m_graph_view.centre_y() - 40.0f + shift) / 8.0f) * 8.0f);

            if (id != 0)
            {
                self.m_graph.clear_selection();
                self.m_graph.find_node(id)->selected = true;
                self.sync_graph_title();
                graph_edited(user, self.m_graph_view, true);
            }

            return;
        }

        if (&source == &self.m_graph_buttons[3])
            self.m_graph_view.arrange();
        else if (&source == &self.m_graph_buttons[4])
            self.m_graph_view.frame_all();
        else if (&source == &self.m_graph_buttons[5])
            self.m_graph_view.remove_selected();
        else
            self.load_graph_demo(self.m_graph_demo == 0 ? 1 : 0);
    }

    bool data_controls_panel::graph_title_action(void* user, text_field& source, text_action action) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (action != text_action::changed && action != text_action::submitted)
            return true;

        if (self.m_graph.selected_nodes() != 1)
            return true;

        char text[64];
        (void)source.get_text(text, sizeof(text));

        for (std::uint32_t i = 0; i < self.m_graph.node_count(); ++i)
        {
            if (self.m_graph.node_at(i).selected)
                self.m_graph.set_title(self.m_graph.node_at(i).id, text);
        }

        return true;
    }

    void data_controls_panel::asset_changed(void* user, asset_field& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (source.empty())
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: cleared", source.name());
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s", source.name(), source.path());

        self.m_has_status = true;
    }

    void data_controls_panel::asset_browse(void* user, asset_field& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        static const char* const samples[] = {"Assets/Textures/brick_albedo.png", "Assets/Textures/moss.jpg", "Assets/Characters/hero_diffuse.tga",
            "Assets/Environment/Rocks/cliff_01.png"};
        source.set_path(samples[self.m_browse_count++ % std::size(samples)]);
        asset_changed(user, source);
    }

    void data_controls_panel::sync_gradient_fields() noexcept
    {
        const gradient_editor::stop_kind kind = m_gradient_editor.selected_kind();
        const bool is_color = kind == gradient_editor::stop_kind::color;
        const bool is_alpha = kind == gradient_editor::stop_kind::alpha;

        m_stop_color.set_enabled(is_color);
        m_stop_position.set_enabled(is_color || is_alpha);
        m_stop_alpha.set_enabled(is_alpha);

        if (is_color && !m_stop_color.is_open())
            m_stop_color.set_color(m_gradient_editor.selected_color());

        m_stop_position.set_value(kind != gradient_editor::stop_kind::none ? static_cast<double>(m_gradient_editor.selected_position()) * 100.0 : 0.0);
        m_stop_alpha.set_value(is_alpha ? static_cast<double>(m_gradient_editor.selected_alpha()) * 100.0 : 0.0);
    }

    void data_controls_panel::gradient_edited(void* user, gradient_editor& source, bool final) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.sync_gradient_fields();

        if (final)
        {
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u colour stops, %u alpha stops", source.name(), source.data()->color_count(), source.data()->alpha_count());
            self.m_has_status = true;
        }
    }

    void data_controls_panel::gradient_selected(void* user, gradient_editor& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.sync_gradient_fields();
        const char* kind = source.selected_kind() == gradient_editor::stop_kind::color ? "colour stop" : source.selected_kind() == gradient_editor::stop_kind::alpha ? "alpha stop" : "no stop";
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s selected, at %.1f%%", source.name(), kind, static_cast<double>(source.selected_position()) * 100.0);
        self.m_has_status = true;
    }

    void data_controls_panel::gradient_color_changed(void* user, color_swatch& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_gradient_editor.set_selected_color(source.color());
    }

    void data_controls_panel::gradient_number_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (&source == &self.m_stop_position)
            self.m_gradient_editor.set_selected_position(static_cast<float>(source.value() / 100.0));
        else
            self.m_gradient_editor.set_selected_alpha(static_cast<float>(source.value() / 100.0));
    }

    void data_controls_panel::gradient_clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (&source == &self.m_gradient_buttons[0])
        {
            const bool fixed = self.m_gradient.mode() == gradient_mode::blend;
            self.m_gradient.set_mode(fixed ? gradient_mode::fixed : gradient_mode::blend);
            source.set_label(fixed ? "Fixed" : "Blend");
        }
        else
        {
            self.m_gradient.reset();
            self.m_gradient_editor.clear_selection();
            self.m_gradient_buttons[0].set_label("Blend");
            self.sync_gradient_fields();
        }
    }

    void data_controls_panel::anim_clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (&source == &self.m_anim_buttons[0])
            self.m_curve_editor.set_mode_of_selected(key_mode::smooth);
        else if (&source == &self.m_anim_buttons[1])
            self.m_curve_editor.set_mode_of_selected(key_mode::linear);
        else if (&source == &self.m_anim_buttons[2])
            self.m_curve_editor.set_mode_of_selected(key_mode::stepped);
        else if (&source == &self.m_anim_buttons[3])
            self.m_curve_editor.set_mode_of_selected(key_mode::free);
        else
        {
            self.m_curve_editor.frame_all();
            self.m_timeline.frame_all();
        }
    }

    void data_controls_panel::curve_edited(void* user, curve_editor& source, bool final) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (final)
        {
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: edited, %u keys selected", source.name(), source.selection().count());
            self.m_has_status = true;
        }
    }

    void data_controls_panel::curve_selected(void* user, curve_editor& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u keys selected", source.name(), source.selection().count());
        self.m_has_status = true;
    }

    void data_controls_panel::curve_time(void* user, curve_editor& source, float time) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_timeline.set_time(time);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: time %.3f s", source.name(), static_cast<double>(time));
        self.m_has_status = true;
    }

    void data_controls_panel::timeline_edited(void* user, timeline_view& source, bool final) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (final)
        {
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: keys moved or changed, %u selected", source.name(), source.selection().count());
            self.m_has_status = true;
        }
    }

    void data_controls_panel::timeline_selected(void* user, timeline_view& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u keys selected", source.name(), source.selection().count());
        self.m_has_status = true;
    }

    void data_controls_panel::timeline_time(void* user, timeline_view& source, float time) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_curve_editor.set_time(time);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: frame %d (%.3f s)", source.name(), static_cast<int>(time * source.fps() + 0.5f), static_cast<double>(time));
        self.m_has_status = true;
    }

    const char* data_controls_panel::thumb_many(void* user, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        char* name = self.m_names[self.m_name_next++ % k_name_ring];
        std::snprintf(name, k_name_length, "Tile %u", index);
        return name;
    }

    const char* data_controls_panel::thumb_label(void* user, std::uint32_t index) noexcept
    {
        (void)user;
        return index < std::size(k_assets) ? k_assets[index].name : "";
    }

    void data_controls_panel::thumb_picture(void* user, void* dc, std::uint32_t index, const dock_rect& box) noexcept
    {
        (void)user;

        if (index >= std::size(k_assets))
            return;

        const std::uint32_t tint = k_chip_colours[k_assets[index].kind];
        draw::raised(dc, box, 2, blend_colour(tint, 0xFFFFFF, 0.22f), blend_colour(tint, 0x000000, 0.35f), blend_colour(tint, 0x000000, 0.4f), true, 0, 0, 0.0f);

        // The extension, large and pale, stands for the picture.
        const char* dot = std::strrchr(k_assets[index].name, '.');
        draw::text(dc, box, dot != nullptr ? dot + 1 : "", 0xFFFFFF, draw::text_bold | draw::text_center);
    }

    bool data_controls_panel::thumb_drag(void* user, thumbnail_grid& source, drag_payload& payload) noexcept
    {
        (void)user;
        rows_ref ref{};

        for (std::uint32_t i = 0; i < source.count() && ref.count < std::size(ref.rows); ++i)
        {
            if (source.is_selected(i))
                ref.rows[ref.count++] = static_cast<std::uint16_t>(i);
        }

        if (ref.count == 0)
            return false;

        payload.set_data(k_thumb_kind, ref);
        payload.set_label(source.label(ref.rows[0])).set_count(ref.count).set_icon(icon_kind::stack).set_allowed(effects::copy | effects::link);
        return true;
    }

    void data_controls_panel::thumb_changed(void* user, thumbnail_grid& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const std::int32_t first = source.first_selected();

        if (source.selected_count() == 0 || first < 0)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: nothing selected", source.name());
        else if (source.selected_count() == 1)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: selected tile %d (%s)", source.name(), first, source.label(static_cast<std::uint32_t>(first)));
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u tiles selected", source.name(), source.selected_count());

        self.m_has_status = true;
    }

    void data_controls_panel::thumb_activated(void* user, thumbnail_grid& source, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: opened tile %u (%s)", source.name(), index, source.label(index));
        self.m_has_status = true;
    }

    const char* data_controls_panel::playlist_row(void* user, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        return index < self.m_playlist_count ? self.m_playlist[index] : "";
    }

    bool data_controls_panel::assets_drag(void* user, list_view& source, drag_payload& payload) noexcept
    {
        (void)user;
        rows_ref ref{};
        ref.source = 0;

        for (std::uint32_t i = 0; i < source.count() && ref.count < std::size(ref.rows); ++i)
        {
            if (source.is_selected(i))
                ref.rows[ref.count++] = static_cast<std::uint16_t>(i);
        }

        if (ref.count == 0)
            return false;

        payload.set_data(k_asset_kind, ref);
        payload.set_label(source.text(ref.rows[0])).set_count(ref.count).set_icon(icon_kind::stack).set_allowed(effects::copy | effects::link);
        return true;
    }

    bool data_controls_panel::playlist_drag(void* user, list_view& source, drag_payload& payload) noexcept
    {
        (void)user;
        rows_ref ref{};
        ref.source = 1;

        for (std::uint32_t i = 0; i < source.count() && ref.count < std::size(ref.rows); ++i)
        {
            if (source.is_selected(i))
                ref.rows[ref.count++] = static_cast<std::uint16_t>(i);
        }

        if (ref.count == 0)
            return false;

        payload.set_data(k_row_kind, ref);
        payload.set_label(source.text(ref.rows[0])).set_count(ref.count).set_icon(icon_kind::menu).set_allowed(effects::move | effects::copy);
        return true;
    }

    bool data_controls_panel::tree_drag(void* user, tree_view& source, drag_payload& payload) noexcept
    {
        (void)user;
        const tree_view::node_id first = source.first_selected();

        if (first == tree_view::k_none)
            return false;

        payload.set_kind(kind_id("NODE")).set_label(source.text(first)).set_count(source.selected_count()).set_icon(icon_kind::folder)
            .set_allowed(effects::all);
        return true;
    }

    bool data_controls_panel::table_drag(void* user, table_view& source, drag_payload& payload) noexcept
    {
        (void)user;
        const std::int32_t first = source.first_selected();

        if (first < 0)
            return false;

        const char* text = source.cell(static_cast<std::uint32_t>(first), 0);
        payload.set_kind(kind_id("ROWS")).set_label(text).set_count(source.selected_count()).set_icon(icon_kind::menu)
            .set_allowed(effects::copy | effects::link);
        return true;
    }

    std::uint32_t data_controls_panel::rows_named(const rows_ref& ref, char (*names)[32]) const noexcept
    {
        std::uint32_t found = 0;

        for (std::uint32_t i = 0; i < ref.count && i < std::size(ref.rows); ++i)
        {
            const std::uint32_t row = ref.rows[i];
            const char* name = ref.source == 0 ? (row < std::size(k_dnd_assets) ? k_dnd_assets[row] : nullptr) : (row < m_playlist_count ? m_playlist[row] : nullptr);

            if (name != nullptr)
                std::snprintf(names[found++], 32, "%s", name);
        }

        return found;
    }

    drop_effect data_controls_panel::playlist_drop(void* user, list_view& target, const drag_event& event, std::int32_t slot, bool commit) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const bool from_library = event.is(k_asset_kind);
        const bool from_playlist = event.is(k_row_kind);
        const bool from_files = event.is(k_files_kind);

        if (!from_library && !from_playlist && !from_files)
            return drop_effect::none;

        if (!commit)
            return event.wanted;

        const rows_ref* ref = event.payload->get<rows_ref>();

        if (slot < 0 || (ref == nullptr && !from_files))
            return drop_effect::none;

        char names[24][32];
        const std::uint32_t count = from_files ? file_names(event.payload->files, names) : self.rows_named(*ref, names);
        const bool moving = from_playlist && ref != nullptr && event.wanted == drop_effect::move;
        std::uint32_t at = static_cast<std::uint32_t>(std::min<std::int32_t>(slot, static_cast<std::int32_t>(self.m_playlist_count)));

        if (moving)
        {
            // The rows leave first, so the place they are put back at counts without them.
            std::uint32_t before = 0;

            for (std::uint32_t i = 0; i < ref->count; ++i)
                before += ref->rows[i] < at ? 1u : 0u;

            for (std::uint32_t i = ref->count; i-- > 0;)
            {
                const std::uint32_t row = ref->rows[i];

                if (row >= self.m_playlist_count)
                    continue;

                for (std::uint32_t j = row; j + 1 < self.m_playlist_count; ++j)
                    std::memcpy(self.m_playlist[j], self.m_playlist[j + 1], sizeof(self.m_playlist[0]));

                --self.m_playlist_count;
            }

            at -= before;
        }

        const std::uint32_t room = static_cast<std::uint32_t>(std::size(self.m_playlist)) - self.m_playlist_count;
        const std::uint32_t put = std::min(count, room);

        for (std::uint32_t j = self.m_playlist_count; j-- > at;)
            std::memcpy(self.m_playlist[j + put], self.m_playlist[j], sizeof(self.m_playlist[0]));

        for (std::uint32_t i = 0; i < put; ++i)
            std::memcpy(self.m_playlist[at + i], names[i], sizeof(self.m_playlist[0]));

        self.m_playlist_count += put;
        target.set_count(self.m_playlist_count);
        target.clear_selection();

        for (std::uint32_t i = 0; i < put; ++i)
            target.add_to_selection(at + i);

        const char* verb = event.wanted == drop_effect::move ? "moved" : event.wanted == drop_effect::link ? "linked" : "copied";
        std::snprintf(self.m_status, sizeof(self.m_status), "Playlist: %s %u row%s to place %u", verb, put, put == 1 ? "" : "s", at);
        self.m_has_status = true;
        return event.wanted;
    }

    void data_controls_panel::zone_dropped(void* user, drop_box& source, const drag_event& event, drop_effect effect) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const char* verb = effect == drop_effect::move ? "moved" : effect == drop_effect::link ? "linked" : "copied";
        const rows_ref* ref = event.payload->get<rows_ref>();

        if (&source == &self.m_dnd_zones[0] && ref != nullptr)
        {
            char names[24][32];

            if (self.rows_named(*ref, names) > 0)
            {
                std::snprintf(self.m_favourite, sizeof(self.m_favourite), "%s", names[0]);
                source.set_text(self.m_favourite);
            }
        }
        else if (&source == &self.m_dnd_zones[1] && ref != nullptr && ref->source == 1)
        {
            for (std::uint32_t i = ref->count; i-- > 0;)
            {
                const std::uint32_t row = ref->rows[i];

                if (row >= self.m_playlist_count)
                    continue;

                for (std::uint32_t j = row; j + 1 < self.m_playlist_count; ++j)
                    std::memcpy(self.m_playlist[j], self.m_playlist[j + 1], sizeof(self.m_playlist[0]));

                --self.m_playlist_count;
            }

            self.m_dnd_playlist.set_count(self.m_playlist_count);
        }

        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s \"%s\"%s", source.name(), verb, event.payload->label,
            event.payload->count > 1 ? " and more" : "");
        self.m_has_status = true;
    }

    void data_controls_panel::fill_tabs() noexcept
    {
        tab_bar& scenes = m_tab_bars[0];
        scenes.set_name("Scenes").set_show_add(true).set_width(520).set_on_event(&data_controls_panel::tab_event_raised, this)
            .set_tooltip("Click or Left and Right choose, drag or Ctrl+Shift+Arrow reorders, Delete or Ctrl+W closes, plus adds");
        scenes.add_tab("Main.scene", icon_kind::folder);
        scenes.add_tab("Forest.scene", icon_kind::folder);
        scenes.add_tab("Harbour.scene", icon_kind::folder);
        scenes.add_tab("A scene with a very long name.scene", icon_kind::folder);
        scenes.set_modified(1, true);
        scenes.select(0);

        tab_bar& crowded = m_tab_bars[1];
        crowded.set_name("Many tabs").set_show_add(true).set_width(520).set_on_event(&data_controls_panel::tab_event_raised, this)
            .set_tooltip("Too many for the width: they shrink, then the row scrolls with the wheel");

        for (std::uint32_t i = 0; i < std::size(k_scenes); ++i)
            crowded.add_tab(k_scenes[i]);

        crowded.set_modified(2, true);
        crowded.select(9);

        tab_bar& fixed = m_tab_bars[2];
        fixed.set_name("Fixed").set_closable(false).set_reorderable(false).set_width(520).set_on_event(&data_controls_panel::tab_event_raised, this)
            .set_tooltip("No crosses, no reordering");
        fixed.add_tab("Scene", icon_kind::layout);
        fixed.add_tab("Game", icon_kind::play);
        fixed.add_tab("Console", icon_kind::menu);
        fixed.add_tab("Profiler", icon_kind::clock);

        for (tab_bar& item : m_tab_bars)
        {
            const bool added = m_set.add(item);
            (void)added;
        }

        m_has_status = false;
    }

    void data_controls_panel::tab_event_raised(void* user, tab_bar& source, tab_event what, std::int32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_has_status = true;

        switch (what)
        {
        case tab_event::select:
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: chose %s", source.name(), source.title(index));
            break;
        case tab_event::close:
        {
            char title[tab_bar::k_title_length];
            std::snprintf(title, sizeof(title), "%s", source.title(index));
            source.remove_tab(index);
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: closed %s, %d left", source.name(), title, source.count());
            break;
        }
        case tab_event::add:
        {
            char title[tab_bar::k_title_length];
            std::snprintf(title, sizeof(title), "Untitled %u", self.m_tab_serial++);
            const std::int32_t added = source.add_tab(title);

            if (added >= 0)
            {
                source.select(added);
                source.set_modified(added, true);
            }

            std::snprintf(self.m_status, sizeof(self.m_status), "%s: added %s", source.name(), title);
            break;
        }
        case tab_event::move:
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: moved %s to place %d", source.name(), source.title(index), index);
            break;
        }

        if (self.m_host != nullptr)
            self.m_host->invalidate_panel(self.m_id);
    }

    void data_controls_panel::apply_search() noexcept
    {
        std::uint32_t kind_counts[std::size(k_chip_names)]{};
        m_found_count = 0;
        const std::uint32_t total = static_cast<std::uint32_t>(std::size(k_assets));

        for (std::uint32_t i = 0; i < total && m_found_count < std::size(m_found); ++i)
        {
            if (!m_search.matches(k_assets[i].name))
                continue;

            ++kind_counts[k_assets[i].kind];

            if (!m_search.any_chip() || m_search.chip_active(k_assets[i].kind))
                m_found[m_found_count++] = i;
        }

        for (std::uint32_t kind = 0; kind < std::size(k_chip_names); ++kind)
            m_search.set_chip_count(kind, static_cast<std::int32_t>(kind_counts[kind]));

        if (m_results >= 0)
        {
            m_lists[m_results].clear_selection();
            m_lists[m_results].set_count(m_found_count);
        }

        std::snprintf(m_status, sizeof(m_status), "Search: %u of %u assets", m_found_count, total);
        m_has_status = true;
    }

    const char* data_controls_panel::asset_row(void* user, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        return index < self.m_found_count ? k_assets[self.m_found[index]].name : nullptr;
    }

    void data_controls_panel::search_changed(void* user, search_box& source) noexcept
    {
        (void)source;
        static_cast<data_controls_panel*>(user)->apply_search();
    }

    void data_controls_panel::search_submitted(void* user, search_box& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "Search submitted: %u results, %u pills", self.m_found_count, source.pill_count());
        self.m_has_status = true;
    }

    bool data_controls_panel::area_acted(void* user, text_area& source, text_action action) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (action == text_action::submitted)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: submitted with Ctrl+Enter, %d lines", source.name(), source.line_count());
        else if (action == text_action::changed || action == text_action::selection_changed)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: line %d, column %d of %d lines, %d characters", source.name(), source.caret_line(),
                source.caret_column(), source.line_count(), source.length());
        else
            return true;

        self.m_has_status = true;
        return true;
    }

    void data_controls_panel::pop_clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);

        if (&source == &self.m_pop_buttons[0])
            self.m_pops[0].toggle(source.area(), self.m_bounds);
        else if (&source == &self.m_pop_buttons[1])
            self.m_pops[1].toggle(source.area(), self.m_bounds);
        else if (&source == &self.m_pop_close[0])
            self.m_pops[0].close();
        else if (&source == &self.m_pop_close[1])
            self.m_pops[1].close();
    }

    void data_controls_panel::pop_closed(void* user, popover& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s popover closed", &source == &self.m_pops[0] ? "Filters" : "Pinned");
        self.m_has_status = true;
    }

    void data_controls_panel::pane_changed(void* user, splitter& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s splitter: %.0f%% / %.0f%%", source.name(), static_cast<double>(source.ratio()) * 100.0,
            (1.0 - static_cast<double>(source.ratio())) * 100.0);
        self.m_has_status = true;
    }

    void data_controls_panel::shortcut_filters(void* user) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_pops[0].toggle(self.m_pop_buttons[0].area(), self.m_bounds);
        std::snprintf(self.m_status, sizeof(self.m_status), "Shortcut Ctrl+K: filters %s", self.m_pops[0].is_open() ? "opened" : "closed");
        self.m_has_status = true;
    }

    void data_controls_panel::shortcut_reset(void* user) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        self.m_split_outer.set_ratio(0.3f);
        self.m_split_inner.set_ratio(0.45f);
        std::snprintf(self.m_status, sizeof(self.m_status), "Shortcut Ctrl+Shift+R: splitters reset");
        self.m_has_status = true;
    }

    void data_controls_panel::range_changed(void* user, range_slider& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %.3g to %.3g", source.name(), source.low(), source.high());
        self.m_has_status = true;
    }

    void data_controls_panel::dial_changed(void* user, angle_dial& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %.1f degrees", source.name(), source.angle());
        self.m_has_status = true;
    }

    void data_controls_panel::option_changed(void* user, option_field& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s", source.name(), source.text());
        self.m_has_status = true;
    }

    void data_controls_panel::swatch_changed(void* user, color_swatch& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const color_rgba c = source.color();
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: #%06X, alpha %d", source.name(), to_rgb24(c), to_byte(c.a));
        self.m_has_status = true;
    }

    void data_controls_panel::fill_vectors() noexcept
    {
        const auto hook = [this](vec_field& item) {
            item.set_on_change(&data_controls_panel::vec_changed, this).set_width(270).set_label_width(80);
            const bool added = m_set.add(item);
            (void)added;
        };

        for (vec2_field& item : m_vec2)
            hook(item);

        for (vec3_field& item : m_vec3)
            hook(item);

        for (vec4_field& item : m_vec4)
            hook(item);

        for (quat_field& item : m_quats)
        {
            item.set_width(270).set_label_width(80);
            item.set_on_change(&data_controls_panel::quat_changed, this);
            const bool added = m_set.add(item);
            (void)added;
        }

        m_quats[0].set_label("Rotation").set_tooltip("Euler angles in degrees: pitch, yaw and roll. Click to type, or drag sideways on a letter");
        m_quats[0].set_euler(15.0, 45.0, 0.0);
        m_quats[1].set_label("Rotation 2");
        m_quats[1].set_quat(quaternion_from_euler(0.0f, 2.2f, 0.4f));
        m_quats[2].set_label("Read only").set_read_only(true);
        m_quats[2].set_euler(90.0, 0.0, 180.0);

        m_vec2[0].set_label("Offset").set_tooltip("Click a field to type, or drag sideways on its letter. Shift is faster and Ctrl finer");
        m_vec2[0].set(0.5, -1.25);
        m_vec2[1].set_label("Disabled");
        m_vec2[1].set(2.0, 3.0);
        m_vec2[1].set_enabled(false);

        m_vec3[0].set_label("Position").set_step(0.1);
        m_vec3[0].set(1.5, 0.0, -3.0);
        m_vec3[1].set_label("Scale").set_step(0.01).set_range(0.001, 1000.0);
        m_vec3[1].set(1.0, 1.0, 1.0);
        m_vec3[2].set_label("Colour").set_axes(vec_axes::rgba).set_step(0.01).set_range(0.0, 1.0).set_tooltip("The same fields with the letters of a colour");
        m_vec3[2].set(0.9, 0.6, 0.2);
        m_vec3[3].set_label("Read only").set_read_only(true);
        m_vec3[3].set(4.0, 5.0, 6.0);

        m_vec4[0].set_label("Rotation").set_step(0.01);
        m_vec4[0].set(0.0, 0.0, 0.0, 1.0);
        m_vec4[1].set_label("Tint").set_axes(vec_axes::rgba).set_step(0.01).set_range(0.0, 1.0);
        m_vec4[1].set(0.2, 0.5, 0.9, 1.0);
        m_vec4[2].set_label("Disabled");
        m_vec4[2].set(1.0, 2.0, 3.0, 4.0);
        m_vec4[2].set_enabled(false);
    }

    void data_controls_panel::quat_changed(void* user, quat_field& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const quaternion q = source.rotation();
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %.1f, %.1f, %.1f degrees = quat %.3f %.3f %.3f %.3f", source.name(), source.pitch(), source.yaw(),
            source.roll(), static_cast<double>(q.x), static_cast<double>(q.y), static_cast<double>(q.z), static_cast<double>(q.w));
        self.m_has_status = true;
    }

    void data_controls_panel::vec_changed(void* user, vec_field& source, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s = %.3f", source.name(), source.axis_name(index), source.value(index));
        self.m_has_status = true;
    }

    namespace
    {
        constexpr field_desc k_pose_fields[] = {
            make_field("translation", field_type::vec3, offsetof(demo_pose, translation)),
            make_field("rotation", field_type::quat, offsetof(demo_pose, rotation)),
            make_field("scale", field_type::vec3, offsetof(demo_pose, scale)),
        };

        constexpr field_desc k_light_fields[] = {
            make_field("color", field_type::color3, offsetof(demo_light, color)),
            make_field("intensity", field_type::f32, offsetof(demo_light, intensity)),
            make_field("enabled", field_type::boolean, offsetof(demo_light, enabled)),
        };

        constexpr field_desc k_camera_fields[] = {
            make_field("fov_y", field_type::f32, offsetof(demo_camera, fov_y)),
            make_field("near_plane", field_type::f32, offsetof(demo_camera, near_plane)),
        };

        struct group_desc
        {
            const char* name;
            const field_desc* fields;
            std::uint32_t count;
        };

        constexpr group_desc k_groups[] = {
            {"pose", k_pose_fields, 3},
            {"light", k_light_fields, 3},
            {"camera", k_camera_fields, 2},
        };
    }

    void data_controls_panel::fill_grids() noexcept
    {
        m_pose.translation[0] = 1.5f;
        m_pose.translation[2] = -3.0f;
        m_light.intensity = 2.5f;
        void* const rows[] = {&m_pose, &m_light, &m_camera};

        property_grid& editable = m_grids[0];
        editable.set_name("Inspector").set_rows(14).set_width(400).set_on_edit(&data_controls_panel::property_edited, this);
        editable.set_tooltip("Click a value to type it, or drag sideways on it. Shift is faster and Ctrl finer. Drag the line between the columns");

        // The second grid shows the same memory and cannot change it, so an edit above shows here as well.
        property_grid& watching = m_grids[1];
        watching.set_name("Read only").set_rows(14).set_width(400).set_read_only(true);

        for (std::uint32_t i = 0; i < 3; ++i)
        {
            (void)editable.add_group(k_groups[i].name, k_groups[i].fields, k_groups[i].count, rows[i]);
            (void)watching.add_group(k_groups[i].name, k_groups[i].fields, k_groups[i].count, rows[i]);
        }

        const bool added_editable = m_set.add(editable);
        const bool added_watching = m_set.add(watching);
        (void)added_editable;
        (void)added_watching;
    }

    void data_controls_panel::property_edited(void* user, property_grid& source, std::uint32_t group, std::uint32_t field, const void* value, bool final) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        source.apply(group, field, value);

        if (final)
        {
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s.%s edited", source.name(), k_groups[group].name, k_groups[group].fields[field].name);
            self.m_has_status = true;
        }
    }

    table_view& data_controls_panel::add_table(std::int32_t section) noexcept
    {
        table_view& item = m_tables[m_table_count < k_max_tables ? m_table_count : k_max_tables - 1];

        if (m_table_count < k_max_tables)
        {
            item.set_on_change(&data_controls_panel::table_changed, this);
            item.set_on_activate(&data_controls_panel::table_activated, this);
            item.set_on_sort(&data_controls_panel::table_sorted, this);
            item.set_drag_source(&data_controls_panel::table_drag, this);
            m_table_section[m_table_count++] = section;
            const bool added = m_set.add(item);
            (void)added;
        }

        return item;
    }

    char* data_controls_panel::next_cell() noexcept
    {
        return m_cells[m_cell_next++ % k_cell_ring];
    }

    namespace
    {
        constexpr std::uint32_t k_files = 240;
        constexpr std::uint32_t k_scores = 2000;

        // A number from a row that looks random and is always the same for it.
        [[nodiscard]] std::uint32_t scramble(std::uint32_t row, std::uint32_t salt) noexcept
        {
            std::uint32_t value = row * 2654435761u + salt * 40503u;
            value ^= value >> 15;
            value *= 2246822519u;
            value ^= value >> 13;
            return value;
        }

        const char* const k_kinds[] = {"Model", "Texture", "Sound", "Script", "Shader", "Level", "Font"};
        const char* const k_stems[] = {"hero", "crate", "forest", "ambient", "menu", "water", "terrain", "lamp", "door", "tower"};
    }

    const char* data_controls_panel::files_cell(void* user, std::uint32_t row, std::uint32_t column) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        char* text = self.next_cell();

        switch (column)
        {
        case 0:
            std::snprintf(text, k_cell_length, "%s_%03u", k_stems[scramble(row, 1) % std::size(k_stems)], row);
            break;

        case 1:
            std::snprintf(text, k_cell_length, "%u KB", 1 + scramble(row, 2) % 20000);
            break;

        case 2:
            std::snprintf(text, k_cell_length, "%s", k_kinds[scramble(row, 3) % std::size(k_kinds)]);
            break;

        default:
            std::snprintf(text, k_cell_length, "2026-%02u-%02u", 1 + scramble(row, 4) % 12, 1 + scramble(row, 5) % 28);
            break;
        }

        return text;
    }

    const char* data_controls_panel::scores_cell(void* user, std::uint32_t row, std::uint32_t column) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        char* text = self.next_cell();

        if (column == 0)
            std::snprintf(text, k_cell_length, "%u", row + 1);
        else if (column == 1)
            std::snprintf(text, k_cell_length, "Player %c%c", 'A' + static_cast<char>(scramble(row, 6) % 26), 'a' + static_cast<char>(scramble(row, 7) % 26));
        else
            std::snprintf(text, k_cell_length, "%u.%02u", scramble(row, 8) % 1000, scramble(row, 9) % 100);

        return text;
    }

    void data_controls_panel::fill_tables() noexcept
    {
        table_view& files = add_table(4).set_name("Files").set_rows(10).set_width(520);
        files.set_tooltip("Click a title to sort, drag the edge between two titles to resize");
        (void)files.add_column("Name", 170);
        (void)files.add_column("Size", 90, column_align::right);
        (void)files.add_column("Kind", 100);
        (void)files.add_column("Modified", 120);
        files.set_source(&data_controls_panel::files_cell, this, k_files);

        table_view& scores = add_table(4).set_name("Scores (multi)").set_rows(10).set_width(360).set_select_mode(list_select_mode::multi);
        scores.set_tooltip("2000 rows. Ctrl adds, Shift takes a range, Ctrl+A selects all");
        (void)scores.add_column("#", 60, column_align::right);
        (void)scores.add_column("Player", 160);
        (void)scores.add_column("Score", 100, column_align::right);
        scores.set_source(&data_controls_panel::scores_cell, this, k_scores);
        scores.sort_by(2, sort_direction::descending);

        table_view& empty = add_table(4).set_name("Empty").set_rows(4).set_width(300);
        (void)empty.add_column("Name", 160);
        (void)empty.add_column("Value", 100, column_align::right);

        table_view& disabled = add_table(4).set_name("Disabled").set_rows(4).set_width(300);
        (void)disabled.add_column("Name", 160);
        (void)disabled.add_column("Size", 100, column_align::right);
        disabled.set_source(&data_controls_panel::files_cell, this, 12);
        disabled.set_enabled(false);
    }

    void data_controls_panel::table_changed(void* user, table_view& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const std::int32_t first = source.first_selected();

        if (source.selected_count() == 0 || first < 0)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: nothing selected", source.name());
        else if (source.selected_count() == 1)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: selected row %d", source.name(), first);
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u rows selected, from row %d", source.name(), source.selected_count(), first);

        self.m_has_status = true;
    }

    void data_controls_panel::table_activated(void* user, table_view& source, std::uint32_t row) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: activated row %u", source.name(), row);
        self.m_has_status = true;
    }

    void data_controls_panel::table_sorted(void* user, table_view& source, std::uint32_t column, sort_direction direction) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const char* how = direction == sort_direction::ascending ? "ascending" : direction == sort_direction::descending ? "descending" : "in source order";
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: sorted by column %u, %s", source.name(), column, how);
        self.m_has_status = true;
    }

    tree_view& data_controls_panel::add_tree(std::int32_t section) noexcept
    {
        tree_view& item = m_trees[m_tree_count < k_max_trees ? m_tree_count : k_max_trees - 1];

        if (m_tree_count < k_max_trees)
        {
            item.set_on_change(&data_controls_panel::tree_changed, this);
            item.set_on_activate(&data_controls_panel::tree_activated, this);
            item.set_on_toggle(&data_controls_panel::tree_toggled, this);
            item.set_drag_source(&data_controls_panel::tree_drag, this);
            m_tree_section[m_tree_count++] = section;
            const bool added = m_set.add(item);
            (void)added;
        }

        return item;
    }

    void data_controls_panel::fill_trees() noexcept
    {
        using node = tree_view::node_id;

        // A scene, the way an outliner shows one.
        tree_view& scene = add_tree(3).set_name("Scene").set_rows(12).set_width(260);
        scene.set_tooltip("Click the chevron to open a row. Right opens, Left closes or goes to the parent");
        const node world = scene.add(tree_view::k_root, "World", true);
        scene.add(world, "Terrain");
        const node lights = scene.add(world, "Lights", true);
        scene.add(lights, "Sun");
        scene.add(lights, "Lamp 1");
        scene.add(lights, "Lamp 2");
        const node props = scene.add(world, "Props");
        scene.add(props, "Crate");
        scene.add(props, "Barrel");
        const node rocks = scene.add(props, "Rocks");
        scene.add(rocks, "Rock A");
        scene.add(rocks, "Rock B");
        const node characters = scene.add(world, "Characters", true);
        const node player = scene.add(characters, "Player", true);
        scene.add(player, "Weapon");
        scene.add(player, "Helmet");
        scene.add(characters, "Enemy 1");
        scene.add(tree_view::k_root, "Camera");
        scene.select(player);

        // Folders of many files, with several rows selected at a time.
        tree_view& files = add_tree(3).set_name("Files (multi)").set_rows(12).set_width(260).set_select_mode(list_select_mode::multi);
        files.set_tooltip("Ctrl adds, Shift takes a range of the rows that show, Ctrl+A selects all");
        static const char* const k_folders[] = {"models", "textures", "sounds", "scripts", "shaders", "levels", "fonts", "ui"};

        for (const char* folder : k_folders)
        {
            const node parent = files.add(tree_view::k_root, folder);

            for (std::uint32_t i = 0; i < 30 && m_tree_text_count < k_tree_names; ++i)
            {
                char* name = m_tree_text[m_tree_text_count++];
                std::snprintf(name, sizeof(m_tree_text[0]), "%s_%02u", folder, i);
                files.add(parent, name);
            }
        }

        add_tree(3).set_name("Empty").set_rows(6).set_width(200);
        tree_view& disabled = add_tree(3).set_name("Disabled").set_rows(6).set_width(200);
        disabled.set_enabled(false);
        const node root = disabled.add(tree_view::k_root, "Root", true);
        disabled.add(root, "Child");
        disabled.select(root);
    }

    void data_controls_panel::tree_changed(void* user, tree_view& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const tree_view::node_id first = source.first_selected();
        const char* text = source.text(first);

        if (source.selected_count() == 0 || text == nullptr)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: nothing selected", source.name());
        else if (source.selected_count() == 1)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: selected %s", source.name(), text);
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u rows selected, from %s", source.name(), source.selected_count(), text);

        self.m_has_status = true;
    }

    void data_controls_panel::tree_activated(void* user, tree_view& source, tree_view::node_id node) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: activated %s", source.name(), source.text(node));
        self.m_has_status = true;
    }

    void data_controls_panel::tree_toggled(void* user, tree_view& source, tree_view::node_id node) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s %s", source.name(), source.text(node), source.is_expanded(node) ? "opened" : "closed");
        self.m_has_status = true;
    }

    const char* data_controls_panel::long_row(void* user, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        char* name = self.m_names[self.m_name_next++ % k_name_ring];
        std::snprintf(name, k_name_length, "Asset %04u", index);
        return name;
    }

    void data_controls_panel::list_changed(void* user, list_view& source) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const std::int32_t first = source.first_selected();

        if (source.selected_count() == 0 || first < 0)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: nothing selected", source.name());
        else if (source.selected_count() == 1)
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: selected row %d", source.name(), first);
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %u rows selected, from row %d", source.name(), source.selected_count(), first);

        self.m_has_status = true;
    }

    void data_controls_panel::list_activated(void* user, list_view& source, std::uint32_t index) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const char* text = source.text(index);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: activated row %u (%s)", source.name(), index, text != nullptr ? text : "");
        self.m_has_status = true;
    }

    void data_controls_panel::bind(dock_host& host, panel_id id) noexcept
    {
        m_host = &host;
        m_id = id;
    }

    std::int32_t data_controls_panel::layout(void* dc, std::int32_t width) noexcept
    {
        m_root.set_padding(draw::px(k_margin)).set_gap(draw::px(k_section_gap));

        for (std::uint32_t s = 0; s < k_max_sections; ++s)
        {
            m_sections[s].set_gap(draw::px(k_gap));
            m_sections[s].set_padding(0, draw::px(k_heading_height) + draw::px(k_heading_to_controls), 0, 0);
        }

        (void)m_root.arrange(dc, {0, 0, width, 0});

        // The panes of the splitters go where the root put the splitters, the outer one first since the inner one is in it.
        m_pane_a1.set_padding(draw::px(8)).set_gap(draw::px(6));
        m_pane_a2.set_padding(0).set_gap(0);
        m_pane_b1.set_padding(draw::px(8)).set_gap(draw::px(6));
        m_pane_b2.set_padding(draw::px(8)).set_gap(draw::px(6));
        m_split_outer.arrange(dc);
        m_split_inner.arrange(dc);

        for (std::uint32_t s = 0; s < k_max_sections && m_heading[s] != nullptr; ++s)
            m_heading_y[s] = m_sections[s].area().y;

        return m_root.area().height;
    }

    void data_controls_panel::paint_content(void* user, void* dc, const view_range& band) noexcept
    {
        auto& self = *static_cast<data_controls_panel*>(user);
        const theme_colours& colours = theme();
        const std::int32_t margin = draw::px(k_margin);
        const std::int32_t content = std::max(self.m_width - 2 * margin, 0);
        const std::uint32_t rule = blend_colour(colours.panel, 0x000000, 0.3f);

        for (std::uint32_t s = 0; s < k_max_sections && self.m_heading[s] != nullptr; ++s)
        {
            if (!band.touches(self.m_heading_y[s], draw::px(k_heading_height) + 1))
                continue;

            draw::text(dc, {margin, self.m_heading_y[s], content, draw::px(k_heading_height)}, self.m_heading[s], colours.text_dim);
            draw::fill(dc, {margin, self.m_heading_y[s] + draw::px(k_heading_height), content, 1}, rule);
        }

        self.m_set.set_view(band);
        self.m_set.paint(dc);
    }

    void data_controls_panel::on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept
    {
        const std::int32_t footer_height = draw::px(k_footer_height);
        m_width = width;
        m_view.set_viewport({0, 0, width, std::max(height - footer_height, 0)});
        const std::int32_t content_height = layout(dc, width);
        m_view.set_content_height(content_height);

        // An open picker stays inside the content, so near the bottom it opens upward.
        const dock_rect bounds{0, 0, width, std::max(content_height, std::max(height - footer_height, 0))};

        m_bounds = bounds;

        for (color_swatch& item : m_swatches)
            item.set_bounds(bounds);

        m_stop_color.set_bounds(bounds);

        for (enum_field& item : m_enums)
            item.set_bounds(bounds);

        for (flags_field& item : m_flag_fields)
            item.set_bounds(bounds);

        for (breadcrumb_bar& item : m_crumbs)
            item.set_bounds(bounds);

        m_view.paint_content(dc, &data_controls_panel::paint_content, this);
        m_view.paint_bar(dc);

        const theme_colours& colours = theme();
        const std::int32_t margin = draw::px(k_margin);
        const std::int32_t footer = height - footer_height;
        draw::fill(dc, {0, footer, width, footer_height}, colours.panel);
        draw::fill(dc, {0, footer, width, 1}, blend_colour(colours.panel, 0x000000, 0.3f));
        draw::text(dc, {margin, footer + 1, std::max(width - 2 * margin, 0), footer_height - 1},
            m_has_status ? m_status : "Nothing chosen yet", colours.text_dim, draw::text_ellipsis);
    }

    bool data_controls_panel::on_frame(std::uint64_t now_ms) noexcept
    {
        const std::int32_t before = m_view.offset();
        const bool scrolling = m_view.animate(now_ms);
        const bool more = m_set.animate(now_ms);
        const bool controls = m_set.take_dirty();

        if (controls)
            m_view.invalidate_content();

        m_frame_dirty = controls || m_view.offset() != before;
        return more || scrolling;
    }

    bool data_controls_panel::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (m_host == nullptr || !m_set.on_key(key, ctrl, shift, alt))
            return false;

        if (const control* focus = m_set.focused())
            m_view.reveal(focus->area(), true);

        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);

        if (m_set.animating() || m_view.animating())
            m_host->request_frames(m_id);

        return true;
    }

    void data_controls_panel::on_char(std::uint32_t code) noexcept
    {
        if (m_host == nullptr || m_set.focused() == nullptr)
            return;

        m_set.on_char(code);
        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);
    }

    void data_controls_panel::on_focus(bool gained) noexcept
    {
        if (gained || m_host == nullptr)
            return;

        m_set.blur();
        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);
    }

    drop_effect data_controls_panel::on_drag_over(const drag_event& event) noexcept
    {
        if (m_host == nullptr)
            return drop_effect::none;

        drag_event inner = event;
        inner.y += m_view.offset();
        drop_effect effect = drop_effect::none;
        const bool over_content = m_view.contains(event.x, event.y);

        if (over_content)
            effect = m_set.drag_over(inner);
        else
            m_set.drag_leave();

        bool changed = m_set.take_dirty();

        // Held near the top or bottom of the panel, the panel scrolls, even over a box that takes the drag, or it could never get to the
        // boxes at its ends.
        const dock_rect& view = m_view.viewport();
        const std::int32_t edge = draw::px(28);

        if (over_content)
        {
            const std::int32_t before = m_view.offset();

            if (event.y < view.y + edge)
                m_view.scroll_by(-draw::px(16), false);
            else if (event.y >= view.y + view.height - edge)
                m_view.scroll_by(draw::px(16), false);

            changed = changed || m_view.offset() != before;
        }

        if (changed)
        {
            m_view.invalidate_content();
            m_host->invalidate_panel(m_id);
        }

        return effect;
    }

    void data_controls_panel::on_drag_leave() noexcept
    {
        m_set.drag_leave();
        (void)m_set.take_dirty();
        m_view.invalidate_content();

        if (m_host != nullptr)
            m_host->invalidate_panel(m_id);
    }

    drop_effect data_controls_panel::on_drop(const drag_event& event) noexcept
    {
        drag_event inner = event;
        inner.y += m_view.offset();
        const drop_effect effect = m_view.contains(event.x, event.y) ? m_set.drop(inner) : drop_effect::none;
        (void)m_set.take_dirty();
        m_view.invalidate_content();

        if (m_host != nullptr)
            m_host->invalidate_panel(m_id);

        return effect;
    }

    void data_controls_panel::show_menu(context_menu& menu, std::int32_t x, std::int32_t y) noexcept
    {
        popup_row rows[context_menu::k_max_items]{};
        const std::uint32_t count = menu.count();

        for (std::uint32_t i = 0; i < count; ++i)
        {
            const menu_item& item = *menu.item(i);
            rows[i] = {item.text(), item.shortcut(), item.separator(), item.enabled(), item.checked()};
        }

        // The menu has its own loop and does not return until it closes, so the mouse is told it left first.
        panel_mouse away;
        away.event = mouse_event::leave;
        (void)m_set.on_mouse(away);
        m_host->set_tooltip(m_id, nullptr, {});
        menu.choose(m_host->popup_menu(m_id, rows, count, x, y));
        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);
    }

    void data_controls_panel::on_mouse(const panel_mouse& mouse) noexcept
    {
        if (m_host == nullptr)
            return;

        panel_mouse inner = mouse;
        inner.y += m_view.offset();

        // The wheel over a list scrolls the list, and the panel when the list is at its end.
        if (mouse.event == mouse_event::wheel && m_view.contains(mouse.x, mouse.y) && m_set.on_wheel(inner))
        {
            m_view.invalidate_content();
            m_host->invalidate_panel(m_id);
            return;
        }

        bool repaint = m_view.on_mouse(mouse);

        if (mouse.event == mouse_event::wheel)
        {
            if (repaint)
                m_host->invalidate_panel(m_id);
        }
        else
        {
            // A control that is held, such as a slider or the pipette, keeps the mouse wherever it goes.
            const bool held = m_set.pressed() != nullptr;
            const bool inside = held || (m_view.contains(mouse.x, mouse.y) && !m_view.captures());
            const bool release = mouse.event == mouse_event::up || mouse.event == mouse_event::leave;

            if (inside || release)
            {
                if (m_set.on_mouse(inner))
                {
                    m_view.invalidate_content();
                    repaint = true;
                }

                if (context_menu* menu = m_set.take_context_menu())
                {
                    show_menu(*menu, mouse.x, mouse.y);
                    repaint = true;
                }
            }
            else if (mouse.event == mouse_event::move)
            {
                panel_mouse away = inner;
                away.event = mouse_event::leave;

                if (m_set.on_mouse(away))
                {
                    m_view.invalidate_content();
                    repaint = true;
                }
            }

            // A press or a drag may have changed the status line.
            if (repaint || mouse.event == mouse_event::down || mouse.event == mouse_event::double_click)
                m_host->invalidate_panel(m_id);
        }

        if (m_set.animating() || m_view.animating())
            m_host->request_frames(m_id);

        const control* hot = m_set.hovered();
        const control* owner = m_set.pressed() != nullptr ? m_set.pressed() : m_set.hovered();
        m_host->set_cursor(m_id, owner != nullptr && mouse.event != mouse_event::leave ? owner->cursor_at(inner.x, inner.y) : mouse_cursor::arrow);
        dock_rect where = hot != nullptr ? hot->tooltip_area() : dock_rect{};
        where.y -= m_view.offset();
        m_host->set_tooltip(m_id, hot != nullptr ? hot->tooltip() : nullptr, where);
    }
}
