#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Gameplay/Object.h"
#include "Engine/Gameplay/ObjectMemory.h"

#include <string>

namespace Engine::Gameplay {
ENGINE_API ObjectManager *GetActiveObjectManager();

class ENGINE_API ObjectManager {
public:
  ObjectManager();
  ~ObjectManager();
  ObjectManager(const ObjectManager &) = delete;
  ObjectManager &operator=(const ObjectManager &) = delete;
  ObjectManager(ObjectManager &&) noexcept;
  ObjectManager &operator=(ObjectManager &&) noexcept;

  ObjectID Reserve();
  ObjectID Add(ObjectPtr object);
  void BindReserved(ObjectID id, ObjectPtr object);
  void Restore(ObjectID id, ObjectPtr object);
  void RejectRestored(ObjectID id);
  void Destroy(ObjectID id);
  void Clear();

  [[nodiscard]] Object *GetObject(ObjectID id);
  [[nodiscard]] const Object *GetObject(ObjectID id) const;
  template <class T> [[nodiscard]] T *GetAs(ObjectID id) {
    return dynamic_cast<T *>(GetObject(id));
  }
  template <class T> [[nodiscard]] const T *GetAs(ObjectID id) const {
    return dynamic_cast<const T *>(GetObject(id));
  }

  void SetDebugName(ObjectID id, std::string name);
  [[nodiscard]] const std::string &GetDebugName(ObjectID id) const;
  [[nodiscard]] std::size_t LiveCount() const;

private:
  class Impl;
  Impl *impl_ = nullptr;
};

template <class T> T *ObjectRef<T>::Resolve() const {
  ObjectManager *manager = GetActiveObjectManager();
  return manager ? dynamic_cast<T *>(manager->GetObject(id_)) : nullptr;
}

template <class T> ObjectRef<T> Entity::GetComponent() const {
  ObjectManager *manager = GetActiveObjectManager();
  if (!manager)
    return {};
  for (const ObjectRef<Component> component : components) {
    if (manager->GetAs<T>(component.GetID()))
      return ObjectRef<T>(component.GetID());
  }
  return {};
}
} // namespace Engine::Gameplay
