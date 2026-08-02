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
    "providerSettings": "CodexProvider.settings.json"
  }
}
```

The paths are resolved relative to the built `AssistantHost` executable. CMake
copies the selected host settings, provider DLLs, and provider settings into
the build output.

## Codex provider

`AssistantProviderCodex.dll` launches the installed Codex CLI and uses its
existing ChatGPT sign-in. Its settings are stored in
`AssistantProviders/Codex/CodexProvider.settings.json`:

```json
{
  "executable": "",
  "model": "gpt-5.5",
  "reasoningEffort": "high"
}
```

- An empty `executable` enables automatic discovery. On Windows the provider
  first checks `%APPDATA%/npm/codex.cmd`, then falls back to `codex` from
  `PATH`.
- An empty `model` lets Codex use the model selected by the user's Codex
  configuration or the current Codex default.
- A non-empty `model` is passed explicitly to `codex exec --model`.
- `reasoningEffort` is passed as Codex model configuration.
- Pasted images are decoded to temporary files, passed with `--image`, and
  deleted after the response.

Codex runs with its native agent tools inside the active Game directory. The
provider does not store ChatGPT credentials in this repository.

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
