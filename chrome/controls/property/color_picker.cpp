#include <chrome/controls/property/color_picker.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace chrome
{
    namespace
    {
        // The layout, at 96 dpi. Everything is placed from the top left corner of the window.
        constexpr std::int32_t k_width = 300;
        constexpr std::int32_t k_pad = 10;
        constexpr std::int32_t k_square_width = 190;
        constexpr std::int32_t k_square_height = 160;
        constexpr std::int32_t k_hue_width = 18;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_preview_width = k_width - 2 * k_pad - k_square_width - k_hue_width - 2 * k_gap;
        constexpr std::int32_t k_rows_top = k_pad + k_square_height + 12;
        constexpr std::int32_t k_row = 26;
        constexpr std::int32_t k_track_height = 10;
        constexpr std::int32_t k_letter_width = 22;
        constexpr std::int32_t k_field_width = 60;
        constexpr std::int32_t k_field_height = 24;
        constexpr std::int32_t k_hex_label = 34;
        constexpr std::int32_t k_hex_width = 110;
        constexpr std::int32_t k_mode_width = 44;
        constexpr std::int32_t k_mode_height = 22;
        constexpr std::int32_t k_pipette_width = 28;
        constexpr std::int32_t k_preset_size = 24;
        constexpr std::int32_t k_preset_gap = 4;
        constexpr std::int32_t k_thumb_radius = 6;
        constexpr std::int32_t k_radius = 4;

        constexpr std::uint32_t k_key_escape = 0x1B;
        constexpr std::uint32_t k_key_enter = 0x0D;

        constexpr std::uint32_t k_preset_colours[10] = {0xFFFFFF, 0x808080, 0x000000, 0xE53935, 0xFB8C00, 0xFDD835, 0x43A047, 0x00ACC1, 0x1E88E5, 0x8E24AA};

        [[nodiscard]] bool contains_part(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }

        [[nodiscard]] float fraction(std::int32_t at, std::int32_t start, std::int32_t length) noexcept
        {
            return length > 1 ? std::clamp(static_cast<float>(at - start) / static_cast<float>(length - 1), 0.0f, 1.0f) : 0.0f;
        }

        // Fills a rectangle with one colour for each pixel, made by the callable from the pixel's place in it.
        template <class Pixel>
        void blit(void* dc, const dock_rect& area, Pixel&& pixel) noexcept
        {
            static constexpr std::int32_t k_limit = 512 * 512;
            static std::uint32_t buffer[k_limit];

            if (area.width <= 0 || area.height <= 0 || area.width * area.height > k_limit)
                return;

            for (std::int32_t y = 0; y < area.height; ++y)
            {
                for (std::int32_t x = 0; x < area.width; ++x)
                    buffer[y * area.width + x] = pixel(x, y);
            }

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = area.width;
            info.bmiHeader.biHeight = -area.height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(static_cast<HDC>(dc), area.x, area.y, static_cast<DWORD>(area.width), static_cast<DWORD>(area.height), 0, 0, 0,
                static_cast<UINT>(area.height), buffer, &info, DIB_RGB_COLORS);
        }

        // A line of a colour round the edge of a rectangle.
        void outline(void* dc, const dock_rect& area, std::uint32_t colour) noexcept
        {
            draw::fill(dc, {area.x, area.y, area.width, 1}, colour);
            draw::fill(dc, {area.x, area.y + area.height - 1, area.width, 1}, colour);
            draw::fill(dc, {area.x, area.y, 1, area.height}, colour);
            draw::fill(dc, {area.x + area.width - 1, area.y, 1, area.height}, colour);
        }

        void ring(void* dc, float cx, float cy, float radius, std::uint32_t colour, float width) noexcept
        {
            constexpr std::uint32_t k_points = 24;
            draw::point points[k_points + 1];

            for (std::uint32_t i = 0; i <= k_points; ++i)
            {
                const float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(k_points);
                points[i] = {cx + std::cos(angle) * radius, cy + std::sin(angle) * radius};
            }

            draw::polyline(dc, points, k_points + 1, colour, width);
        }

        [[nodiscard]] std::uint32_t pack(float r, float g, float b) noexcept
        {
            return to_rgb24({r, g, b, 1.0f});
        }
    }

    color_picker::color_picker() noexcept
    {
        for (std::uint32_t i = 0; i < k_channels; ++i)
        {
            m_fields[i].set_buttons(false).set_decimals(0).set_range(0.0, 255.0).set_step(1.0).set_on_change(&color_picker::channel_changed, this);
            const bool added = m_inner.add(m_fields[i]);
            (void)added;
        }

        m_hex.set_max_length(8).set_width(k_hex_width).set_name("Hex");
        m_hex.set_on_action(&color_picker::hex_acted, this);
        m_hex.set_filter(&color_picker::hex_filter, this);
        const bool added = m_inner.add(m_hex);
        (void)added;
    }

    void color_picker::open(const dock_rect& anchor, const dock_rect& bounds, const color_rgba& color, bool alpha, change_fn handler, void* user) noexcept
    {
        m_open = true;
        m_anchor = anchor;
        m_bounds = bounds;
        m_handler = handler;
        m_user = user;
        m_alpha = alpha;
        m_color = color;
        m_color.a = alpha ? std::clamp(color.a, 0.0f, 1.0f) : 1.0f;
        m_original = m_color;
        m_hsv = to_hsv(m_color, m_hsv.h);
        m_drag = drag_kind::none;
        m_inner.blur();
        set_mode(m_hsv_mode);
    }

    void color_picker::close() noexcept
    {
        if (!m_open)
            return;

        // Leaving a field commits what was typed in it.
        blur_fields();
        m_open = false;
        end_pipette(false);
        m_drag = drag_kind::none;
    }

    void color_picker::set_color(const color_rgba& color) noexcept
    {
        m_color = color;
        m_color.a = m_alpha ? std::clamp(color.a, 0.0f, 1.0f) : 1.0f;
        m_hsv = to_hsv(m_color, m_hsv.h);
        sync_fields();
    }

    void color_picker::blur_fields() noexcept
    {
        m_inner.blur();
    }

    std::int32_t color_picker::height96() const noexcept
    {
        const std::int32_t hex_top = k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8;
        const std::int32_t presets_top = hex_top + k_field_height + 8;
        return presets_top + k_preset_size + k_pad;
    }

    dock_rect color_picker::area() const noexcept
    {
        const std::int32_t width = draw::px(k_width);
        const std::int32_t height = draw::px(height96());
        std::int32_t x = m_anchor.x;
        std::int32_t y = m_anchor.y + m_anchor.height + draw::px(2);

        if (m_bounds.width > 0)
            x = std::max(std::min(x, m_bounds.x + m_bounds.width - width), m_bounds.x);

        // Not enough room below, and more above: the window opens upward.
        if (m_bounds.height > 0 && y + height > m_bounds.y + m_bounds.height)
        {
            const std::int32_t above = m_anchor.y - draw::px(2) - height;

            if (above >= m_bounds.y || m_anchor.y - m_bounds.y > m_bounds.y + m_bounds.height - y)
                y = std::max(above, m_bounds.y);
        }

        return {x, y, width, height};
    }

    bool color_picker::contains(std::int32_t x, std::int32_t y) const noexcept
    {
        return m_open && contains_part(area(), x, y);
    }

    dock_rect color_picker::part(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) const noexcept
    {
        const dock_rect box = area();
        return {box.x + draw::px(x), box.y + draw::px(y), draw::px(width), draw::px(height)};
    }

    dock_rect color_picker::square_rect() const noexcept
    {
        return part(k_pad, k_pad, k_square_width, k_square_height);
    }

    dock_rect color_picker::hue_rect() const noexcept
    {
        return part(k_pad + k_square_width + k_gap, k_pad, k_hue_width, k_square_height);
    }

    dock_rect color_picker::new_rect() const noexcept
    {
        const std::int32_t half = (k_square_height - 4) / 2;
        return part(k_pad + k_square_width + k_gap + k_hue_width + k_gap, k_pad, k_preview_width, half);
    }

    dock_rect color_picker::old_rect() const noexcept
    {
        const std::int32_t half = (k_square_height - 4) / 2;
        return part(k_pad + k_square_width + k_gap + k_hue_width + k_gap, k_pad + half + 4, k_preview_width, half);
    }

    dock_rect color_picker::track_rect(std::uint32_t channel) const noexcept
    {
        const std::int32_t row = k_rows_top + static_cast<std::int32_t>(channel) * k_row;
        const std::int32_t left = k_pad + k_letter_width;
        const std::int32_t right = k_width - k_pad - k_field_width - 8;
        return part(left, row + (k_row - k_track_height) / 2, right - left, k_track_height);
    }

    dock_rect color_picker::hex_rect() const noexcept
    {
        const std::int32_t hex_top = k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8;
        return part(k_pad + k_hex_label, hex_top, k_hex_width, k_field_height);
    }

    dock_rect color_picker::mode_rect(bool hsv) const noexcept
    {
        const std::int32_t hex_top = k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8;
        const std::int32_t right = k_width - k_pad - k_mode_width;
        return part(hsv ? right : right - k_mode_width - 2, hex_top + (k_field_height - k_mode_height) / 2, k_mode_width, k_mode_height);
    }

    dock_rect color_picker::pipette_rect() const noexcept
    {
        const std::int32_t hex_top = k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8;
        return part(k_pad + k_hex_label + k_hex_width + 6, hex_top + (k_field_height - k_mode_height) / 2, k_pipette_width, k_mode_height);
    }

    dock_rect color_picker::preset_rect(std::uint32_t index) const noexcept
    {
        const std::int32_t hex_top = k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8;
        const std::int32_t top = hex_top + k_field_height + 8;
        const std::int32_t span = k_width - 2 * k_pad;
        const std::int32_t size = (span - static_cast<std::int32_t>(k_presets - 1) * k_preset_gap) / static_cast<std::int32_t>(k_presets);
        return part(k_pad + static_cast<std::int32_t>(index) * (size + k_preset_gap), top, size, k_preset_size);
    }

    void color_picker::place() const noexcept
    {
        for (std::uint32_t i = 0; i < k_channels; ++i)
        {
            const std::int32_t row = k_rows_top + static_cast<std::int32_t>(i) * k_row;
            m_fields[i].set_area(i < channel_count() ? part(k_width - k_pad - k_field_width, row + (k_row - k_field_height) / 2, k_field_width, k_field_height) : dock_rect{});
        }

        m_hex.set_area(hex_rect());
    }

    float color_picker::channel_value(std::uint32_t channel) const noexcept
    {
        if (channel == 3)
            return m_color.a;

        if (m_hsv_mode)
            return channel == 0 ? m_hsv.h : channel == 1 ? m_hsv.s : m_hsv.v;

        return channel == 0 ? m_color.r : channel == 1 ? m_color.g : m_color.b;
    }

    void color_picker::apply_hsv() noexcept
    {
        m_color = from_hsv(m_hsv, m_color.a);
    }

    void color_picker::apply_rgb() noexcept
    {
        m_hsv = to_hsv(m_color, m_hsv.h);
    }

    void color_picker::set_channel(std::uint32_t channel, float value) noexcept
    {
        value = std::clamp(value, 0.0f, 1.0f);

        if (channel == 3)
        {
            m_color.a = m_alpha ? value : 1.0f;
        }
        else if (m_hsv_mode)
        {
            (channel == 0 ? m_hsv.h : channel == 1 ? m_hsv.s : m_hsv.v) = value;
            apply_hsv();
        }
        else
        {
            (channel == 0 ? m_color.r : channel == 1 ? m_color.g : m_color.b) = value;
            apply_rgb();
        }
    }

    void color_picker::set_mode(bool hsv) noexcept
    {
        m_hsv_mode = hsv;

        for (std::uint32_t i = 0; i < k_channels; ++i)
        {
            const double most = hsv && i == 0 ? 360.0 : hsv && i < 3 ? 100.0 : 255.0;
            m_fields[i].set_range(0.0, most);
        }

        sync_fields();
    }

    void color_picker::sync_fields() noexcept
    {
        for (std::uint32_t i = 0; i < channel_count(); ++i)
        {
            if (m_inner.focused() == &m_fields[i])
                continue;

            const double scale = m_hsv_mode && i == 0 ? 360.0 : m_hsv_mode && i < 3 ? 100.0 : 255.0;
            m_fields[i].set_value(std::round(static_cast<double>(channel_value(i)) * scale));
        }

        if (m_inner.focused() != &m_hex)
        {
            char text[16];

            if (m_alpha)
                std::snprintf(text, sizeof(text), "%06X%02X", to_rgb24(m_color), to_byte(m_color.a));
            else
                std::snprintf(text, sizeof(text), "%06X", to_rgb24(m_color));

            m_hex.set_text(text);
        }
    }

    void color_picker::changed() noexcept
    {
        sync_fields();

        if (m_handler != nullptr)
            m_handler(m_user, *this);
    }

    void color_picker::drag_to(std::int32_t x, std::int32_t y) noexcept
    {
        if (m_drag == drag_kind::square)
        {
            const dock_rect box = square_rect();
            m_hsv.s = fraction(x, box.x, box.width);
            m_hsv.v = 1.0f - fraction(y, box.y, box.height);
            apply_hsv();
        }
        else if (m_drag == drag_kind::hue)
        {
            const dock_rect box = hue_rect();
            m_hsv.h = fraction(y, box.y, box.height);
            apply_hsv();
        }
        else if (m_drag == drag_kind::channel)
        {
            const dock_rect box = track_rect(m_drag_channel);
            set_channel(m_drag_channel, fraction(x, box.x, box.width));
        }
        else
        {
            return;
        }

        changed();
    }

    void color_picker::pick_screen() noexcept
    {
        POINT where{};

        if (!GetCursorPos(&where))
            return;

        HDC screen = GetDC(nullptr);

        if (screen == nullptr)
            return;

        const COLORREF pixel = GetPixel(screen, where.x, where.y);
        ReleaseDC(nullptr, screen);

        if (pixel == CLR_INVALID)
            return;

        m_loupe.show(where.x, where.y, static_cast<std::uint32_t>((GetRValue(pixel) << 16) | (GetGValue(pixel) << 8) | GetBValue(pixel)));
        m_color.r = static_cast<float>(GetRValue(pixel)) / 255.0f;
        m_color.g = static_cast<float>(GetGValue(pixel)) / 255.0f;
        m_color.b = static_cast<float>(GetBValue(pixel)) / 255.0f;
        apply_rgb();
        changed();
    }

    void color_picker::end_pipette(bool restore) noexcept
    {
        m_loupe.hide();

        if (m_drag != drag_kind::pipette)
            return;

        m_drag = drag_kind::none;

        if (restore)
        {
            m_color = m_before_pipette;
            apply_rgb();
            changed();
        }
    }

    void color_picker::commit_hex() noexcept
    {
        char text[32]{};
        (void)m_hex.get_text(text, sizeof(text));
        const char* digits = text[0] == '#' ? text + 1 : text;
        const std::size_t length = std::strlen(digits);

        // Six digits are a colour, and eight add the alpha. Anything else goes back to the colour.
        if (length != 6 && length != 8)
        {
            char shown[16];
            std::snprintf(shown, sizeof(shown), m_alpha ? "%06X%02X" : "%06X", to_rgb24(m_color), to_byte(m_color.a));
            m_hex.set_text(shown);
            return;
        }

        char pair[3]{};
        std::uint32_t parts[4]{0, 0, 0, 255};

        for (std::size_t i = 0; i * 2 < length; ++i)
        {
            pair[0] = digits[i * 2];
            pair[1] = digits[i * 2 + 1];
            parts[i] = static_cast<std::uint32_t>(std::strtoul(pair, nullptr, 16));
        }

        m_color.r = static_cast<float>(parts[0]) / 255.0f;
        m_color.g = static_cast<float>(parts[1]) / 255.0f;
        m_color.b = static_cast<float>(parts[2]) / 255.0f;

        if (m_alpha && length == 8)
            m_color.a = static_cast<float>(parts[3]) / 255.0f;

        apply_rgb();
        changed();
    }

    void color_picker::channel_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<color_picker*>(user);

        for (std::uint32_t i = 0; i < k_channels; ++i)
        {
            if (&self.m_fields[i] != &source)
                continue;

            const double scale = self.m_hsv_mode && i == 0 ? 360.0 : self.m_hsv_mode && i < 3 ? 100.0 : 255.0;
            self.set_channel(i, static_cast<float>(source.value() / scale));
            self.changed();
            return;
        }
    }

    bool color_picker::hex_acted(void* user, text_field& source, text_action action) noexcept
    {
        (void)source;

        if (action == text_action::submitted || action == text_action::blurred)
            static_cast<color_picker*>(user)->commit_hex();

        return true;
    }

    std::int32_t color_picker::hex_filter(void* user, text_field& source, wchar_t* text, std::int32_t length) noexcept
    {
        (void)user;
        (void)source;
        std::int32_t kept = 0;

        for (std::int32_t i = 0; i < length; ++i)
        {
            wchar_t c = text[i];

            if (c >= L'a' && c <= L'f')
                c = static_cast<wchar_t>(c - L'a' + L'A');

            if ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F'))
                text[kept++] = c;
        }

        return kept;
    }

    bool color_picker::on_mouse(const panel_mouse& mouse) noexcept
    {
        if (!m_open)
            return false;

        place();

        switch (mouse.event)
        {
        case mouse_event::down:
        case mouse_event::double_click:
        {
            if (mouse.button != 0)
                return false;

            const std::int32_t x = mouse.x;
            const std::int32_t y = mouse.y;
            m_drag = drag_kind::none;

            if (contains_part(square_rect(), x, y) || contains_part(hue_rect(), x, y))
            {
                blur_fields();
                m_drag = contains_part(square_rect(), x, y) ? drag_kind::square : drag_kind::hue;
                drag_to(x, y);
                return true;
            }

            for (std::uint32_t c = 0; c < channel_count(); ++c)
            {
                dock_rect track = track_rect(c);

                // The thumb sticks out of the track, so the row is as easy to hit as the track is long.
                track.y -= draw::px(4);
                track.height += draw::px(8);
                track.x -= draw::px(k_thumb_radius);
                track.width += draw::px(2 * k_thumb_radius);

                if (contains_part(track, x, y))
                {
                    blur_fields();
                    m_drag = drag_kind::channel;
                    m_drag_channel = c;
                    drag_to(x, y);
                    return true;
                }
            }

            if (contains_part(pipette_rect(), x, y))
            {
                blur_fields();
                m_drag = drag_kind::pipette;
                m_before_pipette = m_color;
                pick_screen();
                return true;
            }

            if (contains_part(old_rect(), x, y))
            {
                blur_fields();
                m_color = m_original;
                apply_rgb();
                changed();
                return true;
            }

            for (std::uint32_t i = 0; i < k_presets; ++i)
            {
                if (contains_part(preset_rect(i), x, y))
                {
                    blur_fields();
                    const float alpha = m_color.a;
                    m_color = from_rgb24(k_preset_colours[i], alpha);
                    apply_rgb();
                    changed();
                    return true;
                }
            }

            for (const bool hsv : {false, true})
            {
                if (contains_part(mode_rect(hsv), x, y))
                {
                    blur_fields();

                    if (m_hsv_mode != hsv)
                        set_mode(hsv);

                    return true;
                }
            }

            // Anywhere else goes to the fields, which take the focus when they are hit and give it up when they are not.
            (void)m_inner.on_mouse(mouse);
            m_drag = m_inner.focused() != nullptr ? drag_kind::field : drag_kind::none;
            return true;
        }

        case mouse_event::move:
            if (m_drag == drag_kind::square || m_drag == drag_kind::hue || m_drag == drag_kind::channel)
            {
                drag_to(mouse.x, mouse.y);
                return true;
            }

            if (m_drag == drag_kind::pipette)
            {
                pick_screen();
                return true;
            }

            return m_inner.on_mouse(mouse);

        case mouse_event::up:
            if (mouse.button != 0)
                return false;

            if (m_drag == drag_kind::field)
                (void)m_inner.on_mouse(mouse);

            m_loupe.hide();
            m_drag = drag_kind::none;
            return true;

        case mouse_event::leave:
            return m_inner.on_mouse(mouse);

        case mouse_event::wheel:
            break;
        }

        return false;
    }

    bool color_picker::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (!m_open)
            return false;

        place();

        // Escape in the middle of a pipette drop ends it and the loupe, and the picker stays.
        if (m_drag == drag_kind::pipette)
        {
            if (key == k_key_escape)
                end_pipette(true);

            return key == k_key_escape;
        }

        if (m_inner.focused() != nullptr)
        {
            const bool used = m_inner.on_key(key, ctrl, shift, alt);

            // Enter in a field has committed it, and it keeps the focus for more typing.
            return used;
        }

        // With no field in use the window itself takes Escape and Enter, which close it.
        if (key == k_key_escape || key == k_key_enter)
        {
            close();
            return true;
        }

        return m_inner.on_key(key, ctrl, shift, alt);
    }

    void color_picker::on_char(std::uint32_t code) noexcept
    {
        if (m_open)
            m_inner.on_char(code);
    }

    bool color_picker::animate(std::uint64_t now_ms) noexcept
    {
        if (!m_open)
            return false;

        place();
        return m_inner.animate(now_ms);
    }

    void color_picker::paint(void* dc) const noexcept
    {
        if (!m_open)
            return;

        place();
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const std::uint32_t line = blend_colour(colours.title, black, 0.6f);
        const dock_rect box = area();
        draw::rounded(dc, box, k_radius, blend_colour(colours.panel, colours.title, 0.9f), blend_colour(colours.title, black, 0.5f));

        // The saturation and the value: white to the hue along the top, and down to black.
        const dock_rect square = square_rect();
        const float hue = m_hsv.h;
        blit(dc, square, [&](std::int32_t x, std::int32_t y) {
            const color_rgba c = from_hsv({hue, fraction(x, 0, square.width), 1.0f - fraction(y, 0, square.height)}, 1.0f);
            return pack(c.r, c.g, c.b);
        });
        outline(dc, square, line);
        const float marker_x = static_cast<float>(square.x) + m_hsv.s * static_cast<float>(square.width - 1) + 0.5f;
        const float marker_y = static_cast<float>(square.y) + (1.0f - m_hsv.v) * static_cast<float>(square.height - 1) + 0.5f;
        draw::clip(dc, square);
        ring(dc, marker_x, marker_y, static_cast<float>(draw::px(k_thumb_radius)) + 1.0f, black, 1.5f);
        ring(dc, marker_x, marker_y, static_cast<float>(draw::px(k_thumb_radius)), white, 1.5f);
        draw::unclip(dc);

        // The hue goes down the bar from red and back to it.
        const dock_rect bar = hue_rect();
        blit(dc, bar, [&](std::int32_t, std::int32_t y) {
            const color_rgba c = from_hsv({fraction(y, 0, bar.height), 1.0f, 1.0f}, 1.0f);
            return pack(c.r, c.g, c.b);
        });
        outline(dc, bar, line);
        const std::int32_t hue_y = bar.y + static_cast<std::int32_t>(m_hsv.h * static_cast<float>(bar.height - 1) + 0.5f);
        draw::fill(dc, {bar.x - draw::px(2), hue_y - draw::px(2), bar.width + draw::px(4), draw::px(5)}, black);
        draw::fill(dc, {bar.x - draw::px(1), hue_y - draw::px(1), bar.width + draw::px(2), draw::px(3)}, white);

        // The new colour over the old, each over the squares when it is not opaque.
        const auto preview = [&](const dock_rect& area, const color_rgba& colour) {
            if (m_alpha && colour.a < 1.0f)
                paint_checker(dc, area, draw::px(6), &colour);
            else
                draw::fill(dc, area, to_rgb24(colour));

            outline(dc, area, line);
        };
        preview(new_rect(), m_color);
        preview(old_rect(), m_original);

        // A slider for each channel, its track showing what the channel would give.
        const bool hsv_mode = m_hsv_mode;
        static const char* const k_rgba[] = {"R", "G", "B", "A"};
        static const char* const k_hsva[] = {"H", "S", "V", "A"};

        for (std::uint32_t c = 0; c < channel_count(); ++c)
        {
            const std::int32_t row = k_rows_top + static_cast<std::int32_t>(c) * k_row;
            draw::text(dc, part(k_pad, row, k_letter_width, k_row), hsv_mode ? k_hsva[c] : k_rgba[c], colours.text_dim);

            const dock_rect track = track_rect(c);
            const color_rgba base = m_color;
            const color_hsv hsv = m_hsv;
            const std::int32_t cell = draw::px(4);
            blit(dc, track, [&](std::int32_t x, std::int32_t y) {
                const float t = fraction(x, 0, track.width);

                if (c == 3)
                {
                    // Alpha: the colour over squares, from clear to solid.
                    const std::uint32_t ground = (((x / cell) + (y / cell)) & 1) != 0 ? 0x808080 : 0xB4B4B4;
                    const color_rgba back = from_rgb24(ground);
                    return pack(back.r + (base.r - back.r) * t, back.g + (base.g - back.g) * t, back.b + (base.b - back.b) * t);
                }

                color_rgba out = base;

                if (!hsv_mode)
                {
                    (c == 0 ? out.r : c == 1 ? out.g : out.b) = t;
                }
                else
                {
                    color_hsv shown = hsv;
                    (c == 0 ? shown.h : c == 1 ? shown.s : shown.v) = t;

                    // The hue track shows every hue at full strength, whatever the colour is.
                    if (c == 0)
                    {
                        shown.s = 1.0f;
                        shown.v = 1.0f;
                    }

                    out = from_hsv(shown, 1.0f);
                }

                return pack(out.r, out.g, out.b);
            });
            outline(dc, track, line);

            const float thumb_x = static_cast<float>(track.x) + channel_value(c) * static_cast<float>(track.width - 1) + 0.5f;
            const std::int32_t radius = draw::px(k_thumb_radius);
            draw::ellipse(dc, {static_cast<std::int32_t>(thumb_x) - radius, track.y + track.height / 2 - radius, 2 * radius, 2 * radius}, 0xF0F0F0, 0x303030);
        }

        // The hex field's label, and the two ways to read the sliders.
        draw::text(dc, part(k_pad, k_rows_top + static_cast<std::int32_t>(channel_count()) * k_row + 8, k_hex_label, k_field_height), "Hex", colours.text_dim);

        for (const bool hsv : {false, true})
        {
            const dock_rect button = mode_rect(hsv);
            const bool on = m_hsv_mode == hsv;
            const std::uint32_t face = on ? colours.accent : blend_colour(colours.panel, colours.hover, 0.55f);
            draw::rounded(dc, button, 3, face, on ? colours.accent : blend_colour(colours.title, black, 0.5f));
            draw::text(dc, button, hsv ? "HSV" : "RGB", on ? white : colours.text, draw::text_center);
        }

        // The pipette, lit while it is held down.
        {
            const dock_rect button = pipette_rect();
            const bool on = m_drag == drag_kind::pipette;
            draw::rounded(dc, button, 3, on ? colours.accent : blend_colour(colours.panel, colours.hover, 0.55f), on ? colours.accent : blend_colour(colours.title, black, 0.5f));
            const float cx = static_cast<float>(button.x) + static_cast<float>(button.width) * 0.5f;
            const float cy = static_cast<float>(button.y) + static_cast<float>(button.height) * 0.5f;
            const float u = static_cast<float>(draw::px(1));
            const std::uint32_t ink = on ? white : colours.text;
            const draw::point tube[2] = {{cx - 5.0f * u, cy + 5.0f * u}, {cx + 2.0f * u, cy - 2.0f * u}};
            const draw::point tip[2] = {{cx - 7.5f * u, cy + 7.5f * u}, {cx - 5.0f * u, cy + 5.0f * u}};
            const draw::point cap[2] = {{cx, cy - 4.5f * u}, {cx + 4.5f * u, cy}};
            const draw::point bulb[2] = {{cx + 3.0f * u, cy - 3.0f * u}, {cx + 6.0f * u, cy - 6.0f * u}};
            draw::polyline(dc, tube, 2, ink, 3.0f * u);
            draw::polyline(dc, tip, 2, ink, 1.5f * u);
            draw::polyline(dc, cap, 2, ink, 2.0f * u);
            draw::polyline(dc, bulb, 2, ink, 3.5f * u);
        }

        // Colours to start from.
        for (std::uint32_t i = 0; i < k_presets; ++i)
        {
            const dock_rect chip = preset_rect(i);
            draw::rounded(dc, chip, 3, k_preset_colours[i], blend_colour(colours.title, black, 0.5f));
        }

        m_inner.paint(dc);
    }
}
