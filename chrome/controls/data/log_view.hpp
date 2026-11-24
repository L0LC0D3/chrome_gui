#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // What the log view asks for to draw a row. Every string has to stay valid until the next row is asked for, since the
    // view uses it at once and does not keep it.
    struct log_row
    {
        // 0 trace, 1 debug, 2 info, 3 warning, 4 error, 5 fatal.
        std::uint8_t severity{2};
        // The time, shown dimmed before the message. Null for none.
        const char* time{nullptr};
        // A short tag, such as the category, shown dimmed after the time. Null for none.
        const char* tag{nullptr};
        // The message, UTF-8, on one line. A long one is cut with "...".
        const char* text{nullptr};
        // How many times this message came, shown in a badge when it is more than one.
        std::uint32_t count{1};
    };

    // A list of log messages in a frame, made to follow a log that grows. Each row has a mark for its severity, a time, a tag
    // and the message, and rows of warnings and errors are tinted. Only the rows that show are asked for, so it holds
    // thousands at no more cost than ten.
    //
    // A click selects a row, Shift extends the selection to a range, and a drag sweeps over rows. The arrows, Page Up and
    // Page Down, Home and End move the selection (with Shift they extend it), Ctrl+A selects all and Ctrl+C copies the
    // selected rows as text. The wheel and a thin bar on the right scroll it.
    //
    // A view that follows keeps the last row in sight as rows come in. Scrolling up stops it, and scrolling back to the end
    // starts it again, which the follow handler hears.
    //
    //   log_view console;
    //   console.set_source(&row_of, this, count).set_follow(true).set_on_change(&picked, this);
    //   set.add(console);
    class log_view final : public control
    {
    public:
        static constexpr std::int32_t k_none = -1;

        using row_fn = void (*)(void* user, std::uint32_t index, log_row& out) noexcept;
        // Called after the user changed the selection. Choosing from code does not call it.
        using change_fn = void (*)(void* user, log_view& source) noexcept;
        // Called when the view started or stopped following, by scrolling.
        using follow_fn = void (*)(void* user, log_view& source) noexcept;

        // The rows come from a function, called for the rows on show only. The selection is cleared.
        log_view& set_source(row_fn source, void* user, std::uint32_t count) noexcept;
        // The log got longer or shorter. A view that follows goes to the end, and a selection past the new end is dropped.
        log_view& set_count(std::uint32_t count) noexcept;

        // How many rows the view is tall when its place is made by a layout, 1 to 64. A panel that sets the area itself
        // does not need it.
        log_view& set_rows(std::uint32_t rows) noexcept;
        log_view& set_show_time(bool show) noexcept { m_show_time = show; invalidate(); return *this; }
        log_view& set_show_tag(bool show) noexcept { m_show_tag = show; invalidate(); return *this; }
        log_view& set_follow(bool follow) noexcept;
        // Shown in the middle while there are no rows. Null for nothing.
        log_view& set_empty_text(const char* text) noexcept { m_empty = text; return *this; }
        log_view& set_name(const char* name) noexcept { m_name = name; return *this; }
        log_view& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        log_view& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        log_view& set_on_follow(follow_fn handler, void* user) noexcept { m_follow_handler = handler; m_follow_user = user; return *this; }

        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] bool follow() const noexcept { return m_follow; }
        [[nodiscard]] bool show_time() const noexcept { return m_show_time; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }

        // The rows from first to last, both included, are selected, and the last is brought into view. None of the
        // selection functions call the change handler.
        log_view& select(std::int32_t first, std::int32_t last) noexcept;
        log_view& select_all() noexcept;
        log_view& clear_selection() noexcept;
        // The ends of the selection, or k_none.
        [[nodiscard]] std::int32_t selection_first() const noexcept { return m_sel_lo; }
        [[nodiscard]] std::int32_t selection_last() const noexcept { return m_sel_hi; }
        [[nodiscard]] std::uint32_t selected_count() const noexcept { return m_sel_lo == k_none ? 0 : static_cast<std::uint32_t>(m_sel_hi - m_sel_lo + 1); }
        [[nodiscard]] bool is_selected(std::int32_t index) const noexcept { return m_sel_lo != k_none && index >= m_sel_lo && index <= m_sel_hi; }

        // The row under a point in the coordinates of the panel, or k_none.
        [[nodiscard]] std::int32_t row_at_point(std::int32_t x, std::int32_t y) const noexcept;
        // Scrolls the least that brings a row into view.
        void reveal(std::int32_t index) noexcept;
        void scroll_to_end() noexcept;
        // Puts the selected rows on the clipboard as lines of text. false when none is selected or it could not be done.
        bool copy_selection() const noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override { m_drag = drag_kind::none; }
        void on_leave() noexcept override { m_hover = k_none; invalidate(); }
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        enum class drag_kind : std::uint8_t
        {
            none,
            rows,
            bar
        };

        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        [[nodiscard]] dock_rect inner() const noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t max_first() const noexcept;
        [[nodiscard]] bool bar_needed() const noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;
        [[nodiscard]] thumb_span thumb() const noexcept;
        [[nodiscard]] std::int32_t row_near(std::int32_t y) const noexcept;
        void set_first(std::int32_t first) noexcept;
        // After the user scrolled: following means being at the end.
        void scrolled_by_user() noexcept;
        void set_follow_state(bool follow) noexcept;
        // Moves the current row, extending the selection from the anchor when asked.
        void move_current(std::int32_t row, bool extend) noexcept;
        void notify_change() noexcept;

        row_fn m_source{nullptr};
        void* m_source_user{nullptr};
        std::uint32_t m_count{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        follow_fn m_follow_handler{nullptr};
        void* m_follow_user{nullptr};
        const char* m_name{nullptr};
        const char* m_empty{"Nothing logged"};
        std::uint32_t m_rows{12};
        bool m_show_time{true};
        bool m_show_tag{true};
        bool m_follow{true};

        // The selected rows are one range. The anchor stays where the selection began, and the current row moves.
        std::int32_t m_sel_lo{k_none};
        std::int32_t m_sel_hi{k_none};
        std::int32_t m_anchor{k_none};
        std::int32_t m_current{k_none};
        mutable std::int32_t m_first{0};
        std::int32_t m_hover{k_none};
        drag_kind m_drag{drag_kind::none};
        std::int32_t m_grab{0};
    };
}
