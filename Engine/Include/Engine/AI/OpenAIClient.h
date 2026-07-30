#pragma once

#include "Engine/AI/OpenAISettings.h"

#include <string>
#include <string_view>
#include <functional>

namespace Engine
{
    enum class OpenAIStreamEventType
    {
        Status,
        ReasoningSummaryDelta,
        OutputTextDelta
    };

    struct OpenAIStreamEvent
    {
        OpenAIStreamEventType type = OpenAIStreamEventType::Status;
        std::string text;
    };

    using OpenAIStreamCallback =
        std::function<void(const OpenAIStreamEvent&)>;

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
            std::string_view previousResponseId,
            const OpenAIStreamCallback& onEvent) const;

    private:
        OpenAISettings settings_;
    };
}
