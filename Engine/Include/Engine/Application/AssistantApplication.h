#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/Core/Export.h"

#include <string>

namespace Engine
{
    class ENGINE_API AssistantApplication
    {
    public:
        AssistantApplication(
            OpenAISettings settings,
            std::string gameName);
        ~AssistantApplication();

        AssistantApplication(const AssistantApplication&) = delete;
        AssistantApplication& operator=(const AssistantApplication&) = delete;

        void Run();

    private:
        OpenAISettings settings_;
        std::string gameWindowTitle_;
    };
}
