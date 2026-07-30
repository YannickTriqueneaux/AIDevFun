#include "Engine/Application/AssistantApplication.h"
#include "Engine/Core/Settings.h"

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
        const Engine::LauncherSettings settings = Engine::Settings::Load(
            executableDirectory / "settings.json");

        Engine::AssistantApplication application(settings.openAI);
        application.Run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "AssistantHost fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}

