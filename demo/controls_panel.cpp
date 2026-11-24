#include <demo/controls_panel.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/dock_host.hpp>
#include <chrome/theme.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chrome
{
    controls_panel::controls_panel() noexcept
    {
        m_heading[0] = "Button";
        m_heading[1] = "Icon only";
        m_heading[2] = "Icon and label";
        m_heading[3] = "Toggle";

        add(0).set_label("Default").set_tooltip("A standard button");
        add(0).set_label("Primary").set_style(button_style::primary).set_tooltip("The main action of a panel");
        add(0).set_label("Flat").set_style(button_style::flat);
        add(0).set_label("Disabled").set_enabled(false);

        // An icon says little by itself, so these are the ones that need a tooltip.
        add(1).set_icon(icon_kind::plus).set_name("Add").set_tooltip("Add an item");
        add(1).set_icon(icon_kind::play).set_name("Play").set_style(button_style::primary).set_tooltip("Play the scene");
        add(1).set_icon(icon_kind::cross).set_name("Close").set_style(button_style::flat).set_tooltip("Close");
        add(1).set_icon(icon_kind::menu).set_name("Menu").set_style(button_style::flat).set_tooltip("More actions...\nA tooltip can have a second line.");
        add(1).set_icon(icon_kind::plus).set_name("Add (disabled)").set_enabled(false);

        add(2).set_icon(icon_kind::plus).set_label("Add");
        add(2).set_icon(icon_kind::play).set_label("Play").set_style(button_style::primary);
        add(2).set_icon(icon_kind::cross).set_label("Clear").set_style(button_style::flat);
        add(2).set_icon(icon_kind::plus).set_label("Disabled").set_enabled(false);

        add(3).set_icon(icon_kind::check).set_label("Snap").set_toggle(true);
        add(3).set_icon(icon_kind::check).set_label("Grid").set_toggle(true).set_checked(true);
        add(3).set_icon(icon_kind::menu).set_name("Menu toggle").set_style(button_style::flat).set_toggle(true);

        m_heading[4] = "Checkbox";
        m_heading[5] = "Switch";
        m_heading[7] = "Radio, label on the right";

        add_check(4).set_label("Unchecked");
        add_check(4).set_label("Checked").set_checked(true);
        add_check(4).set_label("Tri-state").set_tristate(true).set_state(check_state::mixed).set_tooltip("Click to go through off, on and mixed");
        add_check(4).set_label("Disabled").set_enabled(false);
        add_check(4).set_label("Disabled on").set_checked(true).set_enabled(false);
        add_check(4).set_name("No label").set_checked(true).set_tooltip("A checkbox without a label needs a tooltip");
        add_check(4).set_label("Label on the right").set_label_left(false);

        add_check(5).set_label("Off").set_style(checkbox_style::switch_pill);
        add_check(5).set_label("On").set_style(checkbox_style::switch_pill).set_checked(true);
        add_check(5).set_name("Switch, no label").set_style(checkbox_style::switch_pill).set_tooltip("Switch without a label");
        add_check(5).set_label("Disabled").set_style(checkbox_style::switch_pill).set_enabled(false);
        add_check(5).set_label("Disabled on").set_style(checkbox_style::switch_pill).set_checked(true).set_enabled(false);
        add_check(5).set_label("Label on the right").set_style(checkbox_style::switch_pill).set_label_left(false);

        m_heading[6] = "Radio";
        m_quality.set_name("Quality").set_on_change(&controls_panel::picked, this);
        m_mode.set_name("Mode").set_on_change(&controls_panel::picked, this);

        add_radio(6, m_quality).set_label("Low").set_value(0);
        add_radio(6, m_quality).set_label("Medium").set_value(1).set_tooltip("Radios in a group choose one");
        add_radio(6, m_quality).set_label("High").set_value(2);
        add_radio(6, m_quality).set_label("Disabled").set_value(3).set_enabled(false);
        m_quality.select_index(1);

        add_radio(7, m_mode).set_label("Edit").set_label_left(false);
        add_radio(7, m_mode).set_label("Play").set_label_left(false);
        add_radio(7, m_mode).set_label("Pause").set_label_left(false);
        add_radio(7, m_mode).set_name("Mode 4 (no label)").set_tooltip("A radio without a label needs a tooltip");
        m_mode.select_index(0);

        m_heading[8] = "Label";
        add_label(8).set_text("Body text");
        add_label(8).set_text("Dim text").set_style(label_style::dim);
        add_label(8).set_text("Heading").set_style(label_style::heading);
        add_label(8).set_text("A link").set_style(label_style::link).set_name("A link").set_tooltip("Links can be clicked");
        add_label(8).set_text("Success").set_style(label_style::success);
        add_label(8).set_text("Warning").set_style(label_style::warning);
        add_label(8).set_text("Error").set_style(label_style::error);
        add_label(8).set_text("Disabled").set_enabled(false);

        m_heading[9] = "Label, width and wrapping";
        add_label(9).set_text("Left").set_width(120).set_style(label_style::dim);
        add_label(9).set_text("Centre").set_width(120).set_align(label_align::center).set_style(label_style::dim);
        add_label(9).set_text("Right").set_width(120).set_align(label_align::right).set_style(label_style::dim);
        add_label(9).set_text("This text is longer than its width, so it ends with dots").set_width(140).set_tooltip("Cut to fit");
        add_label(9).set_text("A label that wraps takes a row of its own and breaks its lines at the width of the panel, so a paragraph of help text "
                              "stays readable however wide or narrow the panel is made.").set_wrap(true).set_style(label_style::dim);

        m_heading[10] = "Text field";
        add_field(10).set_placeholder("Name").set_width(200).set_tooltip("Type here. Enter submits and Escape goes back.");
        add_field(10).set_name("Prefilled").set_width(200).set_text("Hello, world");
        add_field(10).set_name("Password").set_placeholder("Password").set_width(200).set_password(true).set_text("secret");
        add_field(10).set_name("Read only").set_width(200).set_read_only(true).set_text("You can select this");
        add_field(10).set_name("Disabled").set_width(200).set_text("Disabled").set_enabled(false);
        add_field(10).set_name("Digits").set_placeholder("Digits only, 8 at most").set_width(200).set_max_length(8)
            .set_filter(
                [](void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept -> std::int32_t {
                    (void)user;
                    (void)source;
                    std::int32_t kept = 0;

                    for (std::int32_t i = 0; i < length; ++i)
                    {
                        if (text[i] >= L'0' && text[i] <= L'9')
                            text[kept++] = text[i];
                    }

                    return kept;
                },
                nullptr);
        add_field(10).set_name("No paste").set_placeholder("Cut and paste are refused").set_width(200);

        m_heading[11] = "Number field";
        add_number(11).set_label("Integer").set_range(0, 100).set_value(50).set_tooltip("0 to 100. Up and Down step, Shift by 10, Ctrl by a tenth.");
        add_number(11).set_label("Float").set_range(0, 1).set_step(0.05).set_decimals(2).set_value(0.5).set_tooltip("0 to 1 in steps of 0.05");
        add_number(11).set_label("Signed").set_range(-100, 100).set_step(0.5).set_decimals(1).set_value(-12.5);
        add_number(11).set_label("Read only").set_read_only(true).set_value(42);
        add_number(11).set_label("Disabled").set_value(7).set_enabled(false);

        m_heading[12] = "Slider";
        add_int_slider(12).set_label("Integer").set_range(0, 10).set_value(4).set_tooltip("Whole numbers, 0 to 10. Arrows, Home, End, Page Up and Down.");
        add_int_slider(12).set_label("Stepped").set_range(0, 100).set_step(10).set_value(30).set_tooltip("Stops every 10");
        add_float_slider(12).set_label("Float").set_range(0.0f, 1.0f).set_value(0.5f).set_tooltip("Any value from 0 to 1");
        add_float_slider(12).set_label("Float stepped").set_range(-1.0f, 1.0f).set_step(0.25f).set_value(0.25f).set_decimals(2);
        add_float_slider(12).set_label("No value").set_range(0.0f, 1.0f).set_value(0.7f).set_show_value(false).set_width(220);
        add_int_slider(12).set_label("Disabled").set_range(0, 10).set_value(6).set_enabled(false);

        m_heading[13] = "Dropdown";
        {
            static constexpr const char* k_sizes[] = {"Tiny", "Small", "Medium", "Large", "Huge"};
            dropdown& sizes = add_dropdown(13).set_label("Size").set_tooltip("Click to open the list. Up and Down change the choice.");

            for (const char* size : k_sizes)
                (void)sizes.add_item(size);

            sizes.select(2);

            add_dropdown(13).set_name("Empty").set_placeholder("Choose...").set_tooltip("Nothing chosen yet");

            static constexpr const char* k_formats[] = {"PNG", "JPEG", "TGA", "BMP", "DDS", "KTX2", "EXR", "HDR", "WebP", "GIF", "TIFF", "PSD",
                "ICO", "QOI"};
            dropdown& formats = add_dropdown(13).set_label("Format").set_width(200).set_tooltip("Scrolls with the wheel and the keys");

            for (const char* format : k_formats)
                (void)formats.add_item(format);

            formats.select(0);

            dropdown& disabled = add_dropdown(13).set_label("Disabled");
            (void)disabled.add_item("Disabled");
            disabled.select(0);
            disabled.set_enabled(false);
        }
        m_heading[14] = "Separator and spacing";
        add(14).set_label("Above");
        add(14).set_label("Plain").set_style(button_style::flat);
        add(14).set_label("Group").set_style(button_style::flat);
        (void)add_separator(14).set_vertical(true);
        add(14).set_label("Next group").set_style(button_style::flat);
        (void)add_separator(14).set_label("Left aligned");
        add_label(14).set_text("Content under a captioned separator").set_wrap(true).set_style(label_style::dim);
        (void)add_separator(14).set_label("Centred").set_align(separator_align::center);
        add_label(14).set_text("More content").set_wrap(true).set_style(label_style::dim);
        (void)add_separator(14).set_label("Right aligned").set_align(separator_align::right);
        (void)add_separator(14);
        (void)add_spacer(14).set_height(16);
        add_label(14).set_text("After a plain separator and a 16 px spacer").set_wrap(true).set_style(label_style::dim);

        m_heading[15] = "Group box and foldout";
        {
            group& transform = add_group(15).set_title("Transform").set_style(group_style::box).set_tooltip("Click to fold. Left and Right fold with the keyboard.");
            (void)transform;
            add_number(15).set_label("Position X").set_range(-100, 100).set_step(0.1).set_decimals(2).set_value(1.5);
            add_number(15).set_label("Position Y").set_range(-100, 100).set_step(0.1).set_decimals(2).set_value(0.0);
            add_number(15).set_label("Rotation").set_range(-180, 180).set_step(1).set_decimals(0).set_value(90);

            (void)add_group(15).set_title("Rendering").set_style(group_style::box).set_expanded(false);
            add_check(15).set_label("Cast shadows").set_checked(true);
            add_check(15).set_label("Receive shadows");
            add_label(15).set_text("This group starts folded.").set_wrap(true).set_style(label_style::dim);

            (void)add_group(15).set_title("Foldout header");
            add_label(15).set_text("A header with a line under it, no frame.").set_wrap(true).set_style(label_style::dim);
            add_check(15).set_label("Visible").set_checked(true);

            (void)add_group(15).set_title("Always open").set_style(group_style::box).set_collapsible(false);
            add_label(15).set_text("A group that cannot be folded works as a titled frame.").set_wrap(true).set_style(label_style::dim);
        }

        m_heading[16] = "Progress bar";
        add_bar(16).set_label("Empty").set_value(0.0f);
        add_bar(16).set_label("Loading").set_value(0.35f).set_tooltip("A bar that fills from the left");
        add_bar(16).set_label("Nearly").set_value(0.72f);
        add_bar(16).set_label("Done").set_value(1.0f);
        add_bar(16).set_label("Unknown").set_indeterminate(true).set_tooltip("Work with no known end");
        add_bar(16).set_name("No label").set_value(0.5f).set_width(220);
        add_bar(16).set_label("No percent").set_value(0.6f).set_show_percent(false);
        add_bar(16).set_label("Disabled").set_value(0.4f).set_enabled(false);

        m_heading[17] = "Spinner";
        add_spinner(17).set_name("Spinner").set_tooltip("Something is working");
        add_spinner(17).set_label("Compiling shaders...");
        add_spinner(17).set_label("Label before").set_label_left(true);
        add_spinner(17).set_label("Large").set_size(32);
        add_spinner(17).set_label("Stopped").set_spinning(false);
        add_spinner(17).set_label("Disabled").set_enabled(false);

        m_heading[18] = "Context menu";
        {
            context_menu& edit = add_menu();
            edit.add_item("Cut", 1).set_shortcut("Ctrl+X");
            edit.add_item("Copy", 2).set_shortcut("Ctrl+C");
            edit.add_item("Paste", 3).set_shortcut("Ctrl+V").set_enabled(false);
            edit.add_separator();
            edit.add_check("Word wrap", 4).set_checked(true);
            edit.add_check("Show whitespace", 5);
            edit.add_separator();
            edit.add_item("Add item", 6);
            edit.add_item("Delete", 7).set_shortcut("Del");

            context_menu& view = add_menu();
            view.add_item("Reset view", 11);
            view.add_item("Frame all", 12).set_shortcut("F");
            view.add_separator();
            view.add_item("Properties...", 13);

            button& target = add(18).set_label("Right-click me").set_tooltip("Opens a menu with cut, copy, check rows and icons");
            target.set_context_menu(&edit);
            button& flat = add(18).set_label("And me").set_style(button_style::flat).set_tooltip("A shorter menu");
            flat.set_context_menu(&view);
            label& note = add_label(18).set_text("Right-click this label for the same menu as the first button.").set_wrap(true).set_style(label_style::dim);
            note.set_context_menu(&edit);
        }

        build_layout();
        apply_groups();
    }

    context_menu& controls_panel::add_menu() noexcept
    {
        context_menu& item = m_menus[m_menu_count < k_max_menus ? m_menu_count : k_max_menus - 1];

        if (m_menu_count < k_max_menus)
        {
            item.set_on_select(&controls_panel::menu_chosen, this);
            ++m_menu_count;
        }

        return item;
    }

    void controls_panel::show_menu(context_menu& menu, std::int32_t x, std::int32_t y) noexcept
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

    void controls_panel::menu_chosen(void* user, context_menu& source, menu_item& item) noexcept
    {
        (void)source;
        auto& self = *static_cast<controls_panel*>(user);

        if (item.checkable())
            std::snprintf(self.m_status, sizeof(self.m_status), "Menu: %s %s  (%u)", item.text(), item.checked() ? "on" : "off", ++self.m_clicks);
        else
            std::snprintf(self.m_status, sizeof(self.m_status), "Menu: %s  (%u)", item.text(), ++self.m_clicks);

        self.m_has_status = true;
    }

    progress_bar& controls_panel::add_bar(std::int32_t section) noexcept
    {
        progress_bar& item = m_bars[m_bar_count < k_max_bars ? m_bar_count : k_max_bars - 1];

        if (m_bar_count < k_max_bars)
        {
            enrol(item, section);
            ++m_bar_count;
        }

        return item;
    }

    spinner& controls_panel::add_spinner(std::int32_t section) noexcept
    {
        spinner& item = m_spinners[m_spinner_count < k_max_spinners ? m_spinner_count : k_max_spinners - 1];

        if (m_spinner_count < k_max_spinners)
        {
            enrol(item, section);
            ++m_spinner_count;
        }

        return item;
    }

    group& controls_panel::add_group(std::int32_t section) noexcept
    {
        group& item = m_groups[m_group_count < k_max_groups ? m_group_count : k_max_groups - 1];

        if (m_group_count < k_max_groups)
        {
            item.set_on_toggle(&controls_panel::group_toggled, this);
            const std::uint32_t at = m_count;
            enrol(item, section);

            if (m_count > at)
                m_group_at[at] = &item;

            ++m_group_count;
        }

        return item;
    }

    void controls_panel::apply_groups() noexcept
    {
        // A group that is folding or unfolding still shows its controls, as far as it has got.
        bool folded = false;
        std::int32_t section = -1;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_section_of[i] != section)
            {
                section = m_section_of[i];
                folded = false;
            }

            if (m_group_at[i] != nullptr)
            {
                folded = m_group_at[i]->reveal() <= 0.0f && !m_group_at[i]->expanded();
                continue;
            }

            m_items[i]->set_visible(!folded);
        }

        // The control with the focus may have been folded away.
        if (const control* focus = m_set.focused(); focus != nullptr && !focus->visible())
            m_set.blur();
    }

    void controls_panel::group_toggled(void* user, group& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        self.apply_groups();
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s  (%u)", source.title(), source.expanded() ? "open" : "folded", ++self.m_clicks);
        self.m_has_status = true;
    }

    separator& controls_panel::add_separator(std::int32_t section) noexcept
    {
        separator& item = m_separators[m_separator_count < k_max_separators ? m_separator_count : k_max_separators - 1];

        if (m_separator_count < k_max_separators)
        {
            enrol(item, section);
            ++m_separator_count;
        }

        return item;
    }

    spacer& controls_panel::add_spacer(std::int32_t section) noexcept
    {
        spacer& item = m_spacers[m_spacer_count < k_max_spacers ? m_spacer_count : k_max_spacers - 1];

        if (m_spacer_count < k_max_spacers)
        {
            enrol(item, section);
            ++m_spacer_count;
        }

        return item;
    }

    dropdown& controls_panel::add_dropdown(std::int32_t section) noexcept
    {
        dropdown& item = m_dropdowns[m_dropdown_count < k_max_dropdowns ? m_dropdown_count : k_max_dropdowns - 1];

        if (m_dropdown_count < k_max_dropdowns)
        {
            item.set_on_change(&controls_panel::dropdown_changed, this);
            enrol(item, section);
            ++m_dropdown_count;
        }

        return item;
    }

    void controls_panel::dropdown_changed(void* user, dropdown& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s  (%u)", source.name(), source.selected_text(), ++self.m_clicks);
        self.m_has_status = true;
    }

    int_slider& controls_panel::add_int_slider(std::int32_t section) noexcept
    {
        int_slider& item = m_int_sliders[m_int_slider_count < k_max_sliders ? m_int_slider_count : k_max_sliders - 1];

        if (m_int_slider_count < k_max_sliders)
        {
            item.set_on_change(&controls_panel::int_slid, this);
            enrol(item, section);
            ++m_int_slider_count;
        }

        return item;
    }

    float_slider& controls_panel::add_float_slider(std::int32_t section) noexcept
    {
        float_slider& item = m_float_sliders[m_float_slider_count < k_max_sliders ? m_float_slider_count : k_max_sliders - 1];

        if (m_float_slider_count < k_max_sliders)
        {
            item.set_on_change(&controls_panel::float_slid, this);
            enrol(item, section);
            ++m_float_slider_count;
        }

        return item;
    }

    void controls_panel::int_slid(void* user, int_slider& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %d  (%u)", source.name(), source.value(), ++self.m_clicks);
        self.m_has_status = true;
    }

    void controls_panel::float_slid(void* user, float_slider& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %.*f  (%u)", source.name(), source.decimals(), static_cast<double>(source.value()), ++self.m_clicks);
        self.m_has_status = true;
    }

    number_field& controls_panel::add_number(std::int32_t section) noexcept
    {
        number_field& item = m_numbers[m_number_count < k_max_numbers ? m_number_count : k_max_numbers - 1];

        if (m_number_count < k_max_numbers)
        {
            item.set_on_change(&controls_panel::number_changed, this);
            enrol(item, section);
            ++m_number_count;
        }

        return item;
    }

    void controls_panel::number_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %.*f  (%u)", source.name(), source.decimals(), source.value(), ++self.m_clicks);
        self.m_has_status = true;
    }

    void controls_panel::enrol(control& item, std::int32_t section) noexcept
    {
        if (m_count < k_max_items && m_set.add(item))
        {
            m_items[m_count] = &item;
            m_section_of[m_count] = section;
            ++m_count;
        }
    }

    button& controls_panel::add(std::int32_t section) noexcept
    {
        // Past the end the last one is handed out again, so a caller never gets a bad reference.
        button& item = m_buttons[m_button_count < k_max_buttons ? m_button_count : k_max_buttons - 1];

        if (m_button_count < k_max_buttons)
        {
            item.set_on_click(&controls_panel::clicked, this);
            enrol(item, section);
            ++m_button_count;
        }

        return item;
    }

    checkbox& controls_panel::add_check(std::int32_t section) noexcept
    {
        checkbox& item = m_checks[m_check_count < k_max_checks ? m_check_count : k_max_checks - 1];

        if (m_check_count < k_max_checks)
        {
            item.set_on_change(&controls_panel::changed, this);
            enrol(item, section);
            ++m_check_count;
        }

        return item;
    }

    radio& controls_panel::add_radio(std::int32_t section, radio_group& group) noexcept
    {
        radio& item = m_radios[m_radio_count < k_max_radios ? m_radio_count : k_max_radios - 1];

        if (m_radio_count < k_max_radios)
        {
            (void)group.add(item);
            enrol(item, section);
            ++m_radio_count;
        }

        return item;
    }

    text_field& controls_panel::add_field(std::int32_t section) noexcept
    {
        text_field& item = m_fields[m_field_count < k_max_fields ? m_field_count : k_max_fields - 1];

        if (m_field_count < k_max_fields)
        {
            item.set_on_action(&controls_panel::field_acted, this);
            enrol(item, section);
            ++m_field_count;
        }

        return item;
    }

    bool controls_panel::field_acted(void* user, text_field& source, text_action action) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);

        // One of the fields turns down cut and paste, to show that the handler can.
        const bool refuse = std::strcmp(source.name(), "No paste") == 0 &&
            (action == text_action::pasted || action == text_action::cut);

        // Moving the caret is too chatty for the status line.
        if (action != text_action::selection_changed)
        {
            char text[64]{};
            (void)source.get_text(text, sizeof(text));
            std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s%s \"%s\"  (%u)", source.name(), text_action_name(action),
                refuse ? " refused" : "", source.password() ? "***" : text, ++self.m_clicks);
            self.m_has_status = true;
        }

        return !refuse;
    }

    label& controls_panel::add_label(std::int32_t section) noexcept
    {
        label& item = m_labels[m_label_count < k_max_labels ? m_label_count : k_max_labels - 1];

        if (m_label_count < k_max_labels)
        {
            item.set_on_click(&controls_panel::linked, this);
            enrol(item, section);
            ++m_label_count;
        }

        return item;
    }

    void controls_panel::linked(void* user, label& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "Link: %s  (%u)", source.name(), ++self.m_clicks);
        self.m_has_status = true;
    }

    void controls_panel::picked(void* user, radio_group& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        const radio* choice = source.selected();
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s  (%u)", source.name(), choice != nullptr ? choice->name() : "none", ++self.m_clicks);
        self.m_has_status = true;
    }

    void controls_panel::changed(void* user, checkbox& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        const char* state = source.state() == check_state::checked ? "on" : source.state() == check_state::mixed ? "mixed" : "off";
        std::snprintf(self.m_status, sizeof(self.m_status), "%s: %s  (%u)", source.name(), state, ++self.m_clicks);
        self.m_has_status = true;
    }

    void controls_panel::bind(dock_host& host, panel_id id) noexcept
    {
        m_host = &host;
        m_id = id;

        // A spinner or an indeterminate bar runs from the start, without waiting for the mouse.
        if (m_set.animating())
            m_host->request_frames(m_id);
    }

    void controls_panel::clicked(void* user, button& source) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        std::snprintf(self.m_status, sizeof(self.m_status), "Clicked: %s  (%u)", source.name(), ++self.m_clicks);
        self.m_has_status = true;
    }

    namespace
    {
        // The spacing of the panel, at 96 dpi. Everything sits on these.
        constexpr std::int32_t k_margin = 16;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_section_gap = 22;
        constexpr std::int32_t k_heading_height = 20;
        constexpr std::int32_t k_heading_to_controls = 10;
        constexpr std::int32_t k_footer_height = 28;
        // The room inside a box around its controls, and between a group bar and what is under it.
        constexpr std::int32_t k_box_inset = 8;
        constexpr std::int32_t k_group_gap = 6;
    }

    void controls_panel::build_layout() noexcept
    {
        m_root.set_kind(layout_kind::column);
        std::int32_t section = -1;
        layout_container* target = nullptr;
        bool loose_added = false;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            const std::int32_t s = m_section_of[i];

            if (s != section)
            {
                section = s;
                m_sections[s].set_kind(layout_kind::column);
                m_loose[s].set_kind(layout_kind::wrap);
                m_root.add(m_sections[s]);
                target = &m_loose[s];
                loose_added = false;
            }

            if (group* head = m_group_at[i]; head != nullptr && m_box_count < k_max_groups)
            {
                layout_container& box = m_boxes[m_box_count];
                layout_container& body = m_bodies[m_box_count];
                box.set_kind(layout_kind::column);
                body.set_kind(layout_kind::wrap);
                box.add(*head);
                box.add(body);
                m_sections[s].add(box);
                m_box_section[m_box_count] = s;
                m_box_group[m_box_count++] = head;
                target = &body;
                continue;
            }

            if (target == &m_loose[s] && !loose_added)
            {
                m_sections[s].add(m_loose[s]);
                loose_added = true;
            }

            target->add(*m_items[i]);
        }
    }

    // The controls sit in rows under their headings, wrapping at the width of the panel. A group takes a row of its own,
    // and the controls after it are left out while it is folded. Those of a box are drawn inside a frame, and are indented.
    // While a group folds or unfolds its controls keep their places and are clipped, and what is under the group moves
    // up or down with the part that shows.
    std::int32_t controls_panel::layout(void* dc, std::int32_t width, std::int32_t offset) noexcept
    {
        const std::int32_t margin = draw::px(k_margin);
        const std::int32_t box_inset = draw::px(k_box_inset);
        const std::int32_t group_gap = draw::px(k_group_gap);

        m_root.set_padding(margin).set_gap(draw::px(k_section_gap));

        // Checkboxes carry their labels, so they need more room between them than buttons do.
        const auto gap_of = [](std::int32_t section) { return draw::px(section >= 4 ? k_gap * 3 : k_gap); };
        std::int32_t section = -1;

        for (std::uint32_t i = 0; i < m_count; ++i)
        {
            if (m_section_of[i] == section)
                continue;

            section = m_section_of[i];
            const std::int32_t gap = gap_of(section);
            m_sections[section].set_gap(gap).set_padding(0, draw::px(k_heading_height) + draw::px(k_heading_to_controls), 0, 0);
            m_loose[section].set_gap(gap);
        }

        for (std::uint32_t g = 0; g < m_box_count; ++g)
        {
            const group& head = *m_box_group[g];
            const bool framed = head.style() == group_style::box;
            m_bodies[g].set_gap(gap_of(m_box_section[g]));
            m_bodies[g].set_padding(framed ? box_inset : 0, group_gap, framed ? box_inset : 0, framed ? box_inset : 0);
            m_bodies[g].set_reveal(head.reveal());
        }

        (void)m_root.arrange(dc, {0, -offset, width, 0});

        for (std::uint32_t s = 0; s < k_max_sections && m_heading[s] != nullptr; ++s)
            m_heading_y[s] = m_sections[s].area().y;

        return m_root.area().height;
    }

    void controls_panel::paint_content(void* user, void* dc, const view_range& band) noexcept
    {
        auto& self = *static_cast<controls_panel*>(user);
        const theme_colours& colours = theme();
        const std::int32_t margin = draw::px(k_margin);
        const std::int32_t content = std::max(self.m_width - 2 * margin, 0);
        const std::uint32_t rule = blend_colour(colours.panel, 0x000000, 0.3f);

        // The headings, each with a rule under it as wide as the content.
        for (std::uint32_t s = 0; s < k_max_sections && self.m_heading[s] != nullptr; ++s)
        {
            if (!band.touches(self.m_heading_y[s], draw::px(k_heading_height) + 1))
                continue;

            draw::text(dc, {margin, self.m_heading_y[s], content, draw::px(k_heading_height)}, self.m_heading[s], colours.text_dim);
            draw::fill(dc, {margin, self.m_heading_y[s] + draw::px(k_heading_height), content, 1}, rule);
        }

        // The frames of the boxes go behind the controls, whose bars are drawn over their tops.
        const std::uint32_t frame_face = blend_colour(colours.panel, 0x000000, 0.08f);
        const std::uint32_t frame_edge = blend_colour(colours.title, 0x000000, 0.5f);

        for (std::uint32_t g = 0; g < self.m_box_count; ++g)
        {
            const dock_rect& frame = self.m_boxes[g].area();

            if (self.m_box_group[g]->style() == group_style::box && band.touches(frame.y, frame.height))
                draw::rounded(dc, self.m_boxes[g].area(), 4, frame_face, frame_edge);
        }

        // Only the controls in the band are drawn or animated, until the scroll leaves it.
        self.m_set.set_view(band);
        self.m_set.paint(dc);
    }

    void controls_panel::on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept
    {
        const std::int32_t footer_height = draw::px(k_footer_height);
        m_width = width;
        m_view.set_viewport({0, 0, width, std::max(height - footer_height, 0)});
        // Everything is placed with the top of the content at 0. Scrolling moves a picture of it, not the controls.
        const std::int32_t content_height = layout(dc, width, 0);
        m_view.set_content_height(content_height);

        // An open list stays inside the content, so near the bottom it opens upward.
        const dock_rect bounds{0, 0, width, std::max(content_height, std::max(height - footer_height, 0))};

        for (std::uint32_t i = 0; i < m_dropdown_count; ++i)
            m_dropdowns[i].set_bounds(bounds);

        m_view.paint_content(dc, &controls_panel::paint_content, this);
        m_view.paint_bar(dc);

        // The footer keeps to the bottom of the panel, under a rule, and says what was clicked last.
        const theme_colours& colours = theme();
        const std::int32_t margin = draw::px(k_margin);
        const std::int32_t content = std::max(width - 2 * margin, 0);
        const std::uint32_t rule = blend_colour(colours.panel, 0x000000, 0.3f);
        const char* status = m_has_status ? m_status : "Nothing clicked yet";
        const std::int32_t footer = height - footer_height;
        draw::fill(dc, {0, footer, width, footer_height}, colours.panel);
        draw::fill(dc, {0, footer, width, 1}, rule);
        draw::text(dc, {margin, footer + 1, content, footer_height - 1}, status, colours.text_dim);
    }

    bool controls_panel::on_frame(std::uint64_t now_ms) noexcept
    {
        const std::int32_t before = m_view.offset();
        const bool scrolling = m_view.animate(now_ms);
        const bool more = m_set.animate(now_ms);

        // A group that has folded all the way hides its controls, and one that opens shows them.
        apply_groups();

        // A switch that slid, a dot that grew or a caret that blinked changed the picture of the content. Scrolling only
        // moves it. A frame where neither happened is not painted.
        const bool controls = m_set.take_dirty();

        if (controls)
            m_view.invalidate_content();

        m_frame_dirty = controls || m_view.offset() != before;
        return more || scrolling;
    }

    bool controls_panel::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (m_host == nullptr || !m_set.on_key(key, ctrl, shift, alt))
            return false;

        // Tab may have moved the focus to a field that is scrolled out of sight.
        if (const control* field = m_set.focused())
            m_view.reveal(field->area(), true);

        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);

        if (m_set.animating() || m_view.animating())
            m_host->request_frames(m_id);

        return true;
    }

    void controls_panel::on_char(std::uint32_t code) noexcept
    {
        if (m_host == nullptr || m_set.focused() == nullptr)
            return;

        m_set.on_char(code);
        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);
    }

    void controls_panel::on_focus(bool gained) noexcept
    {
        // The window lost the keyboard, so a field in it stops taking text.
        if (gained || m_host == nullptr)
            return;

        m_set.blur();
        m_view.invalidate_content();
        m_host->invalidate_panel(m_id);
    }

    void controls_panel::on_mouse(const panel_mouse& mouse) noexcept
    {
        // A click changes the status line as well, so every event that did something repaints the panel.
        if (m_host == nullptr)
            return;

        // The controls are placed at the top of the content, so the mouse is moved down by what is scrolled off.
        panel_mouse inner = mouse;
        inner.y += m_view.offset();

        // The wheel over an open list scrolls the list and not the panel.
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
            // The content only sees the mouse inside the viewport and off the bar. Elsewhere it is told the mouse left, so
            // nothing that is scrolled out of sight can be hit.
            const bool inside = m_view.contains(mouse.x, mouse.y) && !m_view.captures();
            const bool release = mouse.event == mouse_event::up || mouse.event == mouse_event::leave;

            if (inside || release)
            {
                if (m_set.on_mouse(inner))
                {
                    m_view.invalidate_content();
                    repaint = true;
                }

                if (context_menu* menu = m_set.take_context_menu())
                    show_menu(*menu, mouse.x, mouse.y);
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

            if (repaint)
                m_host->invalidate_panel(m_id);
        }

        if (m_set.animating() || m_view.animating())
            m_host->request_frames(m_id);

        // The control under the mouse says what its tooltip is. A control without one hides the previous.
        const control* hot = m_set.hovered();

        // The pointer follows the control that is held, else the one under the mouse, and is the plain arrow off the controls.
        const control* owner = m_set.pressed() != nullptr ? m_set.pressed() : m_set.hovered();
        m_host->set_cursor(m_id, owner != nullptr && mouse.event != mouse_event::leave ? owner->cursor_at(inner.x, inner.y) : mouse_cursor::arrow);
        dock_rect where = hot != nullptr ? hot->area() : dock_rect{};
        where.y -= m_view.offset();
        m_host->set_tooltip(m_id, hot != nullptr ? hot->tooltip() : nullptr, where);
    }
}
