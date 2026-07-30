#include "Engine/UI/PromptConsole.h"

#include "Engine/UI/UiSystem.h"

#include "Engine/Core/Logger.h"

#include <algorithm>
#include <iterator>
#include <chrono>
#include <future>
#include <fstream>
#include <iomanip>
#include <sstream>
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

    std::string FormatTokens(std::uint64_t tokens)
    {
        std::string digits = std::to_string(tokens);
        for (std::ptrdiff_t position =
                 static_cast<std::ptrdiff_t>(digits.size()) - 3;
             position > 0;
             position -= 3)
        {
            digits.insert(static_cast<std::size_t>(position), ",");
        }
        return digits;
    }

    std::string FormatCost(
        std::string_view label,
        const Engine::OpenAITokenUsage& usage,
        bool costAvailable,
        double costUsd)
    {
        std::ostringstream stream;
        stream << label << ": ";
        if (costAvailable)
        {
            stream << "~US$" << std::fixed
                   << std::setprecision(costUsd < 0.01 ? 6 : 4)
                   << costUsd;
        }
        else
        {
            stream << "cost unavailable";
        }
        stream << " (" << FormatTokens(usage.TotalTokens()) << " tokens)";
        return stream.str();
    }
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
        PollAutomaticPrompt();

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
            if (hasCompletedPrompt_)
            {
                ui.TextWrapped(
                    FormatCost(
                        "Last prompt",
                        lastPromptUsage_,
                        lastPromptCostAvailable_,
                        lastPromptCostUsd_),
                    ui.GetAvailableWidth(),
                    {255, 203, 90, 255});
                ui.TextWrapped(
                    FormatCost(
                        "Session total",
                        sessionUsage_,
                        sessionCostAvailable_,
                        sessionCostUsd_),
                    ui.GetAvailableWidth(),
                    {255, 203, 90, 255});
            }
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

    void PromptConsole::PollAutomaticPrompt()
    {
        if (options_.automaticPromptFile.empty() ||
            pendingRequest_.valid() ||
            !std::filesystem::exists(options_.automaticPromptFile))
        {
            return;
        }

        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(
            options_.automaticPromptFile,
            error);
        if (error ||
            (lastAutomaticPromptTime_ &&
             *lastAutomaticPromptTime_ == writeTime))
        {
            return;
        }

        std::ifstream stream(
            options_.automaticPromptFile,
            std::ios::binary);
        if (!stream)
        {
            return;
        }
        std::string prompt{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };
        stream.close();
        lastAutomaticPromptTime_ = writeTime;
        std::filesystem::remove(options_.automaticPromptFile, error);
        if (prompt.empty())
        {
            return;
        }

        Logger::Warning(
            "Starting automatic AI crash recovery investigation.");
        promptInput_ = std::move(prompt);
        SubmitPrompt();
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

        PromptProcessResult result = pendingRequest_.get();
        lastPromptUsage_ = result.usage;
        sessionUsage_ += result.usage;
        lastPromptCostUsd_ = result.estimatedCostUsd;
        sessionCostUsd_ += result.estimatedCostUsd;
        lastPromptCostAvailable_ = result.costAvailable;
        sessionCostAvailable_ &= result.costAvailable;
        hasCompletedPrompt_ = true;

        messages_.insert(
            messages_.end(),
            std::make_move_iterator(result.messages.begin()),
            std::make_move_iterator(result.messages.end()));
        scrollToLatest_ = true;
        Logger::Info(
            FormatCost(
                "Completed prompt estimated cost",
                lastPromptUsage_,
                lastPromptCostAvailable_,
                lastPromptCostUsd_) +
            "; " +
            FormatCost(
                "session",
                sessionUsage_,
                sessionCostAvailable_,
                sessionCostUsd_));
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
