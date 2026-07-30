#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Graphics/Renderer3D.h"

namespace Engine
{
    class ENGINE_API RenderContext
    {
    public:
        void BeginFrame(Color clearColor);
        void EndFrame();

        [[nodiscard]] Renderer2D& Draw2D();
        [[nodiscard]] Renderer3D& Draw3D();
        [[nodiscard]] int GetWidth() const;
        [[nodiscard]] int GetHeight() const;

    private:
        Renderer2D renderer2D_;
        Renderer3D renderer3D_;
    };
}
