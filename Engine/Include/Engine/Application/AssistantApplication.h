#pragma once

#include "Engine/Core/Export.h"

#include <string>

namespace Engine
{
    struct OpenAISettings;

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
        struct Implementation;
        Implementation* implementation_ = nullptr;
    };
}
