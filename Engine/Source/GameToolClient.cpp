#include "Engine/AI/GameToolClient.h"

#include "Engine/IPC/NamedPipe.h"

#include <nlohmann/json.hpp>

namespace Engine
{
    std::string GameToolClient::Execute(
        std::string_view toolName,
        std::string_view argumentsJson) const
    {
        const nlohmann::json arguments =
            nlohmann::json::parse(argumentsJson);
        const nlohmann::json request{
            {"command", toolName},
            {"arguments", arguments}
        };

        return NamedPipeClient{}.Request(
            GameToolsPipeName,
            request.dump(),
            30'000);
    }
}

