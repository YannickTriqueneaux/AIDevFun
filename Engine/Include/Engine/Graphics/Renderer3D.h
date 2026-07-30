#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Camera3D.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Math/Vector3.h"

namespace Engine
{
    class ShaderProgram;

    class ENGINE_API Renderer3D
    {
    public:
        void Begin(const Camera3D& camera);
        void End();
        void BeginShader(const ShaderProgram& shader);
        void EndShader();

        void DrawGrid(int slices, float spacing);
        void DrawLine(Vector3 start, Vector3 end, Color color);
        void DrawCube(Vector3 position, Vector3 size, Color color);
        void DrawCubeOutline(Vector3 position, Vector3 size, Color color);
        void DrawSphere(Vector3 center, float radius, Color color);

    private:
        bool active_ = false;
        bool shaderActive_ = false;
    };
}
