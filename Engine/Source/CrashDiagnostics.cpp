#include "Engine/Core/CrashDiagnostics.h"

#if defined(_WIN32) && defined(ENGINE_CRASH_DIAGNOSTICS)

    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <DbgHelp.h>
    #include <crtdbg.h>

    #include <atomic>
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
    #include <exception>
    #include <string>

namespace
{
    std::wstring OutputDirectory;
    std::wstring ProcessName;
    std::atomic_flag WritingCrash = ATOMIC_FLAG_INIT;
    std::atomic_uint Sequence = 0;
    bool SymbolsInitialized = false;

    std::wstring BuildBasePath()
    {
        SYSTEMTIME time{};
        GetLocalTime(&time);

        wchar_t name[256]{};
        swprintf_s(
            name,
            L"%s_%04u%02u%02u_%02u%02u%02u_%lu_%u",
            ProcessName.c_str(),
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond,
            GetCurrentProcessId(),
            Sequence.fetch_add(1));
        return OutputDirectory + L"\\" + name;
    }

    void WriteText(HANDLE file, const char* text)
    {
        DWORD written = 0;
        WriteFile(
            file,
            text,
            static_cast<DWORD>(strlen(text)),
            &written,
            nullptr);
    }

    USHORT CaptureCrashFrames(
        EXCEPTION_POINTERS* exceptionPointers,
        void** frames,
        USHORT capacity)
    {
        if (!exceptionPointers || !exceptionPointers->ContextRecord)
        {
            return CaptureStackBackTrace(1, capacity, frames, nullptr);
        }

        CONTEXT context = *exceptionPointers->ContextRecord;
        STACKFRAME64 frame{};
        DWORD machineType = 0;
#if defined(_M_X64)
        machineType = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = context.Rip;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
        machineType = IMAGE_FILE_MACHINE_I386;
        frame.AddrPC.Offset = context.Eip;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrStack.Offset = context.Esp;
#else
        return CaptureStackBackTrace(1, capacity, frames, nullptr);
#endif
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        USHORT count = 0;
        if (frame.AddrPC.Offset != 0 && count < capacity)
        {
            frames[count++] =
                reinterpret_cast<void*>(frame.AddrPC.Offset);
        }
        const HANDLE process = GetCurrentProcess();
        const HANDLE thread = GetCurrentThread();
        while (count < capacity &&
               StackWalk64(
                   machineType,
                   process,
                   thread,
                   &frame,
                   &context,
                   nullptr,
                   SymFunctionTableAccess64,
                   SymGetModuleBase64,
                   nullptr))
        {
            if (frame.AddrPC.Offset == 0)
            {
                break;
            }
            void* address =
                reinterpret_cast<void*>(frame.AddrPC.Offset);
            if (count == 0 || frames[count - 1] != address)
            {
                frames[count++] = address;
            }
        }
        return count;
    }

    void WriteSymbolizedFrame(
        HANDLE file,
        USHORT index,
        void* frameAddress)
    {
        const HANDLE process = GetCurrentProcess();
        const DWORD64 address =
            reinterpret_cast<DWORD64>(frameAddress);

        char symbolStorage[
            sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)]{};
        auto* symbol =
            reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 symbolDisplacement = 0;
        const bool hasSymbol =
            SymbolsInitialized &&
            SymFromAddr(
                process,
                address,
                &symbolDisplacement,
                symbol) != FALSE;

        IMAGEHLP_MODULE64 module{};
        module.SizeOfStruct = sizeof(module);
        const bool hasModule =
            SymbolsInitialized &&
            SymGetModuleInfo64(process, address, &module) != FALSE;

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        const bool hasLine =
            SymbolsInitialized &&
            SymGetLineFromAddr64(
                process,
                address,
                &lineDisplacement,
                &line) != FALSE;

