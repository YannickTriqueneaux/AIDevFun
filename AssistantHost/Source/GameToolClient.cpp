#include "AssistantHost/GameToolClient.h"

#include "Development/GameToolsProtocol.h"
#include "Engine/IPC/NamedPipe.h"

#include <nlohmann/json.hpp>

namespace AssistantHost {
std::string GameToolClient::Execute(std::string_view toolName,
                                    std::string_view argumentsJson) const {
  const nlohmann::json arguments = nlohmann::json::parse(argumentsJson);
  const nlohmann::json request{{"command", toolName}, {"arguments", arguments}};

  return Engine::NamedPipeClient{}.Request(Development::GetGameToolsPipeName(),
                                           request.dump(), 30'000);
}
} // namespace AssistantHost
