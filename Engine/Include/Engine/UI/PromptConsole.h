#pragma once

#include "Engine/UI/PromptMessage.h"
#include "Engine/UI/PromptProcessor.h"

#include <string>
#include <vector>

namespace Engine
{
    class UiSystem;

    class PromptConsole
    {
    public:
        PromptConsole();

        void Render(UiSystem& ui, int screenWidth, int screenHeight);

    private:
        void SubmitPrompt();

        std::vector<PromptMessage> messages_;
        std::string promptInput_;
        PromptProcessor processor_;
        bool scrollToLatest_ = true;
        bool focusInput_ = true;
    };
}

