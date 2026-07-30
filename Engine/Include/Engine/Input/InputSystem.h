#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Input/Key.h"

namespace Engine
{
    class ENGINE_API InputSystem
    {
    public:
        void Update();

        [[nodiscard]] bool IsDown(Key key) const;
        [[nodiscard]] bool WasPressed(Key key) const;
    };
}
