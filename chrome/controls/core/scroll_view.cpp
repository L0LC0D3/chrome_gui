#include <chrome/controls/core/scroll_view.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

namespace chrome
{
    namespace
    {
        // One notch of the wheel, at 96 dpi.
        constexpr std::int32_t k_wheel_step = 64;
        // The time the content takes to cover most of the way to where it is going.
        constexpr float k_ease_ms = 60.0f;
    }

    scroll_view::scroll_view() noexcept
    {
        (void)m_set.add(m_bar);
        m_bar.set_on_change(&scroll_view::moved, this);
    }

    scroll_view::~scroll_view()
    {
        release_cache();
    }

    void scroll_view::release_cache() noexcept
    {
        const HDC dc = static_cast<HDC>(m_cache_dc);

        if (dc != nullptr)
        {
            SelectObject(dc, static_cast<HGDIOBJ>(m_cache_old));
            DeleteObject(static_cast<HGDIOBJ>(m_cache_bitmap));
            DeleteDC(dc);
        }

        m_cache_dc = nullptr;
        m_cache_bitmap = nullptr;
        m_cache_old = nullptr;
    }

    void scroll_view::paint_content(void* dc_handle, paint_fn paint, void* user) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        const std::int32_t width = std::max(m_view.width, 1);
        const std::int32_t height = std::clamp(std::max(m_content, m_view.height), 1, 16384);
        const theme_colours& colours = theme();
        const std::uint32_t key = colours.panel ^ (colours.text * 31u) ^ (colours.text_dim * 131u) ^ (colours.accent * 17u) ^
            (colours.title * 7u) ^ static_cast<std::uint32_t>(draw::px(100));

        if (m_cache_dc == nullptr || width != m_cache_width || height != m_cache_height || key != m_cache_key)
        {
            release_cache();
            const HDC cache = CreateCompatibleDC(dc);
            // A section of memory and not a bitmap of the device: those are limited in size, and a tall panel passes the limit, after which
            // drawing into it silently fails.
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            const HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);

            if (cache == nullptr || bitmap == nullptr)
            {
                if (bitmap != nullptr)
                    DeleteObject(bitmap);

                if (cache != nullptr)
                    DeleteDC(cache);

                return;
            }

