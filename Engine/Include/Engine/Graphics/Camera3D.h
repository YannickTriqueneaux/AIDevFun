#pragma once

#include "Engine/Math/Vector3.h"

namespace Engine
{
    enum class CameraProjection
    {
        Perspective,
        Orthographic
    };

    struct Camera3D
    {
        Vector3 position{0.0f, 10.0f, 10.0f};
        Vector3 target{};
        Vector3 up{0.0f, 1.0f, 0.0f};
        float fieldOfView = 45.0f;
        CameraProjection projection = CameraProjection::Perspective;
    };
}
