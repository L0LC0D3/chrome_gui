#include <chrome/controls/property/asset_field.hpp>

#include <chrome/controls/core/clipboard.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/drag_drop.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 24;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_icon = 16;
        constexpr std::int32_t k_button = 22;
        constexpr std::int32_t k_pad = 6;
        constexpr std::uint32_t k_refused = 0xE5534B;

        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_delete = 0x2E;
        constexpr std::uint32_t k_key_c = 0x43;
        constexpr std::uint32_t k_key_v = 0x56;

        [[nodiscard]] char lower(char c) noexcept
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        // The start of the file name in a path, after the last slash.
        [[nodiscard]] const char* file_name(const char* path) noexcept
        {
            const char* name = path;

            for (const char* p = path; *p != '\0'; ++p)
            {
                if (*p == '/' || *p == '\\')
                    name = p + 1;
            }

            return name;
        }

        void copy_text(char* out, std::size_t capacity, const char* text) noexcept
        {
            std::size_t i = 0;

            for (; text != nullptr && text[i] != '\0' && i + 1 < capacity; ++i)
                out[i] = text[i];

            out[i] = '\0';
        }
    }

    bool asset_field::add_kind(std::uint32_t tag) noexcept
    {
        if (m_kind_count >= k_max_kinds)
            return false;

        m_kinds[m_kind_count++] = tag;
        return true;
    }

    asset_field& asset_field::set_extensions(const char* extensions) noexcept
    {
        copy_text(m_extensions, sizeof(m_extensions), extensions);
        return *this;
    }

    asset_field& asset_field::set_path(const char* path) noexcept
    {
        copy_text(m_path, sizeof(m_path), path);
        m_missing = false;
        invalidate();
        return *this;
    }

    void asset_field::changed() noexcept
    {
        invalidate();

        if (m_change != nullptr)
            m_change(m_change_user, *this);
    }

    void asset_field::clear() noexcept
    {
        if (!enabled() || empty())
            return;

        m_path[0] = '\0';
        m_missing = false;
        changed();
    }

    void asset_field::browse() noexcept
    {
        if (enabled() && m_browse != nullptr)
            m_browse(m_browse_user, *this);
    }

    dock_rect asset_field::box() const noexcept
    {
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        return {area().x + label, area().y, std::max(area().width - label, 0), area().height};
    }

    dock_rect asset_field::browse_rect() const noexcept
    {
        const dock_rect frame = box();
        const std::int32_t size = std::min(draw::px(k_button), frame.height - 2);
        return {frame.x + frame.width - size - 1, frame.y + 1, size, std::max(frame.height - 2, 0)};
    }

    dock_rect asset_field::clear_rect() const noexcept
    {
        if (empty())
            return {};

        const dock_rect browse_area = browse_rect();
        const std::int32_t size = std::min(draw::px(k_button), browse_area.width);
        return {browse_area.x - size, browse_area.y, size, browse_area.height};
    }

    asset_field::part asset_field::part_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const auto in = [&](const dock_rect& r) { return r.width > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height; };

        if (in(browse_rect()))
            return part::browse;

        if (in(clear_rect()))
            return part::clear;

        return in(box()) ? part::body : part::none;
    }

    control_size asset_field::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size asset_field::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t label = m_label != nullptr ? draw::px(m_label_width) : 0;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width - label, draw::px(120));
        return {label + width, draw::px(k_height)};
    }

    bool asset_field::extension_ok(const char* path) const noexcept
    {
        if (m_extensions[0] == '\0')
            return true;

        const char* name = file_name(path);
        const char* dot = std::strrchr(name, '.');

        if (dot == nullptr)
            return false;

        ++dot;
        const std::size_t length = std::strlen(dot);

        // Each word of the list, up to a semicolon, is compared with the extension without regard to case.
        for (const char* p = m_extensions; *p != '\0';)
        {
            const char* end = p;

            while (*end != '\0' && *end != ';')
                ++end;

            if (static_cast<std::size_t>(end - p) == length)
            {
                bool same = true;

                for (std::size_t i = 0; i < length; ++i)
                    same = same && lower(p[i]) == lower(dot[i]);

                if (same)
                    return true;
            }

            p = *end == ';' ? end + 1 : end;
        }

        return false;
    }

    drop_effect asset_field::judge(const drag_event& event, char* path_out) const noexcept
    {
        if (event.payload == nullptr || !enabled())
            return drop_effect::none;

        const drag_payload& payload = *event.payload;
        char path[k_path_size]{};

        if (payload.kind == k_files_kind && payload.files != nullptr)
        {
            if (!m_accept_files || payload.files[0] == L'\0')
                return drop_effect::none;

            WideCharToMultiByte(CP_UTF8, 0, payload.files, -1, path, static_cast<int>(sizeof(path)), nullptr, nullptr);

            if (path[0] == '\0' || !extension_ok(path))
                return drop_effect::none;
        }
        else
        {
            bool known = false;

            for (std::uint32_t i = 0; i < m_kind_count; ++i)
                known = known || m_kinds[i] == payload.kind;

            if (!known)
                return drop_effect::none;

            copy_text(path, sizeof(path), payload.label);

            if (path[0] == '\0')
                return drop_effect::none;
        }

        if (m_accept != nullptr && !m_accept(m_accept_user, const_cast<asset_field&>(*this), event))
            return drop_effect::none;

        // A reference to a thing is a link or a copy of the reference, so move is never what it answers with.
        drop_effect effect = drop_effect::none;

        for (const drop_effect candidate : {event.wanted, drop_effect::link, drop_effect::copy})
        {
            if (candidate != drop_effect::none && candidate != drop_effect::move && payload.allows(candidate))
            {
                effect = candidate;
                break;
            }
        }

        if (effect != drop_effect::none && path_out != nullptr)
            copy_text(path_out, k_path_size, path);

        return effect;
    }

    drop_effect asset_field::on_drag_over(const drag_event& event) noexcept
    {
        const drop_effect effect = judge(event, nullptr);

        if (!m_over || effect != m_effect)
            invalidate();

        m_over = true;
        m_effect = effect;
        return effect;
    }

    void asset_field::on_drag_leave() noexcept
    {
        m_over = false;
        m_effect = drop_effect::none;
        invalidate();
    }

    drop_effect asset_field::on_drop(const drag_event& event) noexcept
    {
        char path[k_path_size]{};
        const drop_effect effect = judge(event, path);
        m_over = false;
        m_effect = drop_effect::none;

        if (effect != drop_effect::none)
        {
            copy_text(m_path, sizeof(m_path), path);
            m_missing = false;
            changed();
        }
        else
            invalidate();

        return effect;
    }

    void asset_field::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const bool live = enabled();
        const bool lit = focused();

        if (m_label != nullptr)
            draw::text(dc, {area().x, area().y, draw::px(m_label_width), area().height}, m_label,
                live ? colours.text : blend_colour(colours.text_dim, colours.panel, 0.45f), draw::text_ellipsis);

        const dock_rect frame = box();
        std::uint32_t face = blend_colour(colours.panel, black, 0.3f);
        std::uint32_t edge = lit ? colours.accent : hot() && live ? blend_colour(colours.text_dim, colours.panel, 0.2f) : blend_colour(colours.title, black, 0.5f);

        if (!live)
            edge = blend_colour(colours.panel, black, 0.2f);

        if (m_over)
        {
            face = blend_colour(m_effect != drop_effect::none ? colours.accent : k_refused, colours.panel, 0.8f);
            edge = m_effect != drop_effect::none ? colours.accent : k_refused;
        }
        else if (m_missing)
            edge = k_refused;

        draw::rounded(dc, frame, k_radius, face, edge);

        const dock_rect clear_area = clear_rect();
        const dock_rect browse_area = browse_rect();
        std::int32_t x = frame.x + draw::px(k_pad);
        const std::int32_t right = (clear_area.width > 0 ? clear_area.x : browse_area.x) - draw::px(2);

        // The picture.
        const std::int32_t size = draw::px(k_icon);
        const std::uint32_t ink = !live ? blend_colour(colours.text_dim, colours.panel, 0.45f) : m_missing ? k_refused : empty() ? colours.text_dim : colours.accent;

        if (m_icon != icon_kind::none)
        {
            draw::icon(dc, m_icon, {x, frame.y + (frame.height - size) / 2, size, size}, ink);
            x += size + draw::px(6);
        }

        const dock_rect text_area{x, frame.y, std::max(right - x, 0), frame.height};

        if (m_over)
        {
            draw::text(dc, text_area, m_effect != drop_effect::none ? "Drop to assign" : "Not this kind", colours.text, draw::text_ellipsis);
        }
        else if (empty())
        {
            draw::text(dc, text_area, m_prompt != nullptr ? m_prompt : "", blend_colour(colours.text_dim, face, 0.1f), draw::text_ellipsis);
        }
        else
        {
            // The file name, then where it is in a paler colour if there is room.
            const char* name = file_name(m_path);
            const std::int32_t name_width = draw::text_width(dc, name);
            const std::uint32_t colour = live ? (m_missing ? k_refused : colours.text) : blend_colour(colours.text_dim, colours.panel, 0.45f);
            draw::text(dc, text_area, name, colour, draw::text_ellipsis);

            const std::int32_t gap = draw::px(8);
            const std::int32_t folder_room = text_area.width - name_width - gap;

            if (name > m_path && folder_room > draw::px(40))
            {
                char folder[k_path_size]{};
                copy_text(folder, sizeof(folder), m_path);
                folder[name - m_path > 1 ? name - m_path - 1 : 0] = '\0';
                draw::text(dc, {text_area.x + name_width + gap, text_area.y, folder_room, text_area.height}, folder,
                    blend_colour(colours.text_dim, face, 0.3f), draw::text_ellipsis);
            }
        }

        // The buttons: each a small box that lights when the pointer is on it.
        const auto paint_button = [&](const dock_rect& r, part which, icon_kind icon) {
            if (r.width <= 0)
                return;

            const bool on = live && m_hover == which;
            const bool down = on && m_press == which;

            if (on)
                draw::rounded(dc, r, k_radius - 1, blend_colour(face, 0xFFFFFF, down ? 0.1f : 0.18f), blend_colour(face, 0xFFFFFF, 0.1f));

            const std::int32_t inner = std::min(draw::px(14), std::min(r.width, r.height) - 2);
            draw::icon(dc, icon, {r.x + (r.width - inner) / 2, r.y + (r.height - inner) / 2, inner, inner},
                !live ? blend_colour(colours.text_dim, colours.panel, 0.45f) : on ? colours.text : colours.text_dim);
        };

        paint_button(clear_area, part::clear, icon_kind::cross);
        paint_button(browse_area, part::browse, icon_kind::folder);
    }

    void asset_field::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        m_press = part_at(x, y);
        invalidate();
    }

    void asset_field::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        m_press = part_at(x, y);

        // A second press on the body opens the browser, as a second click on a name in a list opens it; a button just acts again.
        if (m_press == part::body)
        {
            m_press = part::none;
            browse();
        }

        invalidate();
    }

    bool asset_field::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        const part now = part_at(x, y);

        if (now != m_hover)
        {
            m_hover = now;
            return true;
        }

        return false;
    }

    bool asset_field::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        const part now = part_at(x, y);

        if (now == m_hover)
            return false;

        m_hover = now;
        return true;
    }

    void asset_field::on_release() noexcept
    {
        const part pressed = m_press;
        m_press = part::none;

        if (pressed != part::none && pressed == part_at(m_x, m_y) && enabled())
        {
            if (pressed == part::clear)
                clear();
            else if (pressed == part::browse)
                browse();
        }

        invalidate();
    }

    void asset_field::on_leave() noexcept
    {
        if (m_hover != part::none)
        {
            m_hover = part::none;
            invalidate();
        }
    }

    bool asset_field::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)shift;

        if (alt || !enabled())
            return false;

        if (!ctrl && (key == k_key_enter || key == k_key_space))
        {
            browse();
            return true;
        }

        if (!ctrl && key == k_key_delete)
        {
            if (empty())
                return false;

            clear();
            return true;
        }

        if (ctrl && key == k_key_c)
        {
            if (empty())
                return false;

            wchar_t wide[k_path_size]{};
            const int length = MultiByteToWideChar(CP_UTF8, 0, m_path, -1, wide, static_cast<int>(k_path_size));
            clipboard::set(wide, length > 0 ? length - 1 : 0);
            return true;
        }

        if (ctrl && key == k_key_v)
        {
            wchar_t wide[k_path_size]{};
            std::int32_t length = 0;

            if (!clipboard::get(wide, static_cast<std::int32_t>(k_path_size), length) || length <= 0)
                return true;

            // Paths are often copied with quotes, or with a line ending.
            std::int32_t from = 0;

            while (from < length && (wide[from] == L'"' || wide[from] == L' ' || wide[from] == L'\t'))
                ++from;

            while (length > from && (wide[length - 1] == L'"' || wide[length - 1] == L' ' || wide[length - 1] == L'\r' || wide[length - 1] == L'\n' || wide[length - 1] == L'\t'))
                --length;

            wide[length] = L'\0';
            char utf8[k_path_size]{};
            WideCharToMultiByte(CP_UTF8, 0, wide + from, -1, utf8, static_cast<int>(sizeof(utf8)), nullptr, nullptr);

            if (utf8[0] != '\0' && extension_ok(utf8))
            {
                copy_text(m_path, sizeof(m_path), utf8);
                m_missing = false;
                changed();
            }

            return true;
        }

        return false;
    }
}
