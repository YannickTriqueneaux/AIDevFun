#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Math/Vector2.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace Engine {
struct VectorShapeDrawParameters;

struct VectorShapeGroupID {
  static constexpr std::uint32_t InvalidIndex =
      std::numeric_limits<std::uint32_t>::max();
  std::uint32_t index = InvalidIndex;
  std::uint32_t resource = 0;

  [[nodiscard]] constexpr bool IsValid() const {
    return index != InvalidIndex && resource != 0;
  }
  constexpr auto operator<=>(const VectorShapeGroupID &) const = default;
};

struct VectorShapeTransform {
  Vector2 translation{};
  float rotationDegrees = 0.0f;
  Vector2 scale{1.0f, 1.0f};
  float opacity = 1.0f;
};

class ENGINE_API VectorShape {
public:
  VectorShape();
  ~VectorShape();
  VectorShape(const VectorShape &) = delete;
  VectorShape &operator=(const VectorShape &) = delete;
  VectorShape(VectorShape &&other) noexcept;
  VectorShape &operator=(VectorShape &&other) noexcept;

  [[nodiscard]] bool LoadFromSvg(std::string_view source);
  [[nodiscard]] bool Upload();
  void Unload();

  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] bool IsUploaded() const;
  [[nodiscard]] Vector2 GetViewBoxSize() const;
  [[nodiscard]] std::size_t GetGroupCount() const;
  [[nodiscard]] std::size_t GetTriangleCount() const;
  [[nodiscard]] VectorShapeGroupID ResolveGroup(std::string_view name) const;
  template <std::size_t Count>
  [[nodiscard]] std::array<VectorShapeGroupID, Count>
  ResolveGroups(const std::array<std::string_view, Count> &names) const {
    std::array<VectorShapeGroupID, Count> result{};
    for (std::size_t index = 0; index < Count; ++index)
      result[index] = ResolveGroup(names[index]);
    return result;
  }
  [[nodiscard]] bool HasGroup(std::string_view id) const;
  [[nodiscard]] const std::string &GetLastError() const;

private:
  void Draw(const VectorShapeDrawParameters &parameters) const;
  struct Implementation;
  Implementation *implementation_ = nullptr;

  friend class Renderer2D;
  friend class VectorShapePose;
  friend class VectorShapeAnimation;
};

class ENGINE_API VectorShapePose {
public:
  explicit VectorShapePose(const VectorShape &shape);
  ~VectorShapePose();
  VectorShapePose(const VectorShapePose &) = delete;
  VectorShapePose &operator=(const VectorShapePose &) = delete;
  VectorShapePose(VectorShapePose &&other) noexcept;
  VectorShapePose &operator=(VectorShapePose &&other) noexcept;

  void Reset();
  [[nodiscard]] bool SetGroupTransform(VectorShapeGroupID id,
                                       VectorShapeTransform transform);
  [[nodiscard]] bool GetGroupTransform(VectorShapeGroupID id,
                                       VectorShapeTransform &transform) const;

private:
  struct Implementation;
  Implementation *implementation_ = nullptr;

  friend class Renderer2D;
  friend class VectorShape;
  friend class VectorShapeAnimation;
};

class ENGINE_API VectorShapeAnimation {
public:
  explicit VectorShapeAnimation(const VectorShape &shape);
  ~VectorShapeAnimation();
  VectorShapeAnimation(const VectorShapeAnimation &) = delete;
  VectorShapeAnimation &operator=(const VectorShapeAnimation &) = delete;
  VectorShapeAnimation(VectorShapeAnimation &&other) noexcept;
  VectorShapeAnimation &operator=(VectorShapeAnimation &&other) noexcept;

  void SetDuration(float seconds);
  [[nodiscard]] float GetDuration() const;
  [[nodiscard]] bool AddKeyframe(VectorShapeGroupID groupId, float timeSeconds,
                                 VectorShapeTransform transform);
  void Clear();
  [[nodiscard]] bool Sample(float timeSeconds, bool loop,
                            VectorShapePose &pose) const;

private:
  struct Implementation;
  Implementation *implementation_ = nullptr;
};

struct VectorShapeDrawParameters {
  Vector2 position{};
  Vector2 size{};
  float rotationDegrees = 0.0f;
  Color tint{};
  const VectorShapePose *pose = nullptr;
};
} // namespace Engine
