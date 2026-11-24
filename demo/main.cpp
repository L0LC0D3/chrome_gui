// A small application that shows the library: a dock host with the two demo panels.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrome/app_icon.hpp>
#include <chrome/dock_host.hpp>
#include <chrome/toast.hpp>
#include <demo/controls_panel.hpp>
#include <demo/data_controls_panel.hpp>

namespace
{
    constexpr wchar_t k_main_class[] = L"chrome_demo_main";

    struct demo_state
    {
        chrome::dock_host host;
        chrome::controls_panel controls;
        chrome::data_controls_panel data_controls;
        chrome::panel_id controls_id{chrome::k_no_panel};
        chrome::panel_id data_id{chrome::k_no_panel};
    };

    demo_state g_demo;

    void default_layout(void*, chrome::dock_model& model) noexcept
    {
        // Both panels share the main root as tabs.
        const chrome::node_id group = model.dock_tabs(g_demo.data_id, chrome::dock_model::k_main_root, chrome::k_no_node)
            ? model.group_of(g_demo.data_id)
            : chrome::k_no_node;
        (void)model.dock_tabs(g_demo.controls_id, chrome::dock_model::k_main_root, group);
    }

    LRESULT CALLBACK main_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        switch (message)
        {
        case WM_SIZE:
            g_demo.host.layout_main();
            return 0;
        case WM_SETFOCUS:
            g_demo.host.focus_root(chrome::dock_model::k_main_root);
            return 0;
        case WM_DPICHANGED:
            g_demo.host.on_dpi_changed(HIWORD(wparam), reinterpret_cast<const RECT*>(lparam));
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (g_demo.host.dispatch_key(static_cast<std::uint32_t>(wparam), GetKeyState(VK_CONTROL) < 0,
                    GetKeyState(VK_SHIFT) < 0, GetKeyState(VK_MENU) < 0))
                return 0;

            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    WNDCLASSEXW main_class{sizeof(main_class)};
    main_class.lpfnWndProc = main_proc;
    main_class.hInstance = instance;
    main_class.hIcon = chrome::app_icon_big();
    main_class.hIconSm = chrome::app_icon_small();
    main_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    main_class.lpszClassName = k_main_class;

    if (RegisterClassExW(&main_class) == 0 || !chrome::dock_host::register_classes(instance))
        return 1;

    chrome::dock_host::set_window_title_suffix(L" - chrome_gui demo");
    HWND main = CreateWindowExW(0, k_main_class, L"chrome_gui demo", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800, nullptr, nullptr, instance, nullptr);
    demo_state& d = g_demo;

    if (main == nullptr || !d.host.create(main, instance))
        return 1;

    d.controls_id = d.host.add_panel("Controls", &d.controls, false);
    d.controls.bind(d.host, d.controls_id);
    d.data_id = d.host.add_panel("Data Controls", &d.data_controls, false);
    d.data_controls.bind(d.host, d.data_id);

    if (d.controls_id == chrome::k_no_panel || d.data_id == chrome::k_no_panel)
        return 1;

    chrome::toast::install(main);
    d.host.set_default_layout(&default_layout, nullptr);
    d.host.reset_layout();
    d.host.layout_main();
    d.host.sync(false);
    ShowWindow(main, SW_SHOW);
    UpdateWindow(main);

    MSG message;

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
