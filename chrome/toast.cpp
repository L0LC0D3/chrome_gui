#include <chrome/toast.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace chrome::toast
{
    namespace
    {
        constexpr wchar_t k_toast_class[] = L"chrome_toast";
        constexpr wchar_t k_host_class[] = L"chrome_toast_host";
        constexpr UINT_PTR k_timer = 1;
        constexpr UINT k_timer_ms = 16;
        constexpr std::uint32_t k_max_toasts = 5;
        constexpr std::int32_t k_width = 360;
        constexpr std::int32_t k_pad = 12;
        constexpr std::int32_t k_badge = 22;
        constexpr std::int32_t k_close = 16;
        constexpr std::int32_t k_gap = 8;
        constexpr std::int32_t k_max_lines = 4;
        constexpr float k_enter_ms = 180.0f;
        constexpr float k_leave_ms = 220.0f;

        enum class phase : std::uint8_t
        {
            entering,
            showing,
            leaving
        };

        struct item
        {
            bool used{false};
            HWND window{nullptr};
            notice_kind kind{notice_kind::info};
            char title[64]{};
            char text[240]{};
            char action[32]{};
            action_fn on_action{nullptr};
            void* user{nullptr};
            std::uint32_t duration{0};
            // The time it has been on show with the pointer away, and how far through its fade it is.
            float elapsed{0.0f};
            float alpha{0.0f};
            float y{0.0f};
            std::uint64_t serial{0};
            phase state{phase::entering};
            bool hover{false};
            bool hot_close{false};
            bool hot_action{false};
            bool tracking{false};
            // Where the parts are in the window, in pixels, and the size of the window.
            std::int32_t width{0};
            std::int32_t height{0};
            dock_rect badge_box{};
            dock_rect title_box{};
            dock_rect text_box{};
            dock_rect action_box{};
            dock_rect close_box{};
        };

        item g_items[k_max_toasts];
        HWND g_main = nullptr;
        HWND g_host = nullptr;
        HFONT g_font = nullptr;
        bool g_timer_on = false;
        std::uint64_t g_serial = 0;
        std::uint64_t g_last = 0;

        [[nodiscard]] bool inside(const dock_rect& box, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
        }

        // Places the parts of a toast and works out its size, with the font selected in dc.
        void lay_out(item& t, void* dc) noexcept
        {
            const std::int32_t pad = draw::px(k_pad);
            const std::int32_t badge = draw::px(k_badge);
            const std::int32_t text_x = pad + badge + draw::px(10);
            t.width = draw::px(k_width);
            const std::int32_t text_width = std::max(t.width - text_x - pad - draw::px(k_close) - draw::px(6), 1);
            const std::int32_t line = std::max(draw::text_height(dc, "Ag", 4000), 1);
            std::int32_t y = pad;

            t.title_box = {};

            if (t.title[0] != '\0')
            {
                t.title_box = {text_x, y, text_width, line};
                y += line + draw::px(2);
            }

            const std::int32_t wanted = t.text[0] != '\0' ? draw::text_height(dc, t.text, text_width, draw::text_wrap) : 0;
            const std::int32_t text_height = std::min(wanted, line * k_max_lines);
            t.text_box = {text_x, y, text_width, text_height};
            y += text_height;

            t.action_box = {};

            if (t.action[0] != '\0')
            {
                y += draw::px(6);
                t.action_box = {text_x, y, draw::text_width(dc, t.action, draw::text_underline), line};
                y += line;
            }

            t.badge_box = {pad, pad, badge, badge};
            t.close_box = {t.width - pad - draw::px(k_close) + draw::px(2), pad - draw::px(2), draw::px(k_close), draw::px(k_close)};
            t.height = std::max(y, pad + badge) + pad;
        }

        void paint(item& t, HDC dc) noexcept
        {
            const theme_colours& colours = theme();
            const std::uint32_t face = blend_colour(colours.panel, colours.text, 0.07f);
            const std::uint32_t edge = blend_colour(colours.panel, colours.text, 0.24f);
            const std::uint32_t tone = notice::colour(t.kind);
            draw::fill(dc, {0, 0, t.width, t.height}, face);
            draw::outline(dc, {0, 0, t.width, t.height}, 0, edge, 1.0f);
            draw::fill(dc, {0, 0, draw::px(3), t.height}, tone);

            notice::paint_badge(dc, t.kind, t.badge_box);

            if (t.title[0] != '\0')
                draw::text(dc, t.title_box, t.title, colours.text, draw::text_bold | draw::text_ellipsis);

            if (t.text[0] != '\0')
                draw::text(dc, t.text_box, t.text, t.title[0] != '\0' ? colours.text_dim : colours.text, draw::text_wrap);

            if (t.action[0] != '\0')
                draw::text(dc, t.action_box, t.action, t.hot_action ? blend_colour(colours.accent, 0xFFFFFF, 0.3f) : colours.accent, draw::text_underline);

            if (t.hot_close)
                draw::rounded(dc, t.close_box, 3, colours.hover, colours.hover);

            const dock_rect cross{t.close_box.x + draw::px(4), t.close_box.y + draw::px(4), t.close_box.width - draw::px(8), t.close_box.height - draw::px(8)};
            draw::icon(dc, icon_kind::cross, cross, t.hot_close ? colours.text : colours.text_dim);

            // A thin line at the bottom runs down as the time goes.
            if (t.duration > 0 && t.state != phase::leaving)
            {
                const float left = std::clamp(1.0f - t.elapsed / static_cast<float>(t.duration), 0.0f, 1.0f);
                draw::fill(dc, {0, t.height - draw::px(2), static_cast<std::int32_t>(static_cast<float>(t.width) * left), draw::px(2)}, tone);
            }
        }

        void begin_leave(item& t) noexcept
        {
            if (t.state != phase::leaving)
            {
                t.state = phase::leaving;
                t.hot_close = false;
                t.hot_action = false;
            }
        }

        void release(item& t) noexcept
        {
            if (t.window != nullptr)
                DestroyWindow(t.window);

            t = item{};
        }

        LRESULT CALLBACK toast_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_NCCREATE)
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(lparam)->lpCreateParams));

            item* state = reinterpret_cast<item*>(GetWindowLongPtrW(window, GWLP_USERDATA));

            if (state == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            item& t = *state;

            switch (message)
            {
            case WM_ERASEBKGND:
                return 1;

            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;

            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                const HDC dc = BeginPaint(window, &ps);
                const int width = std::max(t.width, 1);
                const int height = std::max(t.height, 1);
                const HDC buffer = CreateCompatibleDC(dc);
                const HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
                const HGDIOBJ old_bitmap = SelectObject(buffer, bitmap);
                const HGDIOBJ old_font = SelectObject(buffer, g_font);
                SetBkMode(buffer, TRANSPARENT);
                draw::frame shapes(buffer);
                paint(t, buffer);
                BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, old_font);
                SelectObject(buffer, old_bitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
                EndPaint(window, &ps);
                return 0;
            }

            case WM_SETCURSOR:
                if (t.hot_close || t.hot_action)
                {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));
                    return TRUE;
                }

                break;

            case WM_MOUSEMOVE:
            {
                const std::int32_t x = GET_X_LPARAM(lparam);
                const std::int32_t y = GET_Y_LPARAM(lparam);
                const bool close = inside(t.close_box, x, y);
                const bool action = t.action[0] != '\0' && inside(t.action_box, x, y);

                if (!t.tracking)
                {
                    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                    TrackMouseEvent(&track);
                    t.tracking = true;
                }

                if (close != t.hot_close || action != t.hot_action)
                {
                    t.hot_close = close;
                    t.hot_action = action;
                    InvalidateRect(window, nullptr, FALSE);
                }

                return 0;
            }

            case WM_MOUSELEAVE:
                t.tracking = false;
                t.hot_close = false;
                t.hot_action = false;
                InvalidateRect(window, nullptr, FALSE);
                return 0;

            case WM_LBUTTONUP:
            {
                if (t.state == phase::leaving)
                    return 0;

                const std::int32_t x = GET_X_LPARAM(lparam);
                const std::int32_t y = GET_Y_LPARAM(lparam);

                if (t.action[0] != '\0' && inside(t.action_box, x, y) && t.on_action != nullptr)
                    t.on_action(t.user);

                // A click anywhere closes it, the cross, the link and the body alike.
                begin_leave(t);
                return 0;
            }

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        // Moves every toast towards its place in the stack, fades them, and ends the ones that are done.
        void tick() noexcept
        {
            const std::uint64_t now = GetTickCount64();
            const float dt = static_cast<float>(std::min<std::uint64_t>(now - g_last, 100));
            g_last = now;

            RECT frame{};
            const bool visible = g_main != nullptr && IsWindow(g_main) && IsWindowVisible(g_main) && !IsIconic(g_main) && GetWindowRect(g_main, &frame);
            POINT cursor{};
            GetCursorPos(&cursor);

            for (item& t : g_items)
            {
                if (!t.used)
                    continue;

                RECT box{};
                GetWindowRect(t.window, &box);
                t.hover = visible && PtInRect(&box, cursor) != FALSE;

                switch (t.state)
                {
                case phase::entering:
                    t.alpha = std::min(t.alpha + dt / k_enter_ms, 1.0f);

                    if (t.alpha >= 1.0f)
                        t.state = phase::showing;

                    break;
                case phase::showing:
                    // The clock stands still while the pointer is on it, or the window is out of sight.
                    if (visible && !t.hover && t.duration > 0)
                        t.elapsed += dt;

                    if (t.duration > 0 && t.elapsed >= static_cast<float>(t.duration))
                        begin_leave(t);

                    break;
                case phase::leaving:
                    t.alpha -= dt / k_leave_ms;
                    break;
                }
            }

            for (item& t : g_items)
            {
                if (t.used && t.state == phase::leaving && t.alpha <= 0.0f)
                    release(t);
            }

            // The newest sits at the bottom, the older ones above it.
            item* order[k_max_toasts];
            std::uint32_t count = 0;

            for (item& t : g_items)
            {
                if (t.used)
                    order[count++] = &t;
            }

            std::sort(order, order + count, [](const item* a, const item* b) { return a->serial > b->serial; });

            const std::int32_t margin = draw::px(16);
            const std::int32_t gap = draw::px(k_gap);
            std::int32_t bottom = frame.bottom - draw::px(40);
            const float follow = 1.0f - std::exp(-dt / 70.0f);

            for (std::uint32_t i = 0; i < count; ++i)
            {
                item& t = *order[i];
                const float target = static_cast<float>(bottom - t.height);
                t.y += (target - t.y) * follow;
                bottom -= t.height + gap;

                if (!visible)
                {
                    ShowWindow(t.window, SW_HIDE);
                    continue;
                }

                // A toast that is entering also rises a little, and the one that leaves is already where it will fade.
                const float lift = t.state == phase::entering ? (1.0f - t.alpha) * static_cast<float>(draw::px(16)) : 0.0f;
                SetWindowPos(t.window, nullptr, frame.right - margin - t.width, static_cast<int>(std::lround(t.y + lift)), t.width, t.height,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
                SetLayeredWindowAttributes(t.window, 0, static_cast<BYTE>(std::clamp(t.alpha, 0.0f, 1.0f) * 255.0f), LWA_ALPHA);

                // The line at the bottom moves with the clock.
                if (t.duration > 0 && t.state == phase::showing && !t.hover)
                    InvalidateRect(t.window, nullptr, FALSE);
            }

            if (count == 0 && g_timer_on)
            {
                KillTimer(g_host, k_timer);
                g_timer_on = false;
            }
        }

        LRESULT CALLBACK host_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_TIMER && wparam == k_timer)
            {
                tick();
                return 0;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        [[nodiscard]] bool register_classes() noexcept
        {
            static bool done = false;

            if (done)
                return true;

            const HINSTANCE module = GetModuleHandleW(nullptr);
            WNDCLASSEXW toast_class{sizeof(toast_class)};
            toast_class.lpfnWndProc = toast_proc;
            toast_class.hInstance = module;
            toast_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            toast_class.lpszClassName = k_toast_class;

            WNDCLASSEXW host_class{sizeof(host_class)};
            host_class.lpfnWndProc = host_proc;
            host_class.hInstance = module;
            host_class.lpszClassName = k_host_class;

            done = (RegisterClassExW(&toast_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) &&
                (RegisterClassExW(&host_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
            return done;
        }

        void copy_text(char* out, std::size_t capacity, const char* text) noexcept
        {
            std::snprintf(out, capacity, "%s", text != nullptr ? text : "");
        }
    }

    void install(void* main_window) noexcept
    {
        g_main = static_cast<HWND>(main_window);

        if (!register_classes())
            return;

        if (g_host == nullptr)
            g_host = CreateWindowExW(0, k_host_class, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);

        if (g_font != nullptr)
            DeleteObject(g_font);

        const UINT dpi = g_main != nullptr ? GetDpiForWindow(g_main) : 96;
        g_font = CreateFontW(-MulDiv(14, static_cast<int>(dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    void show(const options& what) noexcept
    {
        if (g_main == nullptr || g_host == nullptr || g_font == nullptr || (what.text == nullptr && what.title == nullptr))
            return;

        // With every place taken the oldest makes room.
        item* slot = nullptr;
        item* oldest = nullptr;

        for (item& t : g_items)
        {
            if (!t.used)
            {
                slot = &t;
                break;
            }

            if (oldest == nullptr || t.serial < oldest->serial)
                oldest = &t;
        }

        if (slot == nullptr)
        {
            release(*oldest);
            slot = oldest;
        }

        item& t = *slot;
        t = item{};
        t.used = true;
        t.kind = what.kind == notice_kind::none || what.kind == notice_kind::question ? notice_kind::info : what.kind;
        copy_text(t.title, sizeof(t.title), what.title);
        copy_text(t.text, sizeof(t.text), what.text);
        copy_text(t.action, sizeof(t.action), what.action);
        t.on_action = what.on_action;
        t.user = what.user;
        t.duration = what.duration;
        t.serial = ++g_serial;

        const HDC screen = GetDC(nullptr);
        const HGDIOBJ previous = SelectObject(screen, g_font);
        lay_out(t, screen);
        SelectObject(screen, previous);
        ReleaseDC(nullptr, screen);

        RECT frame{};
        GetWindowRect(g_main, &frame);
        t.y = static_cast<float>(frame.bottom - draw::px(40) - t.height);

        t.window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, k_toast_class, L"", WS_POPUP, frame.right - draw::px(16) - t.width,
            static_cast<int>(t.y), t.width, t.height, g_main, nullptr, GetModuleHandleW(nullptr), &t);

        if (t.window == nullptr)
        {
            t = item{};
            return;
        }

        // Windows 11 rounds the corners of a popup that asks for it.
        const int round = 2;
        DwmSetWindowAttribute(t.window, 33, &round, sizeof(round));
        SetLayeredWindowAttributes(t.window, 0, 0, LWA_ALPHA);
        ShowWindow(t.window, SW_SHOWNOACTIVATE);

        if (!g_timer_on)
        {
            g_last = GetTickCount64();
            g_timer_on = SetTimer(g_host, k_timer, k_timer_ms, nullptr) != 0;
        }
    }

    void info(const char* text) noexcept
    {
        options what;
        what.text = text;
        show(what);
    }

    void success(const char* text) noexcept
    {
        options what;
        what.kind = notice_kind::success;
        what.text = text;
        show(what);
    }

    void warning(const char* text) noexcept
    {
        options what;
        what.kind = notice_kind::warning;
        what.text = text;
        what.duration = 6000;
        show(what);
    }

    void error(const char* text) noexcept
    {
        options what;
        what.kind = notice_kind::error;
        what.text = text;
        what.duration = 8000;
        show(what);
    }

    void clear() noexcept
    {
        for (item& t : g_items)
        {
            if (t.used)
                release(t);
        }

        if (g_timer_on && g_host != nullptr)
        {
            KillTimer(g_host, k_timer);
            g_timer_on = false;
        }
    }
}
