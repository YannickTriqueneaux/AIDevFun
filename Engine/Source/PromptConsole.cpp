#include "Engine/UI/PromptConsole.h"

#include "Engine/UI/UiSystem.h"

#include "Engine/Core/Logger.h"

#include <algorithm>
#include <iterator>
#include <chrono>
#include <future>
#include <utility>

namespace
{
    constexpr float PanelWidth = 420.0f;
    constexpr float ToggleWidth = 44.0f;
    constexpr float ToggleHeight = 48.0f;
    constexpr float ToggleButtonSize = 24.0f;
    constexpr float ToggleTop = 12.0f;
    constexpr float MessageWidthRatio = 0.78f;
    constexpr Engine::Color UserColor{110, 190, 255, 255};
    constexpr Engine::Color ResultColor{235, 235, 235, 255};
    constexpr Engine::Color InformationColor{145, 150, 165, 255};
}

namespace Engine
{
    PromptConsole::PromptConsole(
        OpenAISettings settings,
        PromptConsoleOptions options)
        : processor_(std::move(settings)),
          options_(options),
          expanded_(options.expandedByDefault)
    {
        messages_.push_back({
            PromptMessageRole::Information,
            processor_.IsConfigured()
                ? "OpenAI ready with model " + processor_.GetModel() + "."
                : "OpenAI is not configured. Add apiKey to settings.json."
        });
    }

    void PromptConsole::Render(
        UiSystem& ui,
        int screenWidth,
        int screenHeight)
    {
        PollStreamEvents();
        PollPendingRequest();

        const float effectivePanelWidth = options_.fillWindow
            ? static_cast<float>(screenWidth)
            : PanelWidth;
        const float panelLeft =
            static_cast<float>(screenWidth) - effectivePanelWidth;

        if (options_.collapsible)
        {
            const float toggleLeft = expanded_
                ? panelLeft - ToggleWidth
                : static_cast<float>(screenWidth) - ToggleWidth;

            ui.SetNextWindowPosition(
                {toggleLeft, ToggleTop},
                UiCondition::Always);
            ui.SetNextWindowSize(
                {ToggleWidth, ToggleHeight},
                UiCondition::Always);

            const bool toggleVisible = ui.BeginPanel(
                "##EnginePromptToggle",
                false,
                false,
                false);
            if (toggleVisible)
            {
                const char* toggleLabel = expanded_ ? "<" : ">";
                if (ui.Button(
                        toggleLabel,
                        {ToggleButtonSize, ToggleButtonSize}))
                {
                    expanded_ = !expanded_;
                }
            }
            ui.EndPanel();
        }

        if (!expanded_)
        {
            return;
        }

        ui.SetNextWindowPosition(
            {panelLeft, 0.0f},
            UiCondition::Always);
        ui.SetNextWindowSize(
            {effectivePanelWidth, static_cast<float>(screenHeight)},
            UiCondition::Always);

        const bool panelVisible = ui.BeginPanel("Engine Prompt Console", false, false);
        if (panelVisible)
        {
            ui.Text("Activity", {210, 215, 225, 255});
            if (ui.BeginChild(
                    "PromptActivity",
                    {0.0f, 130.0f},
                    true))
            {
                for (const std::string& log : activityLogs_)
                {
                    ui.TextWrapped(
                        log,
                        ui.GetAvailableWidth(),
                        InformationColor);
                }
                if (scrollToLatest_)
                {
                    ui.ScrollToBottom();
                }
            }
            ui.EndChild();

            ui.Text("Conversation", {210, 215, 225, 255});
            ui.Separator();

            const float inputAreaHeight = ui.GetInputHeight() + 8.0f;
            if (ui.BeginChild(
                    "PromptHistory",
                    {0.0f, -inputAreaHeight},
                    false))
            {
                for (const PromptMessage& message : messages_)
                {
                    const float availableWidth = ui.GetAvailableWidth();
                    const float messageWidth = std::max(
                        80.0f,
                        availableWidth * MessageWidthRatio);
                    const bool userMessage =
                        message.role == PromptMessageRole::User;

                    if (userMessage)
                    {
                        ui.SetCursorX(
                            ui.GetCursorX() +
                            std::max(0.0f, availableWidth - messageWidth));
                    }

                    Color color = ResultColor;
                    if (userMessage)
                    {
                        color = UserColor;
                    }
                    else if (message.role == PromptMessageRole::Information)
                    {
                        color = InformationColor;
                    }

                    ui.TextWrapped(message.text, messageWidth, color);
                    ui.Spacing();
                }

                if (scrollToLatest_)
                {
                    ui.ScrollToBottom();
                    scrollToLatest_ = false;
                }
            }
            ui.EndChild();

            if (focusInput_)
            {
                ui.SetKeyboardFocusHere();
                focusInput_ = false;
            }

            if (ui.InputText(
                    "##PromptInput",
                    "Type a prompt and press Enter...",
                    promptInput_))
            {
                SubmitPrompt();
            }
        }
        ui.EndPanel();
    }

