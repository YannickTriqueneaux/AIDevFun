#pragma once

namespace Engine
{
    struct Vector2
    {
        float x = 0.0f;
        float y = 0.0f;

        Vector2& operator+=(const Vector2& other)
        {
            x += other.x;
            y += other.y;
            return *this;
        }
    };

    inline Vector2 operator*(Vector2 vector, float scalar)
    {
        return {vector.x * scalar, vector.y * scalar};
    }

    inline Vector2 operator+(Vector2 left, const Vector2& right)
    {
        left += right;
        return left;
    }
}
