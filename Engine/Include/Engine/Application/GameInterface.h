#pragma once

namespace Engine
{
    class InputSystem;
    class Renderer2D;

    class GameInterface
    {
    public:
        virtual ~GameInterface() = default;

        virtual void Initialize() {}
        virtual void Update(const InputSystem& input, float deltaTime) = 0;
        virtual void Render(Renderer2D& renderer) const = 0;
        virtual void Shutdown() {}
    };
}

