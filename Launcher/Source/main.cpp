#include "Engine/Core/Logger.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace
{
#if defined(_WIN32)
    class Process
    {
    public:
        explicit Process(const std::filesystem::path& executable)
        {
            std::wstring commandLine =
                L"\"" + executable.wstring() + L"\"";
            STARTUPINFOW startupInfo{};
            startupInfo.cb = sizeof(startupInfo);

            if (!CreateProcessW(
                    executable.c_str(),
                    commandLine.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    0,
                    nullptr,
                    executable.parent_path().c_str(),
                    &startupInfo,
                    &processInfo_))
            {
                throw std::runtime_error(
                    "Failed to start " + executable.filename().string());
            }

            Engine::Logger::Info(
                "Started process " + executable.filename().string() +
                " (PID " + std::to_string(processInfo_.dwProcessId) + ").");
        }

        ~Process()
        {
            if (processInfo_.hThread != nullptr)
            {
                CloseHandle(processInfo_.hThread);
            }
            if (processInfo_.hProcess != nullptr)
            {
                CloseHandle(processInfo_.hProcess);
            }
        }

        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;

        void Wait() const
        {
            WaitForSingleObject(processInfo_.hProcess, INFINITE);
        }

        void RequestWindowClose() const
        {
            EnumWindows(
                [](HWND window, LPARAM parameter) -> BOOL
                {
                    const DWORD targetProcessId =
                        static_cast<DWORD>(parameter);
                    DWORD windowProcessId = 0;
                    GetWindowThreadProcessId(window, &windowProcessId);
                    if (windowProcessId == targetProcessId)
                    {
                        PostMessageW(window, WM_CLOSE, 0, 0);
                    }
                    return TRUE;
                },
                static_cast<LPARAM>(processInfo_.dwProcessId));
        }

    private:
        PROCESS_INFORMATION processInfo_{};
    };
#endif
}

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{})
                .parent_path();
        Engine::Logger::Initialize(
            executableDirectory / "Logs" / "Launcher.log");
        Engine::Logger::Info("Launcher started.");

#if defined(_WIN32)
        Process gameHost(executableDirectory / "GameHost.exe");
        Process assistantHost(executableDirectory / "AssistantHost.exe");

        gameHost.Wait();
        Engine::Logger::Info("GameHost exited.");
        assistantHost.RequestWindowClose();
        assistantHost.Wait();
        Engine::Logger::Info("AssistantHost exited.");
#else
        throw std::runtime_error(
            "Multi-process launcher is currently implemented for Windows only.");
#endif
    }
    catch (const std::exception& exception)
    {
        Engine::Logger::Error(exception.what());
        std::cerr << "Launcher fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
