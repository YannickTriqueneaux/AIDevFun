#include "AssistantHost/OpenAIClient.h"

#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#endif

namespace {
#if defined(_WIN32)
class WinHttpHandle {
public:
  explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}

  ~WinHttpHandle() {
    if (handle_ != nullptr) {
      WinHttpCloseHandle(handle_);
    }
  }

  WinHttpHandle(const WinHttpHandle &) = delete;
  WinHttpHandle &operator=(const WinHttpHandle &) = delete;

  [[nodiscard]] HINTERNET Get() const { return handle_; }

private:
  HINTERNET handle_ = nullptr;
};

std::wstring ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    throw std::runtime_error("Failed to encode an HTTP header.");
  }

  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size);
  return result;
}

std::string PostResponses(const std::string &apiKey,
                          const std::string &requestBody,
                          const AssistantHost::OpenAIStreamCallback &onEvent,
                          AssistantHost::OpenAIResponse &response) {
  WinHttpHandle session(WinHttpOpen(
      L"MakeYourOwnGame.AI/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (session.Get() == nullptr) {
    throw std::runtime_error("Unable to initialize the HTTP client.");
  }

  WinHttpSetTimeouts(session.Get(), 10'000, 10'000, 30'000, 300'000);

  WinHttpHandle connection(WinHttpConnect(session.Get(), L"api.openai.com",
                                          INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (connection.Get() == nullptr) {
    throw std::runtime_error("Unable to connect to api.openai.com.");
  }

  WinHttpHandle request(WinHttpOpenRequest(
      connection.Get(), L"POST", L"/v1/responses", nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (request.Get() == nullptr) {
    throw std::runtime_error("Unable to create the OpenAI request.");
  }

  const std::wstring headers = ToWide("Content-Type: application/json\r\n"
                                      "Accept: text/event-stream\r\n"
                                      "Authorization: Bearer " +
                                      apiKey);
  const BOOL sent = WinHttpSendRequest(
      request.Get(), headers.c_str(), static_cast<DWORD>(headers.size()),
      const_cast<char *>(requestBody.data()),
      static_cast<DWORD>(requestBody.size()),
      static_cast<DWORD>(requestBody.size()), 0);
  if (!sent || !WinHttpReceiveResponse(request.Get(), nullptr)) {
    throw std::runtime_error("The OpenAI HTTP request failed.");
  }

  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(request.Get(),
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

  std::string responseBody;
  std::string streamBuffer;

  const auto processEvent = [&](std::string_view eventBlock) {
    constexpr std::string_view DataPrefix = "data:";
    const std::size_t dataPosition = eventBlock.find(DataPrefix);
    if (dataPosition == std::string_view::npos) {
      return;
    }

    std::string_view data = eventBlock.substr(dataPosition + DataPrefix.size());
    while (!data.empty() && (data.front() == ' ' || data.front() == '\t')) {
      data.remove_prefix(1);
    }
    while (!data.empty() && (data.back() == '\r' || data.back() == '\n')) {
      data.remove_suffix(1);
    }
    if (data.empty() || data == "[DONE]") {
      return;
    }

    const nlohmann::json event = nlohmann::json::parse(data);
    const std::string type = event.value("type", "");

    if (type == "response.created") {
      response.id = event.at("response").value("id", "");
      onEvent(
          {AssistantHost::OpenAIStreamEventType::Status, "Response created."});
    } else if (type == "response.in_progress") {
      onEvent(
          {AssistantHost::OpenAIStreamEventType::Status, "Model is working."});
    } else if (type == "response.reasoning_summary_text.delta") {
      onEvent({AssistantHost::OpenAIStreamEventType::ReasoningSummaryDelta,
               event.value("delta", "")});
    } else if (type == "response.output_text.delta") {
      const std::string delta = event.value("delta", "");
      response.text += delta;
      onEvent({AssistantHost::OpenAIStreamEventType::OutputTextDelta, delta});
    } else if (type == "response.output_item.done") {
      const nlohmann::json &item = event.at("item");
      if (item.value("type", "") == "function_call") {
        response.toolCalls.push_back(
            {.callId = item.value("call_id", ""),
             .name = item.value("name", ""),
             .arguments = item.value("arguments", "{}")});
      }
    } else if (type == "response.completed") {
      const nlohmann::json &completedResponse = event.at("response");
      if (response.id.empty()) {
        response.id = completedResponse.value("id", "");
      }
      if (completedResponse.contains("usage") &&
          !completedResponse.at("usage").is_null()) {
        const nlohmann::json &usage = completedResponse.at("usage");
        response.usage.inputTokens = usage.value("input_tokens", 0ULL);
        response.usage.outputTokens = usage.value("output_tokens", 0ULL);
        if (usage.contains("input_tokens_details") &&
            !usage.at("input_tokens_details").is_null()) {
          response.usage.cachedInputTokens =
              usage.at("input_tokens_details").value("cached_tokens", 0ULL);
        }
      }
      onEvent({AssistantHost::OpenAIStreamEventType::Status,
               "Response completed."});
    } else if (type == "response.failed" || type == "error") {
      throw std::runtime_error("OpenAI stream failed: " + event.dump());
    }
  };

  while (true) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.Get(), &available)) {
      throw std::runtime_error("Failed while reading the OpenAI response.");
    }
    if (available == 0) {
      break;
    }

    const std::size_t offset = responseBody.size();
    responseBody.resize(offset + available);
    DWORD bytesRead = 0;
    if (!WinHttpReadData(request.Get(), responseBody.data() + offset, available,
                         &bytesRead)) {
      throw std::runtime_error("Failed while reading the OpenAI response.");
    }
    responseBody.resize(offset + bytesRead);

    streamBuffer.append(responseBody.data() + offset, bytesRead);

    while (true) {
      std::size_t eventEnd = streamBuffer.find("\r\n\r\n");
      std::size_t separatorSize = 4;
      if (eventEnd == std::string::npos) {
        eventEnd = streamBuffer.find("\n\n");
        separatorSize = 2;
      }
      if (eventEnd == std::string::npos) {
        break;
      }

      const std::string eventBlock = streamBuffer.substr(0, eventEnd);
      streamBuffer.erase(0, eventEnd + separatorSize);
      processEvent(eventBlock);
    }
  }

  if (statusCode < 200 || statusCode >= 300) {
    std::string message = "OpenAI API error " + std::to_string(statusCode);
    try {
      const nlohmann::json error = nlohmann::json::parse(responseBody);
      message += ": " + error.at("error").value("message", responseBody);
    } catch (const nlohmann::json::exception &) {
      if (!responseBody.empty()) {
        message += ": " + responseBody;
      }
    }
    throw std::runtime_error(message);
  }

  if (!streamBuffer.empty()) {
    processEvent(streamBuffer);
  }

  return responseBody;
}
#endif
} // namespace

namespace AssistantHost {
namespace {
nlohmann::json CreateGameToolDefinitions() {
  const auto tool = [](const char *name, const char *description,
                       nlohmann::json properties, nlohmann::json required) {
    return nlohmann::json{{"type", "function"},
                          {"name", name},
                          {"description", description},
                          {"strict", true},
                          {"parameters",
                           {{"type", "object"},
                            {"properties", std::move(properties)},
                            {"required", std::move(required)},
                            {"additionalProperties", false}}}};
  };

  return nlohmann::json::array(
      {tool("list_game_files",
            "List editable C++ source files in the Game module.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("list_agent_skills",
            "List read-only repository skills and their trigger descriptions. "
            "Call this before implementation work.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("read_agent_skill",
            "Read one applicable repository SKILL.md completely before "
            "editing Game code.",
            {{"name",
              {{"type", "string"},
               {"description", "Skill name returned by list_agent_skills."}}}},
            {"name"}),
       tool("confirm_no_applicable_skills",
            "Confirm that no listed repository skill applies after reviewing "
            "the list. Provide a concrete reason.",
            {{"reason", {{"type", "string"}}}}, {"reason"}),
       tool("list_agent_documents",
            "List read-only top-level repository documents intended for AI "
            "agents.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("read_agent_document",
            "Read one top-level agent document completely.",
            {{"name", {{"type", "string"}}}}, {"name"}),
       tool("read_game_file",
            "Read one C++ source file inside the Game module.",
            {{"path",
              {{"type", "string"},
               {"description", "Path relative to the Game directory."}}}},
            {"path"}),
       tool("read_game_files",
            "Read several independent Game C++ source files in one call. "
            "Prefer this over repeated read_game_file calls.",
            {{"paths",
              {{"type", "array"},
               {"items", {{"type", "string"}}},
               {"minItems", 1},
               {"maxItems", 16}}}},
            {"paths"}),
       tool("list_engine_files",
            "List C++ source files in the read-only Engine tree.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("read_engine_file",
            "Read one C++ source file from Engine. Engine is read-only.",
            {{"path",
              {{"type", "string"},
               {"description", "Path relative to the Engine directory."}}}},
            {"path"}),
       tool("read_engine_files",
            "Read up to 16 Engine C++ source files in one call. "
            "Engine is read-only.",
            {{"paths",
              {{"type", "array"},
               {"items", {{"type", "string"}}},
               {"minItems", 1},
               {"maxItems", 16}}}},
            {"paths"}),
       tool("search_engine_code",
            "Search exact text across the read-only Engine C++ source tree.",
            {{"query", {{"type", "string"}}}}, {"query"}),
       tool("search_game_code",
            "Search exact text across Game C++ source files.",
            {{"query", {{"type", "string"}}}}, {"query"}),
       tool("apply_game_patch",
            "Replace one unique exact text block in an existing Game C++ "
            "file. Fails when oldText is missing or ambiguous.",
            {{"path", {{"type", "string"}}},
             {"oldText", {{"type", "string"}}},
             {"newText", {{"type", "string"}}}},
            {"path", "oldText", "newText"}),
       tool("apply_game_patches",
            "Apply up to 32 ordered unique exact-text replacements across "
            "active Game files in one call. The entire batch is validated "
            "before files are written. Prefer this over repeated single "
            "patches.",
            {{"patches",
              {{"type", "array"},
               {"minItems", 1},
               {"maxItems", 32},
               {"items",
                {{"type", "object"},
                 {"properties",
                  {{"path", {{"type", "string"}}},
                   {"oldText", {{"type", "string"}}},
                   {"newText", {{"type", "string"}}}}},
                 {"required", {"path", "oldText", "newText"}},
                 {"additionalProperties", false}}}}}},
            {"patches"}),
       tool("replace_game_code_file",
            "Atomically replace the complete contents of one existing Game "
            "C++ file. Use this for intentional whole-file rewrites such as "
            "replacing obsolete template orchestration; prefer exact patches "
            "for localized edits. Only .cpp and .h paths are accepted, and "
            "the complete content is validated as UTF-8 C++ source text.",
            {{"path",
              {{"type", "string"},
               {"description",
                "Existing .cpp or .h path relative to the Game."}}},
             {"content",
              {{"type", "string"},
               {"description", "Complete replacement C++ source text."}}}},
            {"path", "content"}),
       tool("create_game_code_file",
            "Create one new UTF-8 C++ file inside the active Game. Prefer this "
            "for cohesive new features, EntityTypes, ComponentTypes, vector "
            "art, or audio instead of growing unrelated catch-all files. Only "
            ".cpp and .h paths are accepted. Existing files are never "
            "overwritten, and content is validated as C++ source text.",
            {{"path",
              {{"type", "string"},
               {"description", "New .cpp or .h path relative to the Game."}}},
             {"content",
              {{"type", "string"},
               {"description", "Complete C++ source text for the new file."}}}},
            {"path", "content"}),
       tool("delete_game_code_file",
            "Delete one existing .cpp or .h file inside the active Game. Use "
            "only when the file is genuinely obsolete; this cannot target "
            "Engine files, build output, or other extensions.",
            {{"path",
              {{"type", "string"},
               {"description",
                "Existing .cpp or .h path relative to the Game."}}}},
            {"path"}),
       tool(
           "build_game",
           "Compile only the Debug Game DLL using the controlled CMake target.",
           nlohmann::json::object(), nlohmann::json::array()),
       tool("read_build_output",
            "Read output from the latest controlled Game build.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("inspect_crash_diagnostics",
            "Read the newest non-Release crash reports and process logs. "
            "Use this first when GameHost is in crash recovery mode.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("reload_game",
            "Request loading the latest successfully built Game DLL.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("get_reload_status", "Read the latest Game DLL reload status.",
            nlohmann::json::object(), nlohmann::json::array()),
       tool("launch_game",
            "In crash recovery mode, ask Launcher to start the repaired Game. "
            "Call only after a successful build.",
            nlohmann::json::object(), nlohmann::json::array())});
}

OpenAIResponse SendResponseRequest(const OpenAISettings &settings,
                                   std::string_view instructions,
                                   nlohmann::json input,
                                   std::string_view previousResponseId,
                                   const OpenAIStreamCallback &onEvent,
                                   bool allowTools) {
  nlohmann::json request{
      {"model", settings.model},
      {"instructions", instructions},
      {"input", std::move(input)},
      {"reasoning", {{"effort", "medium"}, {"summary", "auto"}}},
      {"store", true},
      {"stream", true}};
  if (allowTools) {
    request["tools"] = CreateGameToolDefinitions();
    request["tool_choice"] = "auto";
    request["parallel_tool_calls"] = true;
  }
  if (!previousResponseId.empty()) {
    request["previous_response_id"] = previousResponseId;
  }

#if defined(_WIN32)
  OpenAIResponse result;
  PostResponses(settings.apiKey, request.dump(), onEvent, result);
#else
  throw std::runtime_error(
      "The OpenAI HTTP transport is currently implemented for Windows only.");
#endif

  if (result.text.empty() && result.toolCalls.empty()) {
    throw std::runtime_error(
        "The OpenAI response contained neither text nor tool calls.");
  }
  return result;
}
} // namespace

OpenAIClient::OpenAIClient(OpenAISettings settings)
    : settings_(std::move(settings)) {}

bool OpenAIClient::IsConfigured() const { return settings_.IsConfigured(); }

const std::string &OpenAIClient::GetModel() const { return settings_.model; }

const std::string &OpenAIClient::GetDisplayName() const {
  static const std::string name = "OpenAI API";
  return name;
}

OpenAICostEstimate
OpenAIClient::EstimateCost(const OpenAITokenUsage &usage) const {
  return EstimateOpenAICost(settings_.model, settings_.pricing, usage);
}

OpenAIResponse
OpenAIClient::CreateResponse(std::string_view instructions,
                             std::string_view prompt,
                             const std::vector<OpenAIImageInput> &images,
                             std::string_view previousResponseId,
                             const OpenAIStreamCallback &onEvent) const {
  if (!IsConfigured()) {
    throw std::runtime_error(
        "OpenAI is not configured. Add apiKey and model to settings.json.");
  }

  nlohmann::json content = nlohmann::json::array(
      {{{"type", "input_text"}, {"text", std::string(prompt)}}});
  for (const OpenAIImageInput &image : images) {
    content.push_back({{"type", "input_image"},
                       {"detail", "auto"},
                       {"image_url", "data:" + image.mimeType + ";base64," +
                                         image.base64Data}});
  }
  nlohmann::json input = nlohmann::json::array(
      {{{"type", "message"}, {"role", "user"}, {"content", content}}});
  return SendResponseRequest(settings_, instructions, std::move(input),
                             previousResponseId, onEvent, true);
}

OpenAIResponse OpenAIClient::ContinueWithToolOutputs(
    std::string_view instructions, const std::vector<OpenAIToolOutput> &outputs,
    std::string_view previousResponseId, const OpenAIStreamCallback &onEvent,
    bool allowTools) const {
  nlohmann::json input = nlohmann::json::array();
  for (const OpenAIToolOutput &output : outputs) {
    input.push_back({{"type", "function_call_output"},
                     {"call_id", output.callId},
                     {"output", output.output}});
  }

  return SendResponseRequest(settings_, instructions, std::move(input),
                             previousResponseId, onEvent, allowTools);
}
} // namespace AssistantHost
