#pragma once

#include <chrome/controls/core/button.hpp>
#include <chrome/controls/core/checkbox.hpp>
#include <chrome/controls/core/label.hpp>
#include <chrome/controls/core/scroll_view.hpp>
#include <chrome/controls/core/text_field.hpp>
#include <chrome/controls/core/number_field.hpp>
#include <chrome/controls/core/slider.hpp>
#include <chrome/controls/core/dropdown.hpp>
#include <chrome/controls/core/group.hpp>
#include <chrome/controls/core/separator.hpp>
#include <chrome/controls/core/spacer.hpp>
#include <chrome/controls/core/progress_bar.hpp>
#include <chrome/controls/core/spinner.hpp>
#include <chrome/controls/core/context_menu.hpp>
#include <chrome/controls/core/radio.hpp>
#include <chrome/controls/core/control.hpp>
#include <chrome/dock.hpp>
#include <chrome/layout.hpp>
#include <chrome/panel.hpp>

#include <cstdint>

namespace chrome
{
    class dock_host;

    // A drafting board for the editor's controls. Each kind of control is put here first, in its states, so it can be
    // judged in the theme before a real panel uses it. The controls themselves live in editor/ui/controls. This panel
    // only owns some of them, puts them in rows under headings, and shows what was clicked.
    class controls_panel final : public panel_content
    {
    public:
        // The most controls this demo panel holds, which sizes its tables.
        static constexpr std::uint32_t k_max_items = 512;

        controls_panel() noexcept;

        // The panel needs its host to ask for a repaint when something changes.
        void bind(dock_host& host, panel_id id) noexcept;

        void on_paint(void* dc, std::int32_t width, std::int32_t height) noexcept override;
        void on_mouse(const panel_mouse& mouse) noexcept override;
        [[nodiscard]] bool on_frame(std::uint64_t now_ms) noexcept override;
        [[nodiscard]] bool frame_dirty() const noexcept override { return m_frame_dirty; }
        [[nodiscard]] bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        static constexpr std::uint32_t k_max_sections = 19;

        static constexpr std::uint32_t k_max_buttons = 24;
        static constexpr std::uint32_t k_max_checks = 24;
        static constexpr std::uint32_t k_max_radios = 16;
        static constexpr std::uint32_t k_max_labels = 24;
        static constexpr std::uint32_t k_max_fields = 12;
        static constexpr std::uint32_t k_max_numbers = 12;
        static constexpr std::uint32_t k_max_sliders = 8;
        static constexpr std::uint32_t k_max_dropdowns = 8;
        static constexpr std::uint32_t k_max_separators = 8;
        static constexpr std::uint32_t k_max_spacers = 4;
        static constexpr std::uint32_t k_max_groups = 8;
        static constexpr std::uint32_t k_max_bars = 8;
        static constexpr std::uint32_t k_max_spinners = 8;
        static constexpr std::uint32_t k_max_menus = 4;

