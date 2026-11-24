#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/number_field.hpp>

#include <cstdint>

namespace chrome
{
    // The letters on the fields of a vector, and their colours: red, green and blue for the first three, and a warm grey
    // for the fourth.
    enum class vec_axes : std::uint8_t
    {
        // X, Y, Z, W
        xyzw,
        // R, G, B, A
        rgba
    };

    // Two to four number fields in a row, for a vector, each behind the letter of its axis in the axis's colour. The label
    // before them is optional. Dragging sideways on a letter changes that component by one step for each pixel, Shift ten
    // times as much and Ctrl a tenth, and clicking a field types into it as in a number field. Tab goes from one field to
    // the next, and leaves the control after the last. The range, the step and the decimals are the same for all of them.
    //
    // This is the base of vec2_field, vec3_field and vec4_field, which are what a panel makes.
    //
    //   vec3_field position;
    //   position.set_label("Position").set_step(0.1).set_on_change(&moved, this);
    //   position.set(0.0, 1.0, 0.0);
    //   set.add(position);
    class vec_field : public control
    {
    public:
        static constexpr std::uint32_t k_max_components = 4;

        // Called after the user changed a component: by typing and committing, by a step, or by a drag on its letter. index
        // says which one. Setting a value from code does not call it.
        using change_fn = void (*)(void* user, vec_field& source, std::uint32_t index) noexcept;

        vec_field(const vec_field&) = delete;
        vec_field& operator=(const vec_field&) = delete;

        // A component's value, which is kept inside the range and rounded to the decimals. false for an index past the end.
        bool set_value(std::uint32_t index, double value) noexcept;
        [[nodiscard]] double value(std::uint32_t index) const noexcept;
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }

        vec_field& set_range(double minimum, double maximum) noexcept;
        vec_field& set_step(double step) noexcept;
        // How many digits follow the point, 0 to 9.
        vec_field& set_decimals(std::int32_t decimals) noexcept;
        vec_field& set_axes(vec_axes axes) noexcept { m_axes = axes; invalidate(); return *this; }
        // The step buttons at the right edge of each field. Off by default, as the fields are narrow.
        vec_field& set_buttons(bool buttons) noexcept;
        vec_field& set_read_only(bool read_only) noexcept;
        // A caption before the fields, UTF-8, which has to outlive the control. Optional.
        vec_field& set_label(const char* label) noexcept { m_label = label; return *this; }
        // The room for the caption at 96 dpi.
        vec_field& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The width of the fields together, without the label, at 96 dpi. 0 fills the row.
        vec_field& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // What it is called, such as in a status line. The label is used when this is not set.
        vec_field& set_name(const char* name) noexcept { m_name = name; return *this; }
        vec_field& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        vec_field& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        // The letter of a component with the axes in use, such as "X".
        [[nodiscard]] const char* axis_name(std::uint32_t index) const noexcept;
        // The colour of the letter of a component, as 0xRRGGBB.
        [[nodiscard]] static std::uint32_t axis_colour(std::uint32_t index) noexcept;
        // The number field of a component, for what the setters above do not cover. Null for an index past the end.
        [[nodiscard]] number_field* field(std::uint32_t index) noexcept { return index < m_count ? &m_fields[index] : nullptr; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] mouse_cursor cursor_at(std::int32_t x, std::int32_t y) const noexcept override;
        // The caret of the field being edited blinks.
        [[nodiscard]] bool animating() const noexcept override { return m_inner.animating(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        explicit vec_field(std::uint32_t components) noexcept;

        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            // A press inside a number field, which belongs to it.
            field,
            // A press on a letter, which changes the value as the mouse moves.
            scrub
        };

        // Puts the fields where the control is. Done before anything that needs the places, since the areas of the fields
        // follow the control's, which the panel sets without telling it.
        void place() const noexcept;
        // The strip with a component's letter, and the number field after it.
        [[nodiscard]] dock_rect letter_rect(std::uint32_t index) const noexcept;
        [[nodiscard]] dock_rect field_rect(std::uint32_t index) const noexcept;
        [[nodiscard]] std::int32_t letter_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t focused_index() const noexcept;
        void notify(std::uint32_t index) noexcept;
        static void field_changed(void* user, number_field& source) noexcept;

        // Placed by place(), which only moves them to the control's area.
        mutable number_field m_fields[k_max_components];
        mutable control_set m_inner;
        std::uint32_t m_count{3};
        vec_axes m_axes{vec_axes::xyzw};
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_label_width{90};
        std::int32_t m_width{240};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};

        drag_kind m_drag{drag_kind::none};
        // The letter being dragged or under the mouse, and where the drag began.
        std::int32_t m_scrub{-1};
        std::int32_t m_hover_letter{-1};
        std::int32_t m_press_x{0};
        double m_press_value{0.0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
    };

    class vec2_field final : public vec_field
    {
    public:
        vec2_field() noexcept : vec_field(2) {}

        vec2_field& set(double x, double y) noexcept;
        [[nodiscard]] double x() const noexcept { return value(0); }
        [[nodiscard]] double y() const noexcept { return value(1); }
    };

    class vec3_field final : public vec_field
    {
    public:
        vec3_field() noexcept : vec_field(3) {}

        vec3_field& set(double x, double y, double z) noexcept;
        [[nodiscard]] double x() const noexcept { return value(0); }
        [[nodiscard]] double y() const noexcept { return value(1); }
        [[nodiscard]] double z() const noexcept { return value(2); }
    };

    class vec4_field final : public vec_field
    {
    public:
        vec4_field() noexcept : vec_field(4) {}

        vec4_field& set(double x, double y, double z, double w) noexcept;
        [[nodiscard]] double x() const noexcept { return value(0); }
        [[nodiscard]] double y() const noexcept { return value(1); }
        [[nodiscard]] double z() const noexcept { return value(2); }
        [[nodiscard]] double w() const noexcept { return value(3); }
    };
}
