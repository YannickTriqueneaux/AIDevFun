#pragma once

#include "Engine/Core/Export.h"

#include <string>

namespace Engine
{
    class GameInterface;

    struct ApplicationConfig
    {
        int windowWidth = 1280;
        int windowHeight = 720;
        int targetFramesPerSecond = 144;
        std::string windowTitle = "Engine Application";
        std::string focusWindowTitle;
        bool verticalSync = true;
    };

    class ENGINE_API Application
    {
    public:
        explicit Application(ApplicationConfig config);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run(GameInterface& game);

    private:
        ApplicationConfig config_;
    };
}
