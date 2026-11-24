#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A box that floats over the top of a page to find one thing among many by typing part of its name, such as a file to open. What is
    // typed has to appear in the labels in that order, with anything between, and capitals do not count: "wtr" finds "shaders/water.shader".
    // Labels that match at the start of a word, in a row, and in the file name (the part after the last '/') rank higher, and shorter ones
    // beat longer. Up and Down move in the list, Page Up and Page Down a page, Enter takes the one chosen, Escape and a click elsewhere
    // shut the box without one.
    //
    // The control is closed until open() is called, and then has the area place() gives it, so it is added to the page's set like any
    // other and its mouse and keys go to it while it is open and focused. The page gives it the focus when it opens, and takes it back
    // when the event comes.
    //
    //   quick_open files;
    //   files.set_source(&label_of, this, count).set_on_event(&picked, this);
    //   set.add(files);
    //   // in lay_out:  files.place(body);
    //   // on Ctrl+P:   files.open();  set.set_focus(&files);
    class quick_open final : public control
    {
    public:
        static constexpr std::int32_t k_max_items = 4096;
        static constexpr std::int32_t k_query = 96;
        static constexpr std::int32_t k_max_rows = 10;

        // The label of item index, UTF-8, which has to stay valid while the box is open.
        using label_fn = const char* (*)(void* user, std::int32_t index) noexcept;
        // The box was closed. item is the index of the one chosen, or -1 when it was shut without choosing.
        using event_fn = void (*)(void* user, quick_open& source, std::int32_t item) noexcept;

        quick_open() noexcept = default;
        quick_open(const quick_open&) = delete;
        quick_open& operator=(const quick_open&) = delete;

        // The items to search, up to k_max_items. Takes effect at the next open().
        quick_open& set_source(label_fn labels, void* user, std::int32_t count) noexcept;
        quick_open& set_on_event(event_fn handler, void* user) noexcept { m_handler = handler; m_handler_user = user; return *this; }
        // Shown in the box while nothing is typed. UTF-8, has to outlive the control.
        quick_open& set_placeholder(const char* text) noexcept { m_placeholder = text; return *this; }

        // Shows the box with nothing typed. The page then gives it the focus.
        void open() noexcept;
        // Shuts the box without choosing. The handler is told.
        void cancel() noexcept;
        [[nodiscard]] bool is_open() const noexcept { return m_open; }
        // Where the page's body is, called as the page is laid out. The box sits at its top, in the middle.
        void place(const dock_rect& bounds) noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override { (void)dc; return {}; }
        void paint(void* dc) const noexcept override { (void)dc; }
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool takes_wheel() const noexcept override { return m_open; }
        [[nodiscard]] bool overlay_open() const noexcept override { return m_open; }
        [[nodiscard]] dock_rect overlay_area() const noexcept override { return m_open ? area() : dock_rect{}; }
        void paint_overlay(void* dc) const noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        bool on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept override;
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        void on_char(std::uint32_t code) noexcept override;
        void on_focus(bool gained) noexcept override;

    private:
        void refilter() noexcept;
        void choose(std::int32_t row) noexcept;
        void move(std::int32_t by, bool wrap) noexcept;
        [[nodiscard]] std::int32_t visible_rows() const noexcept;
        [[nodiscard]] std::int32_t row_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t score(const char* label) const noexcept;

        label_fn m_labels{nullptr};
        void* m_labels_user{nullptr};
        std::int32_t m_count{0};
        event_fn m_handler{nullptr};
        void* m_handler_user{nullptr};
        const char* m_placeholder{"Go to file"};

        bool m_open{false};
        dock_rect m_bounds{};
        char m_query[k_query]{};
        std::int32_t m_query_length{0};
        std::int16_t m_matches[k_max_items]{};
        std::int32_t m_scores[k_max_items]{};
        std::int32_t m_match_count{0};
        std::int32_t m_selected{0};
        std::int32_t m_first{0};
    };
}
