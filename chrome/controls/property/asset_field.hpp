#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A field that holds a reference to an asset or a file, as a path: a picture, the name of the file with the folder it is in
    // after it in a paler colour, a button that clears it and a button that browses. Things dragged onto it are taken when their
    // kind is one it lists, and so are files dragged from outside when their extension is one it lists, so a texture slot can take
    // only textures. Setting the path never calls the handlers, and it can be marked missing, which shows it in red.
    //
    //   Click the browse button, double click the field, or press Enter or Space: the browse handler is called, which opens whatever
    //     picks a file and then gives the path back with set_path.
    //   Clear button, or Delete: empties the field. Ctrl+C copies the path and Ctrl+V takes one from the clipboard.
    //
    //   asset_field texture;
    //   texture.set_label("Albedo").set_prompt("None (Texture)").add_kind(kind_id("ASST")).set_extensions("png;jpg;tga");
    //   texture.set_on_change(&changed, this).set_on_browse(&browse, this);
    //   set.add(texture);
    class asset_field final : public control
    {
    public:
        static constexpr std::uint32_t k_max_kinds = 4;
        static constexpr std::uint32_t k_path_size = 260;
        static constexpr std::uint32_t k_extensions_size = 64;

        // Called after the user changed the path: by a drop, by the clear button or by pasting.
        using change_fn = void (*)(void* user, asset_field& source) noexcept;
        // Called when the user asks to browse. Optional.
        using browse_fn = void (*)(void* user, asset_field& source) noexcept;
        // Decides whether a drag of a listed kind is taken. Optional. Called again as the pointer moves.
        using accept_fn = bool (*)(void* user, asset_field& source, const drag_event& event) noexcept;

        // Shows another path, UTF-8, which is copied. Null or empty clears it. Does not call the handlers.
        asset_field& set_path(const char* path) noexcept;
        [[nodiscard]] const char* path() const noexcept { return m_path; }
        [[nodiscard]] bool empty() const noexcept { return m_path[0] == '\0'; }
        // The path is of a file that is not there. It shows in red.
        asset_field& set_missing(bool missing) noexcept { m_missing = missing; invalidate(); return *this; }
        [[nodiscard]] bool missing() const noexcept { return m_missing; }

        // A kind of dragged thing the field takes, whose label becomes the path. With none listed, only files are taken.
        // false when the list is full.
        bool add_kind(std::uint32_t tag) noexcept;
        // The extensions of files that are taken, without dots and in any case, with semicolons between them. Empty takes every file.
        asset_field& set_extensions(const char* extensions) noexcept;
        // Whether files dragged in from outside are taken at all. On by default.
        asset_field& set_accept_files(bool accept) noexcept { m_accept_files = accept; return *this; }

        // What shows while it is empty.
        asset_field& set_prompt(const char* text) noexcept { m_prompt = text; invalidate(); return *this; }
        asset_field& set_icon(icon_kind icon) noexcept { m_icon = icon; invalidate(); return *this; }
        asset_field& set_label(const char* label) noexcept { m_label = label; return *this; }
        asset_field& set_label_width(std::int32_t width) noexcept { m_label_width = width; return *this; }
        // The width of the box at 96 dpi, without the label. 0 fills the row.
        asset_field& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        asset_field& set_name(const char* name) noexcept { m_name = name; return *this; }
        asset_field& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        asset_field& set_on_change(change_fn handler, void* user) noexcept { m_change = handler; m_change_user = user; return *this; }
        asset_field& set_on_browse(browse_fn handler, void* user) noexcept { m_browse = handler; m_browse_user = user; return *this; }
        asset_field& set_on_accept(accept_fn handler, void* user) noexcept { m_accept = handler; m_accept_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        // A drag is over the field and would be taken.
        [[nodiscard]] bool receiving() const noexcept { return m_over && m_effect != drop_effect::none; }

        // What the keys and buttons do, for code to do as well.
        void clear() noexcept;
        void browse() noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return m_width <= 0; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool is_drop_target() const noexcept override { return true; }
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        [[nodiscard]] dock_rect focus_ring_area() const noexcept override { return box(); }

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_drag(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_release() noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;

    private:
        enum class part : std::uint8_t
        {
            none,
            body,
            clear,
            browse
        };

        // The box itself, which is all of the area but the label.
        [[nodiscard]] dock_rect box() const noexcept;
        [[nodiscard]] dock_rect clear_rect() const noexcept;
        [[nodiscard]] dock_rect browse_rect() const noexcept;
        [[nodiscard]] part part_at(std::int32_t x, std::int32_t y) const noexcept;
        // What a drag would do and what path it would give, or none.
        [[nodiscard]] drop_effect judge(const drag_event& event, char* path_out) const noexcept;
        [[nodiscard]] bool extension_ok(const char* path) const noexcept;
        void changed() noexcept;

        char m_path[k_path_size]{};
        char m_extensions[k_extensions_size]{};
        std::uint32_t m_kinds[k_max_kinds]{};
        std::uint32_t m_kind_count{0};
        bool m_accept_files{true};
        bool m_missing{false};
        const char* m_prompt{"None"};
        const char* m_label{nullptr};
        const char* m_name{nullptr};
        icon_kind m_icon{icon_kind::stack};
        std::int32_t m_label_width{90};
        std::int32_t m_width{0};
        change_fn m_change{nullptr};
        void* m_change_user{nullptr};
        browse_fn m_browse{nullptr};
        void* m_browse_user{nullptr};
        accept_fn m_accept{nullptr};
        void* m_accept_user{nullptr};

        part m_hover{part::none};
        part m_press{part::none};
        std::int32_t m_x{0};
        std::int32_t m_y{0};
        bool m_over{false};
        drop_effect m_effect{drop_effect::none};
    };
}
