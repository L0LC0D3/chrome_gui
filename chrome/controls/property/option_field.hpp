#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // What enum_field and flags_field share: a box that shows the current value as text, and a list that opens over the
    // controls below it, as a dropdown's does. Use one of those two, not this.
    class option_field : public control
    {
    public:
        static constexpr std::uint32_t k_max_options = 32;
        // The rows the list shows at once. A longer list scrolls with the wheel and the keys.
        static constexpr std::uint32_t k_max_rows = 8;
        static constexpr std::int32_t k_none = -1;

        // Called after the user changed the value. Setting it from code does not call it.
        using change_fn = void (*)(void* user, option_field& source) noexcept;

        option_field& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // A caption to the left of the box. Optional. The width is at 96 dpi.
        option_field& set_label(const char* label) noexcept { m_label = label; return *this; }
        option_field& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        option_field& set_name(const char* name) noexcept { m_name = name; return *this; }
        option_field& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        option_field& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }
        // The area the list has to stay inside, in the coordinates of the controls. It opens upward when it does not fit
        // below. An empty area, the default, means it always opens downward.
        option_field& set_bounds(const dock_rect& bounds) noexcept { m_bounds = bounds; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        // The text in the box.
        [[nodiscard]] const char* text() const noexcept { return m_text; }
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        void close() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        // The list stays up while it closes. Opening and closing grow and shrink it from the box.
        [[nodiscard]] bool overlay_open() const noexcept override { return m_open || m_pos > 0.0f; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_pos != (m_open ? 1.0f : 0.0f); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        explicit option_field(bool flags) noexcept : m_flags(flags) {}

        // Adds a choice, or for flags a bit or a group of bits. false when the list is full.
        bool add_option(const char* name, std::uint32_t value) noexcept;
        void clear_options() noexcept;
        // The value as bits: the chosen value of an enum, or the bits that are set.
        [[nodiscard]] std::uint32_t bits() const noexcept { return m_value; }
        void set_bits(std::uint32_t value) noexcept;

        void on_click() noexcept override;
        void on_press(std::int32_t x, std::int32_t y) noexcept override { m_x = x; m_y = y; }
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        [[nodiscard]] dock_rect box() const noexcept;
        // The whole list, at its full height. overlay_area() is the part of it that shows while it opens or closes.
        [[nodiscard]] dock_rect full_area() const noexcept;
        void open() noexcept;
        // Flags have two rows before the options, which set none and all of the bits.
        [[nodiscard]] std::int32_t row_count() const noexcept { return static_cast<std::int32_t>(m_count) + (m_flags ? 2 : 0); }
        [[nodiscard]] const char* row_text(std::int32_t row) const noexcept;
        // A flags row that is ticked. For an enum, the row that is the value.
        [[nodiscard]] bool row_on(std::int32_t row) const noexcept;
        // Acts on a row: an enum takes it as the value and closes the list, flags flip it and the list stays.
        void act(std::int32_t row) noexcept;
        void set_from_user(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t all_bits() const noexcept;
        void rebuild_text() noexcept;
        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        void show_row(std::int32_t index) noexcept;
        bool point_at(std::int32_t x, std::int32_t y) noexcept;

        static constexpr std::uint32_t k_text_size = 192;

        const char* m_names[k_max_options]{};
        std::uint32_t m_values[k_max_options]{};
        std::uint32_t m_count{0};
        std::uint32_t m_value{0};
        bool m_flags{false};
        char m_text[k_text_size]{};
        const char* m_name{nullptr};
        const char* m_label{nullptr};
        std::int32_t m_label_width{90};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
        dock_rect m_bounds{};
        std::int32_t m_width{160};
        std::int32_t m_highlight{k_none};
        std::int32_t m_first{0};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_open{false};
        float m_pos{0.0f};
        std::uint64_t m_stamp{0};
    };

    // One choice out of a list of named values, such as a blend mode. Shown as a dropdown, with each name tied to the
    // number it stands for, so the value read back is the enum itself and not a row.
    //
    //   enum_field blend;
    //   blend.add("Opaque", 0);
    //   blend.add("Alpha", 1);
    //   blend.set_label("Blend").set_on_change(&changed, this);
    //   blend.set_value(1);
    class enum_field final : public option_field
    {
    public:
        enum_field() noexcept : option_field(false) {}

        // Names have to outlive the field. false when the list is full.
        bool add(const char* name, std::int32_t value) noexcept { return add_option(name, static_cast<std::uint32_t>(value)); }
        enum_field& clear() noexcept { clear_options(); return *this; }
        // A value that is not in the list shows as a number.
        enum_field& set_value(std::int32_t value) noexcept { set_bits(static_cast<std::uint32_t>(value)); return *this; }
        [[nodiscard]] std::int32_t value() const noexcept { return static_cast<std::int32_t>(bits()); }
        // A typed enum, for the ones whose values fit a 32 bit number.
        template <typename E>
        bool add(const char* name, E value) noexcept { return add(name, static_cast<std::int32_t>(value)); }
        template <typename E>
        [[nodiscard]] E get() const noexcept { return static_cast<E>(value()); }
    };

    // Any number of ticks out of a list of named bits, such as the layers a camera sees. The box lists what is ticked, or
    // says Nothing or Everything, and the list has a row for each of those two as well. A choice can be more than one bit,
    // and counts as ticked when all of them are set.
    //
    //   flags_field layers;
    //   layers.add("Default", 1u << 0);
    //   layers.add("UI", 1u << 5);
    //   layers.set_label("Culling");
    //   layers.set_flags(0x21);
    class flags_field final : public option_field
    {
    public:
        flags_field() noexcept : option_field(true) {}

        bool add(const char* name, std::uint32_t mask) noexcept { return add_option(name, mask); }
        flags_field& clear() noexcept { clear_options(); return *this; }
        flags_field& set_flags(std::uint32_t flags) noexcept { set_bits(flags); return *this; }
        [[nodiscard]] std::uint32_t flags() const noexcept { return bits(); }
        [[nodiscard]] bool has(std::uint32_t mask) const noexcept { return mask != 0 && (bits() & mask) == mask; }
    };
}
