#pragma once

#include "Engine/Core/Export.h"

#include <functional>
#include <string>
#include <string_view>

namespace Engine
{
    inline constexpr std::string_view GameToolsPipeName =
        R"(\\.\pipe\AITester.GameTools.v1)";

    class ENGINE_API NamedPipeClient
    {
    public:
        [[nodiscard]] std::string Request(
            std::string_view pipeName,
            std::string_view request,
            unsigned long timeoutMilliseconds = 5'000) const;
    };

    class ENGINE_API NamedPipeServer
    {
    public:
        using RequestHandler =
            std::function<std::string(std::string_view request)>;

        NamedPipeServer(
            std::string pipeName,
            RequestHandler handler);
        ~NamedPipeServer();

        NamedPipeServer(const NamedPipeServer&) = delete;
        NamedPipeServer& operator=(const NamedPipeServer&) = delete;

    private:
        struct Impl;
        Impl* impl_ = nullptr;
    };
}
