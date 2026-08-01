#include "Engine/Gameplay/World.h"
#include "Engine/Core/Memory.h"
#include "Engine/Core/Profile.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace Engine::Gameplay {
namespace {
constexpr std::uint32_t Magic = 0x31525747u;
constexpr std::uint32_t Format = 1;
} // namespace

class World::Impl {
public:
  explicit Impl(ObjectManager &objectManager) : objects(objectManager) {}
  struct SpawnRequest {
    ObjectID reserved;
    TypeID type;
    std::string name;
  };
  void CreateEntity(ObjectID id, TypeID type, const std::string &requestedName,
                    const std::vector<ObjectID> *restored);

  ObjectManager &objects;
  std::vector<ComponentType> componentTypes;
  std::unordered_map<TypeID, std::size_t> componentLookup;
  std::unordered_map<TypeID, EntityType> entityTypes;
  std::vector<ObjectID> entities;
  std::vector<SpawnRequest> spawns;
  std::vector<ObjectID> destroys;
};

World::World(ObjectManager &objects)
    : impl_(NEW_MEMORY(Impl, objects).release()) {}
World::~World() { DELETE_MEMORY(impl_); }
World::World(World &&other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}
World &World::operator=(World &&other) noexcept {
  if (this != &other) {
    DELETE_MEMORY(impl_);
    impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
}

void World::RegisterComponent(ComponentType type) {
  if (type.id == 0 || !type.create || !type.updatePool ||
      impl_->componentLookup.contains(type.id))
    throw std::invalid_argument("Invalid or duplicate ComponentType.");
  impl_->componentLookup[type.id] = impl_->componentTypes.size();
  impl_->componentTypes.push_back(std::move(type));
}

void World::RegisterEntity(EntityType type) {
  if (type.id == 0 || impl_->entityTypes.contains(type.id))
    throw std::invalid_argument("Invalid or duplicate EntityType.");
  for (TypeID component : type.components)
    if (!impl_->componentLookup.contains(component))
      throw std::invalid_argument(
          "Entity references an unregistered ComponentType.");
  impl_->entityTypes.emplace(type.id, std::move(type));
}

ObjectRef<Entity> World::Spawn(TypeID type, std::string name) {
  if (!impl_->entityTypes.contains(type))
    throw std::invalid_argument("Unknown EntityType.");
  const ObjectID id = impl_->objects.Reserve();
  impl_->spawns.push_back({id, type, std::move(name)});
  return ObjectRef<Entity>(id);
}

void World::Destroy(ObjectRef<Entity> entity) {
  if (entity.IsAssigned())
    impl_->destroys.push_back(entity.GetID());
}

void World::Impl::CreateEntity(ObjectID id, TypeID type,
                               const std::string &requestedName,
                               const std::vector<ObjectID> *restored) {
  const auto entityType = entityTypes.find(type);
  if (entityType == entityTypes.end())
    throw std::runtime_error("Saved EntityType is no longer registered.");
  const std::string name =
      requestedName.empty() ? entityType->second.name : requestedName;
  ObjectPtr entityOwner = NEW_OBJECT(Entity, type, name);
  Entity *entity = static_cast<Entity *>(entityOwner.get());
  if (restored)
    objects.Restore(id, std::move(entityOwner));
  else
    objects.BindReserved(id, std::move(entityOwner));
  entities.push_back(id);
  objects.SetDebugName(id, name);
  for (std::size_t i = 0; i < entityType->second.components.size(); ++i) {
    const TypeID componentType = entityType->second.components[i];
    const auto &descriptor = componentTypes[componentLookup.at(componentType)];
    ObjectPtr component = descriptor.create(ObjectRef<Entity>(id));
    const ObjectID componentID = restored ? restored->at(i) : objects.Reserve();
    if (restored)
      objects.Restore(componentID, std::move(component));
    else
      objects.BindReserved(componentID, std::move(component));
    entity->components.emplace_back(componentID);
    objects.SetDebugName(componentID, name + "+" + descriptor.name);
  }
}

void World::FlushSpawns() {
  ENGINE_PROFILE_SCOPE("Flush Spawns");
  for (ObjectID id : impl_->destroys) {
    auto *entity = impl_->objects.GetAs<Entity>(id);
    if (!entity)
      continue;
    for (ObjectRef<Component> component : entity->components) {
      impl_->objects.Destroy(component.GetID());
    }
    impl_->entities.erase(
        std::remove(impl_->entities.begin(), impl_->entities.end(), id),
        impl_->entities.end());
    impl_->objects.Destroy(id);
  }
  impl_->destroys.clear();
  for (const auto &spawn : impl_->spawns)
    impl_->CreateEntity(spawn.reserved, spawn.type, spawn.name, nullptr);
  impl_->spawns.clear();
}

void World::Update(float deltaTime) {
  ENGINE_PROFILE_SCOPE("World Update");
  for (const auto &type : impl_->componentTypes) {
    ENGINE_PROFILE_DYNAMIC_SCOPE(type.name.c_str());
    type.updatePool(deltaTime);
  }
  FlushSpawns();
}

std::vector<std::byte> World::Save() const {
  ENGINE_PROFILE_SCOPE("World Save");
  StateWriter writer;
  writer.Value(Magic);
  writer.Value(Format);
  writer.Value(static_cast<std::uint32_t>(impl_->entities.size()));
  for (ObjectID entityID : impl_->entities) {
    const auto *entity = impl_->objects.GetAs<Entity>(entityID);
    writer.Value(entityID);
    writer.Value(entity->GetTypeID());
    writer.String(entity->GetName());
    writer.Value(entity->transform);
    writer.Value(static_cast<std::uint32_t>(entity->components.size()));
    for (ObjectRef<Component> componentRef : entity->components) {
      const ObjectID componentID = componentRef.GetID();
      const auto *component = impl_->objects.GetAs<Component>(componentID);
      StateWriter payload;
      component->SaveState(payload);
      writer.Value(componentID);
      writer.Value(component->GetTypeID());
      writer.Value(component->CurrentStateVersion());
      writer.Value(static_cast<std::uint32_t>(payload.Data().size()));
      writer.Bytes(payload.Data());
    }
  }
  return writer.Take();
}

void World::Resume(std::span<const std::byte> state) {
  ENGINE_PROFILE_SCOPE("World Resume");
  StateReader reader(state);
  if (reader.Value<std::uint32_t>() != Magic ||
      reader.Value<std::uint32_t>() != Format)
    throw std::runtime_error("Unsupported gameplay snapshot format.");
  impl_->objects.Clear();
  impl_->entities.clear();
  impl_->spawns.clear();
  impl_->destroys.clear();
  const auto count = reader.Value<std::uint32_t>();
  for (std::uint32_t n = 0; n < count; ++n) {
    const auto entityID = reader.Value<ObjectID>();
    const auto entityType = reader.Value<TypeID>();
    const auto name = reader.String();
    const auto transform = reader.Value<Transform>();
    const auto componentCount = reader.Value<std::uint32_t>();
    const auto registered = impl_->entityTypes.find(entityType);
    std::vector<ObjectID> ids;
    std::vector<std::tuple<TypeID, std::uint32_t, std::vector<std::byte>>>
        states;
    for (std::uint32_t i = 0; i < componentCount; ++i) {
      ids.push_back(reader.Value<ObjectID>());
      const auto type = reader.Value<TypeID>();
      const auto version = reader.Value<std::uint32_t>();
      const auto size = reader.Value<std::uint32_t>();
      const auto bytes = reader.Bytes(size);
      states.emplace_back(type, version,
                          std::vector<std::byte>(bytes.begin(), bytes.end()));
    }
    bool compatible = registered != impl_->entityTypes.end() &&
                      registered->second.components.size() == componentCount;
    if (compatible) {
      for (std::size_t i = 0; i < states.size(); ++i) {
        if (registered->second.components[i] != std::get<0>(states[i])) {
          compatible = false;
          break;
        }
      }
    }
    if (!compatible) {
      impl_->objects.RejectRestored(entityID);
      for (ObjectID id : ids)
        impl_->objects.RejectRestored(id);
      continue;
    }
    impl_->CreateEntity(entityID, entityType, name, &ids);
    impl_->objects.GetAs<Entity>(entityID)->transform = transform;
    for (std::size_t i = 0; i < ids.size(); ++i) {
      auto *component = impl_->objects.GetAs<Component>(ids[i]);
      auto &[savedType, version, bytes] = states[i];
      if (component->GetTypeID() != savedType)
        throw std::runtime_error(
            "Saved ComponentType does not match entity layout.");
      if (version < component->MinimumStateVersion() ||
          version > component->CurrentStateVersion())
        throw std::runtime_error("Unsupported component state version.");
      StateReader payload(bytes);
      component->LoadState(payload, version);
      payload.RequireEnd();
    }
  }
  reader.RequireEnd();
}

ObjectManager &World::Objects() { return impl_->objects; }
const ObjectManager &World::Objects() const { return impl_->objects; }
const std::vector<ObjectID> &World::Entities() const { return impl_->entities; }
} // namespace Engine::Gameplay
