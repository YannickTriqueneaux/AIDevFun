#include "Engine/UI/PromptProcessor.h"

#include <string>

namespace Engine
{
    namespace
    {
        constexpr std::string_view GameDeveloperInstructions =
            "You are the embedded AI developer for a lightweight C++20 game. "
            "The reusable Engine is persistent and the Game is a hot-reloadable "
            "DLL. Focus proposed changes on the Game module. Preserve the strict "
            "Engine-to-Game dependency direction. Respond in English with concrete, "
            "implementation-oriented guidance and code when useful. You currently "
            "cannot access or modify local files, so clearly identify intended files "
            "and do not claim that changes were applied.";
    }

    PromptProcessor::PromptProcessor(OpenAISettings settings)
        : client_(std::move(settings))
    {
    }

    bool PromptProcessor::IsConfigured() const
    {
        return client_.IsConfigured();
    }

    const std::string& PromptProcessor::GetModel() const
    {
        return client_.GetModel();
    }

    std::vector<PromptMessage> PromptProcessor::Process(
        std::string_view prompt,
        const OpenAIStreamCallback& onEvent)
    {
        try
        {
            const OpenAIResponse response = client_.CreateResponse(
                GameDeveloperInstructions,
                prompt,
                previousResponseId_,
                onEvent);
            previousResponseId_ = response.id;
            return {};
        }
        catch (const std::exception& exception)
        {
            return {{
                PromptMessageRole::Information,
                std::string("Request failed: ") + exception.what()
            }};
        }
    }
}
