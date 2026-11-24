#include <chrome/app_icon.hpp>
#include <chrome/modal_dialog.hpp>

#include <chrome/controls/core/checkbox.hpp>
#include <chrome/controls/core/control.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <algorithm>

namespace chrome
{
    namespace
    {
        constexpr wchar_t k_class[] = L"chrome_modal_dialog";
        constexpr UINT_PTR k_timer = 1;
        constexpr UINT k_timer_ms = 30;
        constexpr std::int32_t k_margin = 20;
        constexpr std::int32_t k_badge = 36;
        constexpr std::int32_t k_gap = 14;
        constexpr std::int32_t k_footer = 56;
        constexpr std::int32_t k_button_height = 28;

        struct dialog;

        // Tells a button's click which dialog and which button it belongs to.
        struct slot
        {
            dialog* owner{nullptr};
            std::uint32_t index{0};
        };

        struct dialog
        {
            HWND window{nullptr};
            HFONT font{nullptr};
            control_set set;
            button buttons[dialog_spec::k_max_buttons];
            slot slots[dialog_spec::k_max_buttons];
            std::uint32_t button_count{0};
            checkbox check;
            dialog_spec spec;
            // Where everything goes, in pixels, worked out once before the window is made.
            dock_rect badge_box{};
            dock_rect heading_box{};
            dock_rect message_box{};
            dock_rect content_box{};
            dock_rect footer_box{};
            std::int32_t width{0};
            std::int32_t height{0};
            std::int32_t result{-1};
            bool done{false};
        };

        bool g_open = false;

        [[nodiscard]] COLORREF colour_of(std::uint32_t rgb) noexcept
        {
            return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }

        void repaint(dialog& d) noexcept
        {
            InvalidateRect(d.window, nullptr, FALSE);
        }

        // Works out the size of the client area and the place of every part, with the font selected in dc.
        void measure(dialog& d, void* dc) noexcept
        {
            const dialog_spec& spec = d.spec;
            const std::int32_t margin = draw::px(k_margin);
            const bool has_badge = spec.icon != notice_kind::none;
            const std::int32_t badge = draw::px(k_badge);
            const std::int32_t text_x = margin + (has_badge ? badge + draw::px(k_gap) : 0);
            d.width = draw::px(spec.width);
            const std::int32_t text_width = std::max(d.width - text_x - margin, 1);
            std::int32_t y = margin;

            const std::int32_t heading_height = spec.heading != nullptr ? draw::text_height(dc, spec.heading, text_width, draw::text_bold | draw::text_wrap) : 0;
            d.heading_box = {text_x, y, text_width, heading_height};
            y += heading_height;

            if (spec.heading != nullptr && spec.message != nullptr)
                y += draw::px(6);

            const std::int32_t message_height = spec.message != nullptr ? draw::text_height(dc, spec.message, text_width, draw::text_wrap) : 0;
            d.message_box = {text_x, y, text_width, message_height};
            y += message_height;
            y = std::max(y, margin + (has_badge ? badge : 0));
            d.badge_box = {margin, margin, badge, badge};

            d.content_box = {};

            if (spec.paint_fn != nullptr && spec.paint_height > 0)
            {
                y += draw::px(k_gap);
                d.content_box = {margin, y, std::max(d.width - 2 * margin, 1), draw::px(spec.paint_height)};
                y += d.content_box.height;
            }

            if (spec.check_label != nullptr && spec.checked != nullptr)
            {
                y += draw::px(k_gap);
                const control_size size = d.check.preferred_size(dc);
                d.check.set_area({margin, y, size.width, size.height});
                y += size.height;
            }

            y += margin;
            d.footer_box = {0, y, d.width, draw::px(k_footer)};
            d.height = y + d.footer_box.height;

            // The buttons sit at the right end of the footer, as wide as their labels need.
            const std::int32_t button_height = draw::px(k_button_height);
            const std::int32_t top = d.footer_box.y + (d.footer_box.height - button_height) / 2;
            std::int32_t right = d.width - margin;

            for (std::uint32_t i = d.button_count; i-- > 0;)
            {
                const std::int32_t width = std::max(draw::px(88), draw::text_width(dc, d.spec.labels[i]) + draw::px(32));
                d.buttons[i].set_area({right - width, top, width, button_height});
                right -= width + draw::px(8);
            }
        }

        // Closes the dialog with a button as the answer, unless the handler wants it left open.
        void click(dialog& d, std::uint32_t index) noexcept
        {
            if (index >= d.button_count)
                return;

            if (d.spec.on_click != nullptr && !d.spec.on_click(d.spec.user, index))
            {
                repaint(d);
                return;
            }

            d.result = static_cast<std::int32_t>(index);
            d.done = true;
        }

