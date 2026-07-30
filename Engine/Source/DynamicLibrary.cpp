#include "Engine/Platform/DynamicLibrary.h"

#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Engine
{
    DynamicLibrary::DynamicLibrary(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        handle_ = static_cast<void*>(LoadLibraryW(path.c_str()));
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW);
#endif

        if (handle_ == nullptr)
        {
            throw std::runtime_error(
                "Failed to load dynamic library: " + path.string());
        }
    }

    DynamicLibrary::~DynamicLibrary()
    {
        if (handle_ == nullptr)
        {
            return;
        }

#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
    {
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
    {
        if (this != &other)
        {
            std::swap(handle_, other.handle_);
        }
        return *this;
    }

    void* DynamicLibrary::GetFunction(const char* functionName) const
    {
#if defined(_WIN32)
        void* function = reinterpret_cast<void*>(
            GetProcAddress(static_cast<HMODULE>(handle_), functionName));
#else
        void* function = dlsym(handle_, functionName);
#endif

        if (function == nullptr)
        {
            throw std::runtime_error(
                std::string("Function not found in dynamic library: ") +
                functionName);
        }

        return function;
    }
}

