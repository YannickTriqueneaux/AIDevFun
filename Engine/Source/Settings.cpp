#include "Engine/Core/Settings.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace Engine
{
    LauncherSettings Settings::Load(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if (!stream)
        {
            throw std::runtime_error(
                "Settings file not found: " + path.string());
        }

        nlohmann::json document;
        try
        {
            stream >> document;
        }
        catch (const nlohmann::json::exception& exception)
        {
            throw std::runtime_error(
                "Invalid settings JSON: " + std::string(exception.what()));
        }

        LauncherSettings settings;
        const nlohmann::json& openAI = document.at("openai");
        settings.openAI.apiKey = openAI.value("apiKey", "");
        settings.openAI.model = openAI.value("model", "gpt-5.5");
        return settings;
    }
}

