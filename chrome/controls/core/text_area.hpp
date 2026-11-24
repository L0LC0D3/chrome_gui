#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/text_field.hpp>

#include <cstdint>

namespace chrome
{
    // What a run of code is, for the colours of a highlighter. 0 is plain text.
    enum class syntax_class : std::uint8_t
    {
        plain,
        comment,
        keyword,
        type,
        intrinsic,
        number,
        string,
        directive,
        semantic,
        function,
        key,
        count
    };

    // The words a completer offers. Words already in the list are not added twice.
    struct completion_list
    {
        static constexpr std::int32_t k_max = 512;
        static constexpr std::int32_t k_text = 40;

        char text[k_max][k_text]{};
        syntax_class kind[k_max]{};
        std::int32_t count{0};

        // false when the list is full or the word is empty, too long, or there already.
        bool add(const char* word, syntax_class word_kind) noexcept;
    };

    // Several lines of editable text in a frame, with the same keys as a text field and the ones lines need: Enter breaks a
    // line, Up and Down move between lines (keeping the column they started from), Home and End go to the ends of a line
    // and with Ctrl to the ends of the text, and Page Up and Page Down move a page. Long lines are wrapped at the width, or
    // scroll sideways when wrapping is off. A thin bar on the right and the wheel scroll it, and a gutter on the left can
    // number the lines. A double click selects a word and a third click a whole line.
    //
    // Ctrl+Enter is "submitted", and Escape leaves the area. Tab leaves it too, unless Tab is set to put spaces in. The
    // actions are the text field's, so a handler can be shared. The text is UTF-8 and holds up to k_capacity characters.
    //
    //   text_area notes;
    //   notes.set_placeholder("Notes").set_rows(6).set_width(320).set_line_numbers(true);
    //   notes.set_on_action(&acted, this);
    //   set.add(notes);
    //
    // Code. A highlighter gives every character a syntax_class and the area paints it in the colour of the class (the colours
    // follow the theme, light or dark). A completer lists the words that fit the place the caret is at: a list opens under
    // the word being typed once it has two letters, or on Ctrl+Space, narrows as typing goes on, Up and Down move in it, and
    // Enter or Tab (or a click) take the word. Escape shuts it.
    class text_area final : public control
    {
    public:
        static constexpr std::int32_t k_capacity = 32768;
        static constexpr std::int32_t k_history = 8;
        // Every line of a text fits however it is broken: a break takes at least one character.
        static constexpr std::int32_t k_max_lines = k_capacity + 2;

        // Every action goes here. For the "before" actions a false stops the action, and any other result is ignored.
        using action_fn = bool (*)(void* user, text_area& source, text_action action) noexcept;
        // Writes one syntax_class for each of the length characters of the text, into classes.
        using highlight_fn = void (*)(void* user, const wchar_t* text, std::int32_t length, std::uint8_t* classes) noexcept;
        // Lists every word that could stand where the caret is; word_start is where the word being typed begins. The area keeps
        // the ones that start with the typed part, so the completer need not.
        using complete_fn = void (*)(void* user, const wchar_t* text, std::int32_t length, std::int32_t word_start, std::int32_t caret, completion_list& out) noexcept;

        text_area& set_highlighter(highlight_fn handler, void* user) noexcept;
        text_area& set_completer(complete_fn handler, void* user) noexcept;
        // Opens the list now, even with nothing typed. false when there is nothing to offer.
        bool show_completions() noexcept;
        [[nodiscard]] bool completions_open() const noexcept { return m_pop_open; }

        text_area() noexcept = default;
        text_area(const text_area&) = delete;
        text_area& operator=(const text_area&) = delete;

        // The text is UTF-8. Setting it does not call the handler, and clears the undo history.
        text_area& set_text(const char* utf8) noexcept;
        // The placeholder, the name and the tooltip are UTF-8 and have to outlive the area.
        text_area& set_placeholder(const char* text) noexcept { m_placeholder = text; return *this; }
        text_area& set_name(const char* name) noexcept { m_name = name; return *this; }
        text_area& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        text_area& set_read_only(bool read_only) noexcept { m_read_only = read_only; return *this; }
        // Breaks long lines at the width. On by default.
        text_area& set_wrap(bool wrap) noexcept { m_wrap = wrap; m_layout_dirty = true; invalidate(); return *this; }
        // Numbers the lines in a gutter on the left.
        text_area& set_line_numbers(bool numbers) noexcept { m_numbers = numbers; m_layout_dirty = true; invalidate(); return *this; }
        // Tab puts spaces in, up to the next multiple of four, instead of moving to the next control.
        text_area& set_tab_inserts(bool inserts) noexcept { m_tab_inserts = inserts; return *this; }
        // The most characters it takes. 0 means as many as fit.
        text_area& set_max_length(std::int32_t length) noexcept { m_max_length = length; return *this; }
        // The width at 96 dpi. 0 fills the row.
        text_area& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        // How many lines high it is, 1 to 64.
        text_area& set_rows(std::int32_t rows) noexcept;
        text_area& set_on_action(action_fn handler, void* user) noexcept { m_handler = handler; m_handler_user = user; return *this; }

