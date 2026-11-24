#include <chrome/controls/property/screen_loupe.hpp>

#include <chrome/controls/core/draw.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cwchar>

namespace chrome
{
    namespace
    {
        constexpr wchar_t k_class_name[] = L"ChromeScreenLoupe";
        // At 96 dpi: how big one screen pixel is made, and the strip under the picture that names the colour.
        constexpr std::int32_t k_cell = 11;
        constexpr std::int32_t k_footer = 28;
        constexpr std::int32_t k_gap = 22;

        [[nodiscard]] COLORREF to_colorref(std::uint32_t rgb) noexcept
        {
            return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }

        void frame(HDC dc, const RECT& area, COLORREF colour) noexcept
        {
            HBRUSH brush = CreateSolidBrush(colour);
            FrameRect(dc, &area, brush);
            DeleteObject(brush);
        }

        void fill(HDC dc, const RECT& area, COLORREF colour) noexcept
        {
            HBRUSH brush = CreateSolidBrush(colour);
            FillRect(dc, &area, brush);
            DeleteObject(brush);
        }

        LRESULT CALLBACK loupe_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            switch (message)
            {
            case WM_PAINT:
            {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window, &paint);
                const auto* loupe = reinterpret_cast<const screen_loupe*>(GetWindowLongPtrW(window, GWLP_USERDATA));

                if (loupe != nullptr)
                    loupe->paint(dc);

                EndPaint(window, &paint);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1;

            // The window is never the target of the mouse, and never takes the focus.
            case WM_NCHITTEST:
                return HTTRANSPARENT;

            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }
    }

    screen_loupe::~screen_loupe()
    {
        if (m_window != nullptr)
            DestroyWindow(static_cast<HWND>(m_window));
    }

    bool screen_loupe::create() noexcept
    {
        if (m_window != nullptr)
            return true;

        static bool registered = false;
        HINSTANCE instance = GetModuleHandleW(nullptr);

        if (!registered)
        {
            WNDCLASSEXW window_class{};
            window_class.cbSize = sizeof(window_class);
            window_class.lpfnWndProc = &loupe_proc;
            window_class.hInstance = instance;
            window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));
            window_class.lpszClassName = k_class_name;
            registered = RegisterClassExW(&window_class) != 0;

            if (!registered)
                return false;
        }

        HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, k_class_name, L"", WS_POPUP, 0, 0, 10, 10,
            nullptr, nullptr, instance, nullptr);

        if (window == nullptr)
            return false;

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_window = window;
        return true;
    }

    void screen_loupe::show(std::int32_t x, std::int32_t y, std::uint32_t rgb) noexcept
    {
        if (!create())
            return;

        m_x = x;
        m_y = y;
        m_rgb = rgb;

        const std::int32_t picture = chrome::draw::px(k_cell) * k_span + 2;
        const std::int32_t width = picture;
        const std::int32_t height = picture + chrome::draw::px(k_footer);
        const std::int32_t gap = chrome::draw::px(k_gap);

        // On the side of the pointer that has room, so it never covers the pixels it shows.
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        const POINT where{x, y};
        const bool known = GetMonitorInfoW(MonitorFromPoint(where, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE;
        std::int32_t left = x + gap;
        std::int32_t top = y + gap;

        if (known)
        {
            if (left + width > monitor.rcWork.right)
                left = x - gap - width;

            if (top + height > monitor.rcWork.bottom)
                top = y - gap - height;
        }

        HWND window = static_cast<HWND>(m_window);

        // Moving the window is the part that flickers, so it is done only when it has to move, and without a repaint of its
        // own: the picture is drawn once below, in one piece.
        if (!m_visible || left != m_left || top != m_top || width != m_width || height != m_height)
        {
            SetWindowPos(window, HWND_TOPMOST, left, top, width, height, SWP_NOACTIVATE | SWP_NOREDRAW | (m_visible ? 0 : SWP_SHOWWINDOW));
            m_left = left;
            m_top = top;
            m_width = width;
            m_height = height;
        }

        m_visible = true;

        HDC dc = GetDC(window);

        if (dc != nullptr)
        {
            paint(dc);
            ReleaseDC(window, dc);
        }
    }

    void screen_loupe::hide() noexcept
    {
        if (m_window != nullptr && m_visible)
            ShowWindow(static_cast<HWND>(m_window), SW_HIDE);

        m_visible = false;
    }

    void screen_loupe::paint(void* device) const noexcept
    {
        // Everything is drawn into a bitmap and put on the window with one copy, so no half-drawn picture is ever on show.
        HDC target = static_cast<HDC>(device);
        const std::int32_t total_width = chrome::draw::px(k_cell) * k_span + 2;
        const std::int32_t total_height = total_width + chrome::draw::px(k_footer);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, total_width, total_height);
        const HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
        const std::int32_t cell = chrome::draw::px(k_cell);
        const std::int32_t picture = cell * k_span;
        const std::int32_t footer = chrome::draw::px(k_footer);
        const RECT whole{0, 0, picture + 2, picture + 2 + footer};
        fill(dc, whole, RGB(0x20, 0x20, 0x20));

        // The screen around the point, blown up with no smoothing so each pixel stays a square.
        HDC screen = GetDC(nullptr);

        if (screen != nullptr)
        {
            SetStretchBltMode(dc, COLORONCOLOR);
            StretchBlt(dc, 1, 1, picture, picture, screen, m_x - k_span / 2, m_y - k_span / 2, k_span, k_span, SRCCOPY | CAPTUREBLT);
            ReleaseDC(nullptr, screen);
        }

        // The pixel that is picked, boxed in black and white so it shows on any colour.
        const std::int32_t middle = 1 + (k_span / 2) * cell;
        const RECT centre{middle, middle, middle + cell, middle + cell};
        const RECT outer{middle - 1, middle - 1, middle + cell + 1, middle + cell + 1};
        frame(dc, outer, RGB(0, 0, 0));
        frame(dc, centre, RGB(255, 255, 255));
        frame(dc, whole, RGB(0x70, 0x70, 0x70));

        // The colour of that pixel, as a chip and in hex.
        const std::int32_t chip = footer * 2 / 3;
        const std::int32_t top = picture + 2 + (footer - chip) / 2;
        const RECT swatch{6, top, 6 + chip, top + chip};
        fill(dc, swatch, to_colorref(m_rgb));
        frame(dc, swatch, RGB(0x70, 0x70, 0x70));

        wchar_t text[16];
        std::swprintf(text, sizeof(text) / sizeof(text[0]), L"#%06X", m_rgb & 0xFFFFFF);
        const HGDIOBJ old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0xE0, 0xE0, 0xE0));
        RECT label{swatch.right + 8, picture + 2, picture, picture + 2 + footer};
        DrawTextW(dc, text, -1, &label, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        SelectObject(dc, old_font);

        BitBlt(target, 0, 0, total_width, total_height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, old_bitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
}
