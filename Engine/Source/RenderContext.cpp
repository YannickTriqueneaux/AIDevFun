#include "Engine/Graphics/RenderContext.h"

#include "raylib.h"

namespace
{
    ::Color ToRaylibColor(Engine::Color color)
    {
        return {color.red, color.green, color.blue, color.alpha};
    }
}

namespace Engine
{
    void RenderContext::BeginFrame(Color clearColor)
    {
        BeginDrawing();
        ClearBackground(ToRaylibColor(clearColor));
    }

    void RenderContext::EndFrame()
    {
        EndDrawing();
    }

    Renderer2D& RenderContext::Draw2D()
    {
        return renderer2D_;
    }

    Renderer3D& RenderContext::Draw3D()
    {
        return renderer3D_;
    }

    int RenderContext::GetWidth() const
    {
        return GetScreenWidth();
    }

    int RenderContext::GetHeight() const
    {
        return GetScreenHeight();
    }
}
