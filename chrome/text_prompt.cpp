#include <chrome/app_icon.hpp>
#include <chrome/text_prompt.hpp>

#include <chrome/controls/core/button.hpp>
#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/controls/core/text_field.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr wchar_t k_class[] = L"chrome_text_prompt";
        constexpr UINT_PTR k_timer = 1;
        constexpr UINT k_timer_ms = 30;
        constexpr std::int32_t k_client_width = 380;
        constexpr std::int32_t k_client_height = 150;
        constexpr std::int32_t k_margin = 16;
        constexpr std::int32_t k_text_capacity = 160;

        struct prompt
        {
            HWND window{nullptr};
            HFONT font{nullptr};
            control_set set;
            text_field field;
            button ok;
            button cancel;
            const char* label{nullptr};
            const char* why{nullptr};
            prompt_check_fn check{nullptr};
            void* user{nullptr};
            char text[k_text_capacity]{};
            bool valid{false};
            bool done{false};
            bool accepted{false};
        };

        [[nodiscard]] COLORREF colour_of(std::uint32_t rgb) noexcept
        {
            return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }

        // Reads the field and asks the check. The error shows only once something has been typed.
        void revalidate(prompt& p) noexcept
        {
            p.field.get_text(p.text, sizeof(p.text));
            p.why = nullptr;
            p.valid = p.check == nullptr || p.check(p.user, p.text, &p.why);

            if (p.text[0] == '\0')
                p.why = nullptr;

            p.ok.set_enabled(p.valid);
        }

        void accept(prompt& p) noexcept
        {
            revalidate(p);

            if (!p.valid)
                return;

            p.accepted = true;
            p.done = true;
        }

        void repaint(prompt& p) noexcept
        {
            InvalidateRect(p.window, nullptr, FALSE);
        }

        void lay_out(prompt& p, void* dc, std::int32_t width, std::int32_t height) noexcept
        {
            const std::int32_t margin = draw::px(k_margin);
            const std::int32_t field_height = p.field.preferred_size(dc).height;
            const std::int32_t field_top = margin + draw::px(24);
            p.field.set_area({margin, field_top, std::max(width - 2 * margin, 1), field_height});

            const std::int32_t button_width = draw::px(88);
            const std::int32_t button_height = draw::px(28);
            const std::int32_t y = height - margin - button_height;
            p.cancel.set_area({width - margin - button_width, y, button_width, button_height});
            p.ok.set_area({width - margin - 2 * button_width - draw::px(8), y, button_width, button_height});
        }

        LRESULT CALLBACK prompt_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_NCCREATE)
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(lparam)->lpCreateParams));

            prompt* state = reinterpret_cast<prompt*>(GetWindowLongPtrW(window, GWLP_USERDATA));

            if (state == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            prompt& p = *state;

            switch (message)
            {
            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT:
            {
                PAINTSTRUCT paint;
                const HDC dc = BeginPaint(window, &paint);
                RECT client{};
                GetClientRect(window, &client);
                const int width = std::max<int>(client.right, 1);
                const int height = std::max<int>(client.bottom, 1);
                const HDC buffer = CreateCompatibleDC(dc);
                const HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
                const HGDIOBJ old_bitmap = SelectObject(buffer, bitmap);
                const HGDIOBJ old_font = SelectObject(buffer, p.font);
                SetBkMode(buffer, TRANSPARENT);
                draw::frame shapes(buffer);

                const theme_colours& colours = theme();
                draw::fill(buffer, {0, 0, width, height}, colours.panel);
                lay_out(p, buffer, width, height);

                const std::int32_t margin = draw::px(k_margin);
                draw::text(buffer, {margin, margin, width - 2 * margin, draw::px(20)}, p.label != nullptr ? p.label : "", colours.text);

                if (p.why != nullptr)
                {
                    const dock_rect& box = p.field.area();
                    draw::text(buffer, {margin, box.y + box.height + draw::px(4), width - 2 * margin, draw::px(18)}, p.why, 0xE05555, draw::text_ellipsis);
                }

                p.set.paint(buffer);
                BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, old_font);
                SelectObject(buffer, old_bitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
                EndPaint(window, &paint);
                return 0;
            }

            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            {
                panel_mouse mouse;
                mouse.x = GET_X_LPARAM(lparam);
                mouse.y = GET_Y_LPARAM(lparam);
                mouse.ctrl = GetKeyState(VK_CONTROL) < 0;
                mouse.shift = GetKeyState(VK_SHIFT) < 0;

                if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
                {
                    mouse.event = message == WM_LBUTTONDBLCLK ? mouse_event::double_click : mouse_event::down;

                    if (GetCapture() != window)
                        SetCapture(window);
                }
                else if (message == WM_LBUTTONUP)
                {
                    mouse.event = mouse_event::up;

                    if (GetCapture() == window)
                        ReleaseCapture();
                }
                else
                {
                    mouse.event = mouse_event::move;
                    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                    TrackMouseEvent(&track);
                }

                const bool changed = p.set.on_mouse(mouse);

                if (changed || p.set.take_dirty())
                    repaint(p);

                return 0;
            }

            case WM_MOUSELEAVE:
            {
                panel_mouse mouse;
                mouse.event = mouse_event::leave;

                if (p.set.on_mouse(mouse))
                    repaint(p);

                return 0;
            }

            case WM_KEYDOWN:
            {
                const bool ctrl = GetKeyState(VK_CONTROL) < 0;
                const bool shift = GetKeyState(VK_SHIFT) < 0;
                const bool alt = GetKeyState(VK_MENU) < 0;

                if (wparam == VK_ESCAPE)
                {
                    p.done = true;
                    return 0;
                }

                if (wparam == VK_RETURN && !p.ok.focused() && !p.cancel.focused())
                {
                    accept(p);
                    repaint(p);
                    return 0;
                }

                // Enter on a focused button clicks it, as Space does.
                if ((wparam == VK_RETURN || wparam == VK_SPACE) && (p.ok.focused() || p.cancel.focused()))
                {
                    if (p.ok.focused() && p.ok.enabled())
                        accept(p);
                    else if (p.cancel.focused())
                        p.done = true;

                    repaint(p);
                    return 0;
                }

                if (p.set.on_key(static_cast<std::uint32_t>(wparam), ctrl, shift, alt) || p.set.take_dirty())
                    repaint(p);

                return 0;
            }

            case WM_CHAR:
                if (wparam >= 32)
                    p.set.on_char(static_cast<std::uint32_t>(wparam));

                if (p.set.take_dirty())
                    repaint(p);

                return 0;

            case WM_TIMER:
                if (wparam == k_timer)
                {
                    const bool moved = p.set.animate(GetTickCount64());

                    if (moved || p.set.take_dirty())
                        repaint(p);
                }

                return 0;

            case WM_CLOSE:
                p.done = true;
                return 0;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        [[nodiscard]] bool register_class() noexcept
        {
            static bool done = false;

            if (done)
                return true;

            WNDCLASSEXW c{sizeof(c)};
            c.lpfnWndProc = prompt_proc;
            c.hInstance = GetModuleHandleW(nullptr);
            c.hIcon = app_icon_big();
            c.hIconSm = app_icon_small();
            c.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            c.lpszClassName = k_class;
            done = RegisterClassExW(&c) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
            return done;
        }

        bool field_acted(void* user, text_field&, text_action action) noexcept
        {
            prompt& p = *static_cast<prompt*>(user);

            if (action == text_action::changed)
            {
                revalidate(p);
                repaint(p);
            }
            else if (action == text_action::submitted)
            {
                accept(p);
                repaint(p);
            }

            return true;
        }

        void ok_clicked(void* user, button&) noexcept
        {
            accept(*static_cast<prompt*>(user));
        }

        void cancel_clicked(void* user, button&) noexcept
        {
            static_cast<prompt*>(user)->done = true;
        }
    }

    bool prompt_text(void* owner_window, const char* title, const char* label, const char* initial, char* out, std::uint32_t capacity,
        prompt_check_fn check, void* user) noexcept
    {
        const HWND owner = static_cast<HWND>(owner_window);

        if (out == nullptr || capacity == 0 || !register_class())
            return false;

        // The state is large, so it does not live on the stack.
        static prompt p;
        p = prompt{};
        p.label = label;
        p.check = check;
        p.user = user;

        const UINT dpi = owner != nullptr ? GetDpiForWindow(owner) : 96;
        p.font = CreateFontW(-MulDiv(14, static_cast<int>(dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        p.field.set_text(initial != nullptr ? initial : "").set_max_length(k_text_capacity - 1).set_on_action(&field_acted, &p);
        p.ok.set_label("OK").set_style(button_style::primary).set_on_click(&ok_clicked, &p);
        p.cancel.set_label("Cancel").set_on_click(&cancel_clicked, &p);
        (void)p.set.add(p.field);
        (void)p.set.add(p.ok);
        (void)p.set.add(p.cancel);

        wchar_t wide_title[96]{};
        MultiByteToWideChar(CP_UTF8, 0, title != nullptr ? title : "", -1, wide_title, 96);

        const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        RECT frame{0, 0, draw::px(k_client_width), draw::px(k_client_height)};
        AdjustWindowRectExForDpi(&frame, style, FALSE, WS_EX_DLGMODALFRAME, dpi);
        const int width = frame.right - frame.left;
        const int height = frame.bottom - frame.top;

        RECT around{0, 0, 1280, 720};

        if (owner != nullptr)
            GetWindowRect(owner, &around);

        p.window = CreateWindowExW(WS_EX_DLGMODALFRAME, k_class, wide_title, style, around.left + (around.right - around.left - width) / 2,
            around.top + (around.bottom - around.top - height) / 3, width, height, owner, nullptr, GetModuleHandleW(nullptr), &p);

        if (p.window == nullptr)
        {
            DeleteObject(p.font);
            return false;
        }

        const theme_colours& colours = theme();
        const BOOL dark = g_theme_mode == theme_mode::dark ? TRUE : FALSE;
        const COLORREF caption = colour_of(colours.title);
        const COLORREF text = colour_of(colours.text);
        const COLORREF border = colour_of(colours.splitter);
        DwmSetWindowAttribute(p.window, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(p.window, 35, &caption, sizeof(caption));
        DwmSetWindowAttribute(p.window, 36, &text, sizeof(text));
        DwmSetWindowAttribute(p.window, 34, &border, sizeof(border));

        if (owner != nullptr)
            EnableWindow(owner, FALSE);

        revalidate(p);
        ShowWindow(p.window, SW_SHOW);
        SetForegroundWindow(p.window);
        SetFocus(p.window);
        p.set.set_focus(&p.field);
        p.field.select_all();
        SetTimer(p.window, k_timer, k_timer_ms, nullptr);

        MSG message;
        bool quit = false;

        while (!p.done)
        {
            const BOOL got = GetMessageW(&message, nullptr, 0, 0);

            if (got == 0)
            {
                // The editor is closing. The quit goes on to its own loop.
                PostQuitMessage(static_cast<int>(message.wParam));
                quit = true;
                break;
            }

            if (got < 0)
                break;

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        (void)quit;
        KillTimer(p.window, k_timer);

        if (owner != nullptr)
            EnableWindow(owner, TRUE);

        // The window is taken down before the owner gets the focus back, or the focus goes elsewhere.
        DestroyWindow(p.window);

        if (owner != nullptr)
            SetForegroundWindow(owner);

        DeleteObject(p.font);

        if (!p.accepted)
            return false;

        std::snprintf(out, capacity, "%s", p.text);
        return true;
    }
}