    void PromptConsole::SubmitPrompt()
    {
        if (pendingRequest_.valid())
        {
            messages_.push_back({
                PromptMessageRole::Information,
                "Wait for the current response before sending another prompt."
            });
            scrollToLatest_ = true;
            return;
        }

        const auto firstContent = promptInput_.find_first_not_of(" \t\r\n");
        if (firstContent == std::string::npos)
        {
            return;
        }

        const auto lastContent = promptInput_.find_last_not_of(" \t\r\n");
        std::string prompt = promptInput_.substr(
            firstContent,
            lastContent - firstContent + 1);

        Logger::Info(
            "User prompt submitted:\n" +
            prompt.substr(0, std::min<std::size_t>(prompt.size(), 4'000)));
        messages_.push_back({PromptMessageRole::User, prompt});
        activityLogs_.push_back("Request queued.");
        activeResponseIndex_.reset();
        activeReasoningLogIndex_.reset();

        pendingRequest_ = std::async(
            std::launch::async,
            [this, prompt = std::move(prompt)]()
            {
                return processor_.Process(
                    prompt,
                    [this](const OpenAIStreamEvent& event)
                    {
                        std::scoped_lock lock(streamEventsMutex_);
                        pendingStreamEvents_.push_back(event);
                    });
            });

        promptInput_.clear();
        scrollToLatest_ = true;
        focusInput_ = true;
    }

    void PromptConsole::PollPendingRequest()
    {
        if (!pendingRequest_.valid() ||
            pendingRequest_.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready)
        {
            return;
        }

        std::vector<PromptMessage> results = pendingRequest_.get();
        messages_.insert(
            messages_.end(),
            std::make_move_iterator(results.begin()),
            std::make_move_iterator(results.end()));
        scrollToLatest_ = true;
        Logger::Info("Pending OpenAI request joined by the UI thread.");
    }

    void PromptConsole::PollStreamEvents()
    {
        std::vector<OpenAIStreamEvent> events;
        {
            std::scoped_lock lock(streamEventsMutex_);
            events.swap(pendingStreamEvents_);
        }

        for (const OpenAIStreamEvent& event : events)
        {
            switch (event.type)
            {
            case OpenAIStreamEventType::Status:
                activityLogs_.push_back(event.text);
                Logger::Info("OpenAI status: " + event.text);
                break;

            case OpenAIStreamEventType::ReasoningSummaryDelta:
                if (!activeReasoningLogIndex_)
                {
                    activityLogs_.push_back("Reasoning summary: ");
                    activeReasoningLogIndex_ = activityLogs_.size() - 1;
                }
                activityLogs_[*activeReasoningLogIndex_] += event.text;
                break;

            case OpenAIStreamEventType::OutputTextDelta:
                if (!activeResponseIndex_)
                {
                    messages_.push_back({PromptMessageRole::Result, ""});
                    activeResponseIndex_ = messages_.size() - 1;
                }
                messages_[*activeResponseIndex_].text += event.text;
                break;
            }
        }

        if (!events.empty())
        {
            scrollToLatest_ = true;
        }
    }
}
