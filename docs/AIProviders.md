# AI providers

The assistant backend is selected at runtime. `AssistantHost` owns the user
interface and conversation workflow, while a dynamically loaded provider DLL
owns vendor-specific authentication, transport, models, and settings.

This boundary keeps AI SDKs and HTTP dependencies out of `Engine` and allows a
provider such as OpenAI, Codex, Claude, or a local model to be added without
changing gameplay code.

## Selecting a provider

`AssistantHost/Config/settings.json` selects one provider DLL and the settings
file consumed by that DLL:

```json
{
  "assistant": {
    "providerLibrary": "AssistantProviderCodex.dll",
    "providerSettings": "CodexProvider.settings.json",
    "promptConfig": "AssistantPrompts.json"
  }
}
```

The paths are resolved relative to the built `AssistantHost` executable. CMake
copies the selected host settings, provider DLLs, and provider settings into
the build output.

## Shared assistant prompts

`AssistantHost/Config/AssistantPrompts.json` contains the provider-neutral
instructions sent to every assistant. `settings.json` selects it with
`assistant.promptConfig`, and CMake copies it beside `AssistantHost`. Keep
creative workflow, Game architecture, tool discipline, and completion policy in
this JSON instead of C++ so prompt behavior can be iterated without recompiling.
The file is reloaded at the start of every submitted prompt, so edits apply to
the next request without restarting `AssistantHost`.
Provider code should add no duplicate behavioral prompt; provider-specific
transport and security constraints remain in provider configuration and code.
## Codex provider

`AssistantProviderCodex.dll` launches the installed Codex CLI and uses its
existing ChatGPT sign-in. Its settings are stored in
`AssistantProviders/Codex/CodexProvider.settings.json`:

```json
{
  "executable": "",
  "gameToolsMcpExecutable": "",
  "model": "",
  "reasoningEffort": "low"
}
```

- An empty `executable` enables automatic discovery. On Windows the provider
  first checks `%APPDATA%/npm/codex.cmd`, then falls back to `codex` from
  `PATH`.
- An empty `model` lets Codex use the model selected by the user's Codex
  configuration or the current Codex default. This is the repository default
  so new users receive a model compatible with their ChatGPT plan.
- A non-empty `model` is passed explicitly to `codex exec --model`.
- `reasoningEffort` is passed as Codex model configuration.
- Pasted images are decoded to temporary files, passed with `--image`, and
  deleted after the response.
- The provider captures the Codex thread ID from JSONL events and uses `codex exec resume` for the next prompt, so short follow-ups retain the preceding request, inspection, tool results, and decisions.
- An empty `gameToolsMcpExecutable` selects `GameToolsMcpServer.exe` beside the
  provider settings. The MCP server exposes the restricted Game tools and
  relays them to `GameToolService` through its existing named pipe.

Codex runs with its local shell disabled and its filesystem sandbox in
read-only mode. Game reads, writes, builds, reloads, and recovery operations
must use the restricted Game Tools MCP server. The provider does not store
ChatGPT credentials in this repository.

`Start.bat` uses `Tools/SetupDevelopmentEnvironment.ps1` to install a missing
Codex CLI through OpenAI's official Windows installer, run `codex login` when
no cached session exists, and skip both steps when they are already complete.

The provider enables Codex MCP tool discovery because current Codex CLI builds
defer custom MCP tools until the model searches for a relevant operation. Each
tool also declares MCP safety annotations, allowing read-only operations and
controlled Game mutations to be handled according to their actual effects.

For a resumed Codex thread, the provider starts the fresh stdio MCP bridge with an inherited-guidance marker. The first prompt still has to inspect applicable skills and `Architecture.md`; later prompts in that same thread reuse the conversation context and are not gated on rereading identical documents. New applicable domains still require their skill.

The MCP bridge exposes repository skills and top-level agent documents as
read-only tools. Before it permits a Game mutation, build, reload, or recovery
launch, Codex must have listed the available skills and read
at least one applicable skill plus `docs/Architecture.md`. When none of the
listed skills applies, Codex must explicitly record that decision and its
reason through `confirm_no_applicable_skills`. Provider instructions require
reading every applicable skill and `docs/AIProviders.md` for provider work.

The provider parses the JSON Lines stream from `codex exec --json` while Codex
is running. Commands, file changes, tool calls, searches, reasoning summaries,
completion status, and token usage are converted into the generic provider
events displayed by the Assistant activity console. Unknown event types are
ignored for forward compatibility, and recent raw output is retained in CLI
failure diagnostics.

## OpenAI API provider

`AssistantProviderOpenAI.dll` calls the OpenAI Responses API. Its local settings
are stored in `AssistantProviders/OpenAI/OpenAIProvider.settings.json`; the
versioned example is `OpenAIProvider.settings.example.json`.

```json
{
  "model": "gpt-5.5",
  "apiKey": "",
  "pricing": {
    "model": "gpt-5.5",
    "inputUsdPerMillion": 5.0,
    "cachedInputUsdPerMillion": 0.5,
    "outputUsdPerMillion": 30.0
  }
}
```

The provider owns API authentication, streaming Responses API calls, tool-call
continuation, token usage, and configured cost estimates. The credential-bearing
settings file is ignored by Git.

## Provider contract

The shared contract is declared in
`Development/Include/Development/AssistantProvider.h`. A provider implements
`Development::AssistantProvider` and exports:

```cpp
extern "C" const Development::AssistantProviderApi *GetAssistantProviderApi();
```

The exported API supplies provider creation and destruction functions. The
loader validates `AssistantProviderApiVersion` before constructing the provider.
Creation receives only:

- the provider's settings path;
- the active Game root.

The interface supports configuration reporting, model identification, cost
estimation, streamed status/reasoning/output events, image inputs, response
continuation, and generic tool results.

## Adding another provider

To add a provider:

1. Create a directory under `AssistantProviders/<ProviderName>/`.
2. Implement `Development::AssistantProvider` without adding vendor types to
   the shared interface.
3. Export a version-compatible `GetAssistantProviderApi` function.
4. Give the DLL its own settings schema and settings file.
5. Add a CMake shared-library target and copy its runtime files beside
   `AssistantHost`.
6. Select the new DLL in `AssistantHost/Config/settings.json`.
7. Test DLL loading, response streaming, attachments, failure handling, and any
   supported tool-call continuation.

Provider dependencies must link only into their provider DLL. They must not be
added to `Engine`, and provider settings must not be parsed by `Engine` or by
unrelated provider DLLs.
