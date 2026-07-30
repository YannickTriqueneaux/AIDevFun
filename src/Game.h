#pragma once

#include "Input.h"
#include "Player.h"

class Game
{
public:
    void Update(float deltaTime);
    void Draw() const;

private:
    Input input_;
    Player player_;
};

