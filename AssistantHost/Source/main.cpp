#include "Engine/Application/AssistantApplication.h"
#include "Engine/Core/Settings.h"
#include "Engine/Core/Logger.h"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path executableDirectory =
            std::filesystem::absolute(
                argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{})
                .parent_path();
        Engine::Logger::Initialize(
            executableDirectory / "Logs" / "AssistantHost.log");
        Engine::Logger::Info("AssistantHost started.");
        const Engine::LauncherSettings settings = Engine::Settings::Load(
            executableDirectory / "settings.json");
        Engine::Logger::Info(
            "Settings loaded. Model: " + settings.openAI.model +
            ". API key configured: " +
            (settings.openAI.apiKey.empty() ? "no." : "yes."));

        Engine::AssistantApplication application(settings.openAI);
        application.Run();
        Engine::Logger::Info("AssistantHost application loop ended.");
    }
    catch (const std::exception& exception)
    {
        Engine::Logger::Error(exception.what());
        std::cerr << "AssistantHost fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
