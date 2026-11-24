#include <chrome/controls/core/text_area.hpp>

#include <chrome/controls/core/clipboard.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwctype>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_line = 19;
        constexpr std::int32_t k_pad = 8;
        constexpr std::int32_t k_pad_v = 5;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_wheel_lines = 3;
        constexpr std::int32_t k_tab_width = 4;
        constexpr std::uint64_t k_blink_ms = 530;
        // A third click this soon after a double click takes the whole line.
        constexpr std::uint64_t k_triple_ms = 500;
        constexpr std::int32_t k_pop_row = 20;
        constexpr std::int32_t k_pop_width = 250;
        constexpr std::int32_t k_pop_inset = 2;
        constexpr std::int32_t k_pop_max_rows = 8;
        constexpr std::int32_t k_pop_bar = 4;
        // Letters that start a completion on their own.
        constexpr std::int32_t k_auto_letters = 2;
        constexpr std::int32_t k_classes = static_cast<std::int32_t>(syntax_class::count);

        constexpr const char* k_class_names[k_classes] = {"", "comment", "keyword", "type", "builtin", "number", "string", "directive", "semantic", "function", "key"};

        [[nodiscard]] bool is_word_unit(wchar_t unit) noexcept
        {
            return (unit >= L'a' && unit <= L'z') || (unit >= L'A' && unit <= L'Z') || (unit >= L'0' && unit <= L'9') || unit == L'_';
        }

        [[nodiscard]] bool is_dark(std::uint32_t colour) noexcept
        {
            const std::uint32_t r = (colour >> 16) & 0xFF;
            const std::uint32_t g = (colour >> 8) & 0xFF;
            const std::uint32_t b = colour & 0xFF;
            return (r * 299 + g * 587 + b * 114) / 1000 < 128;
        }

        // The colours of the classes on a face, light or dark: plain text is the ink.
        void fill_palette(std::uint32_t* scheme, std::uint32_t face, std::uint32_t ink) noexcept
        {
            // plain, comment, keyword, type, intrinsic, number, string, directive, semantic, function, key
            static constexpr std::uint32_t k_dark[k_classes] = {0, 0x6A9955, 0x569CD6, 0x4EC9B0, 0x4FC1FF, 0xB5CEA8, 0xCE9178, 0xC586C0, 0xD7BA7D, 0xDCDCAA, 0x9CDCFE};
            static constexpr std::uint32_t k_light[k_classes] = {0, 0x008000, 0x0000FF, 0x267F99, 0x0070C1, 0x098658, 0xA31515, 0xAF00DB, 0x986801, 0x795E26, 0x001080};
            const std::uint32_t* source = is_dark(face) ? k_dark : k_light;

            scheme[0] = ink;

            for (std::int32_t i = 1; i < k_classes; ++i)
                scheme[i] = source[i];
        }

        [[nodiscard]] bool is_high(wchar_t unit) noexcept
        {
            return unit >= 0xD800 && unit <= 0xDBFF;
        }

        [[nodiscard]] bool is_low(wchar_t unit) noexcept
        {
            return unit >= 0xDC00 && unit <= 0xDFFF;
        }
    }

    text_area& text_area::set_rows(std::int32_t rows) noexcept
    {
        m_rows = std::clamp(rows, 1, 64);
        return *this;
    }

    text_area& text_area::set_text(const char* utf8) noexcept
    {
        std::int32_t bytes = utf8 != nullptr ? static_cast<std::int32_t>(std::strlen(utf8)) : 0;
        std::int32_t count = 0;

        while (bytes > 0 && (count = MultiByteToWideChar(CP_UTF8, 0, utf8, bytes, m_text, k_capacity - 1)) == 0)
            --bytes;

        if (bytes == 0)
            count = 0;

        if (count > 0 && is_high(m_text[count - 1]))
            --count;

        // Lines end in '\n' only: a "\r\n" or a lone '\r' in the text becomes one.
        std::int32_t out = 0;

        for (std::int32_t i = 0; i < count; ++i)
        {
            if (m_text[i] == L'\r')
            {
                if (i + 1 < count && m_text[i + 1] == L'\n')
                    continue;

                m_text[out++] = L'\n';
                continue;
            }

            m_text[out++] = m_text[i];
        }

        m_text[out] = L'\0';
        m_length = out;
        m_caret = 0;
        m_anchor = 0;
        m_undo_count = 0;
        m_redo_count = 0;
        m_typing = false;
        m_scroll_x = 0;
        m_scroll_y = 0;
        m_goal_x = -1;
        m_layout_dirty = true;
        m_syntax_dirty = true;
        close_completions();
        touch(false);
        return *this;
    }

    bool completion_list::add(const char* word, syntax_class word_kind) noexcept
    {
        if (word == nullptr || word[0] == '\0' || count >= k_max || std::strlen(word) >= static_cast<std::size_t>(k_text))
            return false;

        for (std::int32_t i = 0; i < count; ++i)
        {
            if (std::strcmp(text[i], word) == 0)
                return false;
        }

        std::strcpy(text[count], word);
        kind[count] = word_kind;
        ++count;
        return true;
    }

    text_area& text_area::set_highlighter(highlight_fn handler, void* user) noexcept
    {
        m_highlight = handler;
        m_highlight_user = user;
        m_syntax_dirty = true;
        invalidate();
        return *this;
    }

    text_area& text_area::set_completer(complete_fn handler, void* user) noexcept
    {
        m_complete = handler;
        m_complete_user = user;
        close_completions();
        return *this;
    }

    void text_area::ensure_syntax() const noexcept
    {
        if (!m_syntax_dirty)
            return;

        m_syntax_dirty = false;
        std::memset(m_classes, 0, sizeof(m_classes));

        if (m_highlight != nullptr)
            m_highlight(m_highlight_user, m_text, m_length, m_classes);
    }

    void text_area::close_completions() noexcept
    {
        if (!m_pop_open)
            return;

        m_pop_open = false;
        m_match_count = 0;
        invalidate();
    }

    bool text_area::refresh_completions(bool forced) noexcept
    {
        if (m_complete == nullptr || m_read_only || has_selection())
        {
            close_completions();
            return false;
        }

        ensure_syntax();
        std::int32_t start = m_caret;

        while (start > 0 && is_word_unit(m_text[start - 1]))
            --start;

        const std::int32_t typed = m_caret - start;
        char prefix[completion_list::k_text]{};

        // A number is not a word, and neither is anything inside a comment or a string.
        const bool digit = typed > 0 && m_text[start] >= L'0' && m_text[start] <= L'9';
        const bool quiet = m_caret > 0 && (m_classes[m_caret - 1] == static_cast<std::uint8_t>(syntax_class::comment) || m_classes[m_caret - 1] == static_cast<std::uint8_t>(syntax_class::string));

        if (typed >= completion_list::k_text || digit || (!forced && (quiet || typed < (m_pop_open ? 1 : k_auto_letters))))
        {
            close_completions();
            return false;
        }

        for (std::int32_t i = 0; i < typed; ++i)
            prefix[i] = static_cast<char>(m_text[start + i]);

        m_choices.count = 0;
        m_complete(m_complete_user, m_text, m_length, start, m_caret, m_choices);

        m_match_count = 0;

        for (std::int32_t i = 0; i < m_choices.count; ++i)
        {
            const char* word = m_choices.text[i];

            if (_strnicmp(word, prefix, static_cast<std::size_t>(typed)) != 0 || std::strcmp(word, prefix) == 0)
                continue;

            m_matches[m_match_count++] = static_cast<std::int16_t>(i);
        }

        // Words that match the case typed come first, then alphabetical.
        std::sort(m_matches, m_matches + m_match_count, [this, &prefix, typed](std::int16_t a, std::int16_t b) noexcept {
            const bool exact_a = std::strncmp(m_choices.text[a], prefix, static_cast<std::size_t>(typed)) == 0;
            const bool exact_b = std::strncmp(m_choices.text[b], prefix, static_cast<std::size_t>(typed)) == 0;

            if (exact_a != exact_b)
                return exact_a;

            return _stricmp(m_choices.text[a], m_choices.text[b]) < 0;
        });

        if (m_match_count == 0)
        {
            close_completions();
            return false;
        }

        m_word_start = start;
        m_pop_selected = 0;
        m_pop_first = 0;
        m_pop_open = true;
        invalidate();
        return true;
    }

    bool text_area::show_completions() noexcept
    {
        return refresh_completions(true);
    }

    std::int32_t text_area::completion_rows() const noexcept
    {
        return std::min(m_match_count, k_pop_max_rows);
    }

    void text_area::move_completion(std::int32_t by) noexcept
    {
        if (m_match_count == 0)
            return;

        std::int32_t next = m_pop_selected + by;

        if (by == 1 || by == -1)
            next = (next + m_match_count) % m_match_count;
        else
            next = std::clamp(next, 0, m_match_count - 1);

        m_pop_selected = next;
        const std::int32_t rows = completion_rows();

        if (next < m_pop_first)
            m_pop_first = next;
        else if (next >= m_pop_first + rows)
            m_pop_first = next - rows + 1;

        invalidate();
    }

    bool text_area::accept_completion() noexcept
    {
        if (!m_pop_open || m_pop_selected < 0 || m_pop_selected >= m_match_count)
            return false;

        wchar_t word[completion_list::k_text];
        const char* chosen = m_choices.text[m_matches[m_pop_selected]];
        std::int32_t count = 0;

        while (chosen[count] != '\0' && count < completion_list::k_text - 1)
        {
            word[count] = static_cast<wchar_t>(static_cast<unsigned char>(chosen[count]));
            ++count;
        }

        const std::int32_t start = m_word_start;
        close_completions();
        return replace(start, m_caret, word, count, false);
    }

    dock_rect text_area::overlay_area() const noexcept
    {
        if (!m_pop_open)
            return {};

        const std::int32_t line = line_of(m_word_start);
        const std::int32_t inset = draw::px(k_pop_inset);
        const std::int32_t height = completion_rows() * draw::px(k_pop_row) + 2 * inset;
        const std::int32_t width = draw::px(k_pop_width);
        const dock_rect box = m_text_box;
        std::int32_t x = box.x - m_scroll_x + x_of(m_word_start, line);
        std::int32_t y = box.y + (line + 1) * m_line_height - m_scroll_y + draw::px(2);

        // Under the word, or over it when there is no room below.
        if (y + height > box.y + box.height)
            y = box.y + line * m_line_height - m_scroll_y - height - draw::px(2);

        x = std::clamp(x, box.x, std::max(box.x + box.width - width, box.x));
        return {x, y, width, height};
    }

    std::int32_t text_area::completion_row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect list = overlay_area();
        const std::int32_t inset = draw::px(k_pop_inset);

        if (x < list.x || y < list.y + inset || x >= list.x + list.width || y >= list.y + list.height - inset)
            return -1;

        const std::int32_t row = m_pop_first + (y - list.y - inset) / draw::px(k_pop_row);
        return row < m_match_count ? row : -1;
    }

    bool text_area::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_pop_open)
            return false;

        const std::int32_t row = completion_row_at(x, y);

        if (row < 0 || row == m_pop_selected)
            return false;

        m_pop_selected = row;
        return true;
    }

    void text_area::paint_overlay(void* dc) const noexcept
    {
        if (!m_pop_open)
            return;

        const theme_colours& colours = theme();
        const dock_rect list = overlay_area();
        const std::int32_t inset = draw::px(k_pop_inset);
        const std::int32_t row_height = draw::px(k_pop_row);
        const std::int32_t rows = completion_rows();
        const bool scrolls = m_match_count > rows;
        std::uint32_t scheme[k_classes];
        fill_palette(scheme, colours.panel, colours.text);

        draw::rounded(dc, list, 3, blend_colour(colours.panel, colours.title, 0.8f), colours.accent);
        draw::clip(dc, list);

        for (std::int32_t i = 0; i < rows; ++i)
        {
            const std::int32_t index = m_pop_first + i;
            const std::int32_t item = m_matches[index];
            const std::uint32_t kind_colour = scheme[static_cast<std::int32_t>(m_choices.kind[item])];
            const dock_rect row{list.x + inset, list.y + inset + i * row_height, list.width - 2 * inset - (scrolls ? draw::px(k_pop_bar + 3) : 0), row_height};
            const bool chosen = index == m_pop_selected;

            if (chosen)
                draw::rounded(dc, row, 2, colours.accent, colours.accent);

            const std::int32_t mark = draw::px(8);
            draw::rounded(dc, {row.x + draw::px(6), row.y + (row.height - mark) / 2, mark, mark}, 2, kind_colour, kind_colour);
            const std::int32_t text_x = row.x + draw::px(20);
            const std::int32_t name_width = draw::px(64);
            draw::text(dc, {text_x, row.y, std::max(row.width - (text_x - row.x) - name_width - draw::px(6), 0), row.height}, m_choices.text[item], chosen ? 0xFFFFFF : colours.text, draw::text_ellipsis);
            draw::text(dc, {row.x + row.width - name_width - draw::px(6), row.y, name_width, row.height}, k_class_names[static_cast<std::int32_t>(m_choices.kind[item])],
                chosen ? 0xFFFFFF : colours.text_dim, draw::text_right);
        }

        if (scrolls)
        {
            const std::int32_t track = list.height - 2 * inset;
            const std::int32_t thumb_height = std::max(track * rows / m_match_count, draw::px(10));
            const std::int32_t span = m_match_count - rows;
            const std::int32_t at = span > 0 ? (track - thumb_height) * m_pop_first / span : 0;
            const std::uint32_t bar = blend_colour(colours.text_dim, 0x000000, 0.2f);
            draw::rounded(dc, {list.x + list.width - inset - draw::px(k_pop_bar + 1), list.y + inset + at, draw::px(k_pop_bar), thumb_height}, 2, bar, bar);
        }

        draw::unclip(dc);
    }

    void text_area::paint_run(void* dc, std::int32_t left, std::int32_t y, std::int32_t a, std::int32_t b, std::int32_t gap, bool over, std::uint32_t over_colour, const std::uint32_t* palette, std::uint32_t ink) const noexcept
    {
        std::int32_t i = a;

        while (i < b)
        {
            std::int32_t j = i + 1;
            std::uint32_t colour = over ? over_colour : ink;

            if (!over && palette != nullptr)
            {
                const std::uint8_t kind = m_classes[i];
                colour = palette[kind < k_classes ? kind : 0];

                // Spaces go with the run they are in, which keeps the number of draws down.
                while (j < b && (m_classes[j] == kind || m_text[j] == L' '))
                    ++j;
            }
            else
            {
                j = b;
            }

            draw::text(dc, {left + m_x[i], y, m_x[j] - m_x[i] + gap, m_line_height}, m_text + i, j - i, colour);
            i = j;
        }
    }

    std::int32_t text_area::get_text(char* out, std::int32_t capacity) const noexcept
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

    std::int32_t text_area::line_count() const noexcept
    {
        std::int32_t lines = 1;

        for (std::int32_t i = 0; i < m_length; ++i)
        {
            if (m_text[i] == L'\n')
                ++lines;
        }

        return lines;
    }

    std::int32_t text_area::caret_line() const noexcept
    {
        std::int32_t lines = 1;

        for (std::int32_t i = 0; i < m_caret; ++i)
        {
            if (m_text[i] == L'\n')
                ++lines;
        }

        return lines;
    }

    std::int32_t text_area::caret_column() const noexcept
    {
        std::int32_t start = m_caret;

        while (start > 0 && m_text[start - 1] != L'\n')
            --start;

        return m_caret - start + 1;
    }

    void text_area::touch(bool reveal) noexcept
    {
        m_blink_origin = GetTickCount64();
        m_blink_on = true;

        if (reveal)
            m_reveal = true;

        invalidate();
    }

    bool text_area::notify(text_action action) noexcept
    {
        return m_handler != nullptr ? m_handler(m_handler_user, *this, action) : true;
    }

    void text_area::take(snapshot& into) const noexcept
    {
        std::memcpy(into.text, m_text, sizeof(wchar_t) * static_cast<std::size_t>(m_length + 1));
        into.length = m_length;
        into.caret = m_caret;
        into.anchor = m_anchor;
    }

    void text_area::restore(const snapshot& from) noexcept
    {
        std::memcpy(m_text, from.text, sizeof(wchar_t) * static_cast<std::size_t>(from.length + 1));
        m_length = from.length;
        m_caret = from.caret;
        m_anchor = from.anchor;
        m_layout_dirty = true;
        m_syntax_dirty = true;
        close_completions();
    }

    void text_area::push_undo() noexcept
    {
        if (m_undo_count == k_history)
        {
            for (std::int32_t i = 1; i < k_history; ++i)
                m_undo[i - 1] = m_undo[i];

            --m_undo_count;
        }

        take(m_undo[m_undo_count++]);
    }

    std::int32_t text_area::step(std::int32_t position, bool forward) const noexcept
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

    std::int32_t text_area::word_class(std::int32_t index) const noexcept
    {
        const wchar_t unit = m_text[index];

        if (std::iswspace(unit))
            return 0;

        return std::iswalnum(unit) || unit == L'_' || is_high(unit) || is_low(unit) ? 1 : 2;
    }

    std::int32_t text_area::word_step(std::int32_t position, bool forward) const noexcept
    {
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

    void text_area::select(std::int32_t anchor, std::int32_t caret) noexcept
    {
        const std::int32_t a = std::clamp(anchor, 0, m_length);
        const std::int32_t c = std::clamp(caret, 0, m_length);

        if (a == m_anchor && c == m_caret)
            return;

        m_anchor = a;
        m_caret = c;
        m_typing = false;
        close_completions();
        touch(true);
        (void)notify(text_action::selection_changed);
    }

    void text_area::select_all() noexcept
    {
        if (!notify(text_action::select_all))
            return;

        select(0, m_length);
    }

    void text_area::move_caret(std::int32_t position, bool extend) noexcept
    {
        select(extend ? m_anchor : position, position);
    }

    bool text_area::replace(std::int32_t begin, std::int32_t end, const wchar_t* text, std::int32_t count, bool typing) noexcept
    {
        if (m_read_only)
            return false;

        begin = std::clamp(begin, 0, m_length);
        end = std::clamp(end, begin, m_length);
        wchar_t incoming[k_capacity];
        count = text != nullptr ? std::clamp(count, 0, k_capacity - 1) : 0;

        if (count > 0)
            std::memcpy(incoming, text, sizeof(wchar_t) * static_cast<std::size_t>(count));

        const std::int32_t limit = m_max_length > 0 ? std::min(m_max_length, k_capacity - 1) : k_capacity - 1;
        const std::int32_t room = std::max(limit - (m_length - (end - begin)), 0);
        count = std::min(count, room);

        if (count > 0 && is_high(incoming[count - 1]))
            --count;

        if (count == 0 && begin == end)
            return false;

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
        m_goal_x = -1;
        m_layout_dirty = true;
        m_syntax_dirty = true;
        touch(true);
        (void)notify(text_action::changed);
        return true;
    }

    bool text_area::insert(const wchar_t* text, std::int32_t length) noexcept
    {
        return replace(selection_begin(), selection_end(), text, length, false);
    }

    bool text_area::erase_selection() noexcept
    {
        return has_selection() && replace(selection_begin(), selection_end(), nullptr, 0, false);
    }

    void text_area::erase_key(bool forward, bool word) noexcept
    {
        if (has_selection())
        {
            (void)erase_selection();
            return;
        }

        const std::int32_t target = word ? word_step(m_caret, forward) : step(m_caret, forward);
        (void)replace(std::min(m_caret, target), std::max(m_caret, target), nullptr, 0, false);
    }

    bool text_area::copy() noexcept
    {
        if (!has_selection() || !notify(text_action::copied))
            return false;

        // The clipboard has lines end in "\r\n".
        wchar_t out[2 * k_capacity];
        std::int32_t count = 0;

        for (std::int32_t i = selection_begin(); i < selection_end(); ++i)
        {
            if (m_text[i] == L'\n')
                out[count++] = L'\r';

            out[count++] = m_text[i];
        }

        return clipboard::set(out, count);
    }

    bool text_area::cut() noexcept
    {
        if (m_read_only || !has_selection() || !notify(text_action::cut))
            return false;

        wchar_t out[2 * k_capacity];
        std::int32_t count = 0;

        for (std::int32_t i = selection_begin(); i < selection_end(); ++i)
        {
            if (m_text[i] == L'\n')
                out[count++] = L'\r';

            out[count++] = m_text[i];
        }

        return clipboard::set(out, count) && erase_selection();
    }

    bool text_area::paste() noexcept
    {
        if (m_read_only || !notify(text_action::pasted))
            return false;

        wchar_t clip[k_capacity];
        std::int32_t count = 0;

        if (!clipboard::get(clip, k_capacity, count))
            return false;

        // Lines end in '\n' here, and a tab is one space.
        std::int32_t out = 0;

        for (std::int32_t i = 0; i < count; ++i)
        {
            if (clip[i] == L'\r')
            {
                if (i + 1 < count && clip[i + 1] == L'\n')
                    continue;

                clip[out++] = L'\n';
            }
            else
            {
                clip[out++] = clip[i] == L'\t' ? L' ' : clip[i];
            }
        }

        return replace(selection_begin(), selection_end(), clip, out, false);
    }

    bool text_area::undo() noexcept
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
        touch(true);
        (void)notify(text_action::changed);
        return true;
    }

    bool text_area::redo() noexcept
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
        touch(true);
        (void)notify(text_action::changed);
        return true;
    }

    dock_rect text_area::frame_inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    dock_rect text_area::text_box() const noexcept
    {
        const dock_rect in = frame_inner();
        const std::int32_t pad = draw::px(k_pad);
        const std::int32_t pad_v = draw::px(k_pad_v);
        return {in.x + m_gutter + pad, in.y + pad_v, std::max(in.width - m_gutter - pad - draw::px(k_bar_column), 1), std::max(in.height - 2 * pad_v, 1)};
    }

    std::int32_t text_area::view_height() const noexcept
    {
        return text_box().height;
    }

    std::int32_t text_area::content_height() const noexcept
    {
        return m_line_total * m_line_height;
    }

    std::int32_t text_area::max_scroll() const noexcept
    {
        return std::max(content_height() - view_height(), 0);
    }

    std::int32_t text_area::bar_left() const noexcept
    {
        const dock_rect in = frame_inner();
        return in.x + in.width - draw::px(k_bar_column);
    }

    text_area::thumb_span text_area::thumb() const noexcept
    {
        const dock_rect in = frame_inner();
        const std::int32_t content = std::max(content_height(), 1);
        thumb_span span;
        span.length = std::clamp(in.height * view_height() / content, std::min(draw::px(k_min_thumb), in.height), in.height);
        span.travel = in.height - span.length;
        const std::int32_t reach = max_scroll();
        span.start = in.y + (reach > 0 ? span.travel * m_scroll_y / reach : 0);
        return span;
    }

    void text_area::set_scroll(std::int32_t scroll) noexcept
    {
        const std::int32_t clamped = std::clamp(scroll, 0, max_scroll());

        if (clamped == m_scroll_y)
            return;

        m_scroll_y = clamped;
        invalidate();
    }

    void text_area::lay_out(void* dc, std::int32_t width) const noexcept
    {
        if (!m_layout_dirty && width == m_layout_width)
            return;

        m_layout_dirty = false;
        m_layout_width = width;
        m_line_total = 0;
        m_logical_lines = 0;
        m_wide = 0;
        std::int32_t start = 0;

        auto add = [this](std::int32_t from, std::int32_t to, std::int32_t number) noexcept {
            m_lines[m_line_total++] = {from, to, number};
        };

        while (true)
        {
            std::int32_t end = start;

            while (end < m_length && m_text[end] != L'\n')
                ++end;

            ++m_logical_lines;
            draw::caret_positions(dc, m_text + start, end - start, m_x + start);
            m_wide = std::max(m_wide, m_x[end] - m_x[start]);

            if (!m_wrap || width <= 0 || m_x[end] - m_x[start] <= width)
            {
                add(start, end, m_logical_lines);
            }
            else
            {
                std::int32_t from = start;
                bool first = true;

                while (from < end)
                {
                    std::int32_t brk = end;

                    if (m_x[end] - m_x[from] > width)
                    {
                        // Break after the last space that fits, or where the line stops fitting when there is none.
                        std::int32_t i = from;
                        std::int32_t space = -1;

                        for (; i < end; ++i)
                        {
                            if (m_x[i + 1] - m_x[from] > width)
                                break;

                            if (m_text[i] == L' ')
                                space = i;
                        }

                        if (i == from)
                            i = from + 1;

                        brk = space >= from ? space + 1 : i;

                        // Not in the middle of a character made of two units.
                        if (brk > from && brk < end && is_low(m_text[brk]) && is_high(m_text[brk - 1]))
                            brk = brk - 1 > from ? brk - 1 : brk + 1;
                    }

                    add(from, brk, first ? m_logical_lines : 0);
                    first = false;
                    from = brk;
                }
            }

            if (end >= m_length)
                break;

            start = end + 1;
        }
    }

    std::int32_t text_area::line_of(std::int32_t index) const noexcept
    {
        if (m_line_total == 0)
            return 0;

        std::int32_t low = 0;
        std::int32_t high = m_line_total - 1;

        while (low < high)
        {
            const std::int32_t mid = (low + high + 1) / 2;

            if (m_lines[mid].start <= index)
                low = mid;
            else
                high = mid - 1;
        }

        return low;
    }

    std::int32_t text_area::line_end(std::int32_t line) const noexcept
    {
        const visual_line& run = m_lines[std::clamp(line, 0, std::max(m_line_total - 1, 0))];
        const std::int32_t end = std::min(run.end, m_length);

        // A run that a wrap ended is left before its last character, so the caret stays on it.
        if (end >= m_length || m_text[end] == L'\n')
            return end;

        return std::max(end - 1, run.start);
    }

    std::int32_t text_area::x_of(std::int32_t index, std::int32_t line) const noexcept
    {
        if (m_line_total == 0)
            return 0;

        const std::int32_t at = std::clamp(index, 0, m_length);
        return m_x[at] - m_x[std::min(m_lines[std::clamp(line, 0, m_line_total - 1)].start, m_length)];
    }

    std::int32_t text_area::index_at_x(std::int32_t line, std::int32_t x) const noexcept
    {
        if (m_line_total == 0)
            return 0;

        const visual_line& run = m_lines[std::clamp(line, 0, m_line_total - 1)];
        const std::int32_t start = std::min(run.start, m_length);
        const std::int32_t end = std::min(run.end, m_length);

        for (std::int32_t i = start; i < end; ++i)
        {
            if (x < (m_x[i] + m_x[i + 1]) / 2 - m_x[start])
                return is_low(m_text[i]) && i > start && is_high(m_text[i - 1]) ? i - 1 : i;
        }

        return line_end(line);
    }

    std::int32_t text_area::index_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_line_total == 0)
            return 0;

        const dock_rect box = text_box();
        const std::int32_t line = std::clamp((y - box.y + m_scroll_y) / std::max(m_line_height, 1), 0, m_line_total - 1);
        return index_at_x(line, x - box.x + m_scroll_x);
    }

    void text_area::move_lines(std::int32_t lines, bool extend) noexcept
    {
        if (m_line_total == 0)
            return;

        const std::int32_t line = line_of(m_caret);

        if (m_goal_x < 0)
            m_goal_x = x_of(m_caret, line);

        const std::int32_t target = line + lines;
        const std::int32_t goal = m_goal_x;

        // Past the first or the last line the caret goes to the end of the text.
        if (target < 0)
            move_caret(0, extend);
        else if (target >= m_line_total)
            move_caret(m_length, extend);
        else
            move_caret(index_at_x(target, goal), extend);

        m_goal_x = goal;
    }

    bool text_area::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (alt)
            return false;

        // An open list takes the keys that move in it and choose from it.
        if (m_pop_open && !ctrl)
        {
            const std::int32_t rows = completion_rows();

            switch (key)
            {
            case VK_UP:
                move_completion(-1);
                return true;
            case VK_DOWN:
                move_completion(1);
                return true;
            case VK_PRIOR:
                move_completion(-rows);
                return true;
            case VK_NEXT:
                move_completion(rows);
                return true;
            case VK_RETURN:
            case VK_TAB:
                if (accept_completion())
                    return true;

                close_completions();
                break;
            case VK_ESCAPE:
                close_completions();
                return true;
            default:
                break;
            }
        }

        const bool vertical = key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT;

        if (!vertical)
            m_goal_x = -1;

        const std::int32_t page = std::max(view_height() / std::max(m_line_height, 1) - 1, 1);

        switch (key)
        {
        case VK_LEFT:
        case VK_RIGHT:
        {
            const bool forward = key == VK_RIGHT;

            if (has_selection() && !shift)
                move_caret(forward ? selection_end() : selection_begin(), false);
            else
                move_caret(ctrl ? word_step(m_caret, forward) : step(m_caret, forward), shift);

            return true;
        }

        case VK_UP:
            move_lines(-1, shift);
            return true;

        case VK_DOWN:
            move_lines(1, shift);
            return true;

        case VK_PRIOR:
            move_lines(-page, shift);
            return true;

        case VK_NEXT:
            move_lines(page, shift);
            return true;

        case VK_HOME:
            move_caret(ctrl ? 0 : m_lines[line_of(m_caret)].start, shift);
            return true;

        case VK_END:
            move_caret(ctrl ? m_length : line_end(line_of(m_caret)), shift);
            return true;

        case VK_BACK:
            erase_key(false, ctrl);

            if (m_pop_open)
                (void)refresh_completions(false);

            return true;

        case VK_DELETE:
            if (shift)
                (void)cut();
            else
                erase_key(true, ctrl);

            close_completions();
            return true;

        case VK_INSERT:
            if (ctrl)
                (void)copy();
            else if (shift)
                (void)paste();

            return ctrl || shift;

        case VK_RETURN:
        {
            if (ctrl)
            {
                (void)notify(text_action::submitted);
                return true;
            }

            const wchar_t line_break = L'\n';
            (void)replace(selection_begin(), selection_end(), &line_break, 1, true);
            return true;
        }

        case VK_TAB:
        {
            if (!m_tab_inserts || ctrl || shift || m_read_only)
                return false;

            const std::int32_t spaces = k_tab_width - (caret_column() - 1) % k_tab_width;
            wchar_t fill[k_tab_width];

            for (std::int32_t i = 0; i < spaces; ++i)
                fill[i] = L' ';

            (void)replace(selection_begin(), selection_end(), fill, spaces, false);
            return true;
        }

        case VK_ESCAPE:
            (void)notify(text_action::cancelled);
            request_blur();
            return true;

        default:
            break;
        }

        if (!ctrl)
            return false;

        switch (key)
        {
        case VK_SPACE:
            return !m_read_only && refresh_completions(true);
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

    void text_area::on_char(std::uint32_t code) noexcept
    {
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

        if (replace(selection_begin(), selection_end(), &unit, 1, true) && m_complete != nullptr)
            (void)refresh_completions(false);
    }

    void text_area::on_focus(bool gained) noexcept
    {
        m_dragging = false;
        m_bar_drag = false;
        m_typing = false;
        m_pending_high = 0;

        if (gained)
        {
            touch(false);
            (void)notify(text_action::focused);
            return;
        }

        close_completions();
        invalidate();
        (void)notify(text_action::blurred);
    }

    bool text_area::animate(std::uint64_t now_ms) noexcept
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

    void text_area::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_goal_x = -1;
        m_bar_drag = false;

        // A click in the list takes the word it is on, and a click anywhere else shuts the list.
        if (m_pop_open)
        {
            const std::int32_t row = completion_row_at(x, y);
            const dock_rect list = overlay_area();
            const bool inside = x >= list.x && y >= list.y && x < list.x + list.width && y < list.y + list.height;

            if (inside)
            {
                if (row >= 0)
                {
                    m_pop_selected = row;
                    (void)accept_completion();
                }

                return;
            }

            close_completions();
        }

        if (content_height() > view_height() && x >= bar_left())
        {
            const thumb_span span = thumb();

            if (y >= span.start && y < span.start + span.length)
            {
                m_bar_drag = true;
                m_grab = y - span.start;
            }
            else
            {
                // A click on the track moves a page.
                const std::int32_t page = std::max(view_height() - m_line_height, m_line_height);
                set_scroll(m_scroll_y + (y < span.start ? -page : page));
            }

            invalidate();
            return;
        }

        const std::int32_t at = index_at(x, y);

        if (m_double_at != 0 && GetTickCount64() - m_double_at < k_triple_ms)
        {
            m_double_at = 0;
            std::int32_t begin = at;
            std::int32_t end = at;

            while (begin > 0 && m_text[begin - 1] != L'\n')
                --begin;

            while (end < m_length && m_text[end] != L'\n')
                ++end;

            // The line with its break, so cutting it takes the line away.
            select(begin, end < m_length ? end + 1 : end);
            return;
        }

        m_double_at = 0;
        m_dragging = true;
        move_caret(at, GetKeyState(VK_SHIFT) < 0);
    }

    void text_area::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        if (content_height() > view_height() && x >= bar_left())
        {
            on_press(x, y);
            return;
        }

        m_dragging = false;
        m_double_at = GetTickCount64();

        if (m_length == 0)
            return;

        const std::int32_t at = std::min(index_at(x, y), m_length - 1);
        const std::int32_t kind = word_class(at);
        std::int32_t begin = at;
        std::int32_t end = at;

        while (begin > 0 && word_class(begin - 1) == kind && m_text[begin - 1] != L'\n')
            --begin;

        while (end < m_length && word_class(end) == kind && m_text[end] != L'\n')
            ++end;

        select(begin, end);
    }

    bool text_area::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_bar_drag)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - frame_inner().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_scroll_y;
            set_scroll(span.travel > 0 ? (at * max_scroll() + span.travel / 2) / span.travel : 0);
            return m_scroll_y != before;
        }

        if (!m_dragging)
            return false;

        // Past the top or the bottom edge the text moves along.
        const dock_rect box = text_box();

        if (y < box.y)
            set_scroll(m_scroll_y - m_line_height);
        else if (y >= box.y + box.height)
            set_scroll(m_scroll_y + m_line_height);

        const std::int32_t at = index_at(x, std::clamp(y, box.y, box.y + box.height - 1));

        if (at != m_caret)
            move_caret(at, true);

        return true;
    }

    void text_area::on_release() noexcept
    {
        m_dragging = false;
        m_bar_drag = false;
    }

    bool text_area::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        // Over the list the wheel moves the list.
        if (m_pop_open)
        {
            const dock_rect list = overlay_area();

            if (x >= list.x && y >= list.y && x < list.x + list.width && y < list.y + list.height)
            {
                const std::int32_t reach = std::max(m_match_count - completion_rows(), 0);
                m_pop_first = std::clamp(m_pop_first - notches, 0, reach);
                return true;
            }
        }

        const std::int32_t before = m_scroll_y;
        set_scroll(m_scroll_y - notches * k_wheel_lines * m_line_height);

        // At either end the panel gets the wheel, so it can scroll on.
        return m_scroll_y != before;
    }

    control_size text_area::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size text_area::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(120));
        return {width, m_rows * draw::px(k_line) + 2 * draw::px(k_pad_v) + 2 * draw::px(1)};
    }

    void text_area::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();

        std::uint32_t face = blend_colour(colours.panel, black, m_read_only ? 0.14f : 0.28f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.text_dim, colours.panel, hot() ? 0.25f : 0.55f);
        std::uint32_t ink = colours.text;
        std::uint32_t dim = colours.text_dim;

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.4f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            dim = ink;
        }

        draw::rounded(dc, area(), k_radius, face, edge);

        // The gutter is as wide as the most digits a line number has.
        m_line_height = std::max(draw::px(k_line), 1);
        m_gutter = 0;

        if (m_numbers)
        {
            std::int32_t lines = line_count();
            char digits[12];
            std::int32_t count = 0;

            do
            {
                digits[count++] = '8';
                lines /= 10;
            } while (lines > 0 && count < 10);

            digits[count] = '\0';
            m_gutter = draw::text_width(dc, digits) + 2 * draw::px(6);
        }

        const dock_rect inner = frame_inner();
        const dock_rect box = text_box();
        m_text_box = box;
        lay_out(dc, m_wrap ? box.width : 0);

        // Keep the scroll in range, and the caret in view after it moved.
        m_scroll_y = std::clamp(m_scroll_y, 0, max_scroll());

        if (m_reveal)
        {
            const std::int32_t caret_line = line_of(m_caret);
            const std::int32_t top = caret_line * m_line_height;

            if (top < m_scroll_y)
                m_scroll_y = top;
            else if (top + m_line_height > m_scroll_y + box.height)
                m_scroll_y = top + m_line_height - box.height;

            if (!m_wrap)
            {
                const std::int32_t caret_x = x_of(m_caret, caret_line);
                const std::int32_t room = box.width - draw::px(4);

                if (caret_x - m_scroll_x > room)
                    m_scroll_x = caret_x - room;

                if (caret_x < m_scroll_x)
                    m_scroll_x = caret_x;
            }

            m_reveal = false;
            m_scroll_y = std::clamp(m_scroll_y, 0, max_scroll());
        }

        m_scroll_x = m_wrap ? 0 : std::clamp(m_scroll_x, 0, std::max(m_wide - box.width + draw::px(8), 0));

        const std::int32_t first = std::clamp(m_scroll_y / m_line_height, 0, std::max(m_line_total - 1, 0));
        const std::int32_t last = std::min(first + box.height / m_line_height + 2, m_line_total);

        // The gutter, with the number of each line at its first run.
        if (m_gutter > 0)
        {
            draw::fill(dc, {inner.x, inner.y, m_gutter, inner.height}, blend_colour(face, black, 0.2f));
            draw::clip(dc, {inner.x, box.y, m_gutter, box.height});

            for (std::int32_t i = first; i < last; ++i)
            {
                if (m_lines[i].number == 0)
                    continue;

                char number[12];
                std::snprintf(number, sizeof(number), "%d", m_lines[i].number);
                const bool current = lit && i == line_of(m_caret);
                draw::text(dc, {inner.x, box.y + i * m_line_height - m_scroll_y, m_gutter - draw::px(6), m_line_height}, number,
                    current ? colours.text : blend_colour(dim, face, 0.25f), draw::text_right);
            }

            draw::unclip(dc);
        }

        draw::clip(dc, {inner.x + m_gutter, box.y, inner.width - m_gutter - draw::px(k_bar_column), box.height});

        if (m_length == 0)
        {
            if (m_placeholder != nullptr)
                draw::text(dc, {box.x, box.y, box.width, m_line_height}, m_placeholder, dim, draw::text_ellipsis);
        }

        const std::int32_t gap = draw::px(4);
        const std::uint32_t band = blend_colour(face, colours.accent, 0.75f);
        const std::int32_t begin = selection_begin();
        const std::int32_t end = selection_end();
        const std::int32_t caret_line = line_of(m_caret);

        ensure_syntax();
        std::uint32_t scheme[k_classes];
        fill_palette(scheme, face, ink);
        const std::uint32_t* palette = m_highlight != nullptr && enabled() ? scheme : nullptr;

        for (std::int32_t i = first; i < last; ++i)
        {
            const std::int32_t start = std::min(m_lines[i].start, m_length);
            const std::int32_t stop = std::min(m_lines[i].end, m_length);
            const std::int32_t y = box.y + i * m_line_height - m_scroll_y;
            const std::int32_t left = box.x - m_scroll_x - m_x[start];
            const std::int32_t sb = std::max(begin, start);
            const std::int32_t se = std::min(end, stop);
            const bool selected = lit && has_selection() && sb < se;
            // A selection that goes on past the end of the line takes its break too, drawn as a little extra band.
            const bool takes_break = lit && has_selection() && stop < m_length && m_text[stop] == L'\n' && begin <= stop && end > stop;

            if (selected || takes_break)
            {
                const std::int32_t x0 = selected ? left + m_x[sb] : left + m_x[stop];
                const std::int32_t x1 = (selected ? left + m_x[se] : left + m_x[stop]) + (takes_break ? draw::px(5) : 0);
                draw::fill(dc, {x0, y, x1 - x0, m_line_height}, band);
            }

            if (stop > start)
            {
                if (selected)
                {
                    if (sb > start)
                        paint_run(dc, left, y, start, sb, gap, false, 0, palette, ink);

                    paint_run(dc, left, y, sb, se, gap, true, white, palette, ink);

                    if (se < stop)
                        paint_run(dc, left, y, se, stop, gap, false, 0, palette, ink);
                }
                else
                {
                    paint_run(dc, left, y, start, stop, gap, false, 0, palette, ink);
                }
            }

            if (lit && m_blink_on && enabled() && i == caret_line)
            {
                const std::int32_t inset = draw::px(3);
                draw::fill(dc, {left + m_x[std::clamp(m_caret, 0, m_length)], y + inset, std::max(draw::px(1), 1), m_line_height - 2 * inset}, ink);
            }
        }

        draw::unclip(dc);

        // Longer than it shows: a thin bar down the right edge, as a list has.
        if (content_height() > view_height())
        {
            const thumb_span span = thumb();
            const std::int32_t width = draw::px(k_thumb_width);
            const std::int32_t column = draw::px(k_bar_column);
            const std::uint32_t thumb_colour = m_bar_drag ? colours.text_dim : blend_colour(colours.text_dim, face, hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {inner.x + inner.width - (column + width) / 2 - draw::px(1), span.start, width, span.length}, 2, thumb_colour, thumb_colour);
        }
    }
}
