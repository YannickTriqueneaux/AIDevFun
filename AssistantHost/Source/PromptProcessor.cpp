#include "AssistantHost/PromptProcessor.h"

#include "AssistantHost/AssistantPromptConfig.h"

#include "Engine/Core/Logger.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace AssistantHost {
namespace {
constexpr int MaximumToolRounds = 32;

std::string TruncateForLog(std::string_view text) {
  constexpr std::size_t MaximumLogText = 4'000;
  if (text.size() <= MaximumLogText) {
    return std::string(text);
  }
  return std::string(text.substr(0, MaximumLogText)) + "\n[truncated]";
}
} // namespace

PromptProcessor::PromptProcessor(Development::AssistantProvider &provider,
                                 const std::filesystem::path &promptConfigPath)
    : provider_(provider), promptConfigPath_(promptConfigPath) {}

bool PromptProcessor::IsConfigured() const { return provider_.IsConfigured(); }

const std::string &PromptProcessor::GetModel() const {
  return provider_.GetModel();
}

const std::string &PromptProcessor::GetProviderName() const {
  return provider_.GetDisplayName();
}

PromptProcessResult
PromptProcessor::Process(std::string_view prompt,
                         const std::vector<AssistantImageInput> &images,
                         const AssistantStreamCallback &onEvent) {
  PromptProcessResult processResult;
  const auto accountForResponse =
      [this, &processResult](const Development::AssistantResponse &response) {
        const bool firstReportedUsage = !processResult.usageReported;
        processResult.usage += response.usage;
        const Development::AssistantCostEstimate estimate =
            provider_.EstimateCost(response.usage);
        processResult.costAvailable =
            firstReportedUsage
                ? estimate.available
                : processResult.costAvailable && estimate.available;
        processResult.estimatedCostUsd += estimate.usd;
        processResult.usageReported = true;
      };

  try {
    const std::string gameDeveloperInstructions =
        AssistantPromptConfig::Load(promptConfigPath_)
            .GetGameDeveloperInstructions();
    Engine::Logger::Info(
        "Starting assistant response. Previous response ID present: " +
        std::string(previousResponseId_.empty() ? "no." : "yes."));
    Development::AssistantResponse response =
        provider_.CreateResponse(gameDeveloperInstructions, prompt, images,
                                 previousResponseId_, onEvent);
    accountForResponse(response);

    for (int round = 0;
         !response.toolCalls.empty() && round < MaximumToolRounds; ++round) {
      Engine::Logger::Info(
          "Assistant tool round " + std::to_string(round + 1) + " with " +
          std::to_string(response.toolCalls.size()) + " call(s).");
      std::vector<Development::AssistantToolOutput> outputs;
      outputs.reserve(response.toolCalls.size());

      for (const Development::AssistantResponse::ToolCall &call :
           response.toolCalls) {
        Engine::Logger::Info("Tool call " + call.name + " arguments:\n" +
                             TruncateForLog(call.arguments));
        onEvent(
            {AssistantStreamEventType::Status, "Running tool: " + call.name});

        std::string output;
        try {
          output = gameTools_.Execute(call.name, call.arguments);
        } catch (const std::exception &exception) {
          output = std::string(R"({"ok":false,"error":"IPC failure: )") +
                   exception.what() + R"("})";
        }

        Engine::Logger::Info("Tool result " + call.name + ":\n" +
                             TruncateForLog(output));
        outputs.push_back({.callId = call.callId, .output = std::move(output)});
        onEvent(
            {AssistantStreamEventType::Status, "Tool completed: " + call.name});
      }

      const bool finalToolRound = round + 1 >= MaximumToolRounds;
      if (finalToolRound) {
        Engine::Logger::Warning(
            "Tool round budget reached; requesting a final response "
            "with tools disabled.");
        onEvent({AssistantStreamEventType::Status,
                 "Tool budget reached. Producing final response."});
      }

      response = provider_.ContinueWithToolOutputs(gameDeveloperInstructions,
                                                   outputs, response.id,
                                                   onEvent, !finalToolRound);
      accountForResponse(response);
    }

    if (!response.toolCalls.empty()) {
      Engine::Logger::Error(
          "Model returned tool calls even after tools were disabled.");
      throw std::runtime_error(
          "Model returned unexpected tool calls after finalization.");
    }

    previousResponseId_ = response.id;
    Engine::Logger::Info("Assistant response completed. Response ID: " +
                         response.id);
    return processResult;
  } catch (const std::exception &exception) {
    Engine::Logger::Error(std::string("Assistant request failed: ") +
                          exception.what());
    processResult.messages.push_back(
        {PromptMessageRole::Information,
         std::string("Request failed: ") + exception.what()});
    return processResult;
  }
}
} // namespace AssistantHost
