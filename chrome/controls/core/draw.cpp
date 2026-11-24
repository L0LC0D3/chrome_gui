#include <chrome/controls/core/draw.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// Lean windows headers leave out the COM types GDI+ is declared with.
#include <objbase.h>
#include <objidl.h>
#include <propidl.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// GDI+ draws what GDI cannot smooth. Its headers want the min and max of the windows headers.
namespace Gdiplus
{
    using std::max;
    using std::min;
}

#include <gdiplus.h>

#pragma comment(lib, "msimg32.lib")

namespace chrome::draw
{
    namespace
    {
        [[nodiscard]] COLORREF rgb(std::uint32_t colour) noexcept
        {
            return RGB((colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF);
        }

        // GDI+ is started the first time something is drawn and stopped when the program ends.
        struct gdiplus_session
        {
            ULONG_PTR token{0};
            bool ok{false};

            gdiplus_session() noexcept
            {
                Gdiplus::GdiplusStartupInput input;
                ok = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
            }

            ~gdiplus_session()
            {
                if (ok)
                    Gdiplus::GdiplusShutdown(token);
            }
        };

        [[nodiscard]] bool gdiplus_ready() noexcept
        {
            static const gdiplus_session session;
            return session.ok;
        }

        [[nodiscard]] Gdiplus::Color gp(std::uint32_t colour) noexcept
        {
            return Gdiplus::Color(255, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF);
        }

        // The graphics object shared by the shapes of one paint, while a draw::frame is alive.
        struct shared_graphics
        {
            HDC dc{nullptr};
            Gdiplus::Graphics* graphics{nullptr};
            bool active{false};
            std::int32_t depth{0};
            bool pushed[8]{};
            Gdiplus::GraphicsContainer containers[8]{};
        };

        shared_graphics g_shared;

        // The edge of a pixel is a whole number, and edges are smoothed.
        void quality(Gdiplus::Graphics& g) noexcept
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        }

        // The shared graphics of the frame when it is for this dc, and a new one otherwise.
        class scoped_graphics
        {
        public:
            explicit scoped_graphics(HDC dc) noexcept
            {
                if (g_shared.active && g_shared.dc == dc)
                {
                    if (g_shared.graphics == nullptr)
                    {
                        g_shared.graphics = new Gdiplus::Graphics(dc);
                        quality(*g_shared.graphics);
                    }

                    m_graphics = g_shared.graphics;
                    return;
                }

                m_own = new Gdiplus::Graphics(dc);
                quality(*m_own);
                m_graphics = m_own;
            }

            ~scoped_graphics() { delete m_own; }
            scoped_graphics(const scoped_graphics&) = delete;
            scoped_graphics& operator=(const scoped_graphics&) = delete;

            [[nodiscard]] Gdiplus::Graphics& get() const noexcept { return *m_graphics; }

        private:
            Gdiplus::Graphics* m_graphics{nullptr};
            Gdiplus::Graphics* m_own{nullptr};
        };

        // The outline of a rounded box, drawn through the middle of its one pixel edge.
        void rounded_path(Gdiplus::GraphicsPath& path, const dock_rect& area, float radius) noexcept
        {
            const float x = static_cast<float>(area.x) + 0.5f;
            const float y = static_cast<float>(area.y) + 0.5f;
            const float w = static_cast<float>(area.width) - 1.0f;
            const float h = static_cast<float>(area.height) - 1.0f;
            const float r = std::min(radius, std::min(w, h) / 2.0f);
            const float d = 2.0f * r;

            if (r <= 0.0f)
            {
                path.AddRectangle(Gdiplus::RectF(x, y, w, h));
                return;
            }

            path.AddArc(x, y, d, d, 180.0f, 90.0f);
            path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
            path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
            path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
            path.CloseFigure();
        }

