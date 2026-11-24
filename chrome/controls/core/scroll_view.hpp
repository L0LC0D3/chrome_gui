#pragma once

#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/scrollbar.hpp>
#include <chrome/controls/core/view_range.hpp>

#include <cstdint>

namespace chrome
{
    // The scrolling of a panel whose content is taller than the panel. The panel lays its content out from the top as if
    // it were all on screen and never moves it. The view draws it into a surface of its own and copies the part that
    // shows, so a scroll step costs a copy. The mouse is moved down by offset() before the controls see it. The view
    // reads the wheel and owns the scrollbar, and it moves smoothly toward where the wheel sent it.
    //
    //   m_view.set_viewport({0, 0, width, height - footer});
    //   m_view.set_content_height(content);                 // after the layout
    //   m_view.paint_content(dc, &draw_content, this);      // draw_content paints at the content's own coordinates
    //   m_view.paint_bar(dc);
    //   ...and m_view.invalidate_content() whenever a control changes how it looks.
    class scroll_view
    {
    public:
        // Paints the content, at its own coordinates with the top at 0, into dc. Only the band needs drawing: what is outside it
        // is not on show and is drawn when the scroll gets there.
        using paint_fn = void (*)(void* user, void* dc, const view_range& band) noexcept;

        scroll_view() noexcept;
        ~scroll_view();
        scroll_view(const scroll_view&) = delete;
        scroll_view& operator=(const scroll_view&) = delete;

        // The part of the panel that shows the content, in panel coordinates. The bar sits on its right edge.
        void set_viewport(const dock_rect& area) noexcept;
        // The height of everything, in pixels. The offset stays in range.
        void set_content_height(std::int32_t height) noexcept;

        // How far the content is moved up, in pixels. Rounded, so it stays on whole pixels while it eases.
        [[nodiscard]] std::int32_t offset() const noexcept;
        [[nodiscard]] std::int32_t max_offset() const noexcept;
        void scroll_to(std::int32_t offset, bool animate) noexcept;
        void scroll_by(std::int32_t delta, bool animate) noexcept;
        // Brings a rectangle in content coordinates into view.
        void reveal(const dock_rect& area, bool animate) noexcept;

        [[nodiscard]] const dock_rect& viewport() const noexcept { return m_view; }
        [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const noexcept;
        // The mouse is on the bar or dragging it, so the content should not get it.
        [[nodiscard]] bool captures() const noexcept { return m_bar.hot() || m_bar.dragging(); }
        // The wheel, and the mouse on the bar. true when something changed that needs a repaint.
        [[nodiscard]] bool on_mouse(const panel_mouse& mouse) noexcept;

        // Moving toward the target. Ask the host for frames while this is true, and call animate on each.
        [[nodiscard]] bool animating() const noexcept;
        bool animate(std::uint64_t now_ms) noexcept;

        // The content changed, so the next paint_content draws it again.
        void invalidate_content() noexcept { m_dirty = true; }
        // Shows the visible part of the content. The content is drawn by the callback into a surface of its own the size of
        // all of it, and only when it changed or the size, theme or scale did. Scrolling then only copies, which is much
        // cheaper than drawing every control again for each step. Only a band of the content is drawn: the part that shows
        // and a page more each side, and it is drawn again when the scroll leaves it. Content taller than 16384 pixels is cut off.
        void paint_content(void* dc, paint_fn paint, void* user) noexcept;

        // Clips drawing to the viewport, and undoes it. dc is a Win32 HDC.
        void clip(void* dc) const noexcept;
        void unclip(void* dc) const noexcept;
        void paint_bar(void* dc) const noexcept { m_bar.paint(dc); }

    private:
        static void moved(void* user, scrollbar& source) noexcept;
        void place_bar() noexcept;

        scrollbar m_bar;
        control_set m_set;
        dock_rect m_view{};
        std::int32_t m_content{0};
        float m_offset{0.0f};
        float m_target{0.0f};
        std::uint64_t m_stamp{0};
        void* m_cache_dc{nullptr};
        void* m_cache_bitmap{nullptr};
        void* m_cache_old{nullptr};
        std::int32_t m_cache_width{0};
        std::int32_t m_cache_height{0};
        std::uint32_t m_cache_key{0};
        bool m_dirty{true};
        // The band of the cache that holds a current picture.
        view_range m_valid{};

        void release_cache() noexcept;
    };
}