        // The text as UTF-8, cut to fit out. Returns the number of bytes written, without the terminator.
        std::int32_t get_text(char* out, std::int32_t capacity) const noexcept;
        [[nodiscard]] const wchar_t* wide_text() const noexcept { return m_text; }
        [[nodiscard]] std::int32_t length() const noexcept { return m_length; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_placeholder; }
        [[nodiscard]] bool read_only() const noexcept { return m_read_only; }
        // The lines of the text as it was typed, not as it is wrapped.
        [[nodiscard]] std::int32_t line_count() const noexcept;
        // Where the caret is, counted from 1.
        [[nodiscard]] std::int32_t caret_line() const noexcept;
        [[nodiscard]] std::int32_t caret_column() const noexcept;

        [[nodiscard]] std::int32_t caret() const noexcept { return m_caret; }
        [[nodiscard]] std::int32_t selection_begin() const noexcept { return m_caret < m_anchor ? m_caret : m_anchor; }
        [[nodiscard]] std::int32_t selection_end() const noexcept { return m_caret < m_anchor ? m_anchor : m_caret; }
        [[nodiscard]] bool has_selection() const noexcept { return m_caret != m_anchor; }
        // Moves the caret, and selects up to it from the anchor. Clamped to the text.
        void select(std::int32_t anchor, std::int32_t caret) noexcept;
        void select_all() noexcept;

        // What the keys do, for code to do as well. Each asks the handler first, and returns false when it was refused or
        // there was nothing to do.
        bool cut() noexcept;
        bool copy() noexcept;
        bool paste() noexcept;
        bool undo() noexcept;
        bool redo() noexcept;
        // Types text at the caret, over the selection. Line breaks are '\n'. The length limit and the history apply.
        bool insert(const wchar_t* text, std::int32_t length) noexcept;
        bool erase_selection() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool takes_wheel() const noexcept override { return true; }
        // While it has the focus the caret blinks.
        [[nodiscard]] bool animating() const noexcept override { return focused(); }
        bool animate(std::uint64_t now_ms) noexcept override;
        [[nodiscard]] bool overlay_open() const noexcept override { return m_pop_open; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override;
        void paint_overlay(void* dc) const noexcept override;

    protected:
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        struct snapshot
        {
            wchar_t text[k_capacity];
            std::int32_t length{0};
            std::int32_t caret{0};
            std::int32_t anchor{0};
        };

        // One line as it shows: a run of the text with no break in it, and the number of the line it belongs to on the
        // first run of each line, 0 on the runs a wrap made after it.
        struct visual_line
        {
            std::int32_t start{0};
            std::int32_t end{0};
            std::int32_t number{0};
        };

        // Where the thumb of the bar is, in the coordinates of the panel.
        struct thumb_span
        {
            std::int32_t start{0};
            std::int32_t length{0};
            std::int32_t travel{0};
        };

        [[nodiscard]] thumb_span thumb() const noexcept;
        bool notify(text_action action) noexcept;
        bool replace(std::int32_t begin, std::int32_t end, const wchar_t* text, std::int32_t count, bool typing) noexcept;
        void move_caret(std::int32_t position, bool extend) noexcept;
        void touch(bool reveal) noexcept;
        void take(snapshot& into) const noexcept;
        void restore(const snapshot& from) noexcept;
        void push_undo() noexcept;
        [[nodiscard]] std::int32_t step(std::int32_t position, bool forward) const noexcept;
        [[nodiscard]] std::int32_t word_step(std::int32_t position, bool forward) const noexcept;
        [[nodiscard]] std::int32_t word_class(std::int32_t index) const noexcept;
        void erase_key(bool forward, bool word) noexcept;

        // Lays the text out as lines, for a width and the font in dc. Does nothing when nothing changed.
        void lay_out(void* dc, std::int32_t width) const noexcept;
        [[nodiscard]] std::int32_t line_of(std::int32_t index) const noexcept;
        [[nodiscard]] std::int32_t index_at_x(std::int32_t line, std::int32_t x) const noexcept;
        [[nodiscard]] std::int32_t line_end(std::int32_t line) const noexcept;
        [[nodiscard]] std::int32_t x_of(std::int32_t index, std::int32_t line) const noexcept;
        [[nodiscard]] std::int32_t index_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] dock_rect frame_inner() const noexcept;
        [[nodiscard]] dock_rect text_box() const noexcept;
        [[nodiscard]] std::int32_t view_height() const noexcept;
        [[nodiscard]] std::int32_t content_height() const noexcept;
        [[nodiscard]] std::int32_t max_scroll() const noexcept;
        void set_scroll(std::int32_t scroll) noexcept;
        void move_lines(std::int32_t lines, bool extend) noexcept;
        [[nodiscard]] std::int32_t bar_left() const noexcept;

