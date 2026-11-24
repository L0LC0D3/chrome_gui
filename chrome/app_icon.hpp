#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace chrome
{
    // The icon of the application, from the resources of the executable (a resource with id 1), in the size asked for. Windows picks the
    // nearest picture inside the icon file, so a window class asks for the big and the small size the system wants, and the About
    // dialog for the size it draws. Null when the executable has no icon, as with a test program.
    [[nodiscard]] inline HICON app_icon(int width, int height) noexcept
    {
        return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, width, height, LR_SHARED));
    }

    [[nodiscard]] inline HICON app_icon_big() noexcept
    {
        return app_icon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    [[nodiscard]] inline HICON app_icon_small() noexcept
    {
        return app_icon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    }
}
