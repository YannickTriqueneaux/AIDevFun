#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/UI/PromptMessage.h"
#include "Engine/UI/PromptProcessor.h"

#include <string>
#include <future>
#include <vector>

namespace Engine
{
    class UiSystem;

    class PromptConsole
    {
    public:
        explicit PromptConsole(OpenAISettings settings);

        void Render(UiSystem& ui, int screenWidth, int screenHeight);

    private:
        void SubmitPrompt();
        void PollPendingRequest();

        std::vector<PromptMessage> messages_;
        std::string promptInput_;
        PromptProcessor processor_;
        std::future<std::vector<PromptMessage>> pendingRequest_;
        bool expanded_ = false;
        bool scrollToLatest_ = true;
        bool focusInput_ = true;
    };
}
