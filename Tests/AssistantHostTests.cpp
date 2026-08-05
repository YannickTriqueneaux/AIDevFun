#include "AssistantHost/AssistantPromptConfig.h"
#include "AssistantHost/MidiAttachmentProvider.h"
#include "AssistantProviders/Codex/CodexFeatureDetection.h"
#include "AssistantProviders/Codex/CodexEventParser.h"
#include "AssistantProviders/Claude/ClaudeEventParser.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void TestAssistantPromptConfig() {
  const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     "aitester_assistant_prompts_test.json";
  {
    std::ofstream stream(path, std::ios::binary);
    stream << R"({"gameDeveloperInstructions":["First rule.","Second rule."]})";
  }

  const AssistantHost::AssistantPromptConfig config =
      AssistantHost::AssistantPromptConfig::Load(path);
  std::error_code error;
  std::filesystem::remove(path, error);
  Require(config.GetGameDeveloperInstructions() ==
              "First rule.\n\nSecond rule.",
          "Assistant prompt blocks were not loaded in order.");
}
void TestMidiParsing() {
  const std::vector<std::uint8_t> bytes{
      'M',  'T', 'h',  'd',  0,    0,    0,    6, 0,    0,    0,    1, 1,
      0xe0, 'M', 'T',  'r',  'k',  0,    0,    0, 0x1c, 0,    0xff, 3, 4,
      'L',  'e', 'a',  'd',  0,    0xff, 0x51, 3, 0x07, 0xa1, 0x20, 0, 0x90,
      60,   100, 0x83, 0x60, 0x80, 60,   0,    0, 0xff, 0x2f, 0};

  const AssistantHost::MidiFile midi = AssistantHost::ParseMidiFile(bytes);
  Require(midi.format == 0, "MIDI format was not parsed.");
  Require(midi.ticksPerQuarterNote == 480, "MIDI PPQ was not parsed.");
  Require(midi.tracks.size() == 1 && midi.tracks[0].name == "Lead",
          "MIDI track metadata was not parsed.");
  Require(midi.tracks[0].notes.size() == 1,
          "MIDI note events were not paired.");
  const auto &note = midi.tracks[0].notes[0];
  Require(note.note == 60 && note.startTick == 0 && note.durationTicks == 480 &&
              note.velocity == 100,
          "MIDI note properties were not parsed.");

  const std::string description =
      AssistantHost::DescribeMidiForPrompt(midi, "reference.mid");
  Require(description.find("120.00") != std::string::npos,
          "MIDI tempo was not described.");
  Require(description.find("C4(60)") != std::string::npos,
          "MIDI pitch was not described.");
}

void TestCodexEventParsing() {
  using AssistantProviders::Codex::ParseEventLine;
  using Development::AssistantStreamEventType;

  const auto thread =
      ParseEventLine(R"({"type":"thread.started","thread_id":"thread-42"})");
  Require(thread.threadId == "thread-42", "Codex thread ID was not parsed.");
  Require(thread.events.size() == 1, "Codex thread activity was not emitted.");

  const auto command = ParseEventLine(
      R"({"type":"item.started","item":{"type":"command_execution","command":"cmake --build build"}})");
  Require(command.events.size() == 1 &&
              command.events[0].text.find("cmake --build") != std::string::npos,
          "Codex command activity was not parsed.");

  const auto reasoning = ParseEventLine(
      R"({"type":"item.completed","item":{"type":"reasoning","text":"Inspecting the build failure."}})");
  Require(reasoning.events.size() == 1 &&
              reasoning.events[0].type ==
                  AssistantStreamEventType::ReasoningSummaryDelta,
          "Codex reasoning summary was not parsed.");

  const auto completed = ParseEventLine(
      R"({"type":"turn.completed","usage":{"input_tokens":120,"cached_input_tokens":80,"output_tokens":30}})");
  Require(completed.usage.has_value() && completed.usage->inputTokens == 120 &&
              completed.usage->cachedInputTokens == 80 &&
              completed.usage->outputTokens == 30,
          "Codex token usage was not parsed.");

  const auto unknown =
      ParseEventLine(R"({"type":"future.event","new_field":true})");
  Require(unknown.events.empty(),
          "Unknown Codex events should be ignored safely.");
  Require(ParseEventLine("not json").events.empty(),
          "Malformed Codex output should be ignored safely.");

  const auto mcpDiagnostic = ParseEventLine(
      "MCP startup interrupted: game_tools failed to initialize");
  Require(mcpDiagnostic.events.size() == 1 &&
              mcpDiagnostic.events[0].type ==
                  AssistantStreamEventType::Status &&
              mcpDiagnostic.events[0].text.find("game_tools") !=
                  std::string::npos,
          "Plain-text Codex MCP diagnostics were not surfaced.");
}

void TestCodexFeatureDetection() {
  using AssistantProviders::Codex::FeatureListContains;
  constexpr std::string_view features =
      "shell_tool stable true\n"
      "mcp_2026_07_28 under development false\n"
      "unified_exec stable false\n";
  Require(FeatureListContains(features, "mcp_2026_07_28"),
          "A supported Codex feature was not detected.");
  Require(!FeatureListContains(features, "mcp_2026_07_2"),
          "Codex feature detection accepted a partial name.");
  Require(!FeatureListContains("error: unknown command", "mcp_2026_07_28"),
          "Invalid feature output should not enable a flag.");
}

void TestClaudeEventParsing() {
  using Development::AssistantStreamEventType;
  const auto init = AssistantProviders::Claude::ParseEventLine(
      R"({"type":"system","subtype":"init","session_id":"session-1","mcp_servers":[{"name":"game_tools","status":"connected"}]})");
  Require(init.sessionId == "session-1" && init.events.size() == 2,
          "Claude initialization activity was not parsed.");

  const auto failedInit = AssistantProviders::Claude::ParseEventLine(
      R"({"type":"system","subtype":"init","session_id":"session-1","mcp_servers":[{"name":"game_tools","status":"failed"}]})");
  Require(failedInit.failed && failedInit.error.find("game_tools") !=
                                    std::string::npos,
          "A required Claude MCP startup failure was not detected.");

  const auto tool = AssistantProviders::Claude::ParseEventLine(
      R"({"type":"assistant","session_id":"session-1","message":{"content":[{"type":"tool_use","name":"mcp__game_tools__read_game_file"}]}})");
  Require(tool.events.size() == 1 &&
              tool.events[0].type == AssistantStreamEventType::Status &&
              tool.events[0].text.find("game_tools.read_game_file") !=
                  std::string::npos,
          "Claude MCP tool activity was not parsed.");

  const auto result = AssistantProviders::Claude::ParseEventLine(
      R"({"type":"result","subtype":"success","is_error":false,"session_id":"session-1","result":"done","usage":{"input_tokens":10,"cache_read_input_tokens":4,"output_tokens":3}})");
  Require(result.resultText == "done" && result.usage.has_value() &&
              result.usage->inputTokens == 10 &&
              result.usage->cachedInputTokens == 4 &&
              result.usage->outputTokens == 3,
          "Claude result or token usage was not parsed.");
}
} // namespace

int main() {
  try {
    TestAssistantPromptConfig();
    TestMidiParsing();
    TestCodexEventParsing();
    TestCodexFeatureDetection();
    TestClaudeEventParsing();
    std::cout << "AssistantHost tests passed.\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "AssistantHost test failure: " << exception.what() << '\n';
    return 1;
  }
}
