#include "Engine/UI/PromptProcessor.h"

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
            "Do not attempt to modify Engine, Launcher, GameHost, or AssistantHost. "
            "Respond in English with a concise summary of changes and validation.";

        constexpr int MaximumToolRounds = 12;
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
            OpenAIResponse response = client_.CreateResponse(
                GameDeveloperInstructions,
                prompt,
                previousResponseId_,
                onEvent);

            for (int round = 0;
                 !response.toolCalls.empty() && round < MaximumToolRounds;
                 ++round)
            {
                std::vector<OpenAIToolOutput> outputs;
                outputs.reserve(response.toolCalls.size());

                for (const OpenAIResponse::ToolCall& call : response.toolCalls)
                {
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

                    outputs.push_back({
                        .callId = call.callId,
                        .output = std::move(output)
                    });
                    onEvent({
                        OpenAIStreamEventType::Status,
                        "Tool completed: " + call.name
                    });
                }

                response = client_.ContinueWithToolOutputs(
                    GameDeveloperInstructions,
                    outputs,
                    response.id,
                    onEvent);
            }

            if (!response.toolCalls.empty())
            {
                throw std::runtime_error(
                    "Controlled tool round limit reached.");
            }

            previousResponseId_ = response.id;
            return {};
        }
        catch (const std::exception& exception)
        {
            return {{
                PromptMessageRole::Information,
                std::string("Request failed: ") + exception.what()
            }};
        }
    }
}
