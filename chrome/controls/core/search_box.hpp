#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/text_field.hpp>

#include <cstdint>

namespace chrome
{
    // A search field with filters. The first line is the box: a magnifier, the filter words that were put in it as small
    // removable pills, the text being typed, and a cross that clears all of it. Under it a row of chips, each of which is
    // a filter that is on or off, such as a kind of asset, which wraps onto more rows when the width is short.
    //
    // Enter turns the text into a pill (when that is on, as it is by default), so "rock" Enter "grass" Enter filters by
    // both. Backspace in an empty box takes the last pill away, and Escape clears the text, and leaves the box when there
    // is none. Ctrl with 1 to 9 flips the chip of that number. The handler hears every change, from the keyboard or the
    // mouse, and matches() says whether a name passes: every pill and the text are in it, in any case.
    //
    //   search_box search;
    //   search.set_placeholder("Search assets").set_width(360);
    //   search.add_chip("Meshes");
    //   search.add_chip("Textures");
    //   search.set_on_change(&filtered, this);
    //   set.add(search);
    //
    //   // in the handler:
    //   if (source.matches(name) && (!source.any_chip() || source.chip_active(kind_of(name)))) ...
    class search_box final : public control
    {
    public:
        static constexpr std::uint32_t k_max_chips = 16;
        static constexpr std::uint32_t k_max_pills = 6;
        // The most bytes of a pill, with the end.
        static constexpr std::uint32_t k_pill_length = 40;

        using change_fn = void (*)(void* user, search_box& source) noexcept;
        // Called for Enter, whether or not it made a pill.
        using submit_fn = void (*)(void* user, search_box& source) noexcept;

        search_box() noexcept;
        search_box(const search_box&) = delete;
        search_box& operator=(const search_box&) = delete;

        // The placeholder, the name and the tooltip are UTF-8 and have to outlive the box.
        search_box& set_placeholder(const char* text) noexcept;
        search_box& set_name(const char* name) noexcept { m_name = name; return *this; }
        search_box& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        // The width at 96 dpi. 0 fills the row.
        search_box& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // Whether Enter turns the text into a pill. On by default.
        search_box& set_pills(bool pills) noexcept { m_pills = pills; return *this; }
        // A slimmer input row, for a toolbar.
        search_box& set_compact(bool compact) noexcept { m_compact = compact; return *this; }
        search_box& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        search_box& set_on_submit(submit_fn handler, void* user) noexcept { m_submit = handler; m_submit_user = user; return *this; }

        // The text, as UTF-8. Setting it does not call the handler.
        search_box& set_text(const char* utf8) noexcept;
        std::int32_t get_text(char* out, std::int32_t capacity) const noexcept;
        // Takes the text and the pills away, and calls the handler if there was any. The chips stay as they are.
        void clear() noexcept;

        // A chip. The label has to outlive the box. Returns its number, or -1 when there are no more places.
        std::int32_t add_chip(const char* label) noexcept;
        search_box& set_chip_active(std::uint32_t index, bool active) noexcept;
        // A number shown after the label, such as how many things the chip would let through. -1 shows none.
        search_box& set_chip_count(std::uint32_t index, std::int32_t count) noexcept;
        // The colour of the chip when it is on, or 0 for the accent.
        search_box& set_chip_colour(std::uint32_t index, std::uint32_t colour) noexcept;
        [[nodiscard]] std::uint32_t chip_count() const noexcept { return m_chip_count; }
        [[nodiscard]] bool chip_active(std::uint32_t index) const noexcept { return index < m_chip_count && m_chips[index].active; }
        [[nodiscard]] const char* chip_label(std::uint32_t index) const noexcept { return index < m_chip_count ? m_chips[index].label : nullptr; }
        // At least one chip is on.
        [[nodiscard]] bool any_chip() const noexcept;

        [[nodiscard]] std::uint32_t pill_count() const noexcept { return m_pill_count; }
        [[nodiscard]] const char* pill(std::uint32_t index) const noexcept { return index < m_pill_count ? m_pills_text[index] : nullptr; }
        // There is text, a pill or a chip that is on.
        [[nodiscard]] bool has_filter() const noexcept;
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_placeholder; }
        // Every pill, and the text if there is any, is somewhere in the UTF-8 text, in any case (letters of ASCII only).
        [[nodiscard]] bool matches(const char* utf8) const noexcept;
        // The same for any text and word.
        [[nodiscard]] static bool contains_fold(const char* text, const char* word) noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool animating() const noexcept override { return m_inner.animating(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        struct chip
        {
            const char* label{nullptr};
            bool active{false};
            std::int32_t count{-1};
            std::uint32_t colour{0};
        };

        enum class target_kind : std::uint8_t
        {
            none,
            clear,
            pill_cross,
            chip,
            field
        };

        struct target
        {
            target_kind kind{target_kind::none};
            std::int32_t index{0};

            [[nodiscard]] bool operator==(const target& other) const noexcept { return kind == other.kind && index == other.index; }
        };

        [[nodiscard]] static bool field_acted(void* user, text_field& source, text_action action) noexcept;
        [[nodiscard]] std::int32_t chip_width(void* dc, std::uint32_t index) const noexcept;
        // Places the chips from a corner in a width, into out when it is given. Returns how many rows they take.
        [[nodiscard]] std::int32_t lay_out_chips(void* dc, std::int32_t width, std::int32_t x, std::int32_t y, dock_rect* out) const noexcept;
        [[nodiscard]] target target_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] panel_mouse mouse_at(mouse_event event, std::int32_t x, std::int32_t y) const noexcept;
        void place() const noexcept;
        [[nodiscard]] std::int32_t input_height() const noexcept;
        bool commit_pill() noexcept;
        void remove_pill(std::uint32_t index) noexcept;
        void toggle_chip(std::uint32_t index) noexcept;
        void notify_change() noexcept;
        [[nodiscard]] bool text_empty() const noexcept { return m_field.length() == 0; }

        mutable text_field m_field;
        mutable control_set m_inner;

        const char* m_placeholder{nullptr};
        const char* m_name{nullptr};
        std::int32_t m_width{360};
        bool m_pills{true};
        bool m_compact{false};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        submit_fn m_submit{nullptr};
        void* m_submit_user{nullptr};

        chip m_chips[k_max_chips]{};
        std::uint32_t m_chip_count{0};
        char m_pills_text[k_max_pills][k_pill_length]{};
        std::uint32_t m_pill_count{0};

        // What the last paint placed, for the mouse.
        mutable dock_rect m_chip_rects[k_max_chips]{};
        mutable dock_rect m_pill_rects[k_max_pills]{};
        mutable dock_rect m_clear_rect{};
        mutable dock_rect m_field_rect{};
        mutable dock_rect m_row_rect{};
        // The width left for the text after the pills, for deciding whether one more fits.
        mutable std::int32_t m_room{0};

        target m_hover{};
        bool m_drag_field{false};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
    };
}
