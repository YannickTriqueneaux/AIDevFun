#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/Core/Export.h"

namespace Engine
{
    class ENGINE_API AssistantApplication
    {
    public:
        explicit AssistantApplication(OpenAISettings settings);
        ~AssistantApplication();

        AssistantApplication(const AssistantApplication&) = delete;
        AssistantApplication& operator=(const AssistantApplication&) = delete;

        void Run();

    private:
        OpenAISettings settings_;
    };
}

