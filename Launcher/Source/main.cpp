#include "Engine/Core/CrashDiagnostics.h"
#include "Engine/Core/Logger.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
        explicit Process(
            const std::filesystem::path& executable,
            std::wstring_view arguments = {})
        {
            std::wstring commandLine =
                L"\"" + executable.wstring() + L"\"";
            if (!arguments.empty())
            {
                commandLine += L" ";
                commandLine += arguments;
            }
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

        [[nodiscard]] DWORD Wait() const
        {
            WaitForSingleObject(processInfo_.hProcess, INFINITE);
            DWORD exitCode = 1;
            GetExitCodeProcess(processInfo_.hProcess, &exitCode);
            return exitCode;
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

        void FocusWindow() const
        {
            EnumWindows(
                [](HWND window, LPARAM parameter) -> BOOL
                {
                    DWORD windowProcessId = 0;
                    GetWindowThreadProcessId(window, &windowProcessId);
                    if (windowProcessId ==
                        static_cast<DWORD>(parameter))
                    {
                        ShowWindow(window, SW_RESTORE);
                        SetForegroundWindow(window);
                        return FALSE;
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
        Engine::CrashDiagnostics::Install(
            executableDirectory / "Crashes",
            "Launcher");
        {
            std::error_code error;
            std::filesystem::remove(
                executableDirectory / "AIRecovery.prompt",
                error);
        }
#if !defined(NDEBUG)
        if (argc > 1 &&
            std::string_view(argv[1]) == "--test-crash-diagnostics")
        {
#if defined(_WIN32)
            RaiseException(0xE0424242, 0, 0, nullptr);
#else
            std::abort();
#endif
        }
#endif
        Engine::Logger::Info("Launcher started.");

#if defined(_WIN32)
        Process assistantHost(executableDirectory / "AssistantHost.exe");
        constexpr DWORD RecoveryRelaunchExitCode = 42;
        bool keepRunning = true;

        while (keepRunning)
        {
            Process gameHost(executableDirectory / "GameHost.exe");
            const DWORD exitCode = gameHost.Wait();

            Engine::Logger::Info(
                "GameHost exited with code " +
                std::to_string(exitCode) + ".");

            if (exitCode == 0)
            {
                keepRunning = false;
                break;
            }

            const int choice = MessageBoxW(
                nullptr,
                L"The Game crashed.\n\n"
                L"Crash reports and logs were preserved. The Game will not "
                L"restart automatically.\n\n"
                L"Would you like the AI Assistant to inspect the diagnostics, "
                L"repair the Game, build it, and launch the repaired version?\n\n"
                L"Choose No to relaunch the Game immediately without a repair.",
                L"Game crash detected",
                MB_ICONERROR | MB_YESNO | MB_DEFBUTTON1 |
                    MB_SETFOREGROUND);
            if (choice != IDYES)
            {
                Engine::Logger::Warning(
                    "User declined AI repair. Relaunching GameHost unchanged.");
                continue;
            }

            Engine::Logger::Warning(
                "Starting GameHost in AI crash recovery mode.");
            Process recoveryHost(
                executableDirectory / "GameHost.exe",
                L"--recovery");
            assistantHost.FocusWindow();
            const DWORD recoveryExitCode = recoveryHost.Wait();
            if (recoveryExitCode != RecoveryRelaunchExitCode)
            {
                Engine::Logger::Warning(
                    "Crash recovery ended without a relaunch request.");
                keepRunning = false;
                break;
            }

            Engine::Logger::Info(
                "AI recovery completed. Launching the repaired Game.");
        }

        assistantHost.RequestWindowClose();
        static_cast<void>(assistantHost.Wait());
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
