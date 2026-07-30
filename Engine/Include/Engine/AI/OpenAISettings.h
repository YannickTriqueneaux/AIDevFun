#pragma once

#include <string>

namespace Engine
{
    struct OpenAISettings
    {
        std::string apiKey;
        std::string model = "gpt-5.5";

        [[nodiscard]] bool IsConfigured() const
        {
            return !apiKey.empty() && !model.empty();
        }
    };
}

