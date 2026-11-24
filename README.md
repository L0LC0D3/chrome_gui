# chrome_gui
Flexible editor gui framework that supports docking and works via win32 api natively.

Native Win32 and GDI+ controls, docking host with floating windows, menus, toolbars, dialogs and toasts. No dependencies beyond the standard library and the Windows SDK.

## Use
```cmake
add_subdirectory(chrome_gui)
target_link_libraries(my_app PRIVATE chrome::gui)
```
Headers are included as `<chrome/controls/core/button.hpp>`; everything lives in namespace `chrome`.

- `chrome/`: the library (`chrome_gui`, static).
- `demo/`: demo panels (`chrome_gui_demo_panels`, static) and a small demo app (`chrome_gui_demo`, built when this is the top-level project).

Window classes are registered as `chrome_*`. `dock_host::set_window_title_suffix` names floating windows after your application, and an icon with resource id 1 is used for dialogs when present.
