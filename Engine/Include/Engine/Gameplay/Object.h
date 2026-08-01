#pragma once
#include "Engine/Gameplay/ObjectID.h"
#include "Engine/Gameplay/StateArchive.h"
#include "Engine/Math/Vector2.h"
#include <string>
#include <vector>
namespace Engine::Gameplay {
class ObjectManager;
class Component;
class Object {
public:
  virtual ~Object() = default;
  [[nodiscard]] ObjectID GetObjectID() const { return id_; }
  [[nodiscard]] virtual TypeID GetTypeID() const = 0;

private:
  friend class ObjectManager;
  ObjectID id_{};
};
template <class T = Object> class ObjectRef {
public:
  ObjectRef() = default;
  explicit ObjectRef(ObjectID id) : id_(id) {}
  explicit ObjectRef(const Object &object) : id_(object.GetObjectID()) {}
  [[nodiscard]] ObjectID GetID() const { return id_; }
  [[nodiscard]] bool IsAssigned() const { return id_.IsValid(); }
  [[nodiscard]] T *Resolve() const;
  auto operator<=>(const ObjectRef &) const = default;

private:
  ObjectID id_{};
};
struct Transform {
  Engine::Vector2 position{};
  float rotation = 0.0f;
  Engine::Vector2 scale{1.0f, 1.0f};
};
class Entity final : public Object {
public:
  Entity(TypeID typeID, std::string name)
      : typeID_(typeID), name_(std::move(name)) {}
  [[nodiscard]] TypeID GetTypeID() const override { return typeID_; }
  [[nodiscard]] const std::string &GetName() const { return name_; }
  template <class T> [[nodiscard]] ObjectRef<T> GetComponent() const;
  Transform transform;
  std::vector<ObjectRef<Component>> components;

private:
  TypeID typeID_;
  std::string name_;
};
class Component : public Object {
public:
  explicit Component(ObjectRef<Entity> owner) : owner_(owner) {}
  virtual void Update(float) {}
  [[nodiscard]] ObjectRef<Entity> GetOwner() const { return owner_; }
  [[nodiscard]] virtual std::uint32_t CurrentStateVersion() const = 0;
  [[nodiscard]] virtual std::uint32_t MinimumStateVersion() const = 0;
  virtual void SaveState(StateWriter &) const = 0;
  virtual void LoadState(StateReader &, std::uint32_t) = 0;

private:
  ObjectRef<Entity> owner_;
};
} // namespace Engine::Gameplay
