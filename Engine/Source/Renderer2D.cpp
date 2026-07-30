#include "Engine/Graphics/Renderer2D.h"

#include "raylib.h"

#include <string>

namespace
{
    ::Color ToRaylibColor(Engine::Color color)
    {
        return {color.red, color.green, color.blue, color.alpha};
    }

    ::Vector2 ToRaylibVector(Engine::Vector2 vector)
    {
        return {vector.x, vector.y};
    }
}

namespace Engine
{
    void Renderer2D::BeginFrame(Color clearColor)
    {
        BeginDrawing();
        ClearBackground(ToRaylibColor(clearColor));
    }

    void Renderer2D::EndFrame()
    {
        EndDrawing();
    }

    void Renderer2D::DrawCircle(Vector2 center, float radius, Color color)
    {
        DrawCircleV(ToRaylibVector(center), radius, ToRaylibColor(color));
    }

    void Renderer2D::DrawCircleOutline(Vector2 center, float radius, Color color)
    {
        DrawCircleLinesV(ToRaylibVector(center), radius, ToRaylibColor(color));
    }

    void Renderer2D::DrawLine(
        Vector2 start,
        Vector2 end,
        float thickness,
        Color color)
    {
        DrawLineEx(
            ToRaylibVector(start),
            ToRaylibVector(end),
            thickness,
            ToRaylibColor(color));
    }

    void Renderer2D::DrawText(
        std::string_view text,
        Vector2 position,
        int fontSize,
        Color color)
    {
        const std::string terminatedText(text);
        ::DrawText(
            terminatedText.c_str(),
            static_cast<int>(position.x),
            static_cast<int>(position.y),
            fontSize,
            ToRaylibColor(color));
    }

    void Renderer2D::DrawFramesPerSecond(int x, int y)
    {
        DrawFPS(x, y);
    }

    int Renderer2D::GetWidth() const
    {
        return GetScreenWidth();
    }

    int Renderer2D::GetHeight() const
    {
        return GetScreenHeight();
    }
}

