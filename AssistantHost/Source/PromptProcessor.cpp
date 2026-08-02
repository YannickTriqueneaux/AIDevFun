#include "AssistantHost/PromptProcessor.h"

#include "Engine/Core/Logger.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace AssistantHost {
namespace {
constexpr std::string_view GameDeveloperInstructions =
    "You are the embedded AI developer for a lightweight C++20 game. "
    "The reusable Engine is persistent and the Game is a hot-reloadable "
    "DLL. Work only through the provided Game tools. Preserve the strict "
    "Engine-to-Game dependency direction. For implementation requests, inspect "
    "the available repository skills with list_agent_skills, then read every "
    "skill whose description applies before editing. Skills are read-only "
    "mandatory project guidance. Inspect "
    "Engine source through the read-only Engine tools when API behavior or "
    "available capabilities are unclear. Engine access is context-only: never "
    "attempt to modify Engine files. Inspect the relevant Game files, make "
    "minimal exact replacements, build Game, "
    "repair build failures when possible, and request reload only after a "
    "successful build. Never claim a tool succeeded unless its result says so. "
    "When the Game has stopped in crash recovery mode, first call "
    "inspect_crash_diagnostics. Correlate the newest crash report and process "
    "logs with the Game source, implement the smallest justified repair, and "
    "build it. In recovery mode, call launch_game only after build_game "
    "reports "
    "success; reload_game is only for a currently running Game. If evidence is "
    "insufficient, explain that instead of guessing. "
    "Minimize tool round trips: read independent files together with "
    "read_game_files and submit all coherent ordered replacements together "
    "with "
    "apply_game_patches. Prefer one batch over many single-file calls. "
    "Review a complete patch batch for syntax errors before submitting it. "
    "Use create_game_code_file only for genuinely new .cpp or .h files and "
    "delete_game_code_file only when an existing Game code file is obsolete. "
    "After creating or deleting code, inspect related files and build before "
    "requesting reload. "
    "Do not attempt to modify Engine, Launcher, GameHost, or AssistantHost. "
    "Respond in English with a concise summary of changes and validation.";

constexpr int MaximumToolRounds = 32;

std::string TruncateForLog(std::string_view text) {
  constexpr std::size_t MaximumLogText = 4'000;
  if (text.size() <= MaximumLogText) {
    return std::string(text);
  }
  return std::string(text.substr(0, MaximumLogText)) + "\n[truncated]";
}
} // namespace

PromptProcessor::PromptProcessor(Development::AssistantProvider &provider)
    : provider_(provider) {}

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
    Engine::Logger::Info(
        "Starting assistant response. Previous response ID present: " +
        std::string(previousResponseId_.empty() ? "no." : "yes."));
    Development::AssistantResponse response =
        provider_.CreateResponse(GameDeveloperInstructions, prompt, images,
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

      response = provider_.ContinueWithToolOutputs(GameDeveloperInstructions,
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
