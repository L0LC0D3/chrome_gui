#include <chrome/controls/core/quick_open.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_width = 600;
        constexpr std::int32_t k_margin = 24;
        constexpr std::int32_t k_top = 8;
        constexpr std::int32_t k_input_height = 34;
        constexpr std::int32_t k_row_height = 24;
        constexpr std::int32_t k_inset = 4;
        constexpr std::int32_t k_radius = 6;
        constexpr std::int32_t k_bar = 4;

        [[nodiscard]] char lower(char c) noexcept
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        [[nodiscard]] bool separator(char c) noexcept
        {
            return c == '/' || c == '_' || c == '-' || c == '.' || c == ' ';
        }
    }

    quick_open& quick_open::set_source(label_fn labels, void* user, std::int32_t count) noexcept
    {
        m_labels = labels;
        m_labels_user = user;
        m_count = std::clamp(count, 0, k_max_items);
        return *this;
    }

    void quick_open::place(const dock_rect& bounds) noexcept
    {
        m_bounds = bounds;

        if (!m_open)
        {
            set_area({});
            return;
        }

        const std::int32_t width = std::max(std::min(draw::px(k_width), std::max(bounds.width - 2 * draw::px(k_margin), draw::px(200))), 1);
        const std::int32_t rows = std::max(visible_rows(), 1);
        const std::int32_t height = draw::px(k_input_height) + 1 + rows * draw::px(k_row_height) + 2 * draw::px(k_inset);
        set_area({bounds.x + (bounds.width - width) / 2, bounds.y + draw::px(k_top), width, height});
    }

    std::int32_t quick_open::visible_rows() const noexcept
    {
        return std::min(m_match_count, k_max_rows);
    }

    void quick_open::open() noexcept
    {
        m_open = true;
        m_query[0] = '\0';
        m_query_length = 0;
        refilter();
        place(m_bounds);
        invalidate();
    }

    void quick_open::cancel() noexcept
    {
        if (!m_open)
            return;

        m_open = false;
        set_area({});
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_handler_user, *this, -1);
    }

    // How well the query is found in the label, or -1 when it is not in it.
    std::int32_t quick_open::score(const char* label) const noexcept
    {
        if (m_query_length == 0)
            return 0;

        const char* name = std::strrchr(label, '/');
        const std::int32_t name_start = name != nullptr ? static_cast<std::int32_t>(name - label) + 1 : 0;
        std::int32_t total = 0;
        std::int32_t matched = 0;
        std::int32_t previous = -2;
        std::int32_t length = 0;

        for (std::int32_t i = 0; label[i] != '\0'; ++i, ++length)
        {
            if (matched >= m_query_length || lower(label[i]) != m_query[matched])
                continue;

            std::int32_t gain = 1;

            if (i == previous + 1)
                gain += 5;

            if (i == 0 || separator(label[i - 1]))
                gain += 8;

            if (i >= name_start)
                gain += 3;

            total += gain;
            previous = i;
            ++matched;
        }

        if (matched < m_query_length)
            return -1;

        // The query standing whole in the file name is the best there is.
        for (std::int32_t i = name_start; label[i] != '\0'; ++i)
        {
            std::int32_t k = 0;

            while (k < m_query_length && label[i + k] != '\0' && lower(label[i + k]) == m_query[k])
                ++k;

            if (k == m_query_length)
            {
                total += 20 + (i == name_start ? 10 : 0);
                break;
            }
        }

        while (label[length] != '\0')
            ++length;

        return std::max(total * 4 - length / 4, 0);
    }

    void quick_open::refilter() noexcept
    {
        m_match_count = 0;

        for (std::int32_t i = 0; i < m_count && m_labels != nullptr; ++i)
        {
            const char* label = m_labels(m_labels_user, i);

            if (label == nullptr)
                continue;

            const std::int32_t s = score(label);

            if (s < 0)
                continue;

            m_scores[i] = s;
            m_matches[m_match_count++] = static_cast<std::int16_t>(i);
        }

        if (m_query_length > 0)
        {
            std::stable_sort(m_matches, m_matches + m_match_count, [this](std::int16_t a, std::int16_t b) noexcept { return m_scores[a] > m_scores[b]; });
        }

        m_selected = 0;
        m_first = 0;
    }

    void quick_open::choose(std::int32_t row) noexcept
    {
        if (!m_open || row < 0 || row >= m_match_count)
            return;

        const std::int32_t item = m_matches[row];
        m_open = false;
        set_area({});
        invalidate();

        if (m_handler != nullptr)
            m_handler(m_handler_user, *this, item);
    }

    void quick_open::move(std::int32_t by, bool wrap) noexcept
    {
        if (m_match_count == 0)
            return;

        std::int32_t next = m_selected + by;
        next = wrap ? (next + m_match_count) % m_match_count : std::clamp(next, 0, m_match_count - 1);
        m_selected = next;
        const std::int32_t rows = visible_rows();

        if (next < m_first)
            m_first = next;
        else if (next >= m_first + rows)
            m_first = next - rows + 1;

        invalidate();
    }

    std::int32_t quick_open::row_at(std::int32_t x, std::int32_t y) const noexcept
    {
        const dock_rect box = area();
        const std::int32_t top = box.y + draw::px(k_input_height) + 1 + draw::px(k_inset);

        if (x < box.x || x >= box.x + box.width || y < top)
            return -1;

        const std::int32_t row = m_first + (y - top) / draw::px(k_row_height);
        return row < m_match_count && row < m_first + visible_rows() ? row : -1;
    }

    void quick_open::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        choose(row_at(x, y));
    }

    bool quick_open::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t row = row_at(x, y);

        if (row < 0 || row == m_selected)
            return false;

        m_selected = row;
        return true;
    }

    bool quick_open::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        m_first = std::clamp(m_first - notches * 3, 0, std::max(m_match_count - visible_rows(), 0));
        return true;
    }

    bool quick_open::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        (void)shift;

        if (!m_open || alt)
            return false;

        const std::int32_t page = std::max(visible_rows() - 1, 1);

        switch (key)
        {
        case VK_UP:
            move(-1, true);
            return true;
        case VK_DOWN:
            move(1, true);
            return true;
        case VK_PRIOR:
            move(-page, false);
            return true;
        case VK_NEXT:
            move(page, false);
            return true;
        case VK_HOME:
            move(-m_match_count, false);
            return true;
        case VK_END:
            move(m_match_count, false);
            return true;
        case VK_RETURN:
            choose(m_selected);
            return true;
        case VK_ESCAPE:
            cancel();
            return true;
        case VK_BACK:
            if (m_query_length > 0)
            {
                // Ctrl takes the whole query.
                m_query_length = ctrl ? 0 : m_query_length - 1;
                m_query[m_query_length] = '\0';
                refilter();
                invalidate();
            }

            return true;
        case 'P':
            // The key that opened it moves down, as in an editor.
            if (ctrl)
            {
                move(1, true);
                return true;
            }

            return false;
        default:
            // Typing goes through on_char, and the other keys stay with the box.
            return !ctrl;
        }
    }

    void quick_open::on_char(std::uint32_t code) noexcept
    {
        if (!m_open || code < 32 || code >= 127 || m_query_length >= k_query - 1)
            return;

        m_query[m_query_length++] = lower(static_cast<char>(code));
        m_query[m_query_length] = '\0';
        refilter();
        invalidate();
    }

    void quick_open::on_focus(bool gained) noexcept
    {
        // Going to something else shuts the box.
        if (!gained)
            cancel();
    }

    void quick_open::paint_overlay(void* dc) const noexcept
    {
        if (!m_open)
            return;

        const theme_colours& colours = theme();
        const dock_rect box = area();
        const std::uint32_t face = blend_colour(colours.panel, colours.title, 0.8f);
        const std::int32_t pad = draw::px(12);
        const std::int32_t input_height = draw::px(k_input_height);
        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t inset = draw::px(k_inset);

        draw::shadow(dc, box, k_radius, 12, 4, 0.55f);
        draw::rounded(dc, box, k_radius, face, colours.accent);
        draw::clip(dc, box);

        // What is typed, with a caret after it, and how many of the items it leaves.
        char count[32];
        std::snprintf(count, sizeof(count), "%d / %d", m_match_count, m_count);
        const std::int32_t count_width = draw::text_width(dc, count) + pad;
        draw::text(dc, {box.x + box.width - count_width - pad / 2, box.y, count_width, input_height}, count, colours.text_dim, draw::text_right);

        if (m_query_length == 0)
        {
            draw::text(dc, {box.x + pad, box.y, box.width - 2 * pad - count_width, input_height}, m_placeholder, colours.text_dim, draw::text_ellipsis);
            draw::fill(dc, {box.x + pad, box.y + draw::px(8), std::max(draw::px(1), 1), input_height - 2 * draw::px(8)}, colours.text);
        }
        else
        {
            draw::text(dc, {box.x + pad, box.y, box.width - 2 * pad - count_width, input_height}, m_query, colours.text, draw::text_ellipsis);
            const std::int32_t at = std::min(draw::text_width(dc, m_query), box.width - 2 * pad - count_width);
            draw::fill(dc, {box.x + pad + at + 1, box.y + draw::px(8), std::max(draw::px(1), 1), input_height - 2 * draw::px(8)}, colours.text);
        }

        draw::fill(dc, {box.x, box.y + input_height, box.width, 1}, blend_colour(colours.text_dim, face, 0.7f));

        const std::int32_t top = box.y + input_height + 1 + inset;

        if (m_match_count == 0)
        {
            draw::text(dc, {box.x + pad, top, box.width - 2 * pad, row_height}, "No matching files", colours.text_dim);
            draw::unclip(dc);
            return;
        }

        const bool scrolls = m_match_count > visible_rows();
        const std::int32_t bar = scrolls ? draw::px(k_bar + 4) : 0;

        for (std::int32_t i = 0; i < visible_rows(); ++i)
        {
            const std::int32_t row_index = m_first + i;
            const char* label = m_labels(m_labels_user, m_matches[row_index]);
            const dock_rect row{box.x + inset, top + i * row_height, box.width - 2 * inset - bar, row_height};
            const bool chosen = row_index == m_selected;

            if (chosen)
                draw::rounded(dc, row, 3, colours.accent, colours.accent);

            // The file name, and the folder it is in after it, paler.
            const char* slash = std::strrchr(label, '/');
            const char* name = slash != nullptr ? slash + 1 : label;
            const std::int32_t text_x = row.x + pad - inset;
            const std::int32_t name_width = draw::text_width(dc, name);
            draw::text(dc, {text_x, row.y, std::min(name_width + 4, row.width - pad), row.height}, name, chosen ? 0xFFFFFF : colours.text, draw::text_ellipsis);

            if (slash != nullptr)
            {
                char folder[160];
                const std::size_t length = std::min<std::size_t>(static_cast<std::size_t>(slash - label), sizeof(folder) - 1);
                std::memcpy(folder, label, length);
                folder[length] = '\0';
                const std::int32_t folder_x = text_x + name_width + draw::px(10);
                draw::text(dc, {folder_x, row.y, std::max(row.x + row.width - folder_x - pad / 2, 0), row.height}, folder, chosen ? blend_colour(0xFFFFFF, colours.accent, 0.25f) : colours.text_dim,
                    draw::text_ellipsis);
            }
        }

        if (scrolls)
        {
            const std::int32_t track = visible_rows() * row_height;
            const std::int32_t thumb = std::max(track * visible_rows() / m_match_count, draw::px(10));
            const std::int32_t span = m_match_count - visible_rows();
            const std::int32_t at = span > 0 ? (track - thumb) * m_first / span : 0;
            const std::uint32_t colour = blend_colour(colours.text_dim, 0x000000, 0.2f);
            draw::rounded(dc, {box.x + box.width - inset - draw::px(k_bar + 1), top + at, draw::px(k_bar), thumb}, 2, colour, colour);
        }

        draw::unclip(dc);
    }
}
