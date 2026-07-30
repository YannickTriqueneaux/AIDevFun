#pragma once

#include "Engine/AI/OpenAIClient.h"
#include "Engine/UI/PromptMessage.h"

#include <string_view>
#include <vector>

namespace Engine
{
    class PromptProcessor
    {
    public:
        explicit PromptProcessor(OpenAISettings settings);

        [[nodiscard]] bool IsConfigured() const;
        [[nodiscard]] const std::string& GetModel() const;
        [[nodiscard]] std::vector<PromptMessage> Process(
            std::string_view prompt);

    private:
        OpenAIClient client_;
        std::string previousResponseId_;
    };
}