        // Control text is UTF-8. Whatever does not convert shows as nothing.
        int widen(const char* utf8, wchar_t* out, int capacity) noexcept
        {
            if (utf8 == nullptr)
                return 0;

            const int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, capacity);
            return length > 0 ? length - 1 : 0;
        }
    }

    frame::frame(void* dc) noexcept
    {
        m_saved = new shared_graphics(g_shared);
        g_shared = shared_graphics{};
        g_shared.dc = static_cast<HDC>(dc);
        g_shared.active = gdiplus_ready();
        g_shared.depth = 0;
    }

    frame::~frame()
    {
        delete g_shared.graphics;
        g_shared = *static_cast<shared_graphics*>(m_saved);
        delete static_cast<shared_graphics*>(m_saved);
    }

    void clip(void* dc_handle, const dock_rect& area) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        const std::int32_t slot = g_shared.depth < 8 ? g_shared.depth : 7;
        bool pushed = false;
        Gdiplus::Graphics* graphics = nullptr;

        // The shared graphics is made before the limit is set, so the limit can be put on it and taken off again.
        if (g_shared.active && g_shared.dc == dc && g_shared.depth < 8)
        {
            scoped_graphics lease(dc);
            graphics = &lease.get();
            g_shared.containers[slot] = graphics->BeginContainer();
            // A new container starts with the default quality, which does not smooth, so the setting is made again.
            quality(*graphics);
            graphics->SetClip(Gdiplus::Rect(area.x, area.y, area.width, area.height), Gdiplus::CombineModeIntersect);
            pushed = true;
        }

        SaveDC(dc);
        IntersectClipRect(dc, area.x, area.y, area.x + area.width, area.y + area.height);

        if (g_shared.depth < 8)
            g_shared.pushed[g_shared.depth] = pushed;

        ++g_shared.depth;
    }

    void unclip(void* dc_handle) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);

        if (g_shared.depth <= 0)
            return;

        --g_shared.depth;

        if (g_shared.depth < 8 && g_shared.pushed[g_shared.depth] && g_shared.graphics != nullptr)
            g_shared.graphics->EndContainer(g_shared.containers[g_shared.depth]);

        RestoreDC(dc, -1);
    }

    std::int32_t px(std::int32_t pixels) noexcept
    {
        return dock_model::scaled(pixels);
    }

    void image(void* dc, const dock_rect& area, const std::uint8_t* bgra, std::uint32_t width, std::uint32_t height) noexcept
    {
        if (bgra == nullptr || width == 0 || height == 0 || area.width <= 0 || area.height <= 0)
            return;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(width);
        // A negative height says the first row is the top one.
        info.bmiHeader.biHeight = -static_cast<LONG>(height);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        const HDC target = static_cast<HDC>(dc);
        const int saved_mode = SetStretchBltMode(target, HALFTONE);
        SetBrushOrgEx(target, 0, 0, nullptr);
        StretchDIBits(target, area.x, area.y, area.width, area.height, 0, 0, static_cast<int>(width), static_cast<int>(height), bgra, &info, DIB_RGB_COLORS,
            SRCCOPY);
        SetStretchBltMode(target, saved_mode);
    }

    void fill(void* dc, const dock_rect& area, std::uint32_t colour) noexcept
    {
        const RECT rect{area.x, area.y, area.x + area.width, area.y + area.height};
        const HBRUSH brush = CreateSolidBrush(rgb(colour));
        FillRect(static_cast<HDC>(dc), &rect, brush);
        DeleteObject(brush);
    }

    void rounded(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t face, std::uint32_t edge) noexcept
    {
        if (!gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        Gdiplus::GraphicsPath path;
        rounded_path(path, area, static_cast<float>(px(radius)));
        const Gdiplus::SolidBrush brush(gp(face));
        const Gdiplus::Pen pen(gp(edge), 1.0f);
        g.FillPath(&brush, &path);
        g.DrawPath(&pen, &path);
    }

    void outline(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t colour, float width) noexcept
    {
        if (!gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        Gdiplus::GraphicsPath path;
        rounded_path(path, area, static_cast<float>(px(radius)));
        const Gdiplus::Pen pen(gp(colour), width);
        g.DrawPath(&pen, &path);
    }

    namespace
    {
        // The layers of a shadow, drawn from the outside in so the weak outer ones lie under the strong inner ones.
        void shadow_into(Gdiplus::Graphics& g, const dock_rect& area, std::int32_t radius, std::int32_t spread, std::int32_t drop, float strength) noexcept
        {
            const std::int32_t reach = px(spread);
            const std::int32_t steps = std::min(reach, 12);

            for (std::int32_t i = steps; i >= 1; --i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const std::int32_t grow = reach * i / steps;
                const dock_rect layer{area.x - grow, area.y - grow + px(drop), area.width + 2 * grow, area.height + 2 * grow};
                const float weight = (1.0f - t) * (1.0f - t) + 0.02f;
                const BYTE alpha = static_cast<BYTE>(std::clamp(strength * 255.0f * weight * 6.0f / static_cast<float>(steps), 0.0f, 255.0f));
                Gdiplus::GraphicsPath path;
                rounded_path(path, layer, static_cast<float>(px(radius) + grow));
                const Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, 0, 0, 0));
                g.FillPath(&brush, &path);
            }
        }

        void gradient_into(Gdiplus::Graphics& g, const dock_rect& area, std::int32_t radius, std::uint32_t top, std::uint32_t bottom, std::uint32_t edge, bool light) noexcept
        {
            Gdiplus::GraphicsPath path;
            rounded_path(path, area, static_cast<float>(px(radius)));
            const Gdiplus::LinearGradientBrush brush(Gdiplus::Point(area.x, area.y), Gdiplus::Point(area.x, area.y + area.height), gp(top), gp(bottom));
            const Gdiplus::Pen pen(gp(edge), 1.0f);
            g.FillPath(&brush, &path);
            g.DrawPath(&pen, &path);

            if (light)
            {
                // One faint line just inside the top edge, between the round corners, as a lit rim.
                const float inset = static_cast<float>(px(radius)) * 0.8f + 1.0f;
                const Gdiplus::Pen shine(Gdiplus::Color(60, 255, 255, 255), 1.0f);
                const float line_y = static_cast<float>(area.y) + 1.5f;
                g.DrawLine(&shine, static_cast<float>(area.x) + inset, line_y, static_cast<float>(area.x + area.width) - inset, line_y);
            }
        }

        // What makes one raised box, so the same one can be drawn again from a picture.
        struct sprite_key
        {
            std::int32_t width;
            std::int32_t height;
            std::int32_t scale;
            std::int32_t radius;
            std::int32_t spread;
            std::int32_t drop;
            std::int32_t strength;
            std::uint32_t top;
            std::uint32_t bottom;
            std::uint32_t edge;
            std::int32_t light;
        };

        // The picture is a DIB section holding premultiplied pixels, which GDI blends onto a dc much faster than GDI+ draws an image.
        struct sprite
        {
            sprite_key key{};
            HBITMAP image{nullptr};
        };

        constexpr std::uint32_t k_sprites = 96;
        sprite g_sprites[k_sprites];
        std::uint32_t g_sprite_next = 0;
    }

    void shadow(void* dc, const dock_rect& area, std::int32_t radius, std::int32_t spread, std::int32_t drop, float strength) noexcept
    {
        if (!gdiplus_ready() || spread <= 0)
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        shadow_into(lease.get(), area, radius, spread, drop, strength);
    }

    void gradient(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t top, std::uint32_t bottom, std::uint32_t edge, bool light) noexcept
    {
        if (!gdiplus_ready() || area.width <= 1 || area.height <= 1)
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        gradient_into(lease.get(), area, radius, top, bottom, edge, light);
    }

    void raised(void* dc, const dock_rect& area, std::int32_t radius, std::uint32_t top, std::uint32_t bottom, std::uint32_t edge, bool light, std::int32_t spread,
        std::int32_t drop, float strength) noexcept
    {
        if (!gdiplus_ready() || area.width <= 1 || area.height <= 1)
            return;

        sprite_key key{};
        key.width = area.width;
        key.height = area.height;
        key.scale = px(100);
        key.radius = radius;
        key.spread = spread;
        key.drop = drop;
        key.strength = static_cast<std::int32_t>(strength * 1000.0f);
        key.top = top;
        key.bottom = bottom;
        key.edge = edge;
        key.light = light ? 1 : 0;

        const std::int32_t margin = px(spread) + px(drop) + 2;
        const std::int32_t full_width = area.width + 2 * margin;
        const std::int32_t full_height = area.height + 2 * margin;
        sprite* found = nullptr;

        for (sprite& entry : g_sprites)
        {
            if (entry.image != nullptr && std::memcmp(&entry.key, &key, sizeof(key)) == 0)
            {
                found = &entry;
                break;
            }
        }

        if (found == nullptr)
        {
            // Made once: the shadow and the box, drawn into a bitmap that keeps its transparency. The oldest picture makes room.
            sprite& slot = g_sprites[g_sprite_next];
            g_sprite_next = (g_sprite_next + 1) % k_sprites;

            if (slot.image != nullptr)
                DeleteObject(slot.image);

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth = full_width;
            info.bmiHeader.biHeight = -full_height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            slot.image = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            slot.key = key;

            if (slot.image == nullptr || bits == nullptr)
                return;

            {
                Gdiplus::Bitmap canvas(full_width, full_height, full_width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
                Gdiplus::Graphics g(&canvas);
                quality(g);
                const dock_rect inside{margin, margin, area.width, area.height};

                if (spread > 0)
                    shadow_into(g, inside, radius, spread, drop, strength);

                gradient_into(g, inside, radius, top, bottom, edge, light);
                g.Flush(Gdiplus::FlushIntentionSync);
            }

            found = &slot;
        }

        static const HDC source = CreateCompatibleDC(nullptr);
        const HGDIOBJ previous = SelectObject(source, found->image);
        const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(static_cast<HDC>(dc), area.x - margin, area.y - margin, full_width, full_height, source, 0, 0, full_width, full_height, blend);
        SelectObject(source, previous);
    }

    void ellipse(void* dc, const dock_rect& area, std::uint32_t face, std::uint32_t edge) noexcept
    {
        if (!gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        const Gdiplus::RectF box(static_cast<float>(area.x) + 0.5f, static_cast<float>(area.y) + 0.5f,
            static_cast<float>(area.width) - 1.0f, static_cast<float>(area.height) - 1.0f);
        const Gdiplus::SolidBrush brush(gp(face));
        const Gdiplus::Pen pen(gp(edge), 1.0f);
        g.FillEllipse(&brush, box);
        g.DrawEllipse(&pen, box);
    }

    void disc(void* dc, float centre_x, float centre_y, float radius, std::uint32_t face, std::uint32_t edge, float edge_width) noexcept
    {
        if (!gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        // The edge is drawn through the middle of its width, so the circle is pulled in by half of it to keep its size.
        const float r = std::max(radius - edge_width * 0.5f, 0.5f);
        const Gdiplus::RectF box(centre_x - r, centre_y - r, r * 2.0f, r * 2.0f);
        const Gdiplus::SolidBrush brush(gp(face));
        g.FillEllipse(&brush, box);

        if (edge_width > 0.0f)
        {
            const Gdiplus::Pen pen(gp(edge), edge_width);
            g.DrawEllipse(&pen, box);
        }
    }

    layer::layer(void* dc, const dock_rect& area, std::int32_t scale) noexcept
        : m_dc(dc), m_area(area), m_scale(std::max(scale, 1))
    {
        if (!gdiplus_ready() || area.width <= 0 || area.height <= 0)
            return;

        auto* bitmap = new Gdiplus::Bitmap(area.width * m_scale, area.height * m_scale, PixelFormat32bppPARGB);
        auto* graphics = new Gdiplus::Graphics(bitmap);
        graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        m_bitmap = bitmap;
        m_graphics = graphics;
    }

    layer::~layer()
    {
        flush();
    }

    void layer::disc(float centre_x, float centre_y, float radius, std::uint32_t face, std::uint32_t edge, float edge_width) noexcept
    {
        if (m_graphics == nullptr)
            return;

        auto& g = *static_cast<Gdiplus::Graphics*>(m_graphics);
        const float k = static_cast<float>(m_scale);
        const float cx = (centre_x - static_cast<float>(m_area.x)) * k;
        const float cy = (centre_y - static_cast<float>(m_area.y)) * k;
        const float width = edge_width * k;
        const float r = std::max(radius * k - width * 0.5f, 0.5f);
        const Gdiplus::RectF box(cx - r, cy - r, r * 2.0f, r * 2.0f);
        const Gdiplus::SolidBrush brush(gp(face));
        g.FillEllipse(&brush, box);

        if (width > 0.0f)
        {
            const Gdiplus::Pen pen(gp(edge), width);
            g.DrawEllipse(&pen, box);
        }
    }

    void layer::polyline(const point* points, std::uint32_t count, std::uint32_t colour, float width) noexcept
    {
        if (m_graphics == nullptr || count < 2)
            return;

        auto& g = *static_cast<Gdiplus::Graphics*>(m_graphics);
        const float k = static_cast<float>(m_scale);
        Gdiplus::PointF converted[64];
        count = std::min<std::uint32_t>(count, 64);

        for (std::uint32_t i = 0; i < count; ++i)
            converted[i] = Gdiplus::PointF((points[i].x - static_cast<float>(m_area.x)) * k, (points[i].y - static_cast<float>(m_area.y)) * k);

        Gdiplus::Pen pen(gp(colour), width * k);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLines(&pen, converted, static_cast<INT>(count));
    }

    void layer::flush() noexcept
    {
        if (m_graphics == nullptr)
            return;

        delete static_cast<Gdiplus::Graphics*>(m_graphics);
        m_graphics = nullptr;
        auto* bitmap = static_cast<Gdiplus::Bitmap*>(m_bitmap);
        m_bitmap = nullptr;

        {
            scoped_graphics lease(static_cast<HDC>(m_dc));
            Gdiplus::Graphics& g = lease.get();
            const Gdiplus::InterpolationMode mode = g.GetInterpolationMode();
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(bitmap, Gdiplus::Rect(m_area.x, m_area.y, m_area.width, m_area.height), 0, 0, m_area.width * m_scale, m_area.height * m_scale,
                Gdiplus::UnitPixel);
            g.SetInterpolationMode(mode);
        }

        delete bitmap;
    }

    void polygon(void* dc, const point* points, std::uint32_t count, std::uint32_t colour) noexcept
    {
        if (count < 3 || !gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        Gdiplus::PointF converted[64];
        count = std::min<std::uint32_t>(count, 64);

        for (std::uint32_t i = 0; i < count; ++i)
            converted[i] = Gdiplus::PointF(points[i].x, points[i].y);

        const Gdiplus::SolidBrush brush(gp(colour));
        g.FillPolygon(&brush, converted, static_cast<INT>(count));
    }

    void polyline(void* dc, const point* points, std::uint32_t count, std::uint32_t colour, float width) noexcept
    {
        if (count < 2 || !gdiplus_ready())
            return;

        scoped_graphics lease(static_cast<HDC>(dc));
        Gdiplus::Graphics& g = lease.get();
        Gdiplus::PointF converted[64];
        count = std::min<std::uint32_t>(count, 64);

        for (std::uint32_t i = 0; i < count; ++i)
            converted[i] = Gdiplus::PointF(points[i].x, points[i].y);

        Gdiplus::Pen pen(gp(colour), width);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLines(&pen, converted, static_cast<INT>(count));
    }

    namespace
    {
        constexpr int k_text_capacity = 512;

        // The font in dc with the weight and underline the flags ask for, or null when the font is fine as it is.
        // The caller selects it and deletes it.
        [[nodiscard]] HFONT variant_font(HDC dc, std::uint32_t flags) noexcept
        {
            if ((flags & (text_bold | text_underline)) == 0)
                return nullptr;

            const HFONT current = static_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
            LOGFONTW face{};

            if (current == nullptr || GetObjectW(current, sizeof(face), &face) == 0)
                return nullptr;

            if ((flags & text_bold) != 0)
                face.lfWeight = FW_SEMIBOLD;

            if ((flags & text_underline) != 0)
                face.lfUnderline = TRUE;

            return CreateFontIndirectW(&face);
        }

        [[nodiscard]] UINT format_of(std::uint32_t flags) noexcept
        {
            UINT format = DT_NOPREFIX;
            format |= (flags & text_center) != 0 ? DT_CENTER : (flags & text_right) != 0 ? DT_RIGHT : DT_LEFT;

            if ((flags & text_wrap) != 0)
                format |= DT_WORDBREAK;
            else
                format |= DT_SINGLELINE | DT_VCENTER;

            if ((flags & text_ellipsis) != 0)
                format |= DT_END_ELLIPSIS;

            return format;
        }
    }

    std::int32_t text_width(void* dc_handle, const char* utf8, std::uint32_t flags) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        wchar_t wide[k_text_capacity]{};
        const int length = widen(utf8, wide, k_text_capacity);
        const HFONT font = variant_font(dc, flags);
        const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        SIZE size{};
        GetTextExtentPoint32W(dc, wide, length, &size);

        if (font != nullptr)
        {
            SelectObject(dc, previous);
            DeleteObject(font);
        }

        return size.cx;
    }

    std::int32_t text_height(void* dc_handle, const char* utf8, std::int32_t width, std::uint32_t flags) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        wchar_t wide[k_text_capacity]{};
        const int length = widen(utf8, wide, k_text_capacity);
        const HFONT font = variant_font(dc, flags);
        const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        RECT rect{0, 0, width, 0};
        DrawTextW(dc, wide, length, &rect, format_of(flags) | DT_CALCRECT);

        if (font != nullptr)
        {
            SelectObject(dc, previous);
            DeleteObject(font);
        }

        return rect.bottom - rect.top;
    }

    void text(void* dc_handle, const dock_rect& area, const char* utf8, std::uint32_t colour, std::uint32_t flags) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        wchar_t wide[k_text_capacity]{};
        const int length = widen(utf8, wide, k_text_capacity);
        RECT rect{area.x, area.y, area.x + area.width, area.y + area.height};
        const HFONT font = variant_font(dc, flags);
        const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        SetTextColor(dc, rgb(colour));
        // Restoring a clip puts the background mode back to opaque, which would paint a box behind the text.
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, wide, length, &rect, format_of(flags));

        if (font != nullptr)
        {
            SelectObject(dc, previous);
            DeleteObject(font);
        }
    }

    std::int32_t text_width(void* dc_handle, const wchar_t* text, std::int32_t length, std::uint32_t flags) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        const HFONT font = variant_font(dc, flags);
        const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        SIZE size{};
        GetTextExtentPoint32W(dc, text, length, &size);

        if (font != nullptr)
        {
            SelectObject(dc, previous);
            DeleteObject(font);
        }

        return size.cx;
    }

    void text(void* dc_handle, const dock_rect& area, const wchar_t* text, std::int32_t length, std::uint32_t colour, std::uint32_t flags) noexcept
    {
        const HDC dc = static_cast<HDC>(dc_handle);
        RECT rect{area.x, area.y, area.x + area.width, area.y + area.height};
        const HFONT font = variant_font(dc, flags);
        const HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
        SetTextColor(dc, rgb(colour));
        DrawTextW(dc, text, length, &rect, format_of(flags));

        if (font != nullptr)
        {
            SelectObject(dc, previous);
            DeleteObject(font);
        }
    }

    void caret_positions(void* dc_handle, const wchar_t* text, std::int32_t length, std::int32_t* xs) noexcept
    {
        xs[0] = 0;

        if (length <= 0)
            return;

        // The extent after each character, which is where the caret goes after it.
        int partial[k_text_capacity]{};
        const int count = std::min<int>(length, k_text_capacity);
        SIZE size{};
        GetTextExtentExPointW(static_cast<HDC>(dc_handle), text, count, 0, nullptr, partial, &size);

        for (int i = 0; i < count; ++i)
            xs[i + 1] = partial[i];

        for (int i = count; i < length; ++i)
            xs[i + 1] = partial[count - 1];
    }

    void icon(void* dc, icon_kind kind, const dock_rect& box, std::uint32_t colour) noexcept
    {
        const float left = static_cast<float>(box.x);
        const float top = static_cast<float>(box.y);
        const float right = static_cast<float>(box.x + box.width);
        const float bottom = static_cast<float>(box.y + box.height);
        const float mid_x = (left + right) / 2.0f;
        const float mid_y = (top + bottom) / 2.0f;
        // Strokes are a little under two pixels at 96 dpi, which stays soft and readable once smoothed.
        const float stroke = std::max(1.5f, 1.75f * static_cast<float>(px(100)) / 100.0f);
        const float inset = static_cast<float>(px(1));
        // The line ends are round and reach half a stroke past their points, so they are pulled in by as much.
        const float cap = stroke / 2.0f;

        switch (kind)
        {
        case icon_kind::plus:
        {
            const point horizontal[2] = {{left + cap, mid_y}, {right - cap, mid_y}};
            const point vertical[2] = {{mid_x, top + cap}, {mid_x, bottom - cap}};
            polyline(dc, horizontal, 2, colour, stroke);
            polyline(dc, vertical, 2, colour, stroke);
            break;
        }
        case icon_kind::play:
        {
            const point triangle[3] = {{left + static_cast<float>(px(2)), top}, {right, mid_y}, {left + static_cast<float>(px(2)), bottom}};
            polygon(dc, triangle, 3, colour);
            break;
        }
        case icon_kind::cross:
        {
            const point down[2] = {{left + inset + cap, top + inset + cap}, {right - inset - cap, bottom - inset - cap}};
            const point up[2] = {{right - inset - cap, top + inset + cap}, {left + inset + cap, bottom - inset - cap}};
            polyline(dc, down, 2, colour, stroke);
            polyline(dc, up, 2, colour, stroke);
            break;
        }
        case icon_kind::menu:
            for (int row = 0; row < 3; ++row)
            {
                const float y = top + static_cast<float>(px(2) + row * px(4));
                const point bar[2] = {{left + cap, y}, {right - cap, y}};
                polyline(dc, bar, 2, colour, stroke);
            }

            break;
        case icon_kind::check:
        {
            const point mark[3] = {{left + inset + cap, top + static_cast<float>(px(6))}, {left + static_cast<float>(px(4)), bottom - static_cast<float>(px(2)) - cap * 0.5f},
                {right - inset - cap * 0.5f, top + static_cast<float>(px(2)) + cap * 0.5f}};
            polyline(dc, mark, 3, colour, stroke);
            break;
        }
        case icon_kind::trash:
        {
            const float lid = top + static_cast<float>(px(3));
            const float thin = stroke * 0.85f;
            const point bar[2] = {{left + inset, lid}, {right - inset, lid}};
            const point handle[2] = {{mid_x - static_cast<float>(px(2)), top + inset}, {mid_x + static_cast<float>(px(2)), top + inset}};
            const point body[5] = {{left + static_cast<float>(px(2)), lid}, {left + static_cast<float>(px(3)), bottom - inset},
                {right - static_cast<float>(px(3)), bottom - inset}, {right - static_cast<float>(px(2)), lid}, {left + static_cast<float>(px(2)), lid}};
            polyline(dc, bar, 2, colour, thin);
            polyline(dc, handle, 2, colour, thin);
            polyline(dc, body, 5, colour, thin);
            break;
        }
        case icon_kind::stack:
        {
            const float thin = stroke * 0.85f;
            const float s = static_cast<float>(px(3));
            const point back[3] = {{left + s, top + inset}, {right - inset, top + inset}, {right - inset, bottom - s}};
            const point front[5] = {{left + inset, top + s}, {right - s, top + s}, {right - s, bottom - inset}, {left + inset, bottom - inset}, {left + inset, top + s}};
            polyline(dc, back, 3, colour, thin);
            polyline(dc, front, 5, colour, thin);
            break;
        }
        case icon_kind::arrow_down:
        {
            const float thin = stroke * 0.9f;
            const float base = bottom - inset;
            const point line[2] = {{mid_x, top + inset}, {mid_x, base - static_cast<float>(px(3))}};
            const point head[3] = {{mid_x - static_cast<float>(px(3)), base - static_cast<float>(px(6))}, {mid_x, base - static_cast<float>(px(3))},
                {mid_x + static_cast<float>(px(3)), base - static_cast<float>(px(6))}};
            const point floor[2] = {{left + inset, base}, {right - inset, base}};
            polyline(dc, line, 2, colour, thin);
            polyline(dc, head, 3, colour, thin);
            polyline(dc, floor, 2, colour, thin);
            break;
        }
        case icon_kind::clock:
        {
            const float thin = stroke * 0.85f;
            const float radius = std::min(right - left, bottom - top) / 2.0f - inset;
            point ring[25];

            for (int i = 0; i < 25; ++i)
            {
                const float angle = static_cast<float>(i) * 6.2831853f / 24.0f;
                ring[i] = {mid_x + radius * std::cos(angle), mid_y + radius * std::sin(angle)};
            }

            const point hands[3] = {{mid_x, top + inset + static_cast<float>(px(3))}, {mid_x, mid_y}, {mid_x + static_cast<float>(px(3)), mid_y + static_cast<float>(px(2))}};
            polyline(dc, ring, 25, colour, thin);
            polyline(dc, hands, 3, colour, thin);
            break;
        }
        case icon_kind::panel:
        {
            const float thin = stroke * 0.85f;
            const float split = top + (bottom - top) * 0.6f;
            const point frame[5] = {{left + inset, top + inset}, {right - inset, top + inset}, {right - inset, bottom - inset}, {left + inset, bottom - inset}, {left + inset, top + inset}};
            const point rule[2] = {{left + inset, split}, {right - inset, split}};
            polyline(dc, frame, 5, colour, thin);
            polyline(dc, rule, 2, colour, thin);
            break;
        }
        case icon_kind::filter:
        {
            const float thin = stroke * 0.85f;
            const float neck = static_cast<float>(px(2));
            const point funnel[7] = {{left + inset, top + inset}, {right - inset, top + inset}, {mid_x + neck * 0.75f, mid_y}, {mid_x + neck * 0.75f, bottom - inset},
                {mid_x - neck * 0.75f, bottom - static_cast<float>(px(3))}, {mid_x - neck * 0.75f, mid_y}, {left + inset, top + inset}};
            polyline(dc, funnel, 7, colour, thin);
            break;
        }
        case icon_kind::undo:
        case icon_kind::redo:
        {
            // Drawn for undo and mirrored for redo. The box is 12 units wide, whatever the scale.
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.9f;
            const bool flip = kind == icon_kind::redo;
            const auto at = [&](float x, float y) { return point{left + (flip ? 12.0f - x : x) * u, top + y * u}; };
            const point head[3] = {at(4.5f, 1.5f), at(1.5f, 4.5f), at(4.5f, 7.5f)};
            const point body[4] = {at(1.5f, 4.5f), at(7.0f, 4.5f), at(10.5f, 6.5f), at(7.5f, 10.0f)};
            polyline(dc, head, 3, colour, thin);
            polyline(dc, body, 4, colour, thin);
            break;
        }
        case icon_kind::stop:
        {
            const float u = (right - left) / 12.0f;
            const point square[4] = {{left + 2.0f * u, top + 2.0f * u}, {left + 10.0f * u, top + 2.0f * u}, {left + 10.0f * u, top + 10.0f * u}, {left + 2.0f * u, top + 10.0f * u}};
            polygon(dc, square, 4, colour);
            break;
        }
        case icon_kind::pause:
        {
            const float u = (right - left) / 12.0f;
            const point first[4] = {{left + 2.5f * u, top + 1.5f * u}, {left + 5.0f * u, top + 1.5f * u}, {left + 5.0f * u, top + 10.5f * u}, {left + 2.5f * u, top + 10.5f * u}};
            const point second[4] = {{left + 7.0f * u, top + 1.5f * u}, {left + 9.5f * u, top + 1.5f * u}, {left + 9.5f * u, top + 10.5f * u}, {left + 7.0f * u, top + 10.5f * u}};
            polygon(dc, first, 4, colour);
            polygon(dc, second, 4, colour);
            break;
        }
        case icon_kind::save:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            const point body[6] = {at(1.0f, 1.0f), at(8.5f, 1.0f), at(11.0f, 3.5f), at(11.0f, 11.0f), at(1.0f, 11.0f), at(1.0f, 1.0f)};
            const point shutter[4] = {at(3.5f, 1.0f), at(3.5f, 4.0f), at(7.5f, 4.0f), at(7.5f, 1.0f)};
            const point label[4] = {at(3.0f, 11.0f), at(3.0f, 7.0f), at(9.0f, 7.0f), at(9.0f, 11.0f)};
            polyline(dc, body, 6, colour, thin);
            polyline(dc, shutter, 4, colour, thin);
            polyline(dc, label, 4, colour, thin);
            break;
        }
        case icon_kind::folder:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            const point outline[7] = {at(1.0f, 2.5f), at(4.5f, 2.5f), at(6.0f, 4.0f), at(11.0f, 4.0f), at(11.0f, 10.0f), at(1.0f, 10.0f), at(1.0f, 2.5f)};
            polyline(dc, outline, 7, colour, thin);
            break;
        }
        case icon_kind::sun:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const float radius = 2.6f * u;
            point ring[25];

            for (int i = 0; i < 25; ++i)
            {
                const float angle = static_cast<float>(i) * 6.2831853f / 24.0f;
                ring[i] = {mid_x + radius * std::cos(angle), mid_y + radius * std::sin(angle)};
            }

            polyline(dc, ring, 25, colour, thin);

            for (int i = 0; i < 8; ++i)
            {
                const float angle = static_cast<float>(i) * 6.2831853f / 8.0f;
                const point ray[2] = {{mid_x + 4.1f * u * std::cos(angle), mid_y + 4.1f * u * std::sin(angle)},
                    {mid_x + 5.4f * u * std::cos(angle), mid_y + 5.4f * u * std::sin(angle)}};
                polyline(dc, ray, 2, colour, thin);
            }

            break;
        }
        case icon_kind::moon:
        {
            // A disc of radius 5 with a disc of radius 4.2 taken off its upper right, in a box 12 units wide. The two arcs
            // meet where the circles cross.
            const float u = (right - left) / 12.0f;
            const float rad = 3.1415927f / 180.0f;
            point crescent[32];
            std::uint32_t count = 0;

            for (int i = 0; i <= 16; ++i)
            {
                const float angle = (10.8f + 248.4f * static_cast<float>(i) / 16.0f) * rad;
                crescent[count++] = {left + (6.0f + 5.0f * std::cos(angle)) * u, top + (6.0f + 5.0f * std::sin(angle)) * u};
            }

            for (int i = 0; i <= 12; ++i)
            {
                const float angle = (215.0f - 160.0f * static_cast<float>(i) / 12.0f) * rad;
                crescent[count++] = {left + (8.5f + 4.2f * std::cos(angle)) * u, top + (3.5f + 4.2f * std::sin(angle)) * u};
            }

            polygon(dc, crescent, count, colour);
            break;
        }
        case icon_kind::layout:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            const point frame[5] = {at(1.0f, 1.5f), at(11.0f, 1.5f), at(11.0f, 10.5f), at(1.0f, 10.5f), at(1.0f, 1.5f)};
            const point vertical[2] = {at(5.0f, 1.5f), at(5.0f, 10.5f)};
            const point horizontal[2] = {at(5.0f, 6.0f), at(11.0f, 6.0f)};
            polyline(dc, frame, 5, colour, thin);
            polyline(dc, vertical, 2, colour, thin);
            polyline(dc, horizontal, 2, colour, thin);
            break;
        }
        case icon_kind::home:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            const point roof[3] = {at(0.8f, 6.0f), at(6.0f, 1.3f), at(11.2f, 6.0f)};
            const point walls[4] = {at(2.4f, 5.0f), at(2.4f, 10.7f), at(9.6f, 10.7f), at(9.6f, 5.0f)};
            const point door[4] = {at(4.9f, 10.7f), at(4.9f, 7.6f), at(7.1f, 7.6f), at(7.1f, 10.7f)};
            polyline(dc, roof, 3, colour, thin);
            polyline(dc, walls, 4, colour, thin);
            polyline(dc, door, 4, colour, thin);
            break;
        }
        case icon_kind::code:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            const point open[3] = {at(4.4f, 2.8f), at(1.2f, 6.0f), at(4.4f, 9.2f)};
            const point close[3] = {at(7.6f, 2.8f), at(10.8f, 6.0f), at(7.6f, 9.2f)};
            polyline(dc, open, 3, colour, thin);
            polyline(dc, close, 3, colour, thin);
            break;
        }
        case icon_kind::grid:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };

            for (int row = 0; row < 2; ++row)
            {
                for (int column = 0; column < 2; ++column)
                {
                    const float x = 1.5f + 5.0f * static_cast<float>(column);
                    const float y = 1.5f + 5.0f * static_cast<float>(row);
                    const point box[5] = {at(x, y), at(x + 3.5f, y), at(x + 3.5f, y + 3.5f), at(x, y + 3.5f), at(x, y)};
                    polyline(dc, box, 5, colour, thin);
                }
            }

            break;
        }
        case icon_kind::list:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };

            for (int row = 0; row < 3; ++row)
            {
                const float y = 2.5f + 3.5f * static_cast<float>(row);
                const point dot[2] = {at(1.3f, y), at(2.2f, y)};
                const point line[2] = {at(4.0f, y), at(11.0f, y)};
                polyline(dc, dot, 2, colour, thin * 1.4f);
                polyline(dc, line, 2, colour, thin);
            }

            break;
        }
        case icon_kind::eye:
        case icon_kind::eye_off:
        {
            const float u = (right - left) / 12.0f;
            const float thin = stroke * 0.85f;
            const auto at = [&](float x, float y) { return point{left + x * u, top + y * u}; };
            constexpr int k_steps = 8;
            point lid_top[k_steps + 1];
            point lid_bottom[k_steps + 1];

            // The two lids meet at the corners of the eye, and each is half a sine wave.
            for (int i = 0; i <= k_steps; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(k_steps);
                const float lift = 3.3f * std::sin(3.1415927f * t);
                lid_top[i] = at(0.8f + 10.4f * t, 6.0f - lift);
                lid_bottom[i] = at(0.8f + 10.4f * t, 6.0f + lift);
            }

            polyline(dc, lid_top, k_steps + 1, colour, thin);
            polyline(dc, lid_bottom, k_steps + 1, colour, thin);

            if (kind == icon_kind::eye)
            {
                disc(dc, mid_x, mid_y, 1.7f * u, colour, colour, 0.0f);
            }
            else
            {
                const point slash[2] = {at(2.0f, 10.6f), at(10.0f, 1.4f)};
                polyline(dc, slash, 2, colour, thin * 1.15f);
            }

            break;
        }
        case icon_kind::none:
            break;
        }
    }
}
