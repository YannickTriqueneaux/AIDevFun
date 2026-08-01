#include "Engine/Application/GameInstance.h"

#include <vector>

namespace {
Engine::GameInstance *ActiveGameInstance = nullptr;
}

namespace Engine {
class GameInstance::Impl {
public:
  struct ComponentEntry {
    Gameplay::TypeID type = 0;
    GameInstanceComponent *component = nullptr;
    ComponentDestroyFunction destroy = nullptr;
  };
  std::vector<ComponentEntry> components;
};

GameInstance::GameInstance()
    : world_(objectManager_), impl_(NEW_MEMORY(Impl).release()) {
  Activate();
}

GameInstance::~GameInstance() {
  for (auto entry = impl_->components.rbegin();
       entry != impl_->components.rend(); ++entry) {
    if (entry->component && entry->destroy)
      entry->destroy(entry->component);
  }
  DELETE_MEMORY(impl_);
  if (ActiveGameInstance == this) {
    ActiveGameInstance = nullptr;
    Gameplay::SetActiveObjectPoolDomain(0);
  }
}

GameInstance *GameInstance::GetInstance() { return ActiveGameInstance; }

void GameInstance::Activate() {
  ActiveGameInstance = this;
  Gameplay::SetActiveObjectPoolDomain(objectPoolDomain_.Get());
}

Gameplay::ObjectManager *GameInstance::GetObjectManager() {
  return &objectManager_;
}

const Gameplay::ObjectManager *GameInstance::GetObjectManager() const {
  return &objectManager_;
}

Gameplay::World *GameInstance::GetWorld() { return &world_; }

const Gameplay::World *GameInstance::GetWorld() const { return &world_; }

void GameInstance::RegisterComponent(Gameplay::TypeID type,
                                     GameInstanceComponent *component,
                                     ComponentDestroyFunction destroy) {
  if (FindComponent(type))
    throw std::invalid_argument("Duplicate GameInstanceComponent TypeID.");
  impl_->components.push_back({type, component, destroy});
}

GameInstanceComponent *GameInstance::FindComponent(Gameplay::TypeID type) {
  for (const Impl::ComponentEntry &entry : impl_->components) {
    if (entry.type == type)
      return entry.component;
  }
  return nullptr;
}

const GameInstanceComponent *
GameInstance::FindComponent(Gameplay::TypeID type) const {
  for (const Impl::ComponentEntry &entry : impl_->components) {
    if (entry.type == type)
      return entry.component;
  }
  return nullptr;
}
} // namespace Engine

namespace Engine::Gameplay {
ObjectManager *GetActiveObjectManager() {
  GameInstance *instance = GameInstance::GetInstance();
  return instance ? instance->GetObjectManager() : nullptr;
}
} // namespace Engine::Gameplay
