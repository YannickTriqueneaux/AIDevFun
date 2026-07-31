#include "Engine/Graphics/Color.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"
#include "Engine/Math/Vector2.h"
#include "Engine/Math/Vector3.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main()
{
    try
    {
        Engine::Vector2 position{2.0f, 3.0f};
        position += Engine::Vector2{4.0f, -1.0f};
        const Engine::Vector2 moved = position + Engine::Vector2{1.0f, 2.0f};
        const Engine::Vector2 scaled = moved * 2.0f;
        Require(std::abs(scaled.x - 14.0f) < 0.001f, "Vector2 x arithmetic failed.");
        Require(std::abs(scaled.y - 8.0f) < 0.001f, "Vector2 y arithmetic failed.");

        const Engine::Vector3 origin{};
        Require(origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f,
            "Vector3 defaults failed.");
        const Engine::Color white{};
        Require(white.red == 255 && white.green == 255 &&
            white.blue == 255 && white.alpha == 255,
            "Color defaults failed.");

#if defined(ENGINE_AUTOTESTS)
        Engine::InputSystem input;
        input.EnableAutoTestInput();
        input.SetAutoTestKeyDown(Engine::Key::Right, true);
        input.SetAutoTestKeyPressed(Engine::Key::Q, true);
        Require(input.IsDown(Engine::Key::Right), "Injected held key was not visible.");
        Require(input.WasPressed(Engine::Key::Q), "Injected pressed key was not visible.");
        input.ClearAutoTestPressedKeys();
        Require(!input.WasPressed(Engine::Key::Q), "Pressed keys were not cleared.");
#else
        throw std::runtime_error("EngineTests must not run without ENGINE_AUTOTESTS.");
#endif

        std::cout << "Engine pure tests passed.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Engine pure test failure: " << exception.what() << '\n';
        return 1;
    }
}