        char output[2048]{};
        int length = sprintf_s(
            output,
            "  #%02u %p %s%s%s",
            index,
            frameAddress,
            hasModule && module.ModuleName[0] != '\0'
                ? module.ModuleName
                : "<unknown-module>",
            hasSymbol ? "!" : "",
            hasSymbol ? symbol->Name : "");
        if (hasSymbol && symbolDisplacement != 0)
        {
            length += sprintf_s(
                output + length,
                sizeof(output) - length,
                "+0x%llX",
                symbolDisplacement);
        }
        if (hasLine && line.FileName)
        {
            length += sprintf_s(
                output + length,
                sizeof(output) - length,
                " (%s:%lu",
                line.FileName,
                line.LineNumber);
            if (lineDisplacement != 0)
            {
                length += sprintf_s(
                    output + length,
                    sizeof(output) - length,
                    "+0x%lX",
                    lineDisplacement);
            }
            length += sprintf_s(
                output + length,
                sizeof(output) - length,
                ")");
        }
        sprintf_s(
            output + length,
            sizeof(output) - length,
            "\r\n");
        WriteText(file, output);
    }

    void WriteTextReport(
        const std::wstring& path,
        const char* reason,
        EXCEPTION_POINTERS* exceptionPointers)
    {
        const HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }

        if (SymbolsInitialized)
        {
            SymRefreshModuleList(GetCurrentProcess());
        }

        char report[1024]{};
        const DWORD exceptionCode =
            exceptionPointers && exceptionPointers->ExceptionRecord
                ? exceptionPointers->ExceptionRecord->ExceptionCode
                : 0;
        const void* exceptionAddress =
            exceptionPointers && exceptionPointers->ExceptionRecord
                ? exceptionPointers->ExceptionRecord->ExceptionAddress
                : nullptr;

        void* frames[48]{};
        const USHORT frameCount = CaptureCrashFrames(
            exceptionPointers,
            frames,
            48);
        sprintf_s(
            report,
            "process=%ls\r\npid=%lu\r\nthread=%lu\r\n"
            "reason=%s\r\nexception_code=0x%08lX\r\n"
            "exception_address=%p\r\n"
            "callstack=\r\n",
            ProcessName.c_str(),
            GetCurrentProcessId(),
            GetCurrentThreadId(),
            reason,
            exceptionCode,
            exceptionAddress);
        WriteText(file, report);
        for (USHORT index = 0; index < frameCount; ++index)
        {
            WriteSymbolizedFrame(file, index, frames[index]);
        }
        CloseHandle(file);
    }

    void WriteMiniDump(
        const std::wstring& path,
        EXCEPTION_POINTERS* exceptionPointers)
    {
        const HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION exceptionInformation{
            GetCurrentThreadId(),
            exceptionPointers,
            FALSE
        };
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            file,
            static_cast<MINIDUMP_TYPE>(
                MiniDumpNormal | MiniDumpWithThreadInfo),
            exceptionPointers ? &exceptionInformation : nullptr,
            nullptr,
            nullptr);
        CloseHandle(file);
    }

    void RecordCrash(
        const char* reason,
        EXCEPTION_POINTERS* exceptionPointers = nullptr)
    {
        if (WritingCrash.test_and_set())
        {
            return;
        }

        const std::wstring basePath = BuildBasePath();
        WriteTextReport(basePath + L".txt", reason, exceptionPointers);
        WriteMiniDump(basePath + L".dmp", exceptionPointers);
        WritingCrash.clear();
    }

    LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exceptionPointers)
    {
        RecordCrash("unhandled_exception", exceptionPointers);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void OnTerminate()
    {
        RecordCrash("std_terminate");
        TerminateProcess(GetCurrentProcess(), 3);
    }

    void OnPureCall()
    {
        RecordCrash("pure_virtual_call");
        TerminateProcess(GetCurrentProcess(), 3);
    }

    void OnInvalidParameter(
        const wchar_t*,
        const wchar_t*,
        const wchar_t*,
        unsigned int,
        uintptr_t)
    {
        RecordCrash("invalid_parameter");
        TerminateProcess(GetCurrentProcess(), 3);
    }

    int OnCrtReport(int reportType, wchar_t*, int*)
    {
        if (reportType == _CRT_ASSERT || reportType == _CRT_ERROR)
        {
            RecordCrash(
                reportType == _CRT_ASSERT
                    ? "crt_assertion"
                    : "crt_error");
        }
        return FALSE;
    }
}

namespace Engine
{
    void CrashDiagnostics::Install(
        const std::filesystem::path& outputDirectory,
        std::string_view processName)
    {
        OutputDirectory = outputDirectory.wstring();
        ProcessName.assign(processName.begin(), processName.end());
        CreateDirectoryW(OutputDirectory.c_str(), nullptr);

        const std::filesystem::path runtimeDirectory =
            outputDirectory.parent_path();
        const std::wstring symbolSearchPath =
            runtimeDirectory.wstring() + L";" +
            (runtimeDirectory / "GameHotReload").wstring();
        SymSetOptions(
            SymGetOptions() |
            SYMOPT_DEFERRED_LOADS |
            SYMOPT_LOAD_LINES |
            SYMOPT_UNDNAME);
        SymbolsInitialized =
            SymInitializeW(
                GetCurrentProcess(),
                symbolSearchPath.c_str(),
                TRUE) != FALSE;

        SetUnhandledExceptionFilter(&OnUnhandledException);
        std::set_terminate(&OnTerminate);
        _set_purecall_handler(&OnPureCall);
        _set_invalid_parameter_handler(&OnInvalidParameter);
#if defined(_DEBUG)
        _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, &OnCrtReport);
#endif
    }
}

#else

namespace Engine
{
    void CrashDiagnostics::Install(
        const std::filesystem::path&,
        std::string_view)
    {
    }
}

#endif
