#include "Engine/UI/PromptProcessor.h"

#include <string>

namespace Engine
{
    std::vector<PromptMessage> PromptProcessor::Process(
        std::string_view prompt) const
    {
        std::vector<PromptMessage> results;
        results.push_back({
            PromptMessageRole::Information,
            "Prompt accepted (" + std::to_string(prompt.size()) + " characters)."
        });
        results.push_back({
            PromptMessageRole::Result,
            "No prompt provider is connected yet. The engine prompt pipeline "
            "is ready for a local model, HTTP service, or engine command."
        });
        return results;
    }
}

