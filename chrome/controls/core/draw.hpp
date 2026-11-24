#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

// What controls draw with. Colours are 0xRRGGBB, rectangles are in the client pixels of the panel, and dc is a Win32 HDC.
namespace chrome::draw
{
    // While one of these is alive, the shapes drawn to its dc share a single GDI+ graphics object, which is much cheaper
    // than making one per shape. Make one around a panel's whole paint.
    class frame
    {
    public:
        explicit frame(void* dc) noexcept;
        ~frame();
        frame(const frame&) = delete;
        frame& operator=(const frame&) = delete;

    private:
        // The frame that was in force, which comes back when this one ends.
        void* m_saved{nullptr};
    };

    // Shapes drawn at several times the size and shrunk onto the dc when it ends, for small round artwork that plain
    // smoothing leaves rough, such as a dial. Coordinates are those of the dc, as floats, and the shapes must lie inside the
    // area. Nothing shows until the layer is flushed or goes out of scope.
    struct point;

    class layer
    {
    public:
        layer(void* dc, const dock_rect& area, std::int32_t scale = 4) noexcept;
        ~layer();
        layer(const layer&) = delete;
        layer& operator=(const layer&) = delete;

        void disc(float centre_x, float centre_y, float radius, std::uint32_t face, std::uint32_t edge, float edge_width) noexcept;
        void polyline(const point* points, std::uint32_t count, std::uint32_t colour, float width) noexcept;
        // Puts the layer on the dc now. Done once.
        void flush() noexcept;

    private:
        void* m_dc{nullptr};
        dock_rect m_area{};
        std::int32_t m_scale{4};
        void* m_bitmap{nullptr};
        void* m_graphics{nullptr};
    };

    // Limits drawing to the area, inside any limit already set. unclip puts back what there was before. They nest.
    void clip(void* dc, const dock_rect& area) noexcept;
    void unclip(void* dc) noexcept;

    // A size given at 96 dpi, in pixels at the current scale.
    [[nodiscard]] std::int32_t px(std::int32_t pixels) noexcept;
    void fill(void* dc, const dock_rect& area, std::uint32_t colour) noexcept;
    // A box with rounded corners: a one pixel edge around a face. The radius is at 96 dpi.
    void rounded(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t face, std::uint32_t edge) noexcept;
    // A soft shadow under a rounded box: layers of black that grow weaker outward, so the box looks lifted off what is under
    // it. area is the box, spread how far the shadow reaches past it and drop how far it is pushed down, both at 96 dpi.
    // strength is how dark it is next to the box, 0 to 1. Draw it before the box.
    void shadow(void* dc, const dock_rect& area, std::int32_t radius, std::int32_t spread, std::int32_t drop, float strength) noexcept;
    // A rounded box whose face fades from one colour at the top to another at the bottom, with a one pixel edge. With
    // light set, a faint line of white runs along the inside of the top edge, as a lit rim does.
    void gradient(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t top, std::uint32_t bottom, std::uint32_t edge, bool light) noexcept;
    // A gradient box with its shadow, drawn from a picture that is made once and kept, so it costs a copy and not a dozen smoothed
    // shapes. Use it for what is drawn many times with the same look, such as the tiles of a grid. Parameters are those of shadow and
    // gradient together.
    void raised(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t top, std::uint32_t bottom, std::uint32_t edge, bool light, std::int32_t spread,
        std::int32_t drop, float strength) noexcept;
    // A filled circle or ellipse with a one pixel edge.
    void ellipse(void* dc, const dock_rect& area, std::uint32_t face, std::uint32_t edge) noexcept;
    // Only the edge of a rounded box, of any width, with no face, so what is under it shows. The radius is at 96 dpi.
    void outline(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t colour, float width) noexcept;
    // A filled circle at any point, not only on whole pixels, with an edge of any width. It moves smoothly with the point,
    // which a small circle on whole pixels does not.
    void disc(void* dc, float centre_x, float centre_y, float radius, std::uint32_t face, std::uint32_t edge, float edge_width) noexcept;
    // Anti-aliased shapes, for what is not made of pixel aligned lines. Points are in pixels, and a whole number is the
    // edge between two pixels. The width of a line is in pixels. A polygon or polyline takes up to 64 points.
    struct point
    {
        float x{0.0f};
        float y{0.0f};
    };

    void polygon(void* dc, const point* points, std::uint32_t count, std::uint32_t colour) noexcept;
    void polyline(void* dc, const point* points, std::uint32_t count, std::uint32_t colour, float width) noexcept;
    // x of each caret position in UTF-16 text: xs[0] is 0 and xs[length] is the width. xs holds length + 1 values.
    void caret_positions(void* dc, const wchar_t* text, std::int32_t length, std::int32_t* xs) noexcept;

    // How text is drawn, in any mix. Without flags it is one line, starting at the left edge.
    enum text_flag : std::uint32_t
    {
        text_bold = 1,
        text_underline = 2,
        text_center = 4,
        text_right = 8,
        // Breaks lines at the width of the area and starts at its top, instead of centring one line.
        text_wrap = 16,
        // Ends a line that is too long with "...".
        text_ellipsis = 32
    };

    // The width of UTF-8 text in the font selected in dc, with the weight the flags give it.
    [[nodiscard]] std::int32_t text_width(void* dc, const char* utf8, std::uint32_t flags = 0) noexcept;
    // The height text takes when it is broken at a width. One line when it is not wrapped.
    [[nodiscard]] std::int32_t text_height(void* dc, const char* utf8, std::int32_t width, std::uint32_t flags = 0) noexcept;
    // Text in the area, centred vertically unless it wraps.
    void text(void* dc, const dock_rect& area, const char* utf8, std::uint32_t colour, std::uint32_t flags = 0) noexcept;
    // The same for UTF-16 text of a given length, as an editable field has it.
    [[nodiscard]] std::int32_t text_width(void* dc, const wchar_t* text, std::int32_t length, std::uint32_t flags = 0) noexcept;
    void text(void* dc, const dock_rect& area, const wchar_t* text, std::int32_t length, std::uint32_t colour, std::uint32_t flags = 0) noexcept;
    // A picture of B8G8R8A8 pixels, tightly packed with the top row first, stretched to fill the area. The alpha is ignored.
    void image(void* dc, const dock_rect& area, const std::uint8_t* bgra, std::uint32_t width, std::uint32_t height) noexcept;
    // An icon in a square box.
    void icon(void* dc, icon_kind kind, const dock_rect& box, std::uint32_t colour) noexcept;
}