        // Brings the classes up to date with the text.
        void ensure_syntax() const noexcept;
        // Paints text from a to b of one visual line, in the colours of the classes, or all in one colour when over is set.
        void paint_run(void* dc, std::int32_t left, std::int32_t y, std::int32_t a, std::int32_t b, std::int32_t gap, bool over, std::uint32_t over_colour, const std::uint32_t* palette, std::uint32_t ink) const noexcept;
        // Starts or updates the list for the word at the caret. forced opens it with nothing typed.
        bool refresh_completions(bool forced) noexcept;
        void close_completions() noexcept;
        bool accept_completion() noexcept;
        void move_completion(std::int32_t by) noexcept;
        [[nodiscard]] std::int32_t completion_rows() const noexcept;
        [[nodiscard]] std::int32_t completion_row_at(std::int32_t x, std::int32_t y) const noexcept;

        wchar_t m_text[k_capacity]{};
        std::int32_t m_length{0};
        std::int32_t m_caret{0};
        std::int32_t m_anchor{0};

        snapshot m_undo[k_history]{};
        snapshot m_redo[k_history]{};
        std::int32_t m_undo_count{0};
        std::int32_t m_redo_count{0};
        bool m_typing{false};
        wchar_t m_pending_high{0};

        const char* m_placeholder{nullptr};
        const char* m_name{nullptr};
        bool m_read_only{false};
        bool m_wrap{true};
        bool m_numbers{false};
        bool m_tab_inserts{false};
        bool m_dragging{false};
        bool m_bar_drag{false};
        std::int32_t m_max_length{0};
        std::int32_t m_width{320};
        std::int32_t m_rows{6};

        action_fn m_handler{nullptr};
        void* m_handler_user{nullptr};

        // The column the caret keeps while Up and Down move it through shorter lines, in pixels, or -1.
        std::int32_t m_goal_x{-1};
        // When a double click came, so that a third click can take the line.
        std::uint64_t m_double_at{0};
        std::int32_t m_grab{0};

        // What the last paint laid out, for the mouse and the keys.
        mutable visual_line m_lines[k_max_lines];
        mutable std::int32_t m_line_total{0};
        mutable std::int32_t m_x[k_capacity + 2]{};
        mutable std::int32_t m_logical_lines{1};
        mutable bool m_layout_dirty{true};
        mutable std::int32_t m_layout_width{-1};
        mutable std::int32_t m_gutter{0};
        mutable std::int32_t m_line_height{1};
        mutable dock_rect m_text_box{};
        mutable std::int32_t m_scroll_y{0};
        mutable std::int32_t m_scroll_x{0};
        mutable bool m_reveal{false};
        mutable std::int32_t m_wide{0};
        bool m_blink_on{true};
        std::uint64_t m_blink_origin{0};

        highlight_fn m_highlight{nullptr};
        void* m_highlight_user{nullptr};
        mutable std::uint8_t m_classes[k_capacity]{};
        mutable bool m_syntax_dirty{true};

        complete_fn m_complete{nullptr};
        void* m_complete_user{nullptr};
        completion_list m_choices{};
        std::int16_t m_matches[completion_list::k_max]{};
        std::int32_t m_match_count{0};
        std::int32_t m_pop_selected{0};
        std::int32_t m_pop_first{0};
        std::int32_t m_word_start{0};
        bool m_pop_open{false};
    };
}
