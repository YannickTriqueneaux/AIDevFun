#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Math/Vector2.h"
#include "Engine/Math/Vector3.h"

#include <string_view>

namespace Engine
{
    class ENGINE_API ShaderProgram
    {
    public:
        ShaderProgram();
        ~ShaderProgram();

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;
        ShaderProgram(ShaderProgram&& other) noexcept;
        ShaderProgram& operator=(ShaderProgram&& other) noexcept;

        [[nodiscard]] bool LoadFromMemory(
            std::string_view vertexSource,
            std::string_view fragmentSource);
        void Unload();
        [[nodiscard]] bool IsValid() const;

        void SetFloat(std::string_view name, float value) const;
        void SetInt(std::string_view name, int value) const;
        void SetVector2(std::string_view name, Vector2 value) const;
        void SetVector3(std::string_view name, Vector3 value) const;
        void SetColor(std::string_view name, Color value) const;

    private:
        struct Implementation;
        Implementation* implementation_ = nullptr;

        void BeginUse() const;

        friend class Renderer3D;
    };
}