        // The next button or checkbox, in a section, already in the set. The caller sets how it looks.
        [[nodiscard]] button& add(std::int32_t section) noexcept;
        [[nodiscard]] checkbox& add_check(std::int32_t section) noexcept;
        // A radio that joins a group, in a section.
        [[nodiscard]] text_field& add_field(std::int32_t section) noexcept;
        [[nodiscard]] number_field& add_number(std::int32_t section) noexcept;
        [[nodiscard]] int_slider& add_int_slider(std::int32_t section) noexcept;
        [[nodiscard]] float_slider& add_float_slider(std::int32_t section) noexcept;
        [[nodiscard]] dropdown& add_dropdown(std::int32_t section) noexcept;
        [[nodiscard]] separator& add_separator(std::int32_t section) noexcept;
        [[nodiscard]] spacer& add_spacer(std::int32_t section) noexcept;
        [[nodiscard]] group& add_group(std::int32_t section) noexcept;
        [[nodiscard]] progress_bar& add_bar(std::int32_t section) noexcept;
        [[nodiscard]] spinner& add_spinner(std::int32_t section) noexcept;
        // A menu is only rows. Controls ask for it with a right click, and the panel shows it.
        [[nodiscard]] context_menu& add_menu() noexcept;
        void show_menu(context_menu& menu, std::int32_t x, std::int32_t y) noexcept;
        static void menu_chosen(void* user, context_menu& source, menu_item& item) noexcept;
        // Shows the controls of the open groups and hides those of the folded ones.
        void apply_groups() noexcept;
        static void group_toggled(void* user, group& source) noexcept;
        [[nodiscard]] label& add_label(std::int32_t section) noexcept;
        [[nodiscard]] radio& add_radio(std::int32_t section, radio_group& group) noexcept;
        void enrol(control& item, std::int32_t section) noexcept;
        // Groups what was added into layout containers: a column of sections, each a column of a wrap for the controls
        // before the first group and of one box per group, which is the group bar over a wrap of its controls.
        void build_layout() noexcept;
        // Sets the spacing for the dpi and the reveal of the groups, places everything with the content moved up by
        // offset, and returns the height of all of it.
        std::int32_t layout(void* dc, std::int32_t width, std::int32_t offset) noexcept;
        static void clicked(void* user, button& source) noexcept;
        static void changed(void* user, checkbox& source) noexcept;
        // The headings and controls, drawn into the scroll view's cache.
        static void paint_content(void* user, void* dc, const view_range& band) noexcept;
        static void picked(void* user, radio_group& source) noexcept;
        static void linked(void* user, label& source) noexcept;
        static bool field_acted(void* user, text_field& source, text_action action) noexcept;
        static void number_changed(void* user, number_field& source) noexcept;
        static void dropdown_changed(void* user, dropdown& source) noexcept;
        static void int_slid(void* user, int_slider& source) noexcept;
        static void float_slid(void* user, float_slider& source) noexcept;

        dock_host* m_host{nullptr};
        panel_id m_id{k_no_panel};
        button m_buttons[k_max_buttons]{};
        checkbox m_checks[k_max_checks]{};
        std::uint32_t m_button_count{0};
        std::uint32_t m_check_count{0};
        radio m_radios[k_max_radios]{};
        std::uint32_t m_radio_count{0};
        label m_labels[k_max_labels]{};
        text_field m_fields[k_max_fields]{};
        std::uint32_t m_field_count{0};
        number_field m_numbers[k_max_numbers]{};
        std::uint32_t m_number_count{0};
        int_slider m_int_sliders[k_max_sliders]{};
        float_slider m_float_sliders[k_max_sliders]{};
        dropdown m_dropdowns[k_max_dropdowns]{};
        std::uint32_t m_dropdown_count{0};
        separator m_separators[k_max_separators]{};
        spacer m_spacers[k_max_spacers]{};
        std::uint32_t m_separator_count{0};
        std::uint32_t m_spacer_count{0};
        group m_groups[k_max_groups]{};
        std::uint32_t m_group_count{0};
        progress_bar m_bars[k_max_bars]{};
        spinner m_spinners[k_max_spinners]{};
        std::uint32_t m_bar_count{0};
        std::uint32_t m_spinner_count{0};
        context_menu m_menus[k_max_menus]{};
        std::uint32_t m_menu_count{0};
        // The group a control in the set is the title of, or null, by the place of the control in the set.
        group* m_group_at[k_max_items]{};
        layout_container m_root;
        layout_container m_sections[k_max_sections];
        // The controls of a section that come before its first group.
        layout_container m_loose[k_max_sections];
        // A group: its bar over its body, which holds the controls that follow the bar.
        layout_container m_boxes[k_max_groups];
        layout_container m_bodies[k_max_groups];
        group* m_box_group[k_max_groups]{};
        std::int32_t m_box_section[k_max_groups]{};
        std::uint32_t m_box_count{0};
        std::uint32_t m_int_slider_count{0};
        std::uint32_t m_float_slider_count{0};
        bool m_frame_dirty{true};
        std::uint32_t m_label_count{0};
        radio_group m_quality;
        radio_group m_mode;
        control* m_items[k_max_items]{};
        std::int32_t m_section_of[k_max_items]{};
        std::uint32_t m_count{0};
        control_set m_set;
        scroll_view m_view;
        std::int32_t m_width{0};
        const char* m_heading[k_max_sections]{};
        std::int32_t m_heading_y[k_max_sections]{};
        std::uint32_t m_clicks{0};
        char m_status[96]{};
        bool m_has_status{false};
    };
}
