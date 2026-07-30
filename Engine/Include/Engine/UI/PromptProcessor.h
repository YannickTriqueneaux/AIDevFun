#pragma once

#include "Engine/UI/PromptMessage.h"

#include <string_view>
#include <vector>

namespace Engine
{
    class PromptProcessor
    {
    public:
        [[nodiscard]] std::vector<PromptMessage> Process(
            std::string_view prompt) const;
    };
}

