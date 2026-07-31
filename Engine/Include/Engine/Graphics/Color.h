#pragma once

#include <cstdint>

namespace Engine {
struct Color {
  std::uint8_t red = 255;
  std::uint8_t green = 255;
  std::uint8_t blue = 255;
  std::uint8_t alpha = 255;
};
} // namespace Engine
