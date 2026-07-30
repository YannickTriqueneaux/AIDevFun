#include "Engine/Platform/WindowFocus.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <TlHelp32.h>

    #include <cwchar>
    #include <string>
#endif

namespace
{
#if defined(_WIN32)
    struct WindowSearch
    {
        DWORD processId = 0;
        HWND window = nullptr;
    };

    BOOL CALLBACK FindProcessWindow(HWND window, LPARAM parameter)
    {
        auto& search = *reinterpret_cast<WindowSearch*>(parameter);

        DWORD windowProcessId = 0;
        GetWindowThreadProcessId(window, &windowProcessId);
        if (windowProcessId == search.processId &&
            IsWindowVisible(window) &&
            GetWindow(window, GW_OWNER) == nullptr)
        {
            search.window = window;
            return FALSE;
        }

        return TRUE;
    }

    std::wstring ToWide(std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size);
        return result;
    }
#endif
}

namespace Engine
{
    bool WindowFocus::FocusProcessWindow(
        std::string_view processExecutableName)
    {
#if defined(_WIN32)
        const std::wstring expectedName = ToWide(processExecutableName);
        const HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD targetProcessId = 0;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, expectedName.c_str()) == 0)
                {
                    targetProcessId = entry.th32ProcessID;
                    break;
                }
            }
            while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);

        if (targetProcessId == 0)
        {
            return false;
        }

        WindowSearch search{.processId = targetProcessId};
        EnumWindows(FindProcessWindow, reinterpret_cast<LPARAM>(&search));
        if (search.window == nullptr)
        {
            return false;
        }

        if (IsIconic(search.window))
        {
            ShowWindow(search.window, SW_RESTORE);
        }

        return SetForegroundWindow(search.window) != FALSE;
#else
        static_cast<void>(processExecutableName);
        return false;
#endif
    }
}

