#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Math/Vector2.h"

#include <string_view>

namespace Engine
{
    class ENGINE_API Renderer2D
    {
    public:
        void DrawCircle(Vector2 center, float radius, Color color);
        void DrawCircleOutline(Vector2 center, float radius, Color color);
        void DrawLine(Vector2 start, Vector2 end, float thickness, Color color);
        void DrawText(std::string_view text, Vector2 position, int fontSize, Color color);
        void DrawFramesPerSecond(int x, int y);

        [[nodiscard]] int GetWidth() const;
        [[nodiscard]] int GetHeight() const;
    };
}
