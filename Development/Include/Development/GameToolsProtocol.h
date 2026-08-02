#pragma once

#include <string>

#ifndef MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
#define MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT "UnknownGame"
#endif

namespace Development {
inline std::string GetGameToolsPipeName() {
  return R"(\\.\pipe\MakeYourOwnGame.AI.)" MAKE_YOUR_OWN_GAME_AI_GAME_PROJECT
         R"(.GameTools.v1)";
}
} // namespace Development
