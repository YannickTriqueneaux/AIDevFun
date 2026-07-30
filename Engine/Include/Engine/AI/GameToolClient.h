#pragma once

#include <string>
#include <string_view>

namespace Engine
{
    class GameToolClient
    {
    public:
        [[nodiscard]] std::string Execute(
            std::string_view toolName,
            std::string_view argumentsJson) const;
    };
}

