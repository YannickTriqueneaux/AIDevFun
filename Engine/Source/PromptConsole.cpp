#include "Engine/UI/PromptConsole.h"

#include "Engine/UI/UiSystem.h"

#include <algorithm>
#include <iterator>
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
    PromptConsole::PromptConsole()
    {
        messages_.push_back({
            PromptMessageRole::Information,
            "Engine prompt console ready. Type a message below and press Enter."
        });
    }

    void PromptConsole::Render(
        UiSystem& ui,
        int screenWidth,
        int screenHeight)
    {
        const float panelLeft =
            static_cast<float>(screenWidth) - PanelWidth;
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

        if (!expanded_)
        {
            return;
        }

        ui.SetNextWindowPosition(
            {panelLeft, 0.0f},
            UiCondition::Always);
        ui.SetNextWindowSize(
            {PanelWidth, static_cast<float>(screenHeight)},
            UiCondition::Always);

        const bool panelVisible = ui.BeginPanel("Engine Prompt Console", false, false);
        if (panelVisible)
        {
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
        const auto firstContent = promptInput_.find_first_not_of(" \t\r\n");
        if (firstContent == std::string::npos)
        {
            return;
        }

        const auto lastContent = promptInput_.find_last_not_of(" \t\r\n");
        std::string prompt = promptInput_.substr(
            firstContent,
            lastContent - firstContent + 1);

        messages_.push_back({PromptMessageRole::User, prompt});

        std::vector<PromptMessage> results = processor_.Process(prompt);
        messages_.insert(
            messages_.end(),
            std::make_move_iterator(results.begin()),
            std::make_move_iterator(results.end()));

        promptInput_.clear();
        scrollToLatest_ = true;
        focusInput_ = true;
    }
}
