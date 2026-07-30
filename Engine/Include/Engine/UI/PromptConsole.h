#pragma once

#include "Engine/AI/OpenAISettings.h"
#include "Engine/UI/PromptMessage.h"
#include "Engine/UI/PromptProcessor.h"

#include <string>
#include <future>
#include <mutex>
#include <optional>
#include <vector>

namespace Engine
{
    class UiSystem;

    struct PromptConsoleOptions
    {
        bool collapsible = true;
        bool expandedByDefault = false;
        bool fillWindow = false;
    };

    class PromptConsole
    {
    public:
        explicit PromptConsole(
            OpenAISettings settings,
            PromptConsoleOptions options = {});

        void Render(UiSystem& ui, int screenWidth, int screenHeight);

    private:
        void SubmitPrompt();
        void PollPendingRequest();
        void PollStreamEvents();

        std::vector<PromptMessage> messages_;
        std::vector<std::string> activityLogs_;
        std::string promptInput_;
        PromptProcessor processor_;
        PromptConsoleOptions options_;
        std::future<std::vector<PromptMessage>> pendingRequest_;
        std::mutex streamEventsMutex_;
        std::vector<OpenAIStreamEvent> pendingStreamEvents_;
        std::optional<std::size_t> activeResponseIndex_;
        std::optional<std::size_t> activeReasoningLogIndex_;
        bool expanded_ = false;
        bool scrollToLatest_ = true;
        bool focusInput_ = true;
    };
}
