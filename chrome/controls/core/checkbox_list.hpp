#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A column of checkboxes, with headings between groups of them and an optional number at the right of each, such as
    // how many things it would let through. It is for a filter that has many parts, like the categories of a log, which a
    // row of separate checkboxes would spread over too much room. A column break starts a new column beside the last, so
    // a long list can be wide instead of tall, and a link is a row that is only a word to click, such as Reset.
    //
    // A click on a row flips it. With the keyboard focus, Up and Down move between the boxes (a heading is passed over),
    // and Space or Enter flips the current one, Home and End go to the first and the last.
    //
    //   checkbox_list kinds;
    //   kinds.add_header("Levels");
    //   const std::int32_t trace = kinds.add_item("Trace", false);
    //   kinds.add_item("Debug", true);
    //   kinds.set_item_count(trace, 12);
    //   kinds.set_on_change(&flipped, this);
    //   popover.add(kinds);
    class checkbox_list final : public control
    {
    public:
        static constexpr std::uint32_t k_max_rows = 32;
        static constexpr std::int32_t k_none = -1;

        // Called after a click or a key flipped a box, with the number add_item gave it.
        using change_fn = void (*)(void* user, checkbox_list& source, std::uint32_t index) noexcept;

        // The labels have to outlive the list. Both return the row number, or k_none when there are no more places.
        std::int32_t add_header(const char* label) noexcept;
        std::int32_t add_item(const char* label, bool checked) noexcept;
        // A row that does something when clicked and has no box. The handler hears it with its row number.
        std::int32_t add_link(const char* label) noexcept;
        // The next row added starts a new column.
        checkbox_list& column_break() noexcept { m_break_next = true; return *this; }
        checkbox_list& set_checked(std::uint32_t index, bool checked) noexcept;
        // A number shown at the right of the row. -1 shows none.
        checkbox_list& set_item_count(std::uint32_t index, std::int32_t count) noexcept;
        // The width at 96 dpi. 0 fills the row.
        checkbox_list& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        checkbox_list& set_name(const char* name) noexcept { m_name = name; return *this; }
        checkbox_list& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] std::uint32_t row_count() const noexcept { return m_rows; }
        [[nodiscard]] bool checked(std::uint32_t index) const noexcept { return index < m_rows && m_row[index].kind == row_kind::item && m_row[index].checked; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override { m_hover = k_none; invalidate(); }
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        enum class row_kind : std::uint8_t
        {
            header,
            item,
            link
        };

        struct row
        {
            const char* label{nullptr};
            row_kind kind{row_kind::item};
            bool checked{false};
            bool break_before{false};
            std::int32_t count{-1};
        };

        [[nodiscard]] std::uint32_t columns() const noexcept;
        // Where a row is, in the coordinates of the panel, and the height of the tallest column.
        [[nodiscard]] dock_rect place(std::uint32_t index) const noexcept;
        [[nodiscard]] std::int32_t content_height() const noexcept;
        [[nodiscard]] std::int32_t add_row(const char* label, row_kind kind, bool checked) noexcept;

        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        // The next box, not a heading, from a row in a direction, or k_none.
        [[nodiscard]] std::int32_t step(std::int32_t from, std::int32_t direction) const noexcept;
        void flip(std::int32_t index) noexcept;

        row m_row[k_max_rows]{};
        std::uint32_t m_rows{0};
        std::int32_t m_width{0};
        const char* m_name{nullptr};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
        std::int32_t m_hover{k_none};
        std::int32_t m_current{k_none};
        bool m_break_next{false};
    };
}
