#include <chrome/dock_host.hpp>

#include <chrome/theme.hpp>
#include <chrome/controls/core/draw.hpp>
#include <chrome/controls/core/tab_style.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <timeapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace chrome
{
    namespace
    {
        wchar_t g_title_suffix[64]{};
        constexpr wchar_t k_host_class[] = L"chrome_dock_host";
        constexpr wchar_t k_group_class[] = L"chrome_dock_group";
        constexpr wchar_t k_panel_class[] = L"chrome_panel";
        constexpr wchar_t k_viewport_class[] = L"chrome_viewport";
        constexpr wchar_t k_float_class[] = L"chrome_dock_float";
        constexpr wchar_t k_preview_class[] = L"chrome_dock_preview";
        constexpr wchar_t k_menubar_class[] = L"chrome_menubar";
        constexpr wchar_t k_hint_class[] = L"chrome_dock_hint";
        constexpr wchar_t k_tooltip_class[] = L"chrome_tooltip";
        constexpr wchar_t k_ghost_class[] = L"chrome_drag_ghost";
        // How long the mouse rests on a control before its tooltip shows.
        constexpr UINT k_tooltip_delay_ms = 500;
        constexpr UINT_PTR k_tooltip_timer = 2;
        // Pixels of this colour are see-through in the hint window.
        constexpr COLORREF k_hint_key = RGB(255, 0, 255);
        // The multiplication sign, spelled as a code unit so the source encoding cannot change it.
        constexpr wchar_t k_close_glyph[] = {0x00D7, 0};
        constexpr UINT k_menu_tab_base = 5000;
        // The ids of the rows of a menu with an action. They count up from here in the order the rows were added.
        constexpr UINT k_command_base = 6000;
        // How long the layout has to stand still before a viewport takes its new size.
        constexpr std::uint64_t k_settle_ms = 120;
        // How far a pressed tab has to move before it counts as a drag.
        constexpr int k_drag_distance = 6;
        // The line around a floating window, in pixels.
        constexpr int k_float_frame = 1;
        constexpr int k_float_min_width = 320;
        constexpr int k_float_min_height = 240;

        HFONT g_font = nullptr;

        // A size given at 96 dpi, in pixels at the current scale.
        [[nodiscard]] int px(int pixels) noexcept
        {
            return dock_model::scaled(pixels);
        }

        [[nodiscard]] std::uint32_t mix_colour(std::uint32_t a, std::uint32_t b, float t) noexcept
        {
            const auto channel = [&](int shift) {
                const float value = static_cast<float>((a >> shift) & 0xFF) * (1.0f - t) + static_cast<float>((b >> shift) & 0xFF) * t;
                return static_cast<std::uint32_t>(std::lround(value));
            };

            return (channel(16) << 16) | (channel(8) << 8) | channel(0);
        }

        // Moves a window frame so it is on the monitor it is nearest to, and shrinks it when it does not fit.
        void clamp_to_work(RECT& frame) noexcept
        {
            const HMONITOR monitor = MonitorFromRect(&frame, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{sizeof(info)};

            if (monitor == nullptr || !GetMonitorInfoW(monitor, &info))
                return;

            const RECT& work = info.rcWork;
            const int width = std::min<int>(frame.right - frame.left, work.right - work.left);
            const int height = std::min<int>(frame.bottom - frame.top, work.bottom - work.top);
            const int left = std::clamp<int>(frame.left, work.left, work.right - width);
            const int top = std::clamp<int>(frame.top, work.top, work.bottom - height);
            frame = {left, top, left + width, top + height};
        }

        void rebuild_font(UINT dpi) noexcept
        {
            const HFONT fresh = CreateFontW(-MulDiv(14, static_cast<int>(dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            if (fresh == nullptr)
                return;

            const HFONT old = g_font;
            g_font = fresh;

            if (old != nullptr)
                DeleteObject(old);
        }

        [[nodiscard]] COLORREF colour_of(std::uint32_t rgb) noexcept
        {
            return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }

        void fill(HDC dc, const RECT& rect, std::uint32_t rgb) noexcept
        {
            const HBRUSH brush = CreateSolidBrush(colour_of(rgb));
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }

        // Fills a rectangle with a colour that goes from top to bottom, one row at a time.
        void vgradient(HDC dc, const RECT& rect, std::uint32_t top, std::uint32_t bottom) noexcept
        {
            const int height = rect.bottom - rect.top;

            for (int y = 0; y < height; ++y)
            {
                const RECT row{rect.left, rect.top + y, rect.right, rect.top + y + 1};
                fill(dc, row, mix_colour(top, bottom, height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f));
            }
        }

        // The layers of the look. A panel is a card in a darker well, under a strip that is lit from above and casts a
        // shadow onto it.
        [[nodiscard]] std::uint32_t well_colour() noexcept
        {
            return mix_colour(theme().panel, 0x000000, g_theme_mode == theme_mode::dark ? 0.5f : 0.22f);
        }

        [[nodiscard]] std::uint32_t lighter(std::uint32_t colour, float amount) noexcept
        {
            return mix_colour(colour, 0xFFFFFF, amount);
        }

        [[nodiscard]] std::uint32_t shaded(std::uint32_t colour, float amount) noexcept
        {
            return mix_colour(colour, 0x000000, amount);
        }

        // Where the content of a panel sits under its tab strip: a small gap for the shadow, and a frame at the sides
        // and bottom.
        [[nodiscard]] int body_gap() noexcept
        {
            return 0;
        }

        [[nodiscard]] int body_frame() noexcept
        {
            return std::max(px(1), 1);
        }

        // Panel titles are UTF-8. Anything that does not convert shows as empty.
        int widen(const char* text, wchar_t* out, int capacity) noexcept
        {
            const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, out, capacity);
            return length > 0 ? length - 1 : 0;
        }

        // Where each tab of a group sits in the group window, and where its close button is.
        struct tab_geometry
        {
            RECT tab[dock_model::k_max_panels]{};
            RECT close[dock_model::k_max_panels]{};
            panel_id panel[dock_model::k_max_panels]{};
            std::uint32_t count{0};
            // The tabs are scrolled and clipped to avail when they do not all fit, and list is the button for the
            // menu of every tab.
            // Tabs that are hidden while they are dragged out of the strip, and the gaps drawn as a tab in the making.
            bool hidden[dock_model::k_max_panels]{};
            RECT ghost[dock_model::k_max_panels + 1]{};
            std::uint32_t ghosts{0};
            int avail{0};
            bool overflow{false};
            RECT list{};
            // A floating window's strip also holds its window buttons: roll up, maximize, close.
            bool controls{false};
            RECT ctl[3]{};
        };

        [[nodiscard]] tab_geometry measure(HDC dc, const dock_model& model, node_id group, const tab_strip_anim* anim) noexcept
        {
            tab_geometry out;
            const HGDIOBJ previous = SelectObject(dc, g_font);
            int x = 0;
            std::uint32_t visible = 0;
            const std::uint32_t count = std::min(model.tab_count(group), dock_model::k_max_panels);
            const int height = dock_model::tab_height();

            // A gap before the visible tab with this number, as the room it takes.
            const auto open_gap = [&](std::uint32_t before) {
                if (anim == nullptr || before > dock_model::k_max_panels || anim->gap[before] < 1.0f)
                    return;

                const int room = static_cast<int>(std::lround(anim->gap[before]));
                out.ghost[out.ghosts++] = {x, 0, x + room, height};
                x += room;
            };

            for (std::uint32_t i = 0; i < count; ++i)
            {
                const panel_id id = model.tab_at(group, i);
                wchar_t text[64]{};
                const int length = widen(model.title_of(id), text, 64);
                SIZE size{};
                GetTextExtentPoint32W(dc, text, length, &size);

                const bool closable = !model.is_permanent(id);
                const int width = tab_style::natural_width(size.cx, false, closable);
                out.panel[i] = id;
                out.hidden[i] = anim != nullptr && anim->skip == id;

                if (out.hidden[i])
                {
                    const int left = static_cast<int>(std::lround(anim->skip_w));
                    out.tab[i] = {x, 0, x + left, height};
                    out.close[i] = RECT{};
                    x += left;
                    continue;
                }

                open_gap(visible);
                out.tab[i] = {x, 0, x + width, height};
                const dock_rect cross = tab_style::close_rect({x, tab_style::top(), width, height - tab_style::top()});
                out.close[i] = closable ? RECT{cross.x, cross.y, cross.x + cross.width, cross.y + cross.height} : RECT{};
                x += width;
                ++visible;
            }

            open_gap(visible);
            out.count = count;
            SelectObject(dc, previous);
            return out;
        }

        [[nodiscard]] bool inside(const RECT& rect, int x, int y) noexcept
        {
            return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
        }

        [[nodiscard]] std::uint32_t tab_at_point(const tab_geometry& geometry, int x, int y) noexcept
        {
            if (x >= geometry.avail)
                return k_tab_end;

            for (std::uint32_t i = 0; i < geometry.count; ++i)
            {
                if (!geometry.hidden[i] && inside(geometry.tab[i], x, y))
                    return i;
            }

            return k_tab_end;
        }

        // The window's context pointer travels in the create parameter and then lives in the user data.
        template <class context_type>
        [[nodiscard]] context_type* context_of(HWND window, UINT message, LPARAM lparam) noexcept
        {
            if (message == WM_NCCREATE)
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            }

            return reinterpret_cast<context_type*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        }
    }

    // The window procedures live here so they can reach the host's private state.
    struct dock_host_access
    {
        static void apply_group_rect(dock_host& host, node_id group, const dock_rect& rect) noexcept
        {
            host.m_group_cur[group] = rect;
            SetWindowPos(static_cast<HWND>(host.m_group[group]), nullptr, rect.x, rect.y, rect.width, rect.height,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        static void begin_tween(dock_host& host, node_id group, const dock_rect& from, const dock_rect& to) noexcept
        {
            dock_host::tween& slide = host.m_tween[group];
            slide.active = true;
            slide.from = from;
            slide.to = to;
            slide.start = GetTickCount64();
            apply_group_rect(host, group, from);
            host.start_timer();
        }

        static void place_group(void* user, node_id group, const dock_rect& rect) noexcept
        {
            auto& host = *static_cast<dock_host*>(user);

            if (host.m_group[group] == nullptr)
                return;

            dock_host::tween& slide = host.m_tween[group];

            // A group that was just made grows out of its own centre.
            if (!host.m_group_valid[group])
            {
                host.m_group_valid[group] = true;

                if (host.m_animate)
                    begin_tween(host, group, {rect.x + rect.width / 2, rect.y + rect.height / 2, 0, 0}, rect);
                else
                    apply_group_rect(host, group, rect);

                return;
            }

            // Resizes and splitter drags follow the mouse, so they never animate.
            if (!host.m_animate)
            {
                slide.active = false;
                apply_group_rect(host, group, rect);
                return;
            }

            const dock_rect goal = slide.active ? slide.to : host.m_group_cur[group];

            if (!(rect == goal))
                begin_tween(host, group, host.m_group_cur[group], rect);
        }

        // The tabs of a group at their place in its window: scrolled when there are too many, and with the active one
        // brought into view when it changed.
        static tab_geometry geometry_for(dock_host& host, node_id group, bool animated = true) noexcept
        {
            const HWND window = static_cast<HWND>(host.m_group[group]);
            RECT client{};
            GetClientRect(window, &client);
            const HDC dc = GetDC(window);
            tab_geometry geometry = measure(dc, host.m_model, group, animated && host.m_strip[group].live ? &host.m_strip[group] : nullptr);
            ReleaseDC(window, dc);

            // The group in the top right corner of a floating window carries the window's buttons.
            const root_id owner = host.m_model.root_of(group);
            const dock_rect place = host.m_model.rect_of(group);
            geometry.controls = owner != dock_model::k_main_root && owner != k_no_root && place.y == 0 &&
                place.x + place.width >= host.m_model.root_area(owner).width - 1;
            const int width = px(36);
            const int reserve = geometry.controls ? 3 * width : 0;

            for (int i = 0; i < 3 && geometry.controls; ++i)
                geometry.ctl[i] = {client.right - (3 - i) * width, 0, client.right - (2 - i) * width, dock_model::tab_height()};

            int total = 0;

            for (std::uint32_t i = 0; i < geometry.count; ++i)
                total = std::max<int>(total, geometry.tab[i].right);

            for (std::uint32_t i = 0; i < geometry.ghosts; ++i)
                total = std::max<int>(total, geometry.ghost[i].right);

            const int button = px(24);
            const int room = std::max<int>(client.right - reserve, 0);
            geometry.overflow = total > room;
            geometry.avail = geometry.overflow ? std::max<int>(room - button, 0) : room;
            geometry.list = {room - button, 0, room, dock_model::tab_height()};

            std::int32_t& scroll = host.m_tab_scroll[group];
            const panel_id active = host.m_model.active_tab(group);

            if (geometry.overflow && active != host.m_tab_follow[group])
            {
                for (std::uint32_t i = 0; i < geometry.count; ++i)
                {
                    if (geometry.panel[i] != active)
                        continue;

                    if (geometry.tab[i].left < scroll)
                        scroll = geometry.tab[i].left;

                    if (geometry.tab[i].right > scroll + geometry.avail)
                        scroll = geometry.tab[i].right - geometry.avail;
                }
            }

            host.m_tab_follow[group] = active;
            scroll = geometry.overflow ? std::clamp<std::int32_t>(scroll, 0, std::max(total - geometry.avail, 0)) : 0;

            for (std::uint32_t i = 0; i < geometry.ghosts; ++i)
                OffsetRect(&geometry.ghost[i], -scroll, 0);

            for (std::uint32_t i = 0; i < geometry.count; ++i)
            {
                OffsetRect(&geometry.tab[i], -scroll, 0);

                if (geometry.close[i].right > geometry.close[i].left)
                    OffsetRect(&geometry.close[i], -scroll, 0);
            }

            return geometry;
        }

        [[nodiscard]] static bool route_key(dock_host& host, panel_id id, UINT key) noexcept
        {
            const bool ctrl = GetKeyState(VK_CONTROL) < 0;
            const bool shift = GetKeyState(VK_SHIFT) < 0;
            const bool alt = GetKeyState(VK_MENU) < 0;

            if (host.bar_key(key, ctrl, shift, alt))
                return true;

            if (id != k_no_panel && host.m_content[id] != nullptr && host.m_content[id]->on_key(key, ctrl, shift, alt))
                return true;

            return host.m_key_handler != nullptr && host.m_key_handler(host.m_key_user, key, ctrl, shift, alt);
        }

        // The window of one panel. Its messages go to the panel's content.
        static LRESULT panel_message(dock_host& host, panel_id id, HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
            panel_content* content = host.m_content[id];
            const bool viewport = host.m_kind[id] == panel_kind::viewport;

            switch (message)
            {
            case WM_ERASEBKGND:
                return 1;

            case WM_SETCURSOR:
                // While something is dragged the pointer says whether it can be dropped where it is.
                if (host.m_dnd.active)
                {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(host.m_dnd.effect == drop_effect::none ? 32648 : 32512)));
                    return TRUE;
                }

                if (LOWORD(lparam) == HTCLIENT && host.m_cursor[id] != mouse_cursor::arrow)
                {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(host.m_cursor[id] == mouse_cursor::resize_vertical ? 32645 : 32644)));
                    return TRUE;
                }

                break;

            case WM_PAINT:
            {
                // A viewport is painted by whatever draws into it.
                if (viewport)
                    break;

                PAINTSTRUCT paint;
                const HDC dc = BeginPaint(window, &paint);
                RECT client{};
                GetClientRect(window, &client);
                const int width = std::max<int>(client.right, 1);
                const int height = std::max<int>(client.bottom, 1);
                const HDC buffer = CreateCompatibleDC(dc);
                const HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
                const HGDIOBJ old_bitmap = SelectObject(buffer, bitmap);
                fill(buffer, client, theme().panel);

                if (content != nullptr)
                {
                    const HGDIOBJ old_font = SelectObject(buffer, g_font);
                    SetBkMode(buffer, TRANSPARENT);
                    SetTextColor(buffer, colour_of(theme().text));
                    const draw::frame shapes(buffer);
                    content->on_paint(buffer, client.right, client.bottom);
                    SelectObject(buffer, old_font);
                }

                BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, old_bitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
                EndPaint(window, &paint);
                return 0;
            }

            case WM_SIZE:
                if (content != nullptr)
                    content->on_size(LOWORD(lparam), HIWORD(lparam));

                if (!viewport)
                    InvalidateRect(window, nullptr, FALSE);

                return 0;

            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            {
                // The pointer belongs to the drag until the button comes up.
                if (host.m_dnd.active && GetCapture() == window)
                {
                    if (message != WM_MOUSEWHEEL)
                    {
                        POINT at{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                        ClientToScreen(window, &at);

                        if (message == WM_MOUSEMOVE)
                            host.drag_update(at.x, at.y);
                        else if (message == WM_LBUTTONUP)
                            host.drag_finish(true);
                        else if (message == WM_RBUTTONDOWN)
                            host.drag_finish(false);
                    }

                    return 0;
                }

                panel_mouse mouse;
                mouse.ctrl = GetKeyState(VK_CONTROL) < 0;
                mouse.shift = GetKeyState(VK_SHIFT) < 0;
                mouse.x = GET_X_LPARAM(lparam);
                mouse.y = GET_Y_LPARAM(lparam);

                switch (message)
                {
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDBLCLK:
                    mouse.event = message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDBLCLK
                        ? mouse_event::double_click
                        : mouse_event::down;
                    mouse.button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) ? 0
                        : (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) ? 1 : 2;
                    // A strip is only clicked. It leaves the keyboard where it is.
                    if (!dock_host::is_strip(id))
                        SetFocus(window);

                    if (GetCapture() != window)
                        SetCapture(window);

                    break;

                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONUP:
                    mouse.event = mouse_event::up;
                    mouse.button = message == WM_LBUTTONUP ? 0 : message == WM_RBUTTONUP ? 1 : 2;

                    if ((wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0 && GetCapture() == window)
                        ReleaseCapture();

                    break;

                case WM_MOUSEWHEEL:
                {
                    // The wheel reports the cursor in screen coordinates.
                    POINT at{mouse.x, mouse.y};
                    ScreenToClient(window, &at);
                    mouse.x = at.x;
                    mouse.y = at.y;
                    mouse.event = mouse_event::wheel;
                    mouse.wheel = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
                    break;
                }

                default:
                {
                    mouse.event = mouse_event::move;
                    // Ask to be told when the mouse leaves, so a panel can drop its hover.
                    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                    TrackMouseEvent(&track);
                    break;
                }
                }

                if (mouse.event == mouse_event::down || mouse.event == mouse_event::double_click || mouse.event == mouse_event::wheel)
                    host.hide_tooltip(false);

                if (content != nullptr)
                    content->on_mouse(mouse);

                return 0;
            }

            case WM_MOUSELEAVE:
            {
                host.hide_tooltip(true);

                if (content != nullptr)
                {
                    panel_mouse mouse;
                    mouse.event = mouse_event::leave;
                    content->on_mouse(mouse);
                }

                return 0;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
                // During a drag the keys only cancel it or change what it would do: Ctrl copies, Shift moves, Alt links.
                if (host.m_dnd.active)
                {
                    if (wparam == VK_ESCAPE && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
                        host.drag_finish(false);
                    else
                        host.drag_update(host.m_dnd.x, host.m_dnd.y);

                    return 0;
                }

                if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && route_key(host, id, static_cast<UINT>(wparam)))
                    return 0;

                break;

            case WM_CAPTURECHANGED:
                // The mouse was taken away from the drag, such as by another window, so it ends with nothing dropped.
                if (host.m_dnd.active && !host.m_dnd.external && reinterpret_cast<HWND>(lparam) != window)
                    host.drag_finish(false);

                break;

            case WM_CHAR:
                if (content != nullptr)
                    content->on_char(static_cast<std::uint32_t>(wparam));

                return 0;

            case WM_SETFOCUS:
                if (!dock_host::is_strip(id))
                {
                    host.m_model.set_focused(id);
                    host.invalidate_strips();
                }

                if (content != nullptr)
                    content->on_focus(true);

                return 0;

            case WM_KILLFOCUS:
                if (content != nullptr)
                    content->on_focus(false);

                return 0;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        // Opens the gap in the strip a drop would land in, at the place under screen_x, and closes every other gap.
        static void gap_for_target(dock_host& host, const drop_target& target, panel_id dragged, std::int32_t width, const char* title,
            std::int32_t screen_x) noexcept
        {
            if (target.zone != drop_zone::tabs || target.group == k_no_node || !host.m_model.is_group(target.group))
            {
                host.set_tab_drop(k_no_node, 0, 0, nullptr);
                return;
            }

            std::uint32_t index = host.tab_index_at(target.group, screen_x);

            // A tab moved inside its own strip leaves its place first, so the places after it count one less.
            if (dragged != k_no_panel && host.m_model.group_of(dragged) == target.group)
            {
                std::uint32_t current = 0;

                while (current < host.m_model.tab_count(target.group) && host.m_model.tab_at(target.group, current) != dragged)
                    ++current;

                if (index != k_tab_end && index > current)
                    --index;
            }

            host.set_tab_drop(target.group, static_cast<std::int32_t>(index), width, title);
        }

        static void reset_press(dock_host& host) noexcept
        {
            host.end_tab_drag();
            host.m_press_panel = k_no_panel;
            host.m_press_group = k_no_node;
            host.m_press_close = false;
            host.m_dragging = false;
        }

        // The sizing edge of a floating window under a screen point, as a hit test code, or 0 when it is not on one.
        static LRESULT edge_hit(dock_host& host, root_id root, LPARAM lparam) noexcept
        {
            if (root == dock_model::k_main_root || root >= dock_model::k_max_roots || host.m_root_top[root] == nullptr)
                return 0;

            RECT frame{};
            GetWindowRect(static_cast<HWND>(host.m_root_top[root]), &frame);
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int grip = px(6);
            const bool left = x < frame.left + grip;
            const bool right = x >= frame.right - grip;
            const bool top = y < frame.top + grip;
            const bool bottom = y >= frame.bottom - grip;

            if (top && left)
                return HTTOPLEFT;

            if (top && right)
                return HTTOPRIGHT;

            if (bottom && left)
                return HTBOTTOMLEFT;

            if (bottom && right)
                return HTBOTTOMRIGHT;

            if (left)
                return HTLEFT;

            if (right)
                return HTRIGHT;

            if (top)
                return HTTOP;

            return bottom ? HTBOTTOM : 0;
        }

        static LRESULT root_message(dock_host& host, root_id root, HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
            switch (message)
            {
            case WM_TIMER:
                if (wparam == 1)
                    host.tick();
                else if (wparam == k_tooltip_timer)
                    host.show_tooltip();

                return 0;

            case WM_ERASEBKGND:
            {
                RECT client{};
                GetClientRect(window, &client);
                fill(reinterpret_cast<HDC>(wparam), client, theme().splitter);
                return 1;
            }

            case WM_SIZE:
                host.layout_root(root);
                return 0;

            case WM_NCHITTEST:
                if (edge_hit(host, root, lparam) != 0)
                    return HTTRANSPARENT;

                break;

            case WM_LBUTTONDOWN:
            {
                const node_id hit = host.m_model.hit_splitter(root, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

                if (hit != k_no_node)
                {
                    host.m_split_drag = hit;
                    SetCapture(window);
                }

                return 0;
            }

            case WM_MOUSEMOVE:
                if (host.m_split_drag != k_no_node)
                {
                    host.m_model.drag_splitter(host.m_split_drag, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                    host.layout_root(root);
                    // Paint now. A paint message waits behind the stream of mouse moves, so a fast drag would leave the
                    // newly uncovered strips unpainted until it slows down.
                    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                }

                return 0;

            case WM_LBUTTONUP:
                if (GetCapture() == window)
                    ReleaseCapture();

                host.m_split_drag = k_no_node;
                return 0;

            case WM_CAPTURECHANGED:
                host.m_split_drag = k_no_node;
                return 0;

            case WM_SETCURSOR:
            {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window, &point);
                const node_id hit = host.m_split_drag != k_no_node ? host.m_split_drag : host.m_model.hit_splitter(root, point.x, point.y);

                if (hit != k_no_node && LOWORD(lparam) == HTCLIENT)
                {
                    const bool across = host.m_model.axis_of(hit) == dock_axis::horizontal;
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(across ? 32644 : 32645)));
                    return TRUE;
                }

                break;
            }

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        // The window a panel floats as when it is let go at a screen point: the size of the group it came from.
        static dock_rect float_area(dock_host& host, node_id source, const POINT& screen) noexcept
        {
            RECT size{0, 0, px(k_float_min_width), px(k_float_min_height)};
            const HWND source_window = static_cast<HWND>(host.m_group[source]);

            if (source_window != nullptr)
                GetClientRect(source_window, &size);

            return {screen.x - 40, screen.y - 10, std::max<int>(size.right, px(k_float_min_width)),
                std::max<int>(size.bottom, px(k_float_min_height))};
        }

        // A tab was dropped. Docks it, floats it, or does nothing.
        static void finish_drag(dock_host& host, panel_id panel, node_id source, const POINT& screen) noexcept
        {
            host.hide_preview();
            host.refresh_all_areas();
            const drop_target target = host.m_model.find_drop(screen.x, screen.y, panel);

            if (target.zone != drop_zone::none)
            {
                const std::uint32_t index = target.zone == drop_zone::tabs ? host.tab_index_at(target.group, screen.x) : k_tab_end;
                (void)host.m_model.apply_drop(panel, target, index);
            }
            else if (!target.keep)
            {
                const dock_rect area = float_area(host, source, screen);
                const root_id floated = host.m_model.float_panel(panel, area);

                if (floated != k_no_root)
                    host.move_root_window(floated, area);
            }

            host.sync();
        }

        static LRESULT group_message(dock_host& host, node_id group, HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
            switch (message)
            {
            case WM_ERASEBKGND:
                return 1;

            case WM_NCHITTEST:
            {
                const root_id root = host.m_model.root_of(group);

                if (root == k_no_root)
                    break;

                // Near a splitter bar the panel lets the mouse through to the host, which grabs the bar.
                if (root < dock_model::k_max_roots && host.m_root_host[root] != nullptr)
                {
                    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                    ScreenToClient(static_cast<HWND>(host.m_root_host[root]), &point);

                    if (host.m_model.hit_splitter(root, point.x, point.y) != k_no_node)
                        return HTTRANSPARENT;
                }

                if (root != dock_model::k_main_root && edge_hit(host, root, lparam) != 0)
                    return HTTRANSPARENT;

                break;
            }

            case WM_SIZE:
                host.m_resize_stamp = GetTickCount64();
                host.fit_group(group);
                // The tabs that fit depend on the width.
                InvalidateRect(window, nullptr, FALSE);
                return 0;

            case WM_PAINT:
            {
                PAINTSTRUCT paint;
                const HDC screen_dc = BeginPaint(window, &paint);
                RECT client{};
                GetClientRect(window, &client);

                // Everything is drawn off screen and copied in one go, so a repaint never shows a half drawn strip.
                const HDC dc = CreateCompatibleDC(screen_dc);
                const HBITMAP back = CreateCompatibleBitmap(screen_dc, std::max<LONG>(client.right, 1), std::max<LONG>(client.bottom, 1));
                const HGDIOBJ old_back = SelectObject(dc, back);
                fill(dc, client, well_colour());

                RECT strip = client;
                strip.bottom = std::min<LONG>(client.bottom, host.m_model.header_height(group));
                tab_style::paint_strip(dc, {0, 0, client.right, strip.bottom});

                const tab_geometry geometry = geometry_for(host, group);
                const panel_id active = host.m_model.active_tab(group);
                // The tab that has the keyboard shows it. The other groups keep a dimmer line.
                const bool lit = host.m_model.group_of(host.m_model.focused()) == group;
                const std::uint32_t line_colour = lit ? theme().accent : mix_colour(theme().accent, theme().title, 0.6f);
                const HGDIOBJ previous = SelectObject(dc, g_font);
                SetBkMode(dc, TRANSPARENT);
                IntersectClipRect(dc, 0, 0, geometry.avail, strip.bottom);

                for (std::uint32_t i = 0; i < geometry.count; ++i)
                {
                    if (geometry.hidden[i])
                        continue;

                    const bool current = geometry.panel[i] == active;
                    const bool hot = host.m_tab_hot_group == group && host.m_tab_hot == geometry.panel[i];
                    const RECT& box = geometry.tab[i];
                    const bool closable = geometry.close[i].right > geometry.close[i].left;
                    const char* title = host.m_model.title_of(geometry.panel[i]);
                    tab_style::tab_look look;
                    look.title = title != nullptr ? title : "";
                    look.chosen = current;
                    look.hot = hot;
                    look.slot = closable;
                    look.close_shown = closable && (current || hot);
                    look.close_hot = hot && host.m_tab_hot_close;
                    look.line = line_colour;
                    tab_style::paint_tab(dc, {box.left, tab_style::top(), box.right - box.left, strip.bottom - tab_style::top()}, look);
                }

                // The gaps where a drop would land, drawn as the tab that would arrive.
                for (std::uint32_t i = 0; i < geometry.ghosts; ++i)
                {
                    const RECT& cell = geometry.ghost[i];
                    tab_style::tab_look look;
                    look.title = host.m_strip[group].title != nullptr ? host.m_strip[group].title : "";
                    look.ghost = true;
                    tab_style::paint_tab(dc, {cell.left, tab_style::top(), cell.right - cell.left, strip.bottom - tab_style::top()}, look);
                }

                SelectClipRgn(dc, nullptr);

                // Too many tabs: a button at the end of the strip opens the list of all of them.
                if (geometry.overflow)
                {
                    fill(dc, geometry.list, tab_style::bar_colour());
                    const int cx = (geometry.list.left + geometry.list.right) / 2;
                    const int cy = (geometry.list.top + geometry.list.bottom) / 2;
                    const int half = px(4);
                    const float fx = static_cast<float>(cx);
                    const float fy = static_cast<float>(cy);
                    const float fh = static_cast<float>(half);
                    const draw::point triangle[3] = {{fx - fh, fy - fh / 2.0f}, {fx + fh, fy - fh / 2.0f}, {fx, fy + fh / 2.0f + 1.0f}};
                    draw::polygon(dc, triangle, 3, theme().text);
                }

                if (geometry.controls)
                {
                    const HPEN pen = CreatePen(PS_SOLID, std::max(1, px(1)), colour_of(theme().text));
                    const HGDIOBJ old_pen = SelectObject(dc, pen);
                    const HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    const int half = px(5);

                    for (int i = 0; i < 3; ++i)
                    {
                        const RECT& button = geometry.ctl[i];

                        if (host.m_ctl_group == group && host.m_ctl_hot == i)
                            fill(dc, button, i == 2 ? 0xC42B1C : theme().hover);

                        const int cx = (button.left + button.right) / 2;
                        const int cy = (button.top + button.bottom) / 2;

                        if (i == 0)
                        {
                            MoveToEx(dc, cx - half, cy, nullptr);
                            LineTo(dc, cx + half + 1, cy);
                        }
                        else if (i == 1)
                        {
                            Rectangle(dc, cx - half, cy - half, cx + half + 1, cy + half + 1);
                        }
                        else
                        {
                            // The diagonals are smoothed. The other two glyphs are straight lines on whole pixels.
                            const float fx = static_cast<float>(cx) + 0.5f;
                            const float fy = static_cast<float>(cy) + 0.5f;
                            const float fh = static_cast<float>(half) + 0.5f;
                            const float stroke = std::max(1.0f, 1.25f * static_cast<float>(px(100)) / 100.0f);
                            const draw::point down[2] = {{fx - fh, fy - fh}, {fx + fh, fy + fh}};
                            const draw::point up[2] = {{fx + fh, fy - fh}, {fx - fh, fy + fh}};
                            draw::polyline(dc, down, 2, theme().text, stroke);
                            draw::polyline(dc, up, 2, theme().text, stroke);
                        }
                    }

                    SelectObject(dc, old_brush);
                    SelectObject(dc, old_pen);
                    DeleteObject(pen);
                }

                SelectObject(dc, previous);
                BitBlt(screen_dc, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top, dc,
                    paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
                SelectObject(dc, old_back);
                DeleteObject(back);
                DeleteDC(dc);
                EndPaint(window, &paint);
                return 0;
            }

            case WM_LBUTTONDOWN:
            {
                const int x = GET_X_LPARAM(lparam);
                const int y = GET_Y_LPARAM(lparam);
                const tab_geometry geometry = geometry_for(host, group);

                for (int i = 0; i < 3 && geometry.controls; ++i)
                {
                    if (inside(geometry.ctl[i], x, y))
                    {
                        host.window_control(host.m_model.root_of(group), i);
                        return 0;
                    }
                }

                if (geometry.overflow && inside(geometry.list, x, y))
                {
                    host.open_tab_list(group);
                    return 0;
                }

                const std::uint32_t index = tab_at_point(geometry, x, y);

                if (index == k_tab_end)
                {
                    // The empty strip of a floating window's top row is its title bar.
                    const root_id owner = host.m_model.root_of(group);
                    const HWND top = owner < dock_model::k_max_roots ? static_cast<HWND>(host.m_root_top[owner]) : nullptr;

                    if (owner != dock_model::k_main_root && top != nullptr && host.m_model.rect_of(group).y == 0 &&
                        y < dock_model::tab_height() && !IsZoomed(top))
                    {
                        POINT cursor{};
                        GetCursorPos(&cursor);
                        RECT frame{};
                        GetWindowRect(top, &frame);
                        host.m_wdrag_root = owner;
                        host.m_wdrag_cx = cursor.x;
                        host.m_wdrag_cy = cursor.y;
                        host.m_wdrag_wx = frame.left;
                        host.m_wdrag_wy = frame.top;
                        SetCapture(window);
                    }

                    return 0;
                }

                POINT screen{x, y};
                ClientToScreen(window, &screen);
                host.m_press_panel = geometry.panel[index];
                host.m_press_group = group;
                host.m_press_x = screen.x;
                host.m_press_y = screen.y;
                host.m_press_close = inside(geometry.close[index], x, y);
                host.m_dragging = false;

                if (!host.m_press_close)
                {
                    host.m_model.set_active(group, geometry.panel[index]);
                    host.notify_visibility();
                    host.fit_group(group);
                    host.focus_panel(geometry.panel[index]);
                    InvalidateRect(window, nullptr, FALSE);
                }

                SetCapture(window);
                return 0;
            }

            case WM_MOUSELEAVE:
                if (host.m_tab_hot_group == group)
                {
                    host.m_tab_hot_group = k_no_node;
                    host.m_tab_hot = k_no_panel;
                    host.m_tab_hot_close = false;
                    InvalidateRect(window, nullptr, FALSE);
                }

                if (host.m_ctl_group == group)
                {
                    host.m_ctl_group = k_no_node;
                    host.m_ctl_hot = -1;
                    InvalidateRect(window, nullptr, FALSE);
                }

                return 0;

            case WM_MOUSEMOVE:
            {
                // Window buttons light up under the mouse.
                const tab_geometry hover = geometry_for(host, group);

                {
                    // The tab under the mouse lights up, and shows its cross.
                    TRACKMOUSEEVENT track_tab{sizeof(track_tab), TME_LEAVE, window, 0};
                    TrackMouseEvent(&track_tab);
                    const int mx = GET_X_LPARAM(lparam);
                    const int my = GET_Y_LPARAM(lparam);
                    const std::uint32_t over = tab_at_point(hover, mx, my);
                    const panel_id now = over != k_tab_end ? hover.panel[over] : k_no_panel;
                    const bool now_close = over != k_tab_end && inside(hover.close[over], mx, my);

                    if (now != host.m_tab_hot || now_close != host.m_tab_hot_close || group != host.m_tab_hot_group)
                    {
                        const node_id before = host.m_tab_hot_group;

                        if (before != k_no_node && before != group && host.m_group[before] != nullptr)
                            InvalidateRect(static_cast<HWND>(host.m_group[before]), nullptr, FALSE);

                        host.m_tab_hot_group = now != k_no_panel ? group : k_no_node;
                        host.m_tab_hot = now;
                        host.m_tab_hot_close = now_close;
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }

                if (hover.controls)
                {
                    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                    TrackMouseEvent(&track);
                    int hot = -1;

                    for (int i = 0; i < 3; ++i)
                    {
                        if (inside(hover.ctl[i], GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
                            hot = i;
                    }

                    const node_id owner = hot >= 0 ? group : k_no_node;

                    if (hot != host.m_ctl_hot || owner != host.m_ctl_group)
                    {
                        host.m_ctl_hot = hot;
                        host.m_ctl_group = owner;
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }

                if (host.m_wdrag_root != k_no_root && GetCapture() == window)
                {
                    POINT cursor{};
                    GetCursorPos(&cursor);
                    SetWindowPos(static_cast<HWND>(host.m_root_top[host.m_wdrag_root]), nullptr, host.m_wdrag_wx + cursor.x - host.m_wdrag_cx,
                        host.m_wdrag_wy + cursor.y - host.m_wdrag_cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    host.refresh_all_areas();
                    const drop_target target = host.m_model.find_drop(cursor.x, cursor.y, k_no_panel, host.m_wdrag_root);

                    const char* first_title = nullptr;
                    const std::int32_t room = host.root_tabs_width(host.m_wdrag_root, &first_title);
                    gap_for_target(host, target, k_no_panel, room, first_title, cursor.x);

                    // Only the preview goes when no tile is under the cursor. The tiles stay up, so they are not hidden and
                    // shown again on every move.
                    if (target.zone != drop_zone::none)
                        host.show_preview(target.preview, nullptr, target.zone == drop_zone::tabs ? 2 : 0);
                    else if (host.m_preview != nullptr)
                        ShowWindow(static_cast<HWND>(host.m_preview), SW_HIDE);

                    host.show_hint(target);
                    return 0;
                }

                if (host.m_press_panel == k_no_panel || host.m_press_close || GetCapture() != window)
                {
                    if (host.m_press_panel == k_no_panel)
                        host.hide_preview();

                    return 0;
                }

                POINT screen{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(window, &screen);

                if (!host.m_dragging)
                {
                    const int dx = screen.x - host.m_press_x;
                    const int dy = screen.y - host.m_press_y;
                    host.m_dragging = dx * dx + dy * dy > px(k_drag_distance) * px(k_drag_distance);

                    if (host.m_dragging)
                        host.begin_tab_drag(host.m_press_panel, host.m_press_group);
                }

                if (host.m_dragging)
                {
                    host.refresh_all_areas();
                    const drop_target target = host.m_model.find_drop(screen.x, screen.y, host.m_press_panel);

                    const char* title = host.m_model.title_of(host.m_press_panel);
                    gap_for_target(host, target, host.m_press_panel, host.tab_width_of(host.m_press_panel), title, screen.x);

                    if (target.zone != drop_zone::none)
                        host.show_preview(target.preview, title, target.zone == drop_zone::tabs ? 2 : 0);
                    else if (!target.keep)
                        host.show_preview(float_area(host, host.m_press_group, screen), title, 1);
                    else
                        host.hide_preview();

                    // Last, so the compass stays above the preview and the floating outline.
                    host.show_hint(target);
                }

                return 0;
            }

            case WM_LBUTTONUP:
            {
                if (host.m_wdrag_root != k_no_root)
                {
                    const root_id source = host.m_wdrag_root;
                    host.m_wdrag_root = k_no_root;

                    if (GetCapture() == window)
                        ReleaseCapture();

                    host.hide_preview();
                    POINT cursor{};
                    GetCursorPos(&cursor);
                    host.refresh_all_areas();
                    const drop_target target = host.m_model.find_drop(cursor.x, cursor.y, k_no_panel, source);

                    // Dropped on a docking place: all of its tabs go there. Anywhere else it stays where it is.
                    if (target.zone != drop_zone::none)
                    {
                        const std::uint32_t index = target.zone == drop_zone::tabs ? host.tab_index_at(target.group, cursor.x) : k_tab_end;

                        if (host.m_model.apply_root_drop(source, target, index))
                            host.sync();
                    }

                    return 0;
                }

                const panel_id panel = host.m_press_panel;
                const node_id source = host.m_press_group;
                const bool was_close = host.m_press_close;
                const bool was_dragging = host.m_dragging;
                reset_press(host);

                if (GetCapture() == window)
                    ReleaseCapture();

                host.hide_preview();

                if (panel == k_no_panel)
                    return 0;

                POINT screen{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(window, &screen);

                if (was_dragging)
                {
                    finish_drag(host, panel, source, screen);
                    return 0;
                }

                if (was_close)
                {
                    const tab_geometry geometry = geometry_for(host, group);

                    for (std::uint32_t i = 0; i < geometry.count; ++i)
                    {
                        if (geometry.panel[i] == panel && inside(geometry.close[i], GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
                        {
                            (void)host.m_model.close_panel(panel);
                            host.sync();
                            return 0;
                        }
                    }
                }

                return 0;
            }

            case WM_MBUTTONUP:
            {
                const tab_geometry geometry = geometry_for(host, group);
                const std::uint32_t index = tab_at_point(geometry, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

                if (index != k_tab_end && host.m_model.close_panel(geometry.panel[index]))
                    host.sync();

                return 0;
            }

            case WM_LBUTTONDBLCLK:
            {
                const tab_geometry geometry = geometry_for(host, group);
                const std::uint32_t index = tab_at_point(geometry, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                reset_press(host);

                if (GetCapture() == window)
                    ReleaseCapture();

                if (index == k_tab_end)
                    return 0;

                RECT client{};
                GetClientRect(window, &client);
                POINT origin{0, 0};
                ClientToScreen(window, &origin);
                const int width = std::max<int>(client.right, px(k_float_min_width));
                const int height = std::max<int>(client.bottom, px(k_float_min_height));
                const dock_rect area{origin.x + px(24), origin.y + px(24), width, height};
                const root_id floated = host.m_model.float_panel(geometry.panel[index], area);

                if (floated != k_no_root)
                    host.move_root_window(floated, area);

                host.sync();
                return 0;
            }

            case WM_MOUSEWHEEL:
            {
                POINT at{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(window, &at);

                // Over the strip the wheel scrolls the tabs. geometry_for keeps the offset in range.
                if (at.y < host.m_model.header_height(group))
                {
                    host.m_tab_scroll[group] -= GET_WHEEL_DELTA_WPARAM(wparam) * px(60) / WHEEL_DELTA;
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }

                break;
            }

            case WM_CAPTURECHANGED:
                reset_press(host);
                host.m_wdrag_root = k_no_root;
                host.hide_preview();
                return 0;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT float_message(dock_host& host, root_id root, HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
            switch (message)
            {
            // The whole window is client area. What shows around the root is its frame line.
            case WM_NCCALCSIZE:
                if (wparam != 0)
                    return 0;

                break;

            // The system would draw its own frame over the edge.
            case WM_NCPAINT:
                return 0;

            case WM_NCACTIVATE:
                return DefWindowProcW(window, message, wparam, -1);

            case WM_GETMINMAXINFO:
            {
                auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
                MONITORINFO monitor{sizeof(monitor)};

                // Maximized, it fills the work area and leaves the taskbar alone.
                if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
                {
                    info->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left, monitor.rcWork.top - monitor.rcMonitor.top};
                    info->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left, monitor.rcWork.bottom - monitor.rcWork.top};
                }

                info->ptMinTrackSize = {px(160), dock_model::tab_height() + 2 * k_float_frame};
                return 0;
            }

            // Only what the children let through gets here: the sizing edges. Moving the window is done by the tab
            // strip itself, so a drag can dock it.
            case WM_NCHITTEST:
            {
                const LRESULT edge = edge_hit(host, root, lparam);
                return edge != 0 ? edge : HTCLIENT;
            }

            case WM_ERASEBKGND:
            {
                RECT client{};
                GetClientRect(window, &client);
                fill(reinterpret_cast<HDC>(wparam), client, theme().splitter);
                return 1;
            }

            case WM_SIZE:
                if (host.m_root_host[root] != nullptr && wparam != SIZE_MINIMIZED)
                {
                    const int line = k_float_frame;
                    MoveWindow(static_cast<HWND>(host.m_root_host[root]), line, line, std::max<int>(LOWORD(lparam) - 2 * line, 0),
                        std::max<int>(HIWORD(lparam) - 2 * line, 0), TRUE);
                }

                return 0;

            case WM_MOVE:
                host.refresh_area(root);
                return 0;

            case WM_CLOSE:
                host.close_root(root);
                return 0;

            // The window got the keyboard itself, so hand it to a panel in it.
            case WM_SETFOCUS:
                host.focus_root(root);
                return 0;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }
        // One docking tile: a dark square with an outline in the docking colour, and a small picture of where the panel
        // would go. The tile under the cursor fills with the docking colour. root tiles show a thin strip at an edge.
        static void draw_tile(HDC dc, const dock_rect& cell, drop_zone zone, bool active, bool root) noexcept
        {
            const RECT tile{cell.x, cell.y, cell.x + cell.width, cell.y + cell.height};
            fill(dc, tile, theme().hint);
            const RECT face{tile.left + px(2), tile.top + px(2), tile.right - px(2), tile.bottom - px(2)};
            fill(dc, face, active ? theme().hint : theme().title);

            const int pad = px(8);
            RECT mark{tile.left + pad, tile.top + pad, tile.right - pad, tile.bottom - pad};
            const int share_x = (mark.right - mark.left) / (root ? 3 : 2);
            const int share_y = (mark.bottom - mark.top) / (root ? 3 : 2);
            bool tabs = false;

            switch (zone)
            {
            case drop_zone::left:
            case drop_zone::root_left:
                mark.right = mark.left + share_x;
                break;
            case drop_zone::right:
            case drop_zone::root_right:
                mark.left = mark.right - share_x;
                break;
            case drop_zone::top:
            case drop_zone::root_top:
                mark.bottom = mark.top + share_y;
                break;
            case drop_zone::bottom:
            case drop_zone::root_bottom:
                mark.top = mark.bottom - share_y;
                break;
            default:
                // Tabs: a wide body with a short tab on top.
                mark.top += px(3);
                tabs = true;
                break;
            }

            const std::uint32_t ink = active ? 0xFFFFFF : theme().text_dim;
            fill(dc, mark, ink);

            if (tabs)
            {
                const RECT tab{mark.left, mark.top - px(3), mark.left + px(8), mark.top};
                fill(dc, tab, ink);
            }
        }

        // Paints docking tiles. One window holds the compass of a group and another the edge tiles of a whole root,
        // and the gaps are see-through.
        static LRESULT CALLBACK hint_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* host = reinterpret_cast<dock_host*>(context_of<dock_host>(window, message, lparam));

            if (message == WM_ERASEBKGND)
                return 1;

            if (message != WM_PAINT || host == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            PAINTSTRUCT paint;
            const HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            const HBRUSH key = CreateSolidBrush(k_hint_key);
            FillRect(dc, &client, key);
            DeleteObject(key);

            if (window == static_cast<HWND>(host->m_root_hint))
            {
                const drop_target& target = host->m_root_hint_target;
                const dock_rect area{0, 0, client.right, client.bottom};
                const drop_zone sides[4] = {drop_zone::root_left, drop_zone::root_right, drop_zone::root_top, drop_zone::root_bottom};

                for (const drop_zone zone : sides)
                    draw_tile(dc, dock_model::root_tile(area, zone), zone, zone == target.zone, true);
            }
            else
            {
                const drop_target& target = host->m_hint_target;
                const dock_rect square{0, 0, dock_model::compass_extent(), dock_model::compass_extent()};
                const drop_zone zones[5] = {drop_zone::tabs, drop_zone::left, drop_zone::right, drop_zone::top, drop_zone::bottom};

                for (const drop_zone zone : zones)
                    draw_tile(dc, dock_model::compass_tile(square, zone), zone, zone == target.zone, false);
            }

            EndPaint(window, &paint);
            return 0;
        }

        // The tooltip: a small box with the theme's colours, and the text of the control it belongs to.
        static LRESULT CALLBACK tooltip_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* host = context_of<dock_host>(window, message, lparam);

            if (message == WM_ERASEBKGND)
                return 1;

            if (message != WM_PAINT || host == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            PAINTSTRUCT paint;
            const HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            fill(dc, client, mix_colour(theme().title, 0x000000, 0.5f));
            RECT face{client.left + 1, client.top + 1, client.right - 1, client.bottom - 1};
            fill(dc, face, mix_colour(theme().title, 0xFFFFFF, 0.06f));

            wchar_t text[256]{};
            widen(host->m_tip_text != nullptr ? host->m_tip_text : "", text, 256);
            const HGDIOBJ previous = SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, colour_of(theme().text));
            RECT label{face.left + px(8), face.top + px(5), face.right - px(8), face.bottom - px(5)};
            DrawTextW(dc, text, -1, &label, DT_WORDBREAK | DT_NOPREFIX | DT_LEFT);
            SelectObject(dc, previous);
            EndPaint(window, &paint);
            return 0;
        }

        // The picture that follows the pointer during a drag: the source's own, or a small label with its picture, a badge for
        // how many things there are, and a mark for what a drop would do here.
        static void paint_ghost(const dock_host& host, HDC dc, const dock_rect& area) noexcept
        {
            const drag_payload& payload = host.m_dnd.payload;
            const drop_effect effect = host.m_dnd.shown;

            if (payload.ghost != nullptr)
            {
                payload.ghost(payload.ghost_user, dc, area, effect);
                return;
            }

            const std::uint32_t accent = payload.colour != 0 ? payload.colour : theme().accent;
            const std::uint32_t refused = 0xE5534B;
            draw::rounded(dc, area, 4, mix_colour(theme().title, 0xFFFFFF, 0.1f), effect == drop_effect::none ? refused : accent);

            const int middle = area.y + area.height / 2;
            int x = area.x + px(10);
            const int right_end = area.x + area.width - px(8);

            if (payload.icon != icon_kind::none)
            {
                const int size = px(14);
                draw::icon(dc, payload.icon, {x, middle - size / 2, size, size}, accent);
                x += size + px(6);
            }

            // The mark for what a drop would do, at the right end.
            const int mark = px(14);
            int stop = right_end - mark - px(6);
            const dock_rect mark_box{right_end - mark, middle - mark / 2, mark, mark};

            if (effect == drop_effect::copy)
                draw::icon(dc, icon_kind::plus, mark_box, 0x6BAA5A);
            else if (effect == drop_effect::link)
                draw::icon(dc, icon_kind::redo, mark_box, 0x4F8FDB);
            else if (effect == drop_effect::none)
                draw::icon(dc, icon_kind::cross, mark_box, refused);
            else
                stop = right_end;

            if (payload.count > 1)
            {
                char number[16];
                std::snprintf(number, sizeof(number), "%u", payload.count);
                const int width = draw::text_width(dc, number) + px(10);
                const dock_rect badge{stop - width, middle - px(8), width, px(16)};
                draw::rounded(dc, badge, 8, accent, accent);
                draw::text(dc, badge, number, 0xFFFFFF, draw::text_center);
                stop = badge.x - px(6);
            }

            draw::text(dc, {x, area.y, std::max(stop - x, 0), area.height}, payload.label, theme().text, draw::text_ellipsis);
        }

        static LRESULT CALLBACK ghost_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* host = context_of<dock_host>(window, message, lparam);

            if (message == WM_ERASEBKGND)
                return 1;

            // The ghost never takes the mouse, so what is under it still does.
            if (message == WM_NCHITTEST)
                return HTTRANSPARENT;

            if (message != WM_PAINT || host == nullptr)
                return DefWindowProcW(window, message, wparam, lparam);

            PAINTSTRUCT paint;
            const HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            fill(dc, client, 0x010101);
            const HGDIOBJ previous = SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            {
                const draw::frame shapes(dc);
                paint_ghost(*host, dc, {0, 0, client.right, client.bottom});
            }
            SelectObject(dc, previous);
            EndPaint(window, &paint);
            return 0;
        }

        // GWLP_USERDATA holds 1 for the outline of a floating window and 0 for a docked panel.
        static LRESULT CALLBACK preview_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
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
                const LONG_PTR mode = GetWindowLongPtrW(window, GWLP_USERDATA);
                const bool ghost = mode == 1;
                fill(dc, client, ghost ? theme().panel : theme().hint);

                // A docked preview is just the area, with an outline. A label here would sit on top of the real tabs.
                if (mode != 1)
                {
                    const RECT edges[4] = {
                        {0, 0, client.right, 3}, {0, client.bottom - 3, client.right, client.bottom},
                        {0, 0, 3, client.bottom}, {client.right - 3, 0, client.right, client.bottom}};

                    for (const RECT& edge : edges)
                        fill(dc, edge, 0xFFFFFF);

                    EndPaint(window, &paint);
                    return 0;
                }

                RECT strip = client;
                strip.bottom = std::min<LONG>(client.bottom, dock_model::tab_height());
                fill(dc, strip, ghost ? theme().title : theme().hint);

                if (ghost)
                {
                    // An outline, like the frame of the window that is about to appear.
                    const RECT edges[4] = {
                        {0, 0, client.right, 2}, {0, client.bottom - 2, client.right, client.bottom},
                        {0, 0, 2, client.bottom}, {client.right - 2, 0, client.right, client.bottom}};

                    for (const RECT& edge : edges)
                        fill(dc, edge, theme().accent);
                }
                else
                {
                    // The tab strip the panel will get.
                    fill(dc, strip, tab_style::bar_colour());
                    RECT line{0, 0, std::min<LONG>(client.right, px(140)), 2};
                    fill(dc, line, theme().hint);
                }

                wchar_t title[64]{};
                GetWindowTextW(window, title, 64);
                const HGDIOBJ previous = SelectObject(dc, g_font);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, colour_of(theme().text));
                RECT label = strip;
                label.left += 10;
                DrawTextW(dc, title, -1, &label, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
                SelectObject(dc, previous);
                EndPaint(window, &paint);
                return 0;
            }

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        // The rectangle of each title of the bar, in its client area. The title has its ampersand taken out for the width.
        static void menu_rects(const dock_host& host, HDC dc, RECT* out) noexcept
        {
            const HGDIOBJ previous = SelectObject(dc, g_font);
            int x = px(4);

            for (std::uint32_t i = 0; i < host.m_bar_count; ++i)
            {
                wchar_t title[64]{};
                widen(host.m_bar[i].title != nullptr ? host.m_bar[i].title : "", title, 64);
                RECT measure{0, 0, 0, 0};
                DrawTextW(dc, title, -1, &measure, DT_SINGLELINE | DT_CALCRECT);
                const int width = measure.right - measure.left + px(20);
                out[i] = {x, px(2), x + width, dock_host::menu_bar_height() - px(2)};
                x += width;
            }

            SelectObject(dc, previous);
        }

        // The title under a point of the bar, or -1.
        static std::int32_t menu_at(const dock_host& host, HWND bar, int x, int y) noexcept
        {
            const HDC dc = GetDC(bar);
            RECT rects[dock_host::k_max_bar_menus];
            menu_rects(host, dc, rects);
            ReleaseDC(bar, dc);

            for (std::uint32_t i = 0; i < host.m_bar_count; ++i)
            {
                if (inside(rects[i], x, y))
                    return static_cast<std::int32_t>(i);
            }

            return -1;
        }

        // The state of a menu while it is open, for the hook that lets the mouse and the arrow keys move between the
        // titles of the bar. The menu loop is the system's, and this is the only way into it.
        struct bar_track
        {
            dock_host* host{nullptr};
            HWND bar{nullptr};
            HMENU top{nullptr};
            std::int32_t current{-1};
            // The title to go to next, or -1 when the menu is just closing.
            std::int32_t next{-1};
            POINT last{-1, -1};
            HHOOK hook{nullptr};
        };

        static bar_track& track() noexcept
        {
            static bar_track state;
            return state;
        }

        // Whether the highlighted row of a menu opens a submenu.
        static bool highlight_opens_submenu(HMENU menu) noexcept
        {
            const int count = GetMenuItemCount(menu);

            for (int i = 0; i < count; ++i)
            {
                const UINT state = GetMenuState(menu, static_cast<UINT>(i), MF_BYPOSITION);

                if (state != 0xFFFFFFFFu && (state & MF_HILITE) != 0)
                    return (state & MF_POPUP) != 0;
            }

            return false;
        }

        // How many menus are on screen on this thread: 1 while only the first level is open, 2 or more once a submenu is.
        static int open_menu_windows() noexcept
        {
            int count = 0;
            EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM total) -> BOOL {
                wchar_t name[16]{};

                if (IsWindowVisible(window) && GetClassNameW(window, name, 16) > 0 && std::wcscmp(name, L"#32768") == 0)
                    ++*reinterpret_cast<int*>(total);

                return TRUE;
            }, reinterpret_cast<LPARAM>(&count));
            return count;
        }

        static LRESULT CALLBACK menu_hook(int code, WPARAM wparam, LPARAM lparam)
        {
            bar_track& t = track();

            if (code != 2 /* MSGF_MENU */ || t.host == nullptr)
                return CallNextHookEx(t.hook, code, wparam, lparam);

            const MSG& message = *reinterpret_cast<const MSG*>(lparam);

            if (message.message == WM_MOUSEMOVE)
            {
                // Mouse moves repeat without the mouse moving, so only a real move counts.
                if (message.pt.x == t.last.x && message.pt.y == t.last.y)
                    return CallNextHookEx(t.hook, code, wparam, lparam);

                t.last = message.pt;
                POINT local = message.pt;
                ScreenToClient(t.bar, &local);
                const std::int32_t over = menu_at(*t.host, t.bar, local.x, local.y);

                if (over >= 0 && over != t.current)
                {
                    t.next = over;
                    EndMenu();
                }
            }
            else if (message.message == WM_KEYDOWN && (message.wParam == VK_LEFT || message.wParam == VK_RIGHT))
            {
                // Only the first level of the menu goes sideways. In a submenu the system closes it with Left, and a row
                // that opens a submenu uses Right for that.
                if (open_menu_windows() == 1 && !(message.wParam == VK_RIGHT && highlight_opens_submenu(t.top)))
                {
                    const std::int32_t count = static_cast<std::int32_t>(t.host->m_bar_count);
                    const std::int32_t step = message.wParam == VK_RIGHT ? 1 : count - 1;
                    t.next = (t.current + step) % count;
                    EndMenu();
                    return 1;
                }
            }

            return CallNextHookEx(t.hook, code, wparam, lparam);
        }

        // The square at the right end of the bar that switches the theme.
        static RECT theme_button_rect(const RECT& client) noexcept
        {
            const int size = client.bottom - px(5);
            const int right = client.right - px(6);
            return {right - size, px(2), right, px(2) + size};
        }

        // The button of the layouts menu, just left of the theme button: an icon and a small arrow down.
        static RECT layouts_button_rect(const RECT& client) noexcept
        {
            const RECT theme_button = theme_button_rect(client);
            const int right = theme_button.left - px(4);
            return {right - px(42), theme_button.top, right, theme_button.bottom};
        }

        // The action button, just left of the layouts button.
        static RECT action_button_rect(const RECT& client) noexcept
        {
            const RECT layouts = layouts_button_rect(client);
            const int right = layouts.left - px(6);
            return {right - px(88), layouts.top, right, layouts.bottom};
        }

        static LRESULT menubar_message(dock_host& host, HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
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
                vgradient(dc, client, lighter(theme().title, 0.07f), shaded(theme().title, 0.12f));
                fill(dc, RECT{0, client.bottom - 1, client.right, client.bottom}, shaded(theme().title, 0.45f));
                RECT rects[dock_host::k_max_bar_menus];
                menu_rects(host, dc, rects);
                const HGDIOBJ previous = SelectObject(dc, g_font);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, colour_of(theme().text));

                for (std::uint32_t i = 0; i < host.m_bar_count; ++i)
                {
                    // The open menu keeps its title lit while the mouse is in the menu.
                    if (host.m_menu_hot == static_cast<std::int32_t>(i) || host.m_menu_open == static_cast<std::int32_t>(i))
                        fill(dc, rects[i], theme().hover);

                    wchar_t title[64]{};
                    widen(host.m_bar[i].title != nullptr ? host.m_bar[i].title : "", title, 64);
                    DrawTextW(dc, title, -1, &rects[i], DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                }

                // The action button: a word in a frame, lit while the mouse is over it.
                if (host.m_action != nullptr)
                {
                    RECT action = action_button_rect(client);

                    // A raised button in the accent colour: a rounded face that fades down, a darker edge, and a lit rim.
                    {
                        const draw::frame shapes(dc);
                        const std::uint32_t accent = theme().accent;
                        const float lift = host.m_action_hot ? 0.22f : 0.1f;
                        const dock_rect box{action.left, action.top + px(2), action.right - action.left, action.bottom - action.top - px(4)};
                        draw::gradient(dc, box, px(4), blend_colour(accent, 0xFFFFFF, lift + 0.12f), blend_colour(accent, 0x000000, 0.18f - lift * 0.4f),
                            blend_colour(accent, 0x000000, 0.45f), true);

                        // The picture of a package in front of the word, which then sits in what is left of the button.
                        const int icon_size = px(14);
                        draw::icon(dc, icon_kind::stack, {box.x + px(9), box.y + (box.height - icon_size) / 2, icon_size, icon_size}, 0xFFFFFF);
                    }

                    action.left += px(9) + px(14) + px(2);

                    SetTextColor(dc, colour_of(0xFFFFFF));
                    wchar_t label[32]{};
                    widen(host.m_action_label, label, 32);
                    DrawTextW(dc, label, -1, &action, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                    SetTextColor(dc, colour_of(theme().text));
                }

                // The theme button shows the theme that is on: a moon for dark, a sun for light.
                const RECT button = theme_button_rect(client);

                if (host.m_theme_hot)
                    fill(dc, button, theme().hover);

                {
                    const draw::frame shapes(dc);
                    const int icon = px(14);
                    draw::icon(dc, g_theme_mode == theme_mode::dark ? icon_kind::moon : icon_kind::sun,
                        {button.left + (button.right - button.left - icon) / 2, button.top + (button.bottom - button.top - icon) / 2, icon, icon}, theme().text);

                    if (host.m_layouts_menu != nullptr)
                    {
                        const RECT layouts = layouts_button_rect(client);

                        if (host.m_layouts_hot || host.m_layouts_open)
                            fill(dc, layouts, theme().hover);

                        const int mid_y = (layouts.top + layouts.bottom) / 2;
                        draw::icon(dc, icon_kind::layout, {layouts.left + px(7), mid_y - icon / 2, icon, icon}, theme().text);

                        // The arrow down that says it opens a list.
                        const float cx = static_cast<float>(layouts.right - px(11));
                        const float cy = static_cast<float>(mid_y);
                        const float s = static_cast<float>(px(1));
                        const draw::point arrow[3] = {{cx - 3.0f * s, cy - 1.5f * s}, {cx, cy + 1.5f * s}, {cx + 3.0f * s, cy - 1.5f * s}};
                        draw::polyline(dc, arrow, 3, theme().text, 1.5f * s);
                    }
                }

                SelectObject(dc, previous);
                EndPaint(window, &paint);
                return 0;
            }

            case WM_MOUSEMOVE:
            {
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                TrackMouseEvent(&track);
                const std::int32_t hot = menu_at(host, window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                RECT client{};
                GetClientRect(window, &client);
                const bool over_theme = inside(theme_button_rect(client), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                const bool over_layouts = host.m_layouts_menu != nullptr && inside(layouts_button_rect(client), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

                const bool over_action = host.m_action != nullptr && inside(action_button_rect(client), GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

                if (hot != host.m_menu_hot || over_theme != host.m_theme_hot || over_layouts != host.m_layouts_hot || over_action != host.m_action_hot)
                {
                    host.m_menu_hot = hot;
                    host.m_theme_hot = over_theme;
                    host.m_layouts_hot = over_layouts;
                    host.m_action_hot = over_action;
                    InvalidateRect(window, nullptr, FALSE);
                }

                return 0;
            }

            case WM_MOUSELEAVE:
                host.m_menu_hot = -1;
                host.m_theme_hot = false;
                host.m_layouts_hot = false;
                host.m_action_hot = false;
                InvalidateRect(window, nullptr, FALSE);
                return 0;

            case WM_LBUTTONDOWN:
                if (host.m_menu_hot >= 0)
                    host.open_menu(static_cast<std::uint32_t>(host.m_menu_hot));
                else if (host.m_action_hot && host.m_action != nullptr)
                    host.m_action(host.m_action_user);
                else if (host.m_layouts_hot)
                    host.open_layouts_menu();
                else if (host.m_theme_hot)
                    host.set_theme_mode(g_theme_mode == theme_mode::dark ? theme_mode::light : theme_mode::dark);

                return 0;

            default:
                break;
            }

            return DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT CALLBACK menubar_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* host = reinterpret_cast<dock_host*>(context_of<dock_host>(window, message, lparam));
            return host != nullptr ? menubar_message(*host, window, message, wparam, lparam)
                                   : DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT CALLBACK root_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* context = context_of<dock_host::root_context>(window, message, lparam);
            return context != nullptr ? root_message(*context->host, context->root, window, message, wparam, lparam)
                                      : DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT CALLBACK group_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* context = context_of<dock_host::group_context>(window, message, lparam);
            return context != nullptr ? group_message(*context->host, context->node, window, message, wparam, lparam)
                                      : DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT CALLBACK panel_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* slot = context_of<dock_host::panel_slot>(window, message, lparam);
            return slot != nullptr ? panel_message(*slot->host, slot->panel, window, message, wparam, lparam)
                                   : DefWindowProcW(window, message, wparam, lparam);
        }

        static LRESULT CALLBACK float_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            auto* context = context_of<dock_host::root_context>(window, message, lparam);
            return context != nullptr ? float_message(*context->host, context->root, window, message, wparam, lparam)
                                      : DefWindowProcW(window, message, wparam, lparam);
        }
    };

    bool dock_host::register_classes(void* instance) noexcept
    {
        const HINSTANCE module = static_cast<HINSTANCE>(instance);
        const HCURSOR arrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

        if (g_font == nullptr)
            rebuild_font(GetDpiForSystem());

        WNDCLASSEXW host{sizeof(host)};
        host.lpfnWndProc = dock_host_access::root_proc;
        host.hInstance = module;
        host.hCursor = arrow;
        host.lpszClassName = k_host_class;

        WNDCLASSEXW group{sizeof(group)};
        group.style = CS_DBLCLKS;
        group.lpfnWndProc = dock_host_access::group_proc;
        group.hInstance = module;
        group.hCursor = arrow;
        group.lpszClassName = k_group_class;

        WNDCLASSEXW panel{sizeof(panel)};
        panel.style = CS_DBLCLKS;
        panel.lpfnWndProc = dock_host_access::panel_proc;
        panel.hInstance = module;
        panel.hCursor = arrow;
        panel.lpszClassName = k_panel_class;

        WNDCLASSEXW viewport{sizeof(viewport)};
        viewport.style = CS_OWNDC | CS_DBLCLKS;
        viewport.lpfnWndProc = dock_host_access::panel_proc;
        viewport.hInstance = module;
        viewport.hCursor = arrow;
        viewport.lpszClassName = k_viewport_class;

        WNDCLASSEXW floating{sizeof(floating)};
        floating.lpfnWndProc = dock_host_access::float_proc;
        floating.hInstance = module;
        floating.hCursor = arrow;
        floating.lpszClassName = k_float_class;

        WNDCLASSEXW menubar{sizeof(menubar)};
        menubar.lpfnWndProc = dock_host_access::menubar_proc;
        menubar.hInstance = module;
        menubar.hCursor = arrow;
        menubar.lpszClassName = k_menubar_class;

        WNDCLASSEXW hint{sizeof(hint)};
        hint.lpfnWndProc = dock_host_access::hint_proc;
        hint.hInstance = module;
        hint.lpszClassName = k_hint_class;

        WNDCLASSEXW tooltip{sizeof(tooltip)};
        tooltip.lpfnWndProc = dock_host_access::tooltip_proc;
        tooltip.hInstance = module;
        tooltip.lpszClassName = k_tooltip_class;

        WNDCLASSEXW ghost{sizeof(ghost)};
        ghost.lpfnWndProc = dock_host_access::ghost_proc;
        ghost.hInstance = module;
        ghost.lpszClassName = k_ghost_class;

        WNDCLASSEXW preview{sizeof(preview)};
        preview.lpfnWndProc = dock_host_access::preview_proc;
        preview.hInstance = module;
        preview.lpszClassName = k_preview_class;

        return g_font != nullptr && RegisterClassExW(&host) != 0 && RegisterClassExW(&group) != 0 &&
            RegisterClassExW(&panel) != 0 && RegisterClassExW(&viewport) != 0 && RegisterClassExW(&floating) != 0 &&
            RegisterClassExW(&preview) != 0 && RegisterClassExW(&menubar) != 0 && RegisterClassExW(&tooltip) != 0 &&
            RegisterClassExW(&hint) != 0 && RegisterClassExW(&ghost) != 0;
    }

    namespace
    {
        // The main window's way of taking files dropped from outside. It lives as long as the program, so it does not count references.
        class file_drop_target final : public IDropTarget
        {
        public:
            explicit file_drop_target(dock_host& host) noexcept : m_host(host) {}

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) noexcept override
            {
                if (out == nullptr)
                    return E_POINTER;

                if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IDropTarget))
                {
                    *out = static_cast<IDropTarget*>(this);
                    return S_OK;
                }

                *out = nullptr;
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() noexcept override { return 2; }
            ULONG STDMETHODCALLTYPE Release() noexcept override { return 1; }

            HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL at, DWORD* effect) noexcept override
            {
                m_has = read(data);
                return update(keys, at, effect);
            }

            HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL at, DWORD* effect) noexcept override
            {
                return update(keys, at, effect);
            }

            HRESULT STDMETHODCALLTYPE DragLeave() noexcept override
            {
                m_has = false;
                m_host.file_drag_leave();
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keys, POINTL at, DWORD* effect) noexcept override
            {
                (void)data;
                const drop_effect result = m_has ? m_host.file_drag_drop(at.x, at.y, keys) : drop_effect::none;
                m_has = false;
                *effect = convert(result) & *effect;
                return S_OK;
            }

        private:
            static DWORD convert(drop_effect effect) noexcept
            {
                return effect == drop_effect::copy ? DROPEFFECT_COPY : effect == drop_effect::move ? DROPEFFECT_MOVE
                    : effect == drop_effect::link ? DROPEFFECT_LINK : DROPEFFECT_NONE;
            }

            HRESULT update(DWORD keys, POINTL at, DWORD* effect) noexcept
            {
                if (!m_has)
                {
                    *effect = DROPEFFECT_NONE;
                    return S_OK;
                }

                const DWORD wanted = convert(m_host.file_drag_over(m_list, at.x, at.y, keys));
                *effect = (wanted & *effect) != 0 ? wanted : DROPEFFECT_NONE;
                return S_OK;
            }

            // Takes the paths out of the data, when it is a list of files.
            bool read(IDataObject* data) noexcept
            {
                m_list[0] = L'\0';

                if (data == nullptr)
                    return false;

                FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
                STGMEDIUM medium{};

                if (FAILED(data->GetData(&format, &medium)))
                    return false;

                const HDROP drop = static_cast<HDROP>(medium.hGlobal);
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
                std::size_t used = 0;

                for (UINT i = 0; i < count; ++i)
                {
                    const UINT length = DragQueryFileW(drop, i, nullptr, 0);

                    if (used + length + 2 > k_capacity)
                        break;

                    DragQueryFileW(drop, i, m_list + used, static_cast<UINT>(k_capacity - used - 1));
                    used += length + 1;
                }

                m_list[used] = L'\0';
                ReleaseStgMedium(&medium);
                return used > 0;
            }

            static constexpr std::size_t k_capacity = 8192;
            dock_host& m_host;
            bool m_has{false};
            wchar_t m_list[k_capacity]{};
        };

        // The one target, which every window of the dock takes files with. Null until the host is created, or without OLE.
        IDropTarget* g_files_target = nullptr;
    }

    bool dock_host::create(void* main_window, void* instance) noexcept
    {
        m_main = main_window;
        m_instance = instance;
        m_holder = CreateWindowExW(0, k_panel_class, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, static_cast<HINSTANCE>(instance), nullptr);

        for (root_id i = 0; i < dock_model::k_max_roots; ++i)
            m_root_context[i] = {this, i};

        for (node_id i = 0; i < dock_model::k_max_nodes; ++i)
            m_group_context[i] = {this, i};

        m_root_top[0] = main_window;
        m_root_host[0] = CreateWindowExW(0, k_host_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 100, 100,
            static_cast<HWND>(main_window), nullptr, static_cast<HINSTANCE>(instance), &m_root_context[0]);
        m_root_serial[0] = m_model.root_serial(0);

        for (node_id i = 0; i < dock_model::k_max_nodes; ++i)
            m_tab_follow[i] = k_no_panel;

        apply_dpi(GetDpiForWindow(static_cast<HWND>(main_window)));
        drag_drop::install(this);

        // Files dragged in from Explorer. Without OLE the window simply does not take them.
        static file_drop_target files_target{*this};

        if (SUCCEEDED(OleInitialize(nullptr)))
        {
            g_files_target = &files_target;
            (void)RegisterDragDrop(static_cast<HWND>(main_window), g_files_target);
        }

        return m_holder != nullptr && m_root_host[0] != nullptr;
    }

    panel_id dock_host::add_panel(const char* title, panel_content* content, bool permanent, panel_kind kind) noexcept
    {
        const panel_id next = m_model.panel_count();

        if (next >= dock_model::k_max_panels || title == nullptr)
            return k_no_panel;

        m_slot[next] = {this, next};
        const HWND window = CreateWindowExW(0, kind == panel_kind::viewport ? k_viewport_class : k_panel_class, L"",
            WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 10, 10, static_cast<HWND>(m_holder), nullptr, static_cast<HINSTANCE>(m_instance), &m_slot[next]);

        if (window == nullptr)
            return k_no_panel;

        const panel_id id = m_model.add_panel({title, window, permanent});

        if (id != next)
        {
            DestroyWindow(window);
            return k_no_panel;
        }

        m_content[id] = content;
        m_kind[id] = kind;
        return id;
    }

    panel_id dock_host::add_strip(panel_content* content, strip_edge edge, std::int32_t height) noexcept
    {
        if (m_strip_count >= k_max_strips || m_main == nullptr)
            return k_no_panel;

        const panel_id id = dock_model::k_max_panels + m_strip_count;
        m_slot[id] = {this, id};
        const HWND window = CreateWindowExW(0, k_panel_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10,
            static_cast<HWND>(m_main), nullptr, static_cast<HINSTANCE>(m_instance), &m_slot[id]);

        if (window == nullptr)
            return k_no_panel;

        m_content[id] = content;
        m_kind[id] = panel_kind::standard;
        m_strip_window[m_strip_count] = window;
        m_strip_edge[m_strip_count] = edge;
        m_strip_height[m_strip_count] = height;
        ++m_strip_count;
        layout_main();
        return id;
    }

    void* dock_host::window_of(panel_id panel) const noexcept
    {
        if (is_strip(panel))
            return panel - dock_model::k_max_panels < m_strip_count ? m_strip_window[panel - dock_model::k_max_panels] : nullptr;

        return panel < m_model.panel_count() ? m_model.handle_of(panel) : nullptr;
    }

    void dock_host::invalidate_panel(panel_id panel) noexcept
    {
        if (const HWND window = static_cast<HWND>(window_of(panel)))
            InvalidateRect(window, nullptr, FALSE);
    }

    void dock_host::request_frames(panel_id panel) noexcept
    {
        if (panel < dock_model::k_max_panels && m_content[panel] != nullptr)
        {
            m_frames[panel] = true;
            start_timer();
        }
    }

    bool dock_host::create_root_windows(root_id root) noexcept
    {
        // No caption: the tab strip is the title bar. The frame is a one pixel line around the root.
        // An application window: it has its own taskbar button and Alt+Tab entry, and the main window still owns it, so it
        // stays above it.
        const DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
        const DWORD extended = WS_EX_APPWINDOW;
        const dock_rect area = m_model.root_area(root);
        const int line = k_float_frame;
        RECT frame{area.x - line, area.y - line, area.x + area.width + line, area.y + area.height + line};
        clamp_to_work(frame);

        const HWND top = CreateWindowExW(extended, k_float_class, L"", style, frame.left, frame.top, frame.right - frame.left,
            frame.bottom - frame.top, static_cast<HWND>(m_main), nullptr, static_cast<HINSTANCE>(m_instance), &m_root_context[root]);

        if (top == nullptr)
            return false;

        const HWND surface = CreateWindowExW(0, k_host_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, line, line, area.width, area.height,
            top, nullptr, static_cast<HINSTANCE>(m_instance), &m_root_context[root]);

        if (surface == nullptr)
        {
            DestroyWindow(top);
            return false;
        }

        // A margin of one pixel is what makes the system draw its shadow around a window without a frame.
        const MARGINS shadow{1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(top, &shadow);

        // A floating window takes files too.
        if (g_files_target != nullptr)
            (void)RegisterDragDrop(top, g_files_target);

        m_root_top[root] = top;
        m_root_host[root] = surface;
        m_root_serial[root] = m_model.root_serial(root);
        apply_theme();
        ShowWindow(top, SW_SHOWNOACTIVATE);
        return true;
    }

    bool dock_host::create_group_window(node_id group) noexcept
    {
        const HWND parent = static_cast<HWND>(m_root_host[m_model.root_of(group)]);

        if (parent == nullptr)
            return false;

        const HWND window = CreateWindowExW(0, k_group_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10, 10,
            parent, nullptr, static_cast<HINSTANCE>(m_instance), &m_group_context[group]);

        if (window == nullptr)
            return false;

        m_group[group] = window;
        m_group_serial[group] = m_model.node_serial(group);
        return true;
    }

    void dock_host::refresh_area(root_id root) noexcept
    {
        const HWND surface = static_cast<HWND>(m_root_host[root]);

        if (surface == nullptr)
            return;

        POINT origin{0, 0};
        ClientToScreen(surface, &origin);
        RECT client{};
        GetClientRect(surface, &client);
        m_model.set_root_area(root, {origin.x, origin.y, client.right, client.bottom});
    }

    void dock_host::refresh_all_areas() noexcept
    {
        for (root_id i = 0; i < dock_model::k_max_roots; ++i)
        {
            if (m_model.root_used(i))
                refresh_area(i);
        }
    }

    void dock_host::layout_root(root_id root) noexcept
    {
        refresh_area(root);
        m_model.layout(root, &dock_host_access::place_group, this);
    }

    void dock_host::layout_main() noexcept
    {
        const HWND main = static_cast<HWND>(m_main);
        const HWND surface = static_cast<HWND>(m_root_host[0]);

        if (surface == nullptr)
            return;

        RECT client{};
        GetClientRect(main, &client);
        int top = m_menubar != nullptr && m_chrome_shown ? menu_bar_height() : 0;
        int bottom = client.bottom;

        if (m_menubar != nullptr)
        {
            ShowWindow(static_cast<HWND>(m_menubar), m_chrome_shown ? SW_SHOWNA : SW_HIDE);

            if (m_chrome_shown)
                MoveWindow(static_cast<HWND>(m_menubar), 0, 0, client.right, top, TRUE);
        }

        // Strips stack away from their edge, in the order they were added.
        for (std::uint32_t i = 0; i < m_strip_count; ++i)
        {
            ShowWindow(static_cast<HWND>(m_strip_window[i]), m_chrome_shown ? SW_SHOWNA : SW_HIDE);

            if (!m_chrome_shown)
                continue;

            const int height = px(m_strip_height[i]);
            const int y = m_strip_edge[i] == strip_edge::top ? top : bottom - height;
            MoveWindow(static_cast<HWND>(m_strip_window[i]), 0, y, client.right, height, TRUE);

            if (m_strip_edge[i] == strip_edge::top)
                top += height;
            else
                bottom -= height;
        }

        MoveWindow(surface, 0, top, client.right, std::max<int>(bottom - top, 0), TRUE);
    }

    void dock_host::set_chrome_shown(bool shown) noexcept
    {
        if (m_chrome_shown == shown)
            return;

        m_chrome_shown = shown;
        layout_main();
        sync(false);
    }

    void dock_host::fit_group(node_id group) noexcept
    {
        const HWND window = static_cast<HWND>(m_group[group]);

        if (window == nullptr)
            return;

        RECT client{};
        GetClientRect(window, &client);
        const panel_id active = m_model.active_tab(group);
        const int frame = body_frame();
        const int header = m_model.header_height(group);
        const int top = header + (header > 0 ? body_gap() : 0);
        const int body = std::max<int>(client.bottom - top - frame, 0);
        const int body_width = std::max<int>(client.right - 2 * frame, 0);
        const std::uint64_t now = GetTickCount64();

        for (std::uint32_t i = 0; i < m_model.tab_count(group); ++i)
        {
            const panel_id id = m_model.tab_at(group, i);
            const HWND content = static_cast<HWND>(m_model.handle_of(id));

            if (id != active)
            {
                ShowWindow(content, SW_HIDE);
                continue;
            }

            // A swapchain is rebuilt for every size it gets, so a viewport that is already on screen keeps its size
            // while the layout moves. tick gives it the new one once the layout has been still for a moment.
            const bool wait = m_kind[id] == panel_kind::viewport && IsWindowVisible(content) && GetParent(content) == window &&
                now - m_resize_stamp < k_settle_ms;

            if (wait)
            {
                m_resize_pending = true;
                start_timer();
                continue;
            }

            SetWindowPos(content, nullptr, frame, top, body_width, body, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }

    void dock_host::sync(bool animate) noexcept
    {
        hide_tooltip(true);

        HWND stale_groups[dock_model::k_max_nodes]{};
        HWND stale_tops[dock_model::k_max_roots]{};
        std::uint32_t stale_group_count = 0;
        std::uint32_t stale_top_count = 0;

        // Windows that no longer match the model are set aside. Their panels move out before they are destroyed.
        for (root_id r = 1; r < dock_model::k_max_roots; ++r)
        {
            const bool used = m_model.root_used(r);

            if (m_root_top[r] != nullptr && (!used || m_root_serial[r] != m_model.root_serial(r)))
            {
                stale_tops[stale_top_count++] = static_cast<HWND>(m_root_top[r]);
                m_root_top[r] = nullptr;
                m_root_host[r] = nullptr;
            }

            if (used && m_root_top[r] == nullptr)
                (void)create_root_windows(r);
        }

        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            const bool group = m_model.is_group(n);

            if (m_group[n] != nullptr && (!group || m_group_serial[n] != m_model.node_serial(n)))
            {
                stale_groups[stale_group_count++] = static_cast<HWND>(m_group[n]);
                m_group[n] = nullptr;
                m_tween[n].active = false;
                m_group_valid[n] = false;
            }

            if (group && m_group[n] == nullptr)
            {
                m_group_valid[n] = false;
                m_tween[n].active = false;
                m_tab_scroll[n] = 0;
                m_tab_follow[n] = k_no_panel;
                m_strip[n] = {};
                (void)create_group_window(n);
            }
        }

        for (panel_id p = 0; p < m_model.panel_count(); ++p)
        {
            const HWND content = static_cast<HWND>(m_model.handle_of(p));
            const node_id group = m_model.group_of(p);
            const HWND parent = group != k_no_node ? static_cast<HWND>(m_group[group]) : static_cast<HWND>(m_holder);

            if (parent != nullptr && GetParent(content) != parent)
                SetParent(content, parent);

            if (group == k_no_node)
                ShowWindow(content, SW_HIDE);
        }

        for (std::uint32_t i = 0; i < stale_group_count; ++i)
        {
            if (IsWindow(stale_groups[i]))
                DestroyWindow(stale_groups[i]);
        }

        for (std::uint32_t i = 0; i < stale_top_count; ++i)
        {
            if (IsWindow(stale_tops[i]))
            {
                if (g_files_target != nullptr)
                    (void)RevokeDragDrop(stale_tops[i]);

                DestroyWindow(stale_tops[i]);
            }
        }

        m_animate = animate;

        for (root_id r = 0; r < dock_model::k_max_roots; ++r)
        {
            if (!m_model.root_used(r) || m_root_host[r] == nullptr)
                continue;

            layout_root(r);
        }

        m_animate = false;

        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            if (!m_model.is_group(n) || m_group[n] == nullptr)
                continue;

            fit_group(n);
            InvalidateRect(static_cast<HWND>(m_group[n]), nullptr, FALSE);

            // A floating window is named after the panel it shows.
            const root_id r = m_model.root_of(n);

            if (r != dock_model::k_main_root && m_model.root_node(r) == n && m_root_top[r] != nullptr)
            {
                // The name shows in the taskbar and in Alt+Tab.
                wchar_t title[128]{};
                widen(m_model.title_of(m_model.active_tab(n)), title, 64);
                wcscat_s(title, g_title_suffix);
                SetWindowTextW(static_cast<HWND>(m_root_top[r]), title);
            }
        }

        notify_visibility();

        // Moving a window between parents drops its focus, so give it back.
        const panel_id focus = m_model.focused();

        if (focus != k_no_panel && GetActiveWindow() != nullptr && GetFocus() == nullptr)
        {
            const HWND content = static_cast<HWND>(m_model.handle_of(focus));

            if (IsWindowVisible(content))
                SetFocus(content);
        }
    }

    void dock_host::notify_visibility() noexcept
    {
        for (panel_id p = 0; p < m_model.panel_count(); ++p)
        {
            const node_id group = m_model.group_of(p);
            const bool shown = group != k_no_node && m_model.active_tab(group) == p;

            if (shown == m_shown[p])
                continue;

            m_shown[p] = shown;

            if (m_content[p] != nullptr)
                m_content[p]->on_visible(shown);
        }
    }

    void dock_host::invalidate_strips() noexcept
    {
        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            if (m_group[n] == nullptr || !m_model.is_group(n))
                continue;

            RECT client{};
            GetClientRect(static_cast<HWND>(m_group[n]), &client);
            client.bottom = std::min<LONG>(client.bottom, dock_model::tab_height());
            InvalidateRect(static_cast<HWND>(m_group[n]), &client, FALSE);
        }
    }

    void dock_host::focus_panel(panel_id panel) noexcept
    {
        if (!m_model.is_open(panel))
            return;

        m_model.set_focused(panel);
        const HWND content = static_cast<HWND>(m_model.handle_of(panel));

        if (IsWindowVisible(content))
            SetFocus(content);

        invalidate_strips();
    }

    void dock_host::focus_root(root_id root) noexcept
    {
        panel_id target = m_model.focused();

        if (target == k_no_panel || !m_model.is_open(target) || m_model.root_of(m_model.group_of(target)) != root)
        {
            target = k_no_panel;

            for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
            {
                if (m_model.is_group(n) && m_model.root_of(n) == root)
                {
                    target = m_model.active_tab(n);
                    break;
                }
            }
        }

        if (target != k_no_panel)
            focus_panel(target);
    }

    void dock_host::set_key_handler(key_handler_fn handler, void* user) noexcept
    {
        m_key_handler = handler;
        m_key_user = user;
    }

    // F10 and Alt plus a mnemonic open a menu of the bar before any panel sees the key.
    bool dock_host::bar_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (m_bar_count == 0 || m_menubar == nullptr || !m_chrome_shown || ctrl || shift)
            return false;

        if (!alt && key == VK_F10)
        {
            open_menu(0);
            return true;
        }

        if (!alt || key < 'A' || key > 'Z')
            return false;

        for (std::uint32_t i = 0; i < m_bar_count; ++i)
        {
            const char* title = m_bar[i].title;
            const char* mark = title != nullptr ? std::strchr(title, '&') : nullptr;

            if (mark != nullptr && mark[1] != 0 && std::toupper(static_cast<unsigned char>(mark[1])) == static_cast<int>(key))
            {
                open_menu(i);
                return true;
            }
        }

        return false;
    }

    bool dock_host::dispatch_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (bar_key(key, ctrl, shift, alt))
            return true;

        const panel_id focus = m_model.focused();

        if (focus != k_no_panel && m_content[focus] != nullptr && m_content[focus]->on_key(key, ctrl, shift, alt))
            return true;

        return m_key_handler != nullptr && m_key_handler(m_key_user, key, ctrl, shift, alt);
    }

    void dock_host::close_root(root_id root) noexcept
    {
        panel_id members[dock_model::k_max_panels]{};
        std::uint32_t count = 0;

        for (panel_id p = 0; p < m_model.panel_count(); ++p)
        {
            const node_id group = m_model.group_of(p);

            if (group != k_no_node && m_model.root_of(group) == root)
                members[count++] = p;
        }

        for (std::uint32_t i = 0; i < count; ++i)
            (void)m_model.close_panel(members[i]);

        sync();
    }

    void dock_host::show_preview(const dock_rect& screen, const char* title, std::uint32_t mode) noexcept
    {
        if (m_preview == nullptr)
        {
            m_preview = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                k_preview_class, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, static_cast<HINSTANCE>(m_instance), nullptr);
        }

        if (m_preview == nullptr)
            return;

        const HWND window = static_cast<HWND>(m_preview);
        const bool shown = IsWindowVisible(window) != FALSE;
        const bool ghost = mode == 1;
        bool repaint = !shown;

        if (GetWindowLongPtrW(window, GWLP_USERDATA) != static_cast<LONG_PTR>(mode) || !shown)
        {
            SetWindowLongPtrW(window, GWLP_USERDATA, static_cast<LONG_PTR>(mode));
            // The outline of a floating window is nearly solid. A docked preview lets the layout show through.
            SetLayeredWindowAttributes(window, 0, ghost ? 215 : (mode == 2 ? 110 : 120), LWA_ALPHA);
            repaint = true;
        }

        // The floating outline follows the cursor exactly. A docked preview glides from wherever it was.
        m_preview_target = screen;
        m_preview_smooth = shown && !ghost;

        if (!m_preview_smooth)
            m_preview_cur = screen;

        if (title != m_preview_title)
        {
            m_preview_title = title;
            wchar_t text[64]{};
            widen(title != nullptr ? title : "", text, 64);
            SetWindowTextW(window, text);
            repaint = true;
        }

        // Only a window that was hidden is raised. Raising it on every move would re-stack it against the other
        // overlays between two frames, and they would flicker.
        if (!shown)
        {
            SetWindowPos(window, HWND_TOPMOST, m_preview_cur.x, m_preview_cur.y, m_preview_cur.width, m_preview_cur.height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            m_restack = true;
        }
        else
        {
            SetWindowPos(window, nullptr, m_preview_cur.x, m_preview_cur.y, m_preview_cur.width, m_preview_cur.height,
                SWP_NOACTIVATE | SWP_NOZORDER);
        }

        repaint = repaint || m_preview_cur.width != m_preview_drawn.width || m_preview_cur.height != m_preview_drawn.height;
        m_preview_drawn = m_preview_cur;

        if (repaint)
            InvalidateRect(window, nullptr, FALSE);

        if (m_preview_smooth)
            start_timer();
    }

    void dock_host::start_timer() noexcept
    {
        if (!m_timer_on && m_root_host[0] != nullptr)
        {
            // The default timer tick is 15.6 ms, which makes an animation step unevenly. 1 ms makes the 10 ms timer keep time.
            timeBeginPeriod(1);
            m_timer_on = SetTimer(static_cast<HWND>(m_root_host[0]), 1, 10, nullptr) != 0;

            if (!m_timer_on)
                timeEndPeriod(1);
        }
    }

    void dock_host::tick() noexcept
    {
        // Wait for the display to be ready for a frame. A plain timer wakes at uneven times (a frame every 10 or 20 ms), which
        // shows as stutter, and waking with the display gives every step the same length.
        DwmFlush();

        static constexpr float k_slide_ms = 180.0f;
        const std::uint64_t now = GetTickCount64();
        bool busy = false;

        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            tween& slide = m_tween[n];

            if (!slide.active)
                continue;

            if (m_group[n] == nullptr)
            {
                slide.active = false;
                continue;
            }

            const float u = std::min(static_cast<float>(now - slide.start) / k_slide_ms, 1.0f);
            const float eased = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
            const auto mix = [eased](std::int32_t from, std::int32_t to) {
                return from + static_cast<std::int32_t>(std::lround(static_cast<float>(to - from) * eased));
            };
            const dock_rect rect{mix(slide.from.x, slide.to.x), mix(slide.from.y, slide.to.y),
                mix(slide.from.width, slide.to.width), mix(slide.from.height, slide.to.height)};
            dock_host_access::apply_group_rect(*this, n, rect);

            if (u >= 1.0f)
                slide.active = false;
            else
                busy = true;
        }

        if (m_resize_pending)
        {
            if (now - m_resize_stamp >= k_settle_ms)
            {
                m_resize_pending = false;

                for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
                {
                    if (m_group[n] != nullptr && m_model.is_group(n))
                        fit_group(n);
                }
            }
            else
            {
                busy = true;
            }
        }

        if (m_preview != nullptr && m_preview_smooth && IsWindowVisible(static_cast<HWND>(m_preview)))
        {
            const auto chase = [](std::int32_t from, std::int32_t to) {
                const std::int32_t delta = to - from;

                if (delta == 0)
                    return from;

                std::int32_t step = static_cast<std::int32_t>(std::lround(static_cast<float>(delta) * 0.35f));

                if (step == 0)
                    step = delta > 0 ? 1 : -1;

                return from + step;
            };

            m_preview_cur = {chase(m_preview_cur.x, m_preview_target.x), chase(m_preview_cur.y, m_preview_target.y),
                chase(m_preview_cur.width, m_preview_target.width), chase(m_preview_cur.height, m_preview_target.height)};
            SetWindowPos(static_cast<HWND>(m_preview), nullptr, m_preview_cur.x, m_preview_cur.y, m_preview_cur.width,
                m_preview_cur.height, SWP_NOACTIVATE | SWP_NOZORDER);

            if (m_preview_cur.width != m_preview_drawn.width || m_preview_cur.height != m_preview_drawn.height)
                InvalidateRect(static_cast<HWND>(m_preview), nullptr, FALSE);

            m_preview_drawn = m_preview_cur;
            busy = busy || !(m_preview_cur == m_preview_target);
        }

        for (panel_id p = 0; p < dock_model::k_max_panels; ++p)
        {
            if (!m_frames[p])
                continue;

            const bool more = m_content[p] != nullptr && m_content[p]->on_frame(now);
            m_frames[p] = more;

            // Paint now. A paint message queues behind the other input, so the frame would land late and unevenly.
            if (p < m_model.panel_count() && (m_content[p] == nullptr || m_content[p]->frame_dirty()))
                RedrawWindow(static_cast<HWND>(m_model.handle_of(p)), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            busy = busy || more;
        }

        busy = step_strips() || busy;

        // A drag asks the panel under it again while the pointer rests, so a list can scroll as it is held at its edge.
        if (m_dnd.active)
        {
            busy = true;

            if (now - m_dnd.stamp >= 40)
                drag_update(m_dnd.x, m_dnd.y);
        }

        if (!busy && m_root_host[0] != nullptr)
        {
            KillTimer(static_cast<HWND>(m_root_host[0]), 1);
            m_timer_on = false;
            timeEndPeriod(1);
        }
    }

    void dock_host::hide_preview() noexcept
    {
        if (m_preview != nullptr)
            ShowWindow(static_cast<HWND>(m_preview), SW_HIDE);

        if (m_hint != nullptr)
            ShowWindow(static_cast<HWND>(m_hint), SW_HIDE);

        if (m_root_hint != nullptr)
            ShowWindow(static_cast<HWND>(m_root_hint), SW_HIDE);
    }

    // Draws the docking tiles for the point a panel is dragged to: the compass in the middle of the group under it, and
    // a tile on each edge of the root. They show the places a panel can go before the cursor is over one. The tile
    // under the cursor lights up, and the preview shows its area.
    void dock_host::show_hint(const drop_target& target) noexcept
    {
        const auto place = [this](void*& slot, bool wanted, const dock_rect& area, drop_target& stored, const drop_target& next) {
            if (slot == nullptr && wanted)
            {
                slot = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                    k_hint_class, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, static_cast<HINSTANCE>(m_instance), this);

                if (slot != nullptr)
                    SetLayeredWindowAttributes(static_cast<HWND>(slot), k_hint_key, 235, LWA_COLORKEY | LWA_ALPHA);
            }

            if (slot == nullptr)
                return;

            const HWND window = static_cast<HWND>(slot);

            if (!wanted)
            {
                ShowWindow(window, SW_HIDE);
                return;
            }

            // Repaint only when the lit tile or the place changed. A root-sized window is not cheap to redraw.
            const bool fresh = !IsWindowVisible(window);
            const bool changed = fresh || stored.zone != next.zone || !(stored.compass == next.compass) ||
                !(stored.root_area == next.root_area);
            stored = next;

            if (fresh)
            {
                SetWindowPos(window, HWND_TOPMOST, area.x, area.y, area.width, area.height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                m_restack = true;
            }
            else
            {
                SetWindowPos(window, nullptr, area.x, area.y, area.width, area.height, SWP_NOACTIVATE | SWP_NOZORDER);
            }

            if (changed)
                InvalidateRect(window, nullptr, FALSE);
        };

        place(m_root_hint, target.root_hint, target.root_area, m_root_hint_target, target);
        place(m_hint, target.hint, target.compass, m_hint_target, target);

        // When an overlay appeared, stack all of them once, bottom to top: the preview, the edge tiles, the compass.
        if (m_restack)
        {
            m_restack = false;

            for (void* overlay : {m_preview, m_root_hint, m_hint})
            {
                if (overlay != nullptr && IsWindowVisible(static_cast<HWND>(overlay)))
                {
                    SetWindowPos(static_cast<HWND>(overlay), HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
        }
    }

    std::uint32_t dock_host::tab_index_at(node_id group, std::int32_t screen_x) noexcept
    {
        const HWND window = static_cast<HWND>(m_group[group]);

        if (window == nullptr)
            return k_tab_end;

        POINT point{screen_x, 0};
        ScreenToClient(window, &point);
        const tab_geometry geometry = dock_host_access::geometry_for(*this, group, false);
        std::uint32_t index = 0;

        while (index < geometry.count && (geometry.tab[index].left + geometry.tab[index].right) / 2 < point.x)
            ++index;

        return index;
    }

    void dock_host::toggle_panel(panel_id panel) noexcept
    {
        if (panel >= m_model.panel_count())
            return;

        if (m_model.is_open(panel))
        {
            if (!m_model.close_panel(panel))
                return;
        }
        else if (m_model.locked())
        {
            // The set of panels is fixed, so a panel that is closed stays closed.
            return;
        }
        else if (!m_model.restore_home(panel))
        {
            // It has no remembered place, so it floats in the middle of the window.
            RECT main{};
            GetWindowRect(static_cast<HWND>(m_main), &main);
            const int width = px(420);
            const int height = px(520);
            const int x = main.left + ((main.right - main.left) - width) / 2;
            const int y = main.top + ((main.bottom - main.top) - height) / 2;

            if (m_model.float_panel(panel, {x, y, width, height}) == k_no_root)
                return;
        }

        sync();
    }

    void dock_host::install_menu_bar() noexcept
    {
        m_menubar = CreateWindowExW(0, k_menubar_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 100, menu_bar_height(),
            static_cast<HWND>(m_main), nullptr, static_cast<HINSTANCE>(m_instance), this);
        apply_theme();
        layout_main();
    }

    void dock_host::set_menu_bar(const menu_bar_entry* entries, std::uint32_t count) noexcept
    {
        m_bar_count = entries != nullptr ? std::min(count, k_max_bar_menus) : 0;

        for (std::uint32_t i = 0; i < m_bar_count; ++i)
            m_bar[i] = entries[i];

        if (m_menubar != nullptr)
            InvalidateRect(static_cast<HWND>(m_menubar), nullptr, FALSE);
    }

    void dock_host::fill_menu(void* handle, context_menu& menu) noexcept
    {
        const HMENU popup = static_cast<HMENU>(handle);
        menu.notify_open();

        for (std::uint32_t i = 0; i < menu.count(); ++i)
        {
            const menu_item& item = *menu.item(i);

            if (item.separator())
            {
                AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
                continue;
            }

            // The text after a tab is the shortcut, which the menu draws against the right edge.
            wchar_t text[160]{};
            widen(item.text() != nullptr ? item.text() : "", text, 96);

            if (item.shortcut() != nullptr)
            {
                const std::size_t length = std::wcslen(text);
                text[length] = L'\t';
                widen(item.shortcut(), text + length + 1, static_cast<int>(sizeof(text) / sizeof(text[0]) - length - 1));
            }

            const UINT flags = MF_STRING | (item.enabled() ? 0 : MF_GRAYED) | (item.checked() ? MF_CHECKED : 0);

            if (item.submenu() != nullptr)
            {
                const HMENU sub = CreatePopupMenu();
                fill_menu(sub, *item.submenu());
                AppendMenuW(popup, flags | MF_POPUP, reinterpret_cast<UINT_PTR>(sub), text);
            }
            else if (m_command_count < k_max_commands)
            {
                m_command[m_command_count] = {&menu, i};
                AppendMenuW(popup, flags, k_command_base + m_command_count, text);
                ++m_command_count;
            }
            else
            {
                // No id is left, so the row is shown but cannot be chosen.
                AppendMenuW(popup, flags | MF_GRAYED, 0, text);
            }
        }
    }

    bool dock_host::run_command(std::uint32_t command) noexcept
    {
        if (command < k_command_base || command - k_command_base >= m_command_count)
            return false;

        const menu_command& row = m_command[command - k_command_base];
        row.menu->choose(static_cast<std::int32_t>(row.index));
        return true;
    }

    void dock_host::open_menu(std::uint32_t first) noexcept
    {
        if (m_menubar == nullptr || m_menu_open >= 0 || first >= m_bar_count)
            return;

        const HWND bar = static_cast<HWND>(m_menubar);
        std::int32_t index = static_cast<std::int32_t>(first);

        // Moving to another title closes the menu and opens the next one here, so the loop is not a recursion.
        while (index >= 0 && index < static_cast<std::int32_t>(m_bar_count))
        {
            const HDC dc = GetDC(bar);
            RECT rects[k_max_bar_menus];
            dock_host_access::menu_rects(*this, dc, rects);
            ReleaseDC(bar, dc);

            m_command_count = 0;
            const HMENU popup = CreatePopupMenu();

            if (m_bar[index].menu != nullptr)
                fill_menu(popup, *m_bar[index].menu);

            m_menu_open = index;
            InvalidateRect(bar, nullptr, FALSE);

            auto& t = dock_host_access::track();
            t = {};
            t.host = this;
            t.bar = bar;
            t.top = popup;
            t.current = index;
            GetCursorPos(&t.last);
            t.hook = SetWindowsHookExW(WH_MSGFILTER, &dock_host_access::menu_hook, nullptr, GetCurrentThreadId());

            POINT at{rects[index].left, rects[index].bottom};
            ClientToScreen(bar, &at);
            const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
                at.x, at.y, static_cast<HWND>(m_main), nullptr));

            if (t.hook != nullptr)
                UnhookWindowsHookEx(t.hook);

            const std::int32_t next = t.next;
            t = {};
            DestroyMenu(popup);
            m_menu_open = -1;
            m_menu_hot = -1;
            InvalidateRect(bar, nullptr, FALSE);

            if (chosen != 0)
            {
                // The popup is gone, so a handler may rebuild any menu.
                (void)run_command(chosen);
                return;
            }

            index = next;
        }
    }

    void dock_host::set_bar_action(const char* label, bar_action_fn handler, void* user) noexcept
    {
        std::snprintf(m_action_label, sizeof(m_action_label), "%s", label != nullptr ? label : "");
        m_action = label != nullptr ? handler : nullptr;
        m_action_user = user;
        m_action_hot = false;

        if (m_menubar != nullptr)
            InvalidateRect(static_cast<HWND>(m_menubar), nullptr, FALSE);
    }

    void dock_host::set_layouts_menu(context_menu* menu) noexcept
    {
        m_layouts_menu = menu;

        if (m_menubar != nullptr)
            InvalidateRect(static_cast<HWND>(m_menubar), nullptr, FALSE);
    }

    void dock_host::open_layouts_menu() noexcept
    {
        if (m_layouts_menu == nullptr || m_menubar == nullptr || m_menu_open >= 0 || m_layouts_open)
            return;

        const HWND bar = static_cast<HWND>(m_menubar);
        RECT client{};
        GetClientRect(bar, &client);
        const RECT button = dock_host_access::layouts_button_rect(client);

        m_command_count = 0;
        const HMENU popup = CreatePopupMenu();
        fill_menu(popup, *m_layouts_menu);
        m_layouts_open = true;
        InvalidateRect(bar, nullptr, FALSE);

        // The menu hangs from the button's right edge, so it stays inside the window.
        POINT at{button.right, button.bottom};
        ClientToScreen(bar, &at);
        const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
            at.x, at.y, static_cast<HWND>(m_main), nullptr));
        DestroyMenu(popup);
        m_layouts_open = false;
        m_layouts_hot = false;
        InvalidateRect(bar, nullptr, FALSE);

        // The popup is gone, so the handler may ask a question of its own, or change the menu.
        (void)run_command(chosen);
    }

    void dock_host::popup_menu(panel_id panel, context_menu& menu, std::int32_t x, std::int32_t y) noexcept
    {
        const HWND window = static_cast<HWND>(window_of(panel));

        if (window == nullptr)
            return;

        m_command_count = 0;
        const HMENU popup = CreatePopupMenu();
        fill_menu(popup, menu);
        POINT at{x, y};
        ClientToScreen(window, &at);
        const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY,
            at.x, at.y, window, nullptr));
        DestroyMenu(popup);
        (void)run_command(chosen);
    }

    void dock_host::move_root_window(root_id root, const dock_rect& screen) noexcept
    {
        const HWND top = static_cast<HWND>(m_root_top[root]);

        if (root == dock_model::k_main_root || top == nullptr || m_root_serial[root] != m_model.root_serial(root))
            return;

        RECT frame{screen.x - k_float_frame, screen.y - k_float_frame, screen.x + screen.width + k_float_frame,
            screen.y + screen.height + k_float_frame};
        clamp_to_work(frame);
        SetWindowPos(top, nullptr, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void dock_host::set_theme_mode(theme_mode mode) noexcept
    {
        g_theme_mode = mode;
        apply_theme();
    }

    // The window frames and menus belong to the system, so they are told about the theme instead of painting it.
    void dock_host::apply_theme() noexcept
    {
        using set_app_mode_fn = int(WINAPI*)(int);
        using flush_menu_fn = void(WINAPI*)();

        if (const HMODULE uxtheme = LoadLibraryW(L"uxtheme.dll"))
        {
            // Undocumented ordinals: SetPreferredAppMode and FlushMenuThemes. 2 forces dark menus, 3 forces light ones.
            const auto set_mode = reinterpret_cast<set_app_mode_fn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
            const auto flush = reinterpret_cast<flush_menu_fn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));

            if (set_mode != nullptr)
                set_mode(g_theme_mode == theme_mode::dark ? 2 : 3);

            if (flush != nullptr)
                flush();
        }

        const theme_colours& colours = theme();
        const BOOL dark = g_theme_mode == theme_mode::dark ? TRUE : FALSE;
        const COLORREF caption = colour_of(colours.title);
        const COLORREF text = colour_of(colours.text);
        const COLORREF border = colour_of(colours.splitter);

        for (root_id r = 0; r < dock_model::k_max_roots; ++r)
        {
            const HWND top = static_cast<HWND>(m_root_top[r]);

            if (top == nullptr)
                continue;

            const int square = 1;
            DwmSetWindowAttribute(top, 33, &square, sizeof(square));
            DwmSetWindowAttribute(top, 20, &dark, sizeof(dark));
            DwmSetWindowAttribute(top, 35, &caption, sizeof(caption));
            DwmSetWindowAttribute(top, 36, &text, sizeof(text));
            DwmSetWindowAttribute(top, 34, &border, sizeof(border));
            RedrawWindow(top, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
        }
    }

    void dock_host::set_window_title_suffix(const wchar_t* suffix) noexcept
    {
        wcsncpy_s(g_title_suffix, suffix != nullptr ? suffix : L"", _TRUNCATE);
    }

    std::int32_t dock_host::menu_bar_height() noexcept
    {
        return dock_model::scaled(26);
    }

    void dock_host::window_control(root_id root, std::int32_t which) noexcept
    {
        const HWND top = root < dock_model::k_max_roots ? static_cast<HWND>(m_root_top[root]) : nullptr;

        if (top == nullptr || root == dock_model::k_main_root)
            return;

        m_ctl_group = k_no_node;
        m_ctl_hot = -1;

        if (which == 2)
            close_root(root);
        else if (which == 1)
            ShowWindow(top, IsZoomed(top) ? SW_RESTORE : SW_MAXIMIZE);
        else
            ShowWindow(top, SW_MINIMIZE);
    }

    void dock_host::open_tab_list(node_id group) noexcept
    {
        const HWND window = static_cast<HWND>(m_group[group]);

        if (window == nullptr)
            return;

        const tab_geometry geometry = dock_host_access::geometry_for(*this, group);
        const HMENU popup = CreatePopupMenu();
        const panel_id active = m_model.active_tab(group);

        for (std::uint32_t i = 0; i < m_model.tab_count(group); ++i)
        {
            wchar_t title[64]{};
            widen(m_model.title_of(m_model.tab_at(group, i)), title, 64);
            AppendMenuW(popup, MF_STRING | (m_model.tab_at(group, i) == active ? MF_CHECKED : 0), k_menu_tab_base + i, title);
        }

        POINT at{geometry.list.right, geometry.list.bottom};
        ClientToScreen(window, &at);
        const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
            at.x, at.y, window, nullptr));
        DestroyMenu(popup);

        if (chosen < k_menu_tab_base || chosen >= k_menu_tab_base + m_model.tab_count(group))
            return;

        const panel_id panel = m_model.tab_at(group, chosen - k_menu_tab_base);
        m_model.set_active(group, panel);
        notify_visibility();
        fit_group(group);
        focus_panel(panel);
        InvalidateRect(window, nullptr, FALSE);
    }

    void dock_host::apply_dpi(std::uint32_t dpi) noexcept
    {
        m_dpi = std::max<std::uint32_t>(dpi, 96);
        dock_model::set_scale(static_cast<float>(m_dpi) / 96.0f);
        rebuild_font(m_dpi);
    }

    void dock_host::on_dpi_changed(std::uint32_t dpi, const void* suggested) noexcept
    {
        apply_dpi(dpi);
        const HWND main = static_cast<HWND>(m_main);

        if (suggested != nullptr)
        {
            const RECT& frame = *static_cast<const RECT*>(suggested);
            SetWindowPos(main, nullptr, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        m_resize_stamp = 0;
        layout_main();
        sync(false);
        RedrawWindow(main, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    void dock_host::set_default_layout(default_layout_fn build, void* user) noexcept
    {
        m_default_layout = build;
        m_default_user = user;
    }

    void dock_host::reset_layout() noexcept
    {
        m_model.reset();

        if (m_default_layout != nullptr)
            m_default_layout(m_default_user, m_model);

        sync(true);
    }

    // kdrn-layout 1
    // theme <0 dark, 1 light>
    // window <x> <y> <width> <height> <maximized>
    // then the model's text
    std::size_t dock_host::save_layout(char* out, std::size_t capacity) noexcept
    {
        refresh_all_areas();
        WINDOWPLACEMENT placement{sizeof(placement)};
        GetWindowPlacement(static_cast<HWND>(m_main), &placement);
        const RECT& frame = placement.rcNormalPosition;
        const int head = std::snprintf(out, capacity, "kdrn-layout 1\ntheme %u\nwindow %ld %ld %ld %ld %u\n",
            g_theme_mode == theme_mode::dark ? 0u : 1u, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top,
            placement.showCmd == SW_SHOWMAXIMIZED ? 1u : 0u);

        if (head < 0 || static_cast<std::size_t>(head) >= capacity)
            return 0;

        const std::size_t body = m_model.save(out + head, capacity - static_cast<std::size_t>(head));
        return body == 0 ? 0 : static_cast<std::size_t>(head) + body;
    }

    bool dock_host::restore_layout(const char* text, std::size_t size, bool live) noexcept
    {
        if (text == nullptr || size == 0)
            return false;

        const char* cursor = text;
        const char* end = text + size;
        char line[160];

        const auto next = [&]() noexcept {
            while (cursor < end && (*cursor == '\n' || *cursor == '\r'))
                ++cursor;

            std::size_t n = 0;

            while (cursor < end && *cursor != '\n' && *cursor != '\r')
            {
                if (n + 1 < sizeof(line))
                    line[n++] = *cursor;

                ++cursor;
            }

            line[n] = 0;
            return n > 0;
        };

        unsigned theme_value = 0;
        long x = 0;
        long y = 0;
        long width = 0;
        long height = 0;
        unsigned maximized = 0;

        if (!next() || std::strcmp(line, "kdrn-layout 1") != 0 || !next() || std::sscanf(line, "theme %u", &theme_value) != 1 ||
            !next() || std::sscanf(line, "window %ld %ld %ld %ld %u", &x, &y, &width, &height, &maximized) != 5)
            return false;

        if (!m_model.load(cursor, static_cast<std::size_t>(end - cursor)))
            return false;

        // A running editor takes the arrangement of the panels and leaves the theme and the window as they are.
        if (live)
        {
            sync(false);
            return true;
        }

        g_theme_mode = theme_value == 1 ? theme_mode::light : theme_mode::dark;
        apply_theme();

        if (width >= 320 && height >= 240)
        {
            RECT frame{x, y, x + width, y + height};
            clamp_to_work(frame);
            // The window is not shown yet. This only sets where it appears, and where it goes back to from maximized.
            WINDOWPLACEMENT placement{sizeof(placement)};
            placement.showCmd = SW_HIDE;
            placement.rcNormalPosition = frame;
            SetWindowPlacement(static_cast<HWND>(m_main), &placement);
        }

        m_start_maximized = maximized != 0;
        return true;
    }

    std::int32_t dock_host::tab_width_of(panel_id panel) noexcept
    {
        const HWND main = static_cast<HWND>(m_main);
        const HDC dc = GetDC(main);
        const HGDIOBJ previous = SelectObject(dc, g_font);
        wchar_t text[64]{};
        const int length = widen(m_model.title_of(panel), text, 64);
        SIZE size{};
        GetTextExtentPoint32W(dc, text, length, &size);
        SelectObject(dc, previous);
        ReleaseDC(main, dc);
        return tab_style::natural_width(size.cx, false, !m_model.is_permanent(panel));
    }

    std::int32_t dock_host::root_tabs_width(root_id root, const char** first_title) noexcept
    {
        std::int32_t total = 0;
        *first_title = nullptr;

        for (panel_id p = 0; p < m_model.panel_count(); ++p)
        {
            const node_id group = m_model.group_of(p);

            if (group == k_no_node || m_model.root_of(group) != root)
                continue;

            if (*first_title == nullptr)
                *first_title = m_model.title_of(p);

            total += tab_width_of(p);
        }

        return total;
    }

    void dock_host::begin_tab_drag(panel_id panel, node_id group) noexcept
    {
        if (group >= dock_model::k_max_nodes)
            return;

        m_strip[group] = {};
        m_strip[group].live = true;
        m_strip[group].skip = panel;
        m_strip[group].skip_w = static_cast<float>(tab_width_of(panel));
        start_timer();
    }

    void dock_host::set_tab_drop(node_id group, std::int32_t slot, std::int32_t width, const char* title) noexcept
    {
        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            tab_strip_anim& strip = m_strip[n];

            if (n == group)
            {
                strip.live = true;
                strip.slot = slot;
                strip.want = static_cast<float>(width);
                strip.title = title;
            }
            else if (strip.slot >= 0)
            {
                strip.slot = -1;
            }
        }

        start_timer();
    }

    void dock_host::end_tab_drag() noexcept
    {
        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            if (!m_strip[n].live)
                continue;

            m_strip[n] = {};

            if (m_group[n] != nullptr)
                InvalidateRect(static_cast<HWND>(m_group[n]), nullptr, FALSE);
        }
    }

    // Moves every strip a step toward its gaps. true while something is still moving.
    bool dock_host::step_strips() noexcept
    {
        const auto chase = [](float& value, float target) {
            const float delta = target - value;

            if (std::fabs(delta) < 0.75f)
            {
                const bool moved = value != target;
                value = target;
                return moved;
            }

            value += delta * 0.35f;
            return true;
        };

        bool busy = false;

        for (node_id n = 0; n < dock_model::k_max_nodes; ++n)
        {
            tab_strip_anim& strip = m_strip[n];

            if (!strip.live || m_group[n] == nullptr || !m_model.is_group(n))
                continue;

            bool moved = false;

            for (std::uint32_t b = 0; b <= dock_model::k_max_panels; ++b)
                moved = chase(strip.gap[b], static_cast<std::int32_t>(b) == strip.slot ? strip.want : 0.0f) || moved;

            if (strip.skip != k_no_panel)
                moved = chase(strip.skip_w, 0.0f) || moved;

            if (moved)
            {
                RECT client{};
                GetClientRect(static_cast<HWND>(m_group[n]), &client);
                client.bottom = std::min<LONG>(client.bottom, dock_model::tab_height());
                InvalidateRect(static_cast<HWND>(m_group[n]), &client, FALSE);
                busy = true;
            }
        }

        return busy;
    }

    std::int32_t dock_host::popup_menu(panel_id panel, const popup_row* rows, std::uint32_t count, std::int32_t x, std::int32_t y) noexcept
    {
        const HWND window = static_cast<HWND>(window_of(panel));

        if (window == nullptr || rows == nullptr || count == 0)
            return -1;

        const HMENU popup = CreatePopupMenu();

        for (std::uint32_t i = 0; i < count; ++i)
        {
            const popup_row& row = rows[i];

            if (row.separator)
            {
                AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
                continue;
            }

            // The text after a tab is the shortcut, which the menu draws against the right edge.
            wchar_t text[160]{};
            widen(row.text != nullptr ? row.text : "", text, 96);

            if (row.shortcut != nullptr)
            {
                const std::size_t length = std::wcslen(text);
                text[length] = L'	';
                widen(row.shortcut, text + length + 1, static_cast<int>(sizeof(text) / sizeof(text[0]) - length - 1));
            }

            AppendMenuW(popup, MF_STRING | (row.enabled ? 0 : MF_GRAYED) | (row.checked ? MF_CHECKED : 0), i + 1, text);
        }

        POINT at{x, y};
        ClientToScreen(window, &at);
        const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY,
            at.x, at.y, window, nullptr));
        DestroyMenu(popup);
        return chosen >= 1 && chosen <= count ? static_cast<std::int32_t>(chosen) - 1 : -1;
    }

    void dock_host::set_cursor(panel_id panel, mouse_cursor cursor) noexcept
    {
        if (window_of(panel) == nullptr || m_cursor[panel] == cursor)
            return;

        m_cursor[panel] = cursor;

        // The mouse is over the panel now, so the pointer changes without waiting for it to move.
        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(cursor == mouse_cursor::resize_horizontal ? 32644 : cursor == mouse_cursor::resize_vertical ? 32645 : 32512)));
    }

    void dock_host::set_tooltip(panel_id panel, const char* text, const dock_rect& area) noexcept
    {
        if (text == nullptr || *text == 0 || window_of(panel) == nullptr)
        {
            hide_tooltip(true);
            return;
        }

        if (panel == m_tip_panel && text == m_tip_text && area == m_tip_local)
            return;

        const bool showing = m_tip_window != nullptr && IsWindowVisible(static_cast<HWND>(m_tip_window));
        m_tip_panel = panel;
        m_tip_text = text;
        m_tip_local = area;

        // Moving from one control with a tooltip to the next shows it at once. Otherwise the mouse has to rest first.
        if (showing)
            show_tooltip();
        else if (m_root_host[0] != nullptr)
            SetTimer(static_cast<HWND>(m_root_host[0]), k_tooltip_timer, k_tooltip_delay_ms, nullptr);
    }

    void dock_host::hide_tooltip(bool forget) noexcept
    {
        if (m_root_host[0] != nullptr)
            KillTimer(static_cast<HWND>(m_root_host[0]), k_tooltip_timer);

        if (m_tip_window != nullptr)
            ShowWindow(static_cast<HWND>(m_tip_window), SW_HIDE);

        if (forget)
        {
            m_tip_text = nullptr;
            m_tip_panel = k_no_panel;
            m_tip_local = {};
        }
    }

    // Puts the tooltip under its control, or over it when there is no room below, and on the monitor.
    void dock_host::show_tooltip() noexcept
    {
        if (m_root_host[0] != nullptr)
            KillTimer(static_cast<HWND>(m_root_host[0]), k_tooltip_timer);

        if (m_tip_text == nullptr || window_of(m_tip_panel) == nullptr || (!is_strip(m_tip_panel) && !m_model.is_open(m_tip_panel)))
            return;

        const HWND content = static_cast<HWND>(window_of(m_tip_panel));

        if (!IsWindowVisible(content))
            return;

        if (m_tip_window == nullptr)
        {
            m_tip_window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, k_tooltip_class, L"",
                WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, static_cast<HINSTANCE>(m_instance), this);

            if (m_tip_window == nullptr)
                return;
        }

        // The size the text needs, at most 300 pixels wide.
        wchar_t text[256]{};
        widen(m_tip_text, text, 256);
        const HDC dc = GetDC(static_cast<HWND>(m_tip_window));
        const HGDIOBJ previous = SelectObject(dc, g_font);
        RECT measure{0, 0, px(300), 0};
        DrawTextW(dc, text, -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_LEFT);
        SelectObject(dc, previous);
        ReleaseDC(static_cast<HWND>(m_tip_window), dc);
        const int width = measure.right - measure.left + 2 * px(8) + 2;
        const int height = measure.bottom - measure.top + 2 * px(5) + 2;

        POINT origin{m_tip_local.x, m_tip_local.y};
        ClientToScreen(content, &origin);
        RECT anchor{origin.x, origin.y, origin.x + m_tip_local.width, origin.y + m_tip_local.height};
        const HMONITOR monitor = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        RECT work{0, 0, 1920, 1080};

        if (monitor != nullptr && GetMonitorInfoW(monitor, &info))
            work = info.rcWork;

        int x = std::clamp<int>(anchor.left, work.left, std::max<int>(work.right - width, work.left));
        int y = anchor.bottom + px(6);

        if (y + height > work.bottom)
            y = anchor.top - height - px(6);

        SetWindowPos(static_cast<HWND>(m_tip_window), HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(static_cast<HWND>(m_tip_window), nullptr, FALSE);
    }

    panel_id dock_host::panel_at(std::int32_t x, std::int32_t y, std::int32_t& local_x, std::int32_t& local_y) const noexcept
    {
        const POINT screen{x, y};
        const HWND under = WindowFromPoint(screen);

        if (under == nullptr)
            return k_no_panel;

        for (panel_id id = 0; id < k_slots; ++id)
        {
            const HWND window = static_cast<HWND>(window_of(id));

            if (window == nullptr || !IsWindowVisible(window) || (under != window && !IsChild(window, under)))
                continue;

            POINT local = screen;
            ScreenToClient(window, &local);
            local_x = local.x;
            local_y = local.y;
            return id;
        }

        return k_no_panel;
    }

    bool dock_host::begin_drag(const drag_payload& payload) noexcept
    {
        if (m_dnd.active || m_main == nullptr || payload.allowed == 0)
            return false;

        if (m_ghost == nullptr)
        {
            m_ghost = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, k_ghost_class, L"",
                WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, static_cast<HINSTANCE>(m_instance), this);

            if (m_ghost == nullptr)
                return false;

            // The face is drawn on a nearly black key colour that is cut away, and the rest is a little see-through.
            SetLayeredWindowAttributes(static_cast<HWND>(m_ghost), 0x010101, 225, LWA_COLORKEY | LWA_ALPHA);
        }

        m_dnd = {};
        m_dnd.payload = payload;
        m_dnd.active = true;
        m_dnd.capture = GetCapture();
        m_dnd.stamp = GetTickCount64();
        hide_tooltip(false);

        // The size of the ghost: its own, or what the label, picture, badge and mark need.
        int width = payload.ghost_width;
        int height = payload.ghost_height;

        if (payload.ghost == nullptr || width <= 0 || height <= 0)
        {
            const HDC dc = GetDC(static_cast<HWND>(m_ghost));
            const HGDIOBJ previous = SelectObject(dc, g_font);
            wchar_t text[drag_payload::k_label_size]{};
            const int length = widen(payload.label, text, static_cast<int>(drag_payload::k_label_size));
            SIZE size{};
            GetTextExtentPoint32W(dc, text, length, &size);
            char number[16];
            std::snprintf(number, sizeof(number), "%u", payload.count);
            SIZE badge{};
            GetTextExtentPoint32A(dc, number, static_cast<int>(std::strlen(number)), &badge);
            SelectObject(dc, previous);
            ReleaseDC(static_cast<HWND>(m_ghost), dc);
            width = px(10) + (payload.icon != icon_kind::none ? px(20) : 0) + size.cx + (payload.count > 1 ? px(6) + badge.cx + px(10) : 0) + px(6) + px(14) + px(8);
            width = std::min(width, px(320));
            height = px(28);
        }

        m_dnd.ghost_w = width;
        m_dnd.ghost_h = height;
        POINT cursor{};
        GetCursorPos(&cursor);
        m_dnd.x = cursor.x;
        m_dnd.y = cursor.y;
        SetWindowPos(static_cast<HWND>(m_ghost), HWND_TOPMOST, cursor.x + px(14), cursor.y + px(14), width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        start_timer();
        drag_update(cursor.x, cursor.y);
        return true;
    }

    void dock_host::drag_update(std::int32_t x, std::int32_t y) noexcept
    {
        if (!m_dnd.active)
            return;

        m_dnd.x = x;
        m_dnd.y = y;
        m_dnd.stamp = GetTickCount64();

        if (m_ghost != nullptr && !m_dnd.external)
            SetWindowPos(static_cast<HWND>(m_ghost), nullptr, x + px(14), y + px(14), m_dnd.ghost_w, m_dnd.ghost_h, SWP_NOACTIVATE | SWP_NOZORDER);

        if (!m_dnd.external)
        {
            m_dnd.ctrl = GetKeyState(VK_CONTROL) < 0;
            m_dnd.shift = GetKeyState(VK_SHIFT) < 0;
            m_dnd.alt = GetKeyState(VK_MENU) < 0;
        }

        drag_event event;
        event.payload = &m_dnd.payload;
        event.ctrl = m_dnd.ctrl;
        event.shift = m_dnd.shift;
        event.alt = m_dnd.alt;
        event.wanted = drag_drop::wanted_effect(m_dnd.payload, event.ctrl, event.shift, event.alt);

        std::int32_t local_x = 0;
        std::int32_t local_y = 0;
        const panel_id panel = panel_at(x, y, local_x, local_y);
        spring_tabs(x, y);

        if (panel != m_dnd.over)
        {
            if (m_dnd.over != k_no_panel && m_content[m_dnd.over] != nullptr)
                m_content[m_dnd.over]->on_drag_leave();

            m_dnd.over = panel;
        }

        event.x = local_x;
        event.y = local_y;
        drop_effect effect = panel != k_no_panel && m_content[panel] != nullptr ? m_content[panel]->on_drag_over(event) : drop_effect::none;

        // A place can only answer with what the source allows.
        if (effect != drop_effect::none && !m_dnd.payload.allows(effect))
            effect = drop_effect::none;

        m_dnd.effect = effect;

        if (effect != m_dnd.shown)
        {
            m_dnd.shown = effect;

            if (m_ghost != nullptr)
                InvalidateRect(static_cast<HWND>(m_ghost), nullptr, FALSE);
        }

        // The pointer follows the answer at once, without waiting for the next move message. Files from outside have the pointer of
        // the program they come from, which follows what is returned to it.
        if (!m_dnd.external)
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(effect == drop_effect::none ? 32648 : 32512)));
    }

    void dock_host::spring_tabs(std::int32_t x, std::int32_t y) noexcept
    {
        constexpr std::uint64_t k_rest = 600;
        const POINT screen{x, y};
        const HWND under = WindowFromPoint(screen);
        panel_id rest = k_no_panel;
        node_id rest_group = k_no_node;

        for (node_id n = 0; n < dock_model::k_max_nodes && under != nullptr; ++n)
        {
            if (!m_model.is_group(n) || m_group[n] == nullptr || static_cast<HWND>(m_group[n]) != under)
                continue;

            POINT local = screen;
            ScreenToClient(under, &local);
            const tab_geometry geometry = dock_host_access::geometry_for(*this, n, false);
            const std::uint32_t index = tab_at_point(geometry, local.x, local.y);

            if (index != k_tab_end && geometry.panel[index] != m_model.active_tab(n))
            {
                rest = geometry.panel[index];
                rest_group = n;
            }

            break;
        }

        const std::uint64_t now = GetTickCount64();

        if (rest != m_dnd.spring_panel)
        {
            m_dnd.spring_panel = rest;
            m_dnd.spring_since = now;
            return;
        }

        if (rest == k_no_panel || now - m_dnd.spring_since < k_rest)
            return;

        m_model.set_active(rest_group, rest);
        notify_visibility();
        fit_group(rest_group);

        if (m_group[rest_group] != nullptr)
            InvalidateRect(static_cast<HWND>(m_group[rest_group]), nullptr, FALSE);

        m_dnd.spring_panel = k_no_panel;
    }

    drop_effect dock_host::drag_finish(bool drop) noexcept
    {
        if (!m_dnd.active)
            return drop_effect::none;

        drop_effect result = drop_effect::none;
        const panel_id over = m_dnd.over;

        if (over != k_no_panel && m_content[over] != nullptr)
        {
            if (drop && m_dnd.effect != drop_effect::none)
            {
                std::int32_t local_x = 0;
                std::int32_t local_y = 0;

                if (panel_at(m_dnd.x, m_dnd.y, local_x, local_y) == over)
                {
                    drag_event event;
                    event.payload = &m_dnd.payload;
                    event.x = local_x;
                    event.y = local_y;
                    event.ctrl = m_dnd.ctrl;
                    event.shift = m_dnd.shift;
                    event.alt = m_dnd.alt;
                    event.wanted = drag_drop::wanted_effect(m_dnd.payload, event.ctrl, event.shift, event.alt);
                    result = m_content[over]->on_drop(event);

                    if (result != drop_effect::none && !m_dnd.payload.allows(result))
                        result = drop_effect::none;
                }
            }
            else
            {
                m_content[over]->on_drag_leave();
            }
        }

        if (m_ghost != nullptr)
            ShowWindow(static_cast<HWND>(m_ghost), SW_HIDE);

        const drag_done_fn done = m_dnd.payload.done;
        void* const user = m_dnd.payload.done_user;
        const HWND capture = static_cast<HWND>(m_dnd.capture);
        m_dnd = {};

        if (capture != nullptr && GetCapture() == capture)
            ReleaseCapture();

        if (done != nullptr)
            done(user, result);

        return result;
    }

    drop_effect dock_host::file_drag_over(const wchar_t* files, std::int32_t x, std::int32_t y, std::uint32_t keys) noexcept
    {
        if (m_main == nullptr || files == nullptr)
            return drop_effect::none;

        // A drag of the program's own has the pointer to itself.
        if (m_dnd.active && !m_dnd.external)
            return drop_effect::none;

        if (!m_dnd.active)
        {
            std::size_t used = 0;
            std::uint32_t count = 0;

            while (files[used] != L'\0')
            {
                const std::size_t length = std::wcslen(files + used);

                if (used + length + 2 > k_file_capacity)
                    break;

                std::memcpy(m_files + used, files + used, (length + 1) * sizeof(wchar_t));
                used += length + 1;
                ++count;
            }

            m_files[used] = L'\0';

            if (count == 0)
                return drop_effect::none;

            // The label is the name of the first file, cut short.
            const wchar_t* name = m_files;

            for (const wchar_t* at = m_files; *at != L'\0'; ++at)
            {
                if (*at == L'\\' || *at == L'/')
                    name = at + 1;
            }

            char label[drag_payload::k_label_size]{};
            const int wide = static_cast<int>(std::min<std::size_t>(std::wcslen(name), 20));
            const int written = WideCharToMultiByte(CP_UTF8, 0, name, wide, label, static_cast<int>(sizeof(label)) - 1, nullptr, nullptr);
            label[written > 0 ? written : 0] = '\0';

            drag_payload payload;
            payload.set_kind(k_files_kind).set_label(label).set_count(count).set_icon(icon_kind::stack).set_allowed(effects::copy | effects::link);
            payload.files = m_files;
            m_dnd = {};
            m_dnd.payload = payload;
            m_dnd.active = true;
            m_dnd.external = true;
            m_dnd.stamp = GetTickCount64();
            hide_tooltip(false);
            start_timer();
        }

        m_dnd.ctrl = (keys & MK_CONTROL) != 0;
        m_dnd.shift = (keys & MK_SHIFT) != 0;
        m_dnd.alt = (keys & 0x20) != 0;
        drag_update(x, y);
        return m_dnd.effect;
    }

    void dock_host::file_drag_leave() noexcept
    {
        if (m_dnd.active && m_dnd.external)
            drag_finish(false);
    }

    drop_effect dock_host::file_drag_drop(std::int32_t x, std::int32_t y, std::uint32_t keys) noexcept
    {
        if (!m_dnd.active || !m_dnd.external)
            return drop_effect::none;

        m_dnd.ctrl = (keys & MK_CONTROL) != 0;
        m_dnd.shift = (keys & MK_SHIFT) != 0;
        m_dnd.alt = (keys & 0x20) != 0;
        drag_update(x, y);
        return drag_finish(true);
    }

    void dock_host::cancel_drag() noexcept
    {
        drag_finish(false);
    }
}
