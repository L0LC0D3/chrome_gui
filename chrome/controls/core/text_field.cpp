#include <chrome/controls/core/text_field.hpp>

#include <chrome/controls/core/clipboard.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <cwctype>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_height = 28;
        constexpr std::int32_t k_pad = 8;
        constexpr std::int32_t k_radius = 3;
        // The caret is on for this long and off for this long.
        constexpr std::uint64_t k_blink_ms = 530;

        [[nodiscard]] bool is_high(wchar_t unit) noexcept
        {
            return unit >= 0xD800 && unit <= 0xDBFF;
        }

        [[nodiscard]] bool is_low(wchar_t unit) noexcept
        {
            return unit >= 0xDC00 && unit <= 0xDFFF;
        }
    }

    const char* text_action_name(text_action action) noexcept
    {
        switch (action)
        {
        case text_action::changed:
            return "changed";
        case text_action::submitted:
            return "submitted";
        case text_action::cancelled:
            return "cancelled";
        case text_action::focused:
            return "focused";
        case text_action::blurred:
            return "blurred";
        case text_action::selection_changed:
            return "selection changed";
        case text_action::cut:
            return "cut";
        case text_action::copied:
            return "copied";
        case text_action::pasted:
            return "pasted";
        case text_action::select_all:
            return "select all";
        case text_action::undone:
            return "undone";
        case text_action::redone:
            return "redone";
        }

        return "";
    }

    text_field& text_field::set_text(const char* utf8) noexcept
    {
        std::int32_t bytes = utf8 != nullptr ? static_cast<std::int32_t>(std::strlen(utf8)) : 0;
        std::int32_t count = 0;

        // Text that is too long for the field is cut at the last whole character that fits.
        while (bytes > 0 && (count = MultiByteToWideChar(CP_UTF8, 0, utf8, bytes, m_text, k_capacity - 1)) == 0)
            --bytes;

        if (bytes == 0)
            count = 0;

        if (count > 0 && is_high(m_text[count - 1]))
            --count;

        m_text[count] = L'\0';
        m_length = count;
        m_caret = count;
        m_anchor = count;
        m_undo_count = 0;
        m_redo_count = 0;
        m_typing = false;
        m_scroll = 0;
        touch();
        return *this;
    }

    std::int32_t text_field::get_text(char* out, std::int32_t capacity) const noexcept
    {
        if (out == nullptr || capacity <= 0)
            return 0;

        std::int32_t units = m_length;
        std::int32_t bytes = 0;

        while (units > 0 && (bytes = WideCharToMultiByte(CP_UTF8, 0, m_text, units, out, capacity - 1, nullptr, nullptr)) == 0)
            --units;

        if (units == 0)
            bytes = 0;

        out[bytes] = '\0';
        return bytes;
    }

    void text_field::touch() noexcept
    {
        m_blink_origin = GetTickCount64();
        m_blink_on = true;
        invalidate();
    }

    bool text_field::notify(text_action action) noexcept
    {
        return m_handler != nullptr ? m_handler(m_handler_user, *this, action) : true;
    }

    void text_field::take(snapshot& into) const noexcept
    {
        std::memcpy(into.text, m_text, sizeof(wchar_t) * static_cast<std::size_t>(m_length + 1));
        into.length = m_length;
        into.caret = m_caret;
        into.anchor = m_anchor;
    }

    void text_field::restore(const snapshot& from) noexcept
    {
        std::memcpy(m_text, from.text, sizeof(wchar_t) * static_cast<std::size_t>(from.length + 1));
        m_length = from.length;
        m_caret = from.caret;
        m_anchor = from.anchor;
    }

    void text_field::push_undo() noexcept
    {
        if (m_undo_count == k_history)
        {
            for (std::int32_t i = 1; i < k_history; ++i)
                m_undo[i - 1] = m_undo[i];

            --m_undo_count;
        }

        take(m_undo[m_undo_count++]);
    }

    std::int32_t text_field::step(std::int32_t position, bool forward) const noexcept
    {
        if (forward)
        {
            if (position >= m_length)
                return m_length;

            return is_high(m_text[position]) && position + 1 < m_length && is_low(m_text[position + 1]) ? position + 2 : position + 1;
        }

        if (position <= 0)
            return 0;

        return is_low(m_text[position - 1]) && position >= 2 && is_high(m_text[position - 2]) ? position - 2 : position - 1;
    }

    std::int32_t text_field::word_class(std::int32_t index) const noexcept
    {
        const wchar_t unit = m_text[index];

        if (std::iswspace(unit))
            return 0;

        return std::iswalnum(unit) || unit == L'_' || is_high(unit) || is_low(unit) ? 1 : 2;
    }

    std::int32_t text_field::word_step(std::int32_t position, bool forward) const noexcept
    {
        // The words of a password are not for anyone to see.
        if (m_password)
            return forward ? m_length : 0;

        if (forward)
        {
            if (position >= m_length)
                return m_length;

            const std::int32_t kind = word_class(position);

            if (kind != 0)
            {
                while (position < m_length && word_class(position) == kind)
                    ++position;
            }

            while (position < m_length && word_class(position) == 0)
                ++position;

            return position;
        }

        while (position > 0 && word_class(position - 1) == 0)
            --position;

        if (position > 0)
        {
            const std::int32_t kind = word_class(position - 1);

            while (position > 0 && word_class(position - 1) == kind)
                --position;
        }

        return position;
    }

    void text_field::select(std::int32_t anchor, std::int32_t caret) noexcept
    {
        const std::int32_t a = std::clamp(anchor, 0, m_length);
        const std::int32_t c = std::clamp(caret, 0, m_length);

        if (a == m_anchor && c == m_caret)
            return;

        m_anchor = a;
        m_caret = c;
        m_typing = false;
        touch();
        (void)notify(text_action::selection_changed);
    }

    void text_field::select_all() noexcept
    {
        if (!notify(text_action::select_all))
            return;

        select(0, m_length);
    }

    void text_field::move_caret(std::int32_t position, bool extend) noexcept
    {
        select(extend ? m_anchor : position, position);
    }

    bool text_field::replace(std::int32_t begin, std::int32_t end, const wchar_t* text, std::int32_t count, bool typing) noexcept
    {
        if (m_read_only)
            return false;

        begin = std::clamp(begin, 0, m_length);
        end = std::clamp(end, begin, m_length);
        wchar_t incoming[k_capacity];
        count = text != nullptr ? std::clamp(count, 0, k_capacity - 1) : 0;

        if (count > 0)
            std::memcpy(incoming, text, sizeof(wchar_t) * static_cast<std::size_t>(count));

        if (count > 0 && m_filter != nullptr)
            count = std::clamp(m_filter(m_filter_user, *this, incoming, count), 0, count);

        // What fits: the capacity, and the limit if there is one.
        const std::int32_t limit = m_max_length > 0 ? std::min(m_max_length, k_capacity - 1) : k_capacity - 1;
        const std::int32_t room = std::max(limit - (m_length - (end - begin)), 0);
        count = std::min(count, room);

        // A character is not cut in half.
        if (count > 0 && is_high(incoming[count - 1]))
            --count;

        if (count == 0 && begin == end)
            return false;

        // Typing without a selection goes on with the same undo step.
        if (!(typing && m_typing && begin == end))
            push_undo();

        m_typing = typing && begin == end;
        m_redo_count = 0;
        std::memmove(m_text + begin + count, m_text + end, sizeof(wchar_t) * static_cast<std::size_t>(m_length - end + 1));

        if (count > 0)
            std::memcpy(m_text + begin, incoming, sizeof(wchar_t) * static_cast<std::size_t>(count));

        m_length = m_length - (end - begin) + count;
        m_caret = begin + count;
        m_anchor = m_caret;
        touch();
        (void)notify(text_action::changed);
        return true;
    }

    bool text_field::insert(const wchar_t* text, std::int32_t length) noexcept
    {
        return replace(selection_begin(), selection_end(), text, length, false);
    }

    bool text_field::erase_selection() noexcept
    {
        return has_selection() && replace(selection_begin(), selection_end(), nullptr, 0, false);
    }

    void text_field::erase_key(bool forward, bool word) noexcept
    {
        if (has_selection())
        {
            (void)erase_selection();
            return;
        }

        const std::int32_t target = word ? word_step(m_caret, forward) : step(m_caret, forward);
        (void)replace(std::min(m_caret, target), std::max(m_caret, target), nullptr, 0, false);
    }

    bool text_field::copy() noexcept
    {
        if (m_password || !has_selection() || !notify(text_action::copied))
            return false;

        return clipboard::set(m_text + selection_begin(), selection_end() - selection_begin());
    }

    bool text_field::cut() noexcept
    {
        if (m_password || m_read_only || !has_selection() || !notify(text_action::cut))
            return false;

        return clipboard::set(m_text + selection_begin(), selection_end() - selection_begin()) && erase_selection();
    }

    bool text_field::paste() noexcept
    {
        if (m_read_only || !notify(text_action::pasted))
            return false;

        wchar_t clip[k_capacity];
        std::int32_t count = 0;

        if (!clipboard::get(clip, k_capacity, count))
            return false;

        // One line: line breaks and tabs become spaces, and a break at the very end goes.
        while (count > 0 && (clip[count - 1] == L'\r' || clip[count - 1] == L'\n'))
            --count;

        for (std::int32_t i = 0; i < count; ++i)
        {
            if (clip[i] == L'\r' || clip[i] == L'\n' || clip[i] == L'\t')
                clip[i] = L' ';
        }

        return replace(selection_begin(), selection_end(), clip, count, false);
    }

    bool text_field::undo() noexcept
    {
        if (m_undo_count == 0 || !notify(text_action::undone))
            return false;

        if (m_redo_count == k_history)
        {
            for (std::int32_t i = 1; i < k_history; ++i)
                m_redo[i - 1] = m_redo[i];

            --m_redo_count;
        }

        take(m_redo[m_redo_count++]);
        restore(m_undo[--m_undo_count]);
        m_typing = false;
        touch();
        (void)notify(text_action::changed);
        return true;
    }

    bool text_field::redo() noexcept
    {
        if (m_redo_count == 0 || !notify(text_action::redone))
            return false;

        if (m_undo_count == k_history)
        {
            for (std::int32_t i = 1; i < k_history; ++i)
                m_undo[i - 1] = m_undo[i];

            --m_undo_count;
        }

        take(m_undo[m_undo_count++]);
        restore(m_redo[--m_redo_count]);
        m_typing = false;
        touch();
        (void)notify(text_action::changed);
        return true;
    }

    bool text_field::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt)
            return false;

        switch (key)
        {
        case VK_LEFT:
        case VK_RIGHT:
        {
            const bool forward = key == VK_RIGHT;

            // With a selection and no shift the caret goes to that end of it.
            if (has_selection() && !shift)
                move_caret(forward ? selection_end() : selection_begin(), false);
            else
                move_caret(ctrl ? word_step(m_caret, forward) : step(m_caret, forward), shift);

            return true;
        }

        case VK_HOME:
            move_caret(0, shift);
            return true;

        case VK_END:
            move_caret(m_length, shift);
            return true;

        case VK_BACK:
            erase_key(false, ctrl);
            return true;

        case VK_DELETE:
            if (shift)
                (void)cut();
            else
                erase_key(true, ctrl);

            return true;

        case VK_INSERT:
            if (ctrl)
                (void)copy();
            else if (shift)
                (void)paste();

            return ctrl || shift;

        case VK_RETURN:
            (void)notify(text_action::submitted);
            return true;

        case VK_ESCAPE:
        {
            // Back to what it was when the field was entered, and out of the field.
            if (!m_read_only && std::wcscmp(m_text, m_focus_text) != 0)
            {
                m_length = static_cast<std::int32_t>(std::wcslen(m_focus_text));
                std::memcpy(m_text, m_focus_text, sizeof(wchar_t) * static_cast<std::size_t>(m_length + 1));
                m_caret = m_length;
                m_anchor = m_length;
                m_typing = false;
                touch();
                (void)notify(text_action::changed);
            }

            (void)notify(text_action::cancelled);
            request_blur();
            return true;
        }

        default:
            break;
        }

        if (!ctrl)
            return false;

        switch (key)
        {
        case 'A':
            select_all();
            return true;
        case 'C':
            (void)copy();
            return true;
        case 'X':
            (void)cut();
            return true;
        case 'V':
            (void)paste();
            return true;
        case 'Z':
            (void)(shift ? redo() : undo());
            return true;
        case 'Y':
            (void)redo();
            return true;
        default:
            return false;
        }
    }

    void text_field::on_char(std::uint32_t code) noexcept
    {
        // Control characters came as keys already.
        if (code < 32 || code == 127 || m_read_only)
            return;

        const wchar_t unit = static_cast<wchar_t>(code);

        if (is_high(unit))
        {
            m_pending_high = unit;
            return;
        }

        if (is_low(unit) && m_pending_high != 0)
        {
            const wchar_t pair[2] = {m_pending_high, unit};
            m_pending_high = 0;
            (void)replace(selection_begin(), selection_end(), pair, 2, true);
            return;
        }

        m_pending_high = 0;
        (void)replace(selection_begin(), selection_end(), &unit, 1, true);
    }

    void text_field::on_focus(bool gained) noexcept
    {
        m_dragging = false;
        m_typing = false;
        m_pending_high = 0;

        if (gained)
        {
            std::memcpy(m_focus_text, m_text, sizeof(wchar_t) * static_cast<std::size_t>(m_length + 1));
            // Entering a field selects what is in it, and a click then puts the caret where it landed.
            m_anchor = 0;
            m_caret = m_length;
            touch();
            (void)notify(text_action::focused);
            return;
        }

        invalidate();
        (void)notify(text_action::blurred);
    }

    bool text_field::animate(std::uint64_t now_ms) noexcept
    {
        if (!focused())
            return false;

        const bool on = ((now_ms - m_blink_origin) / k_blink_ms) % 2 == 0;

        if (on != m_blink_on)
        {
            m_blink_on = on;
            invalidate();
        }

        return true;
    }

    std::int32_t text_field::index_at(std::int32_t x) const noexcept
    {
        const std::int32_t relative = x - m_inner_left + m_scroll;

        for (std::int32_t i = 0; i < m_length; ++i)
        {
            if (relative < (m_xs[i] + m_xs[i + 1]) / 2)
                return is_low(m_text[i]) && i > 0 && is_high(m_text[i - 1]) ? i - 1 : i;
        }

        return m_length;
    }

    void text_field::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        (void)y;
        const bool extend = GetKeyState(VK_SHIFT) < 0;
        m_dragging = true;
        move_caret(index_at(x), extend);
    }

    void text_field::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        (void)y;

        if (m_length == 0)
            return;

        // The word under the mouse, with the same classes as Ctrl and the arrows.
        std::int32_t at = std::min(index_at(x), m_length - 1);
        std::int32_t begin = at;
        std::int32_t end = at;

        if (!m_password)
        {
            const std::int32_t kind = word_class(at);

            while (begin > 0 && word_class(begin - 1) == kind)
                --begin;

            while (end < m_length && word_class(end) == kind)
                ++end;
        }
        else
        {
            begin = 0;
            end = m_length;
        }

        m_dragging = false;
        select(begin, end);
    }

    bool text_field::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        (void)y;

        if (!m_dragging)
            return false;

        const std::int32_t at = index_at(x);

        if (at == m_caret)
            return false;

        move_caret(at, true);
        return true;
    }

    void text_field::on_release() noexcept
    {
        m_dragging = false;
    }

    control_size text_field::preferred_size(void* dc) const noexcept
    {
        (void)dc;
        return {draw::px(m_width), draw::px(k_height)};
    }

    void text_field::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool hot = this->hot();
        const bool lit = focused();

        std::uint32_t face = blend_colour(colours.panel, black, m_read_only ? 0.14f : 0.28f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.text_dim, colours.panel, hot ? 0.25f : 0.55f);
        std::uint32_t ink = colours.text;
        std::uint32_t dim = colours.text_dim;

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.4f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            dim = ink;
        }

        dock_rect box = area();
        const std::int32_t left_room = std::min(draw::px(m_reserved_left), box.width - 1);
        box.x += left_room;
        box.width -= left_room;
        if (!m_frameless)
            draw::rounded(dc, box, k_radius, face, edge);

        const std::int32_t pad = m_frameless ? draw::px(2) : draw::px(k_pad);
        const dock_rect inner{box.x + pad, box.y + 1, std::max(box.width - 2 * pad - draw::px(m_reserved_right), 1), std::max(box.height - 2, 1)};

        // A password shows dots, and everything below measures what is shown.
        wchar_t masked[k_capacity];
        const wchar_t* shown = m_text;

        if (m_password)
        {
            for (std::int32_t i = 0; i < m_length; ++i)
                masked[i] = static_cast<wchar_t>(0x2022);

            masked[m_length] = L'\0';
            shown = masked;
        }

        draw::caret_positions(dc, shown, m_length, m_xs);
        m_inner_left = inner.x;

        // Keep the caret in view: scroll as little as it takes, and not past the end of the text.
        const std::int32_t caret_x = m_xs[std::clamp(m_caret, 0, m_length)];
        const std::int32_t room = inner.width - draw::px(2);

        if (caret_x - m_scroll > room)
            m_scroll = caret_x - room;

        if (caret_x - m_scroll < 0)
            m_scroll = caret_x;

        m_scroll = std::clamp(m_scroll, 0, std::max(m_xs[m_length] - room, 0));

        const std::int32_t left = inner.x - m_scroll;
        const std::int32_t text_width = m_xs[m_length] + draw::px(4);
        const std::int32_t begin = selection_begin();
        const std::int32_t end = selection_end();

        draw::clip(dc, inner);

        if (m_length == 0)
        {
            if (m_placeholder != nullptr)
                draw::text(dc, inner, m_placeholder, dim);
        }
        else if (lit && has_selection())
        {
            const std::int32_t gap = draw::px(4);
            const std::uint32_t band = blend_colour(face, colours.accent, 0.75f);
            draw::fill(dc, {left + m_xs[begin], box.y + gap, m_xs[end] - m_xs[begin], box.height - 2 * gap}, band);

            // The text in three runs, so the selected one can be white on the band.
            if (begin > 0)
                draw::text(dc, {left, box.y, m_xs[begin] + gap, box.height}, shown, begin, ink);

            draw::text(dc, {left + m_xs[begin], box.y, m_xs[end] - m_xs[begin] + gap, box.height}, shown + begin, end - begin, white);

            if (end < m_length)
                draw::text(dc, {left + m_xs[end], box.y, text_width - m_xs[end], box.height}, shown + end, m_length - end, ink);
        }
        else
        {
            draw::text(dc, {left, box.y, text_width, box.height}, shown, m_length, ink);
        }

        if (lit && m_blink_on && enabled())
        {
            const std::int32_t gap = draw::px(6);
            draw::fill(dc, {left + caret_x, box.y + gap, std::max(draw::px(1), 1), box.height - 2 * gap}, ink);
        }

        draw::unclip(dc);
    }
}
