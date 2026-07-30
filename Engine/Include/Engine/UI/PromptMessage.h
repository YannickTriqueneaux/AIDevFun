#pragma once

#include <string>

namespace Engine
{
    enum class PromptMessageRole
    {
        User,
        Result,
        Information
    };

    struct PromptMessage
    {
        PromptMessageRole role = PromptMessageRole::Information;
        std::string text;
    };
}

