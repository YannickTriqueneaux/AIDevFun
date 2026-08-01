#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Core/Memory.h"
#include "Engine/Gameplay/ObjectManager.h"
#include "Engine/Gameplay/World.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Engine {
class GameInstanceComponent {
public:
  virtual ~GameInstanceComponent() = default;
  [[nodiscard]] virtual Gameplay::TypeID GetTypeID() const = 0;
};

class ENGINE_API GameInstance {
public:
  GameInstance();
  ~GameInstance();
  GameInstance(const GameInstance &) = delete;
  GameInstance &operator=(const GameInstance &) = delete;

  [[nodiscard]] static GameInstance *GetInstance();
  void Activate();

  [[nodiscard]] Gameplay::ObjectManager *GetObjectManager();
  [[nodiscard]] const Gameplay::ObjectManager *GetObjectManager() const;
  [[nodiscard]] Gameplay::World *GetWorld();
  [[nodiscard]] const Gameplay::World *GetWorld() const;

  template <class T, class... Arguments>
  T *AddComponent(Arguments &&...arguments);
  template <class T> [[nodiscard]] T *GetComponent();
  template <class T> [[nodiscard]] const T *GetComponent() const;

private:
  using ComponentDestroyFunction = void (*)(GameInstanceComponent *) noexcept;
  void RegisterComponent(Gameplay::TypeID type,
                         GameInstanceComponent *component,
                         ComponentDestroyFunction destroy);
  [[nodiscard]] GameInstanceComponent *FindComponent(Gameplay::TypeID type);
  [[nodiscard]] const GameInstanceComponent *
  FindComponent(Gameplay::TypeID type) const;

  Gameplay::ObjectPoolDomainScope objectPoolDomain_;
  Gameplay::ObjectManager objectManager_;
  Gameplay::World world_;
  class Impl;
  Impl *impl_ = nullptr;
};

template <class T, class... Arguments>
T *GameInstance::AddComponent(Arguments &&...arguments) {
  static_assert(std::is_base_of_v<GameInstanceComponent, T>);
  auto owner = NEW_MEMORY(T, std::forward<Arguments>(arguments)...);
  T *component = owner.get();
  RegisterComponent(component->GetTypeID(), component,
                    [](GameInstanceComponent *value) noexcept {
                      T *concrete = static_cast<T *>(value);
                      DELETE_MEMORY(concrete);
                    });
  owner.release();
  return component;
}

template <class T> T *GameInstance::GetComponent() {
  static_assert(std::is_base_of_v<GameInstanceComponent, T>);
  return dynamic_cast<T *>(FindComponent(T::Type));
}

template <class T> const T *GameInstance::GetComponent() const {
  static_assert(std::is_base_of_v<GameInstanceComponent, T>);
  return dynamic_cast<const T *>(FindComponent(T::Type));
}
} // namespace Engine