            m_cache_dc = cache;
            m_cache_bitmap = bitmap;
            m_cache_old = SelectObject(cache, bitmap);
            m_cache_width = width;
            m_cache_height = height;
            m_cache_key = key;
            m_valid = {};
            m_dirty = true;
        }

        const HDC cache = static_cast<HDC>(m_cache_dc);

        const std::int32_t top = std::clamp(offset(), 0, std::max(height - m_view.height, 0));
        const view_range shown{top, std::min(top + m_view.height, height)};

        // The picture is drawn again when the content changed, or when the scroll has left the part that was drawn.
        if (m_dirty || !m_valid.covers(shown))
        {
            const view_range band = scroll_paint_range(top, m_view.height, height);
            const dock_rect area{0, band.top, width, band.bottom - band.top};
            // The limit of the last band can outlive its clip, so the new band is filled with none in force.
            SelectClipRgn(cache, nullptr);
            draw::fill(cache, area, colours.panel);
            SelectObject(cache, GetCurrentObject(dc, OBJ_FONT));
            SetBkMode(cache, TRANSPARENT);
            const draw::frame shapes(cache);
            draw::clip(cache, area);
            paint(user, cache, band);
            draw::unclip(cache);
            m_valid = band;
            m_dirty = false;
        }

        BitBlt(dc, m_view.x, m_view.y, std::min(width, m_view.width), m_view.height, cache, 0, top, SRCCOPY);
    }

    void scroll_view::moved(void* user, scrollbar& source) noexcept
    {
        // The thumb moved it by hand, so there is nothing to ease.
        auto& self = *static_cast<scroll_view*>(user);
        self.m_offset = static_cast<float>(source.position());
        self.m_target = self.m_offset;
    }

    void scroll_view::place_bar() noexcept
    {
        const std::int32_t thickness = scrollbar::thickness();
        m_bar.set_area({m_view.x + m_view.width - thickness, m_view.y, thickness, m_view.height});
        m_bar.set_range(m_content, m_view.height);
        m_bar.set_enabled(m_bar.needed());
    }

    void scroll_view::set_viewport(const dock_rect& area) noexcept
    {
        m_view = area;
        place_bar();
        m_target = std::clamp(m_target, 0.0f, static_cast<float>(max_offset()));
        m_offset = std::clamp(m_offset, 0.0f, static_cast<float>(max_offset()));
        m_bar.set_position(offset());
    }

    void scroll_view::set_content_height(std::int32_t height) noexcept
    {
        m_content = std::max(height, 0);
        place_bar();
        m_target = std::clamp(m_target, 0.0f, static_cast<float>(max_offset()));
        m_offset = std::clamp(m_offset, 0.0f, static_cast<float>(max_offset()));
        m_bar.set_position(offset());
    }

    std::int32_t scroll_view::offset() const noexcept
    {
        return static_cast<std::int32_t>(std::lround(m_offset));
    }

    std::int32_t scroll_view::max_offset() const noexcept
    {
        return std::max(m_content - m_view.height, 0);
    }

    void scroll_view::scroll_to(std::int32_t offset, bool animate) noexcept
    {
        m_target = std::clamp(static_cast<float>(offset), 0.0f, static_cast<float>(max_offset()));

        if (!animate)
        {
            m_offset = m_target;
            m_bar.set_position(this->offset());
        }
    }

    void scroll_view::scroll_by(std::int32_t delta, bool animate) noexcept
    {
        scroll_to(static_cast<std::int32_t>(std::lround(m_target)) + delta, animate);
    }

    void scroll_view::reveal(const dock_rect& area, bool animate) noexcept
    {
        const std::int32_t top = area.y;
        const std::int32_t bottom = area.y + area.height;
        const std::int32_t current = static_cast<std::int32_t>(std::lround(m_target));

        if (top < current)
            scroll_to(top, animate);
        else if (bottom > current + m_view.height)
            scroll_to(bottom - m_view.height, animate);
    }

    bool scroll_view::contains(std::int32_t x, std::int32_t y) const noexcept
    {
        return x >= m_view.x && x < m_view.x + m_view.width && y >= m_view.y && y < m_view.y + m_view.height;
    }

    bool scroll_view::on_mouse(const panel_mouse& mouse) noexcept
    {
        if (mouse.event == mouse_event::wheel)
        {
            if (!contains(mouse.x, mouse.y) || max_offset() == 0 || mouse.wheel == 0)
                return false;

            scroll_by(-mouse.wheel * draw::px(k_wheel_step), true);
            return true;
        }

        return m_set.on_mouse(mouse);
    }

    bool scroll_view::animating() const noexcept
    {
        return std::fabs(m_target - m_offset) > 0.0f;
    }

    bool scroll_view::animate(std::uint64_t now_ms) noexcept
    {
        if (!animating())
        {
            m_stamp = 0;
            return false;
        }

        const float dt = m_stamp != 0 && now_ms > m_stamp ? static_cast<float>(now_ms - m_stamp) : 10.0f;
        m_stamp = now_ms;
        m_offset += (m_target - m_offset) * (1.0f - std::exp(-dt / k_ease_ms));

        if (std::fabs(m_target - m_offset) < 0.5f)
        {
            m_offset = m_target;
            m_stamp = 0;
        }

        m_bar.set_position(offset());
        return animating();
    }

    void scroll_view::clip(void* dc) const noexcept
    {
        draw::clip(dc, m_view);
    }

    void scroll_view::unclip(void* dc) const noexcept
    {
        draw::unclip(dc);
    }
}
