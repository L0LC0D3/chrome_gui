#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // What happened in a text field. The first group is told after the fact. The second is asked before it is done:
    // the handler returns false to stop it.
    enum class text_action : std::uint8_t
    {
        // After. The text is different now, whether typed, pasted, erased, undone or set by a revert.
        changed,
        // After. Enter was pressed.
        submitted,
        // After. Escape was pressed: the text went back to what it was when the field got the focus, and it lost the focus.
        cancelled,
        // After.
        focused,
        blurred,
        // After. The caret or the selection moved.
        selection_changed,

        // Before. A false from the handler stops these.
        cut,
        copied,
        pasted,
        select_all,
        undone,
        redone
    };

    [[nodiscard]] const char* text_action_name(text_action action) noexcept;

    // One line of editable text: a caret, a selection (shift and the arrows, the mouse, double-click for a word), the
    // usual keys (Home, End, Delete, Backspace, Ctrl for words), cut, copy, paste, select all, undo and redo, and a
    // placeholder that shows while it is empty. It can be a password field or read-only.
    //
    //   text_field name;
    //   name.set_placeholder("Name").set_width(200).set_on_action(&acted, this);
    //   set.add(name);
    //
    //   // The handler hears every action. Returning false from a "before" action refuses it.
    //   bool acted(void* user, text_field& field, text_action action) { ... return true; }
    class text_field : public control
    {
    public:
        // The most wide characters it holds, with room for the end.
        static constexpr std::int32_t k_capacity = 256;
        static constexpr std::int32_t k_history = 16;

        // Every action goes here. For the "before" actions a false stops the action, and any other result is ignored.
        using action_fn = bool (*)(void* user, text_field& source, text_action action) noexcept;
        // Looks at text about to go in, typed or pasted, and may change it. Write what is accepted back into text and
        // return how many characters that is, so a number field can drop everything but digits.
        using filter_fn = std::int32_t (*)(void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept;

        // The text is UTF-8. Setting it does not call the handler, and clears the undo history.
        text_field& set_text(const char* utf8) noexcept;
        // The placeholder, the name and the tooltip are UTF-8 and have to outlive the field.
        text_field& set_placeholder(const char* text) noexcept { m_placeholder = text; return *this; }
        text_field& set_name(const char* name) noexcept { m_name = name; return *this; }
        text_field& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        // Shows dots instead of the text, and refuses to copy or cut it.
        text_field& set_password(bool password) noexcept { m_password = password; return *this; }
        text_field& set_read_only(bool read_only) noexcept { m_read_only = read_only; return *this; }
        // Leaves out the box round the text, for a field that sits inside something that has its own, such as a search box.
        text_field& set_frameless(bool frameless) noexcept { m_frameless = frameless; return *this; }
        // The most characters it takes. 0 means as many as fit.
        text_field& set_max_length(std::int32_t length) noexcept { m_max_length = length; return *this; }
        // A fixed width at 96 dpi.
        text_field& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        text_field& set_on_action(action_fn handler, void* user) noexcept { m_handler = handler; m_handler_user = user; return *this; }
        text_field& set_filter(filter_fn filter, void* user) noexcept { m_filter = filter; m_filter_user = user; return *this; }

        // The text as UTF-8, cut to fit out. Returns the number of bytes written, without the terminator.
        std::int32_t get_text(char* out, std::int32_t capacity) const noexcept;
        [[nodiscard]] const wchar_t* wide_text() const noexcept { return m_text; }
        [[nodiscard]] std::int32_t length() const noexcept { return m_length; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_placeholder; }
        [[nodiscard]] bool read_only() const noexcept { return m_read_only; }
        [[nodiscard]] bool password() const noexcept { return m_password; }

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
        // Types text at the caret, over the selection. The filter, the length limit and the history all apply.
        bool insert(const wchar_t* text, std::int32_t length) noexcept;
        // Removes the selection. false when there is none, or the field is read-only.
        bool erase_selection() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        // While it has the focus the caret blinks.
        [[nodiscard]] bool animating() const noexcept override { return focused(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;
        // Keeps the text clear of this much room at the right edge, at 96 dpi, for a control built on the field.
        void reserve_right(std::int32_t width) noexcept { m_reserved_right = width; }
        // The same at the left edge: the box of the field starts this far in, and the room is for something drawn there.
        void reserve_left(std::int32_t width) noexcept { m_reserved_left = width; }

    private:
        struct snapshot
        {
            wchar_t text[k_capacity];
            std::int32_t length{0};
            std::int32_t caret{0};
            std::int32_t anchor{0};
        };

        bool notify(text_action action) noexcept;
        bool replace(std::int32_t begin, std::int32_t end, const wchar_t* text, std::int32_t count, bool typing) noexcept;
        void move_caret(std::int32_t position, bool extend) noexcept;
        void touch() noexcept;
        void take(snapshot& into) const noexcept;
        void restore(const snapshot& from) noexcept;
        void push_undo() noexcept;
        [[nodiscard]] std::int32_t step(std::int32_t position, bool forward) const noexcept;
        [[nodiscard]] std::int32_t word_step(std::int32_t position, bool forward) const noexcept;
        [[nodiscard]] std::int32_t index_at(std::int32_t x) const noexcept;
        [[nodiscard]] std::int32_t word_class(std::int32_t index) const noexcept;
        void erase_key(bool forward, bool word) noexcept;

        wchar_t m_text[k_capacity]{};
        std::int32_t m_length{0};
        std::int32_t m_caret{0};
        std::int32_t m_anchor{0};

        snapshot m_undo[k_history]{};
        snapshot m_redo[k_history]{};
        std::int32_t m_undo_count{0};
        std::int32_t m_redo_count{0};
        // The text when the field got the focus, for Escape to go back to.
        wchar_t m_focus_text[k_capacity]{};
        // The run of typing in progress: it is one undo step.
        bool m_typing{false};
        // The first half of a character outside the basic plane, waiting for the second.
        wchar_t m_pending_high{0};

        const char* m_placeholder{nullptr};
        const char* m_name{nullptr};
        bool m_password{false};
        bool m_read_only{false};
        bool m_frameless{false};
        bool m_dragging{false};
        std::int32_t m_max_length{0};
        std::int32_t m_width{180};
        std::int32_t m_reserved_right{0};
        std::int32_t m_reserved_left{0};

        action_fn m_handler{nullptr};
        void* m_handler_user{nullptr};
        filter_fn m_filter{nullptr};
        void* m_filter_user{nullptr};

        // What the last paint measured, for the mouse: where each caret position is, and how far the text is scrolled.
        mutable std::int32_t m_xs[k_capacity + 1]{};
        mutable std::int32_t m_scroll{0};
        mutable std::int32_t m_inner_left{0};
        bool m_blink_on{true};
        std::uint64_t m_blink_origin{0};
    };
}
