#include "AssistantHost/AssistantApplication.h"
#include "AssistantHost/MidiAttachmentProvider.h"
#include "AssistantHost/Settings.h"
#include "Engine/Core/CrashDiagnostics.h"
#include "Engine/Core/Logger.h"

#include <exception>
#include <filesystem>
#include <iostream>

#ifndef MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
#define MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT "UnknownGame"
#endif
#ifndef MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR
#define MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR "."
#endif

int main(int argc, char **argv) {
  try {
    const std::filesystem::path executableDirectory =
        std::filesystem::absolute(argc > 0 ? std::filesystem::path(argv[0])
                                           : std::filesystem::path{})
            .parent_path();
    Engine::Logger::Initialize(executableDirectory / "Logs" /
                               "AssistantHost.log");
    Engine::CrashDiagnostics::Install(executableDirectory / "Crashes",
                                      "AssistantHost");
    Engine::Logger::Info("AssistantHost started.");
    const AssistantHost::LauncherSettings settings =
        AssistantHost::Settings::Load(executableDirectory / "settings.json");
    Engine::Logger::Info(
        "Assistant provider selected: " + settings.providerLibrary + ".");

    AssistantHost::MidiAttachmentProvider midiAttachments;
    AssistantHost::AssistantApplication application(
        settings, MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT, executableDirectory,
        MAKE_YOUR_OWN_GAME_AI_ACTIVE_GAME_DIR, &midiAttachments);
    application.Run();
    Engine::Logger::Info("AssistantHost application loop ended.");
  } catch (const std::exception &exception) {
    Engine::Logger::Error(exception.what());
    std::cerr << "AssistantHost fatal error: " << exception.what() << '\n';
    return 1;
  }

  return 0;
}