        [[nodiscard]] std::int32_t focused_button(const dialog& d) noexcept
        {
            for (std::uint32_t i = 0; i < d.button_count; ++i)
            {
                if (d.buttons[i].focused())
                    return static_cast<std::int32_t>(i);
            }

            return -1;
        }

        LRESULT CALLBACK dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_NCCREATE)
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<const CREATESTRUCTW*>(lparam)->lpCreateParams));

            dialog* state = reinterpret_cast<dialog*>(GetWindowLongPtrW(window, GWLP_USERDATA));

            if (state == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            dialog& d = *state;

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
                const HGDIOBJ old_font = SelectObject(buffer, d.font);
                SetBkMode(buffer, TRANSPARENT);
                draw::frame shapes(buffer);

                const theme_colours& colours = theme();
                draw::fill(buffer, {0, 0, width, height}, colours.panel);

                if (d.spec.icon != notice_kind::none)
                    notice::paint_badge(buffer, d.spec.icon, d.badge_box);

                if (d.spec.heading != nullptr)
                    draw::text(buffer, d.heading_box, d.spec.heading, colours.text, draw::text_bold | draw::text_wrap);

                if (d.spec.message != nullptr)
                    draw::text(buffer, d.message_box, d.spec.message, colours.text_dim, draw::text_wrap);

                if (d.content_box.height > 0)
                    d.spec.paint_fn(d.spec.paint_user, buffer, d.content_box);

                // The footer is a little darker, with a line above it.
                draw::fill(buffer, d.footer_box, blend_colour(colours.panel, 0x000000, 0.18f));
                draw::fill(buffer, {0, d.footer_box.y, width, 1}, colours.splitter);

                d.set.paint(buffer);
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

                const bool changed = d.set.on_mouse(mouse);

                if (changed || d.set.take_dirty())
                    repaint(d);

                return 0;
            }

            case WM_MOUSELEAVE:
            {
                panel_mouse mouse;
                mouse.event = mouse_event::leave;

                if (d.set.on_mouse(mouse))
                    repaint(d);

                return 0;
            }

            case WM_KEYDOWN:
            {
                const bool ctrl = GetKeyState(VK_CONTROL) < 0;
                const bool shift = GetKeyState(VK_SHIFT) < 0;
                const bool alt = GetKeyState(VK_MENU) < 0;
                const std::int32_t on_button = focused_button(d);

                if (wparam == VK_ESCAPE)
                {
                    d.result = d.spec.cancel_button;
                    d.done = true;
                    return 0;
                }

                // Enter presses the button that has the focus, or else the default one. Space only a focused button.
                if (wparam == VK_RETURN || (wparam == VK_SPACE && on_button >= 0))
                {
                    click(d, on_button >= 0 ? static_cast<std::uint32_t>(on_button) : d.spec.default_button);
                    repaint(d);
                    return 0;
                }

                // The arrows move along the buttons.
                if ((wparam == VK_LEFT || wparam == VK_RIGHT) && on_button >= 0)
                {
                    const std::int32_t next = std::clamp(on_button + (wparam == VK_RIGHT ? 1 : -1), 0, static_cast<std::int32_t>(d.button_count) - 1);
                    d.set.set_focus(&d.buttons[next]);
                    repaint(d);
                    return 0;
                }

                if (d.set.on_key(static_cast<std::uint32_t>(wparam), ctrl, shift, alt) || d.set.take_dirty())
                    repaint(d);

                return 0;
            }

            case WM_CHAR:
                if (wparam >= 32)
                    d.set.on_char(static_cast<std::uint32_t>(wparam));

                if (d.set.take_dirty())
                    repaint(d);

                return 0;

            case WM_TIMER:
                if (wparam == k_timer)
                {
                    const bool moved = d.set.animate(GetTickCount64());

                    if (moved || d.set.take_dirty())
                        repaint(d);
                }

                return 0;

            case WM_CLOSE:
                d.result = d.spec.cancel_button;
                d.done = true;
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
            c.lpfnWndProc = dialog_proc;
            c.hInstance = GetModuleHandleW(nullptr);
            c.hIcon = app_icon_big();
            c.hIconSm = app_icon_small();
            c.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            c.lpszClassName = k_class;
            done = RegisterClassExW(&c) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
            return done;
        }

        void button_clicked(void* user, button&) noexcept
        {
            const slot& s = *static_cast<const slot*>(user);
            click(*s.owner, s.index);
        }
    }

    std::int32_t show_dialog(void* owner_window, const dialog_spec& spec) noexcept
    {
        const HWND owner = static_cast<HWND>(owner_window);

        if (g_open || !register_class())
            return -1;

        // The state is large, so it does not live on the stack.
        static dialog d;
        d = dialog{};
        d.spec = spec;

        if (d.spec.button_count == 0)
            d.spec.add_button("OK", button_style::primary);

        d.button_count = d.spec.button_count;
        d.spec.default_button = std::min(d.spec.default_button, d.button_count - 1);

        const UINT dpi = owner != nullptr ? GetDpiForWindow(owner) : 96;
        d.font = CreateFontW(-MulDiv(14, static_cast<int>(dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        for (std::uint32_t i = 0; i < d.button_count; ++i)
        {
            d.slots[i] = {&d, i};
            d.buttons[i].set_label(d.spec.labels[i]).set_style(d.spec.styles[i]).set_on_click(&button_clicked, &d.slots[i]);
            (void)d.set.add(d.buttons[i]);
        }

        const bool has_check = d.spec.check_label != nullptr && d.spec.checked != nullptr;

        if (has_check)
        {
            d.check.set_label(d.spec.check_label).set_checked(*d.spec.checked);
            (void)d.set.add(d.check);
        }

        // Measured on the screen's device context with the dialog's font, before the window exists.
        const HDC screen = GetDC(nullptr);
        const HGDIOBJ previous = SelectObject(screen, d.font);
        measure(d, screen);
        SelectObject(screen, previous);
        ReleaseDC(nullptr, screen);

        wchar_t wide_title[96]{};
        MultiByteToWideChar(CP_UTF8, 0, d.spec.title != nullptr ? d.spec.title : "", -1, wide_title, 96);

        const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        RECT frame{0, 0, d.width, d.height};
        AdjustWindowRectExForDpi(&frame, style, FALSE, WS_EX_DLGMODALFRAME, dpi);
        const int width = frame.right - frame.left;
        const int height = frame.bottom - frame.top;

        RECT around{0, 0, 1280, 720};

        if (owner != nullptr)
            GetWindowRect(owner, &around);

        g_open = true;
        d.window = CreateWindowExW(WS_EX_DLGMODALFRAME, k_class, wide_title, style, around.left + (around.right - around.left - width) / 2,
            around.top + (around.bottom - around.top - height) / 3, width, height, owner, nullptr, GetModuleHandleW(nullptr), &d);

        if (d.window == nullptr)
        {
            DeleteObject(d.font);
            g_open = false;
            return -1;
        }

        const theme_colours& colours = theme();
        const BOOL dark = g_theme_mode == theme_mode::dark ? TRUE : FALSE;
        const COLORREF caption = colour_of(colours.title);
        const COLORREF text = colour_of(colours.text);
        const COLORREF border = colour_of(colours.splitter);
        DwmSetWindowAttribute(d.window, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(d.window, 35, &caption, sizeof(caption));
        DwmSetWindowAttribute(d.window, 36, &text, sizeof(text));
        DwmSetWindowAttribute(d.window, 34, &border, sizeof(border));

        if (owner != nullptr)
            EnableWindow(owner, FALSE);

        ShowWindow(d.window, SW_SHOW);
        SetForegroundWindow(d.window);
        SetFocus(d.window);
        d.set.set_focus(&d.buttons[d.spec.default_button]);
        SetTimer(d.window, k_timer, k_timer_ms, nullptr);

        MSG message;

        while (!d.done)
        {
            const BOOL got = GetMessageW(&message, nullptr, 0, 0);

            if (got == 0)
            {
                // The editor is closing. The quit goes on to its own loop.
                PostQuitMessage(static_cast<int>(message.wParam));
                break;
            }

            if (got < 0)
                break;

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        KillTimer(d.window, k_timer);

        if (has_check)
            *d.spec.checked = d.check.checked();

        if (owner != nullptr)
            EnableWindow(owner, TRUE);

        // The window is taken down before the owner gets the focus back, or the focus goes elsewhere.
        DestroyWindow(d.window);

        if (owner != nullptr)
            SetForegroundWindow(owner);

        DeleteObject(d.font);
        g_open = false;
        return d.result;
    }

    bool confirm_dialog(void* owner, const char* title, const char* heading, const char* message, const char* yes, const char* no,
        notice_kind icon) noexcept
    {
        dialog_spec spec;
        spec.set_title(title).set_heading(heading).set_message(message).set_icon(icon);
        spec.add_button(yes, button_style::primary).add_button(no).set_default(0).set_cancel(1);
        return show_dialog(owner, spec) == 0;
    }
}
