#pragma once

#include "Engine/Graphics/Color.h"
#include <cstddef>
#include <span>
#include <vector>

namespace Engine
{
    class InputSystem;
    class RenderContext;
    class Serializer;
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
        [[nodiscard]] virtual std::vector<std::byte> SaveResumeState() const { return {}; }
        virtual void ResumeFromState(std::span<const std::byte>) {}
#if defined(ENGINE_AUTOTESTS)
        // Development-only, in-process state inspection hook.
        virtual void SerializeAutoTestState(Serializer&) {}
#endif
    };
}
