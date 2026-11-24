#include <chrome/controls/core/clipboard.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstring>

namespace chrome::clipboard
{
    bool set(const wchar_t* text, std::int32_t length) noexcept
    {
        if (text == nullptr || length < 0 || !OpenClipboard(nullptr))
            return false;

        bool done = false;
        const SIZE_T bytes = (static_cast<SIZE_T>(length) + 1) * sizeof(wchar_t);
        const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);

        if (memory != nullptr)
        {
            wchar_t* target = static_cast<wchar_t*>(GlobalLock(memory));

            if (target != nullptr)
            {
                std::memcpy(target, text, static_cast<std::size_t>(length) * sizeof(wchar_t));
                target[length] = L'\0';
                GlobalUnlock(memory);
                EmptyClipboard();

                // The clipboard owns the memory once it has been handed over.
                done = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }

            if (!done)
                GlobalFree(memory);
        }

        CloseClipboard();
        return done;
    }

    bool get(wchar_t* out, std::int32_t capacity, std::int32_t& length) noexcept
    {
        length = 0;

        if (out == nullptr || capacity <= 0 || !IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(nullptr))
            return false;

        bool done = false;
        const HANDLE data = GetClipboardData(CF_UNICODETEXT);

        if (data != nullptr)
        {
            const wchar_t* source = static_cast<const wchar_t*>(GlobalLock(data));

            if (source != nullptr)
            {
                std::int32_t count = 0;

                while (source[count] != L'\0' && count < capacity - 1)
                {
                    out[count] = source[count];
                    ++count;
                }

                out[count] = L'\0';
                length = count;
                done = true;
                GlobalUnlock(data);
            }
        }

        CloseClipboard();
        return done;
    }
}
