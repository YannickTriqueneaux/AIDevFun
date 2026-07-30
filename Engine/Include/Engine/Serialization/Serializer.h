#pragma once

#include <string>
#include <string_view>

namespace Engine
{
    class Serializer
    {
    public:
        virtual ~Serializer() = default;

        virtual void Value(std::string_view name, bool& value) = 0;
        virtual void Value(std::string_view name, int& value) = 0;
        virtual void Value(std::string_view name, float& value) = 0;
        virtual void Value(std::string_view name, std::string& value) = 0;
    };

    class Serializable
    {
    public:
        virtual ~Serializable() = default;
        virtual void Serialize(Serializer& serializer) = 0;
    };
}

