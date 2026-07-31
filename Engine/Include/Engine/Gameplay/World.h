#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Gameplay/ObjectManager.h"

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace Engine::Gameplay {
struct ComponentType {
  TypeID id;
  std::string name;
  std::function<ObjectPtr(ObjectRef<Entity>)> create;
};
struct EntityType {
  TypeID id;
  std::string name;
  std::vector<TypeID> components;
};

class ENGINE_API World {
public:
  World();
  ~World();
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  World(World &&) noexcept;
  World &operator=(World &&) noexcept;

  void RegisterComponent(ComponentType type);
  void RegisterEntity(EntityType type);
  [[nodiscard]] ObjectRef<Entity> Spawn(TypeID entityType,
                                        std::string name = {});
  void Destroy(ObjectRef<Entity> entity);
  void FlushSpawns();
  void Update(float deltaTime);
  [[nodiscard]] std::vector<std::byte> Save() const;
  void Resume(std::span<const std::byte> state);
  [[nodiscard]] ObjectManager &Objects();
  [[nodiscard]] const ObjectManager &Objects() const;
  [[nodiscard]] const std::vector<ObjectID> &Entities() const;

private:
  class Impl;
  Impl *impl_ = nullptr;
};
} // namespace Engine::Gameplay
