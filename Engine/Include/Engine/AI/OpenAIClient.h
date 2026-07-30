#pragma once

#include "Engine/AI/OpenAISettings.h"

#include <string>
#include <string_view>

namespace Engine
{
    struct OpenAIResponse
    {
        std::string id;
        std::string text;
    };

    class OpenAIClient
    {
    public:
        explicit OpenAIClient(OpenAISettings settings);

        [[nodiscard]] bool IsConfigured() const;
        [[nodiscard]] const std::string& GetModel() const;
        [[nodiscard]] OpenAIResponse CreateResponse(
            std::string_view instructions,
            std::string_view prompt,
            std::string_view previousResponseId = {}) const;

    private:
        OpenAISettings settings_;
    };
}

