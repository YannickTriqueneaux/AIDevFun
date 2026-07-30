#pragma once

#include "Engine/Graphics/Color.h"

namespace Engine
{
    class InputSystem;
    class RenderContext;
    class UiSystem;

    class GameInterface
    {
    public:
        virtual ~GameInterface() = default;

        virtual void Initialize() {}
        virtual void Update(const InputSystem& input, float deltaTime) = 0;
        [[nodiscard]] virtual Color GetClearColor() const = 0;
        virtual void Render(RenderContext& context) const = 0;
        virtual void RenderUi(UiSystem&) {}
        virtual void Shutdown() {}
    };
}
