#include "Engine/UI/PromptProcessor.h"

#include "Engine/Core/Logger.h"

#include <string>
#include <stdexcept>
#include <utility>

namespace Engine
{
    namespace
    {
        constexpr std::string_view GameDeveloperInstructions =
            "You are the embedded AI developer for a lightweight C++20 game. "
            "The reusable Engine is persistent and the Game is a hot-reloadable "
            "DLL. Work only through the provided Game tools. Preserve the strict "
            "Engine-to-Game dependency direction. For implementation requests, inspect "
            "the relevant Game files, make minimal exact replacements, build Game, "
            "repair build failures when possible, and request reload only after a "
            "successful build. Never claim a tool succeeded unless its result says so. "
            "Minimize tool round trips: read independent files together with "
            "read_game_files and submit all coherent ordered replacements together with "
            "apply_game_patches. Prefer one batch over many single-file calls. "
            "Review a complete patch batch for syntax errors before submitting it. "
            "Do not attempt to modify Engine, Launcher, GameHost, or AssistantHost. "
            "Respond in English with a concise summary of changes and validation.";

        constexpr int MaximumToolRounds = 32;

        std::string TruncateForLog(std::string_view text)
        {
            constexpr std::size_t MaximumLogText = 4'000;
            if (text.size() <= MaximumLogText)
            {
                return std::string(text);
            }
            return std::string(text.substr(0, MaximumLogText)) +
                "\n[truncated]";
        }
    }

    PromptProcessor::PromptProcessor(OpenAISettings settings)
        : client_(std::move(settings))
    {
    }

    bool PromptProcessor::IsConfigured() const
    {
        return client_.IsConfigured();
    }

    const std::string& PromptProcessor::GetModel() const
    {
        return client_.GetModel();
    }

    std::vector<PromptMessage> PromptProcessor::Process(
        std::string_view prompt,
        const OpenAIStreamCallback& onEvent)
    {
        try
        {
            Logger::Info(
                "Starting OpenAI response. Previous response ID present: " +
                std::string(previousResponseId_.empty() ? "no." : "yes."));
            OpenAIResponse response = client_.CreateResponse(
                GameDeveloperInstructions,
                prompt,
                previousResponseId_,
                onEvent);

            for (int round = 0;
                 !response.toolCalls.empty() && round < MaximumToolRounds;
                 ++round)
            {
                Logger::Info(
                    "OpenAI tool round " + std::to_string(round + 1) +
                    " with " + std::to_string(response.toolCalls.size()) +
                    " call(s).");
                std::vector<OpenAIToolOutput> outputs;
                outputs.reserve(response.toolCalls.size());

                for (const OpenAIResponse::ToolCall& call : response.toolCalls)
                {
                    Logger::Info(
                        "Tool call " + call.name + " arguments:\n" +
                        TruncateForLog(call.arguments));
                    onEvent({
                        OpenAIStreamEventType::Status,
                        "Running tool: " + call.name
                    });

                    std::string output;
                    try
                    {
                        output = gameTools_.Execute(
                            call.name,
                            call.arguments);
                    }
                    catch (const std::exception& exception)
                    {
                        output = std::string(
                            R"({"ok":false,"error":"IPC failure: )") +
                            exception.what() + R"("})";
                    }

                    Logger::Info(
                        "Tool result " + call.name + ":\n" +
                        TruncateForLog(output));
                    outputs.push_back({
                        .callId = call.callId,
                        .output = std::move(output)
                    });
                    onEvent({
                        OpenAIStreamEventType::Status,
                        "Tool completed: " + call.name
                    });
                }

                const bool finalToolRound =
                    round + 1 >= MaximumToolRounds;
                if (finalToolRound)
                {
                    Logger::Warning(
                        "Tool round budget reached; requesting a final response "
                        "with tools disabled.");
                    onEvent({
                        OpenAIStreamEventType::Status,
                        "Tool budget reached. Producing final response."
                    });
                }

                response = client_.ContinueWithToolOutputs(
                    GameDeveloperInstructions,
                    outputs,
                    response.id,
                    onEvent,
                    !finalToolRound);
            }

            if (!response.toolCalls.empty())
            {
                Logger::Error(
                    "Model returned tool calls even after tools were disabled.");
                throw std::runtime_error(
                    "Model returned unexpected tool calls after finalization.");
            }

            previousResponseId_ = response.id;
            Logger::Info(
                "OpenAI response completed. Response ID: " + response.id);
            return {};
        }
        catch (const std::exception& exception)
        {
            Logger::Error(
                std::string("OpenAI request failed: ") + exception.what());
            return {{
                PromptMessageRole::Information,
                std::string("Request failed: ") + exception.what()
            }};
        }
    }
}
