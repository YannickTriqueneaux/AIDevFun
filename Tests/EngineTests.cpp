#include "Engine/Application/GameInstance.h"
#include "Engine/Core/Memory.h"
#include "Engine/Gameplay/World.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"
#include "Engine/Math/Vector2.h"
#include "Engine/Math/Vector3.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace Engine::Gameplay;
constexpr TypeID TestEntity = StableTypeID("Tests.Entity");
constexpr TypeID FirstType = StableTypeID("Tests.First");
constexpr TypeID SecondType = StableTypeID("Tests.Second");
std::vector<int> updateOrder;
struct alignas(32) GeneralAllocation {
  int values[8]{};
  explicit GeneralAllocation(int initial) { values[0] = initial; }
};
class TestComponent final : public Component {
public:
  TestComponent(ObjectRef<Entity> owner, TypeID type, int marker)
      : Component(owner), type_(type), marker_(marker) {}
  TypeID GetTypeID() const override { return type_; }
  std::uint32_t CurrentStateVersion() const override { return 2; }
  std::uint32_t MinimumStateVersion() const override { return 1; }
  void Update(float) override {
    updateOrder.push_back(marker_);
    ++ticks;
  }
  void SaveState(StateWriter &w) const override { w.Value(ticks); }
  void LoadState(StateReader &r, std::uint32_t version) override {
    ticks = r.Value<int>();
    if (version == 1)
      ticks += 100;
  }
  int ticks = 0;

private:
  TypeID type_;
  int marker_;
};

class TestGameInstanceComponent final : public Engine::GameInstanceComponent {
public:
  static constexpr TypeID Type = StableTypeID("Tests.GameInstanceComponent");
  explicit TestGameInstanceComponent(int initialValue) : value(initialValue) {}
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  int value = 0;
};

std::vector<int> gameInstanceComponentDestructionOrder;

class FirstInstanceComponent final : public Engine::GameInstanceComponent {
public:
  static constexpr TypeID Type = StableTypeID("Tests.FirstInstanceComponent");
  ~FirstInstanceComponent() override {
    gameInstanceComponentDestructionOrder.push_back(1);
  }
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
};

class SecondInstanceComponent final : public Engine::GameInstanceComponent {
public:
  static constexpr TypeID Type = StableTypeID("Tests.SecondInstanceComponent");
  ~SecondInstanceComponent() override {
    gameInstanceComponentDestructionOrder.push_back(2);
  }
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
};

void ConfigureTestWorld(World &w) {
  w.RegisterComponent({FirstType, "First", [](ObjectRef<Entity> owner) {
                         return NEW_OBJECT(TestComponent, owner, FirstType, 1);
                       }});
  w.RegisterComponent({SecondType, "Second", [](ObjectRef<Entity> owner) {
                         return NEW_OBJECT(TestComponent, owner, SecondType, 2);
                       }});
  w.RegisterEntity({TestEntity, "TestEntity", {SecondType, FirstType}});
}

void ConfigureReplacementWorld(World &w) {
  w.RegisterComponent({FirstType, "First", [](ObjectRef<Entity> owner) {
                         return NEW_OBJECT(TestComponent, owner, FirstType, 1);
                       }});
  w.RegisterEntity(
      {StableTypeID("Tests.ReplacementEntity"), "Replacement", {FirstType}});
}

void Require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <class Function>
void RequireThrows(Function &&function, const char *message) {
  try {
    function();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error(message);
}

void TestGameInstanceLifecycle() {
  Require(Engine::GameInstance::GetInstance() == nullptr,
          "GameInstance singleton was not initially empty.");

  auto firstOwner = NEW_MEMORY(Engine::GameInstance);
  auto secondOwner = NEW_MEMORY(Engine::GameInstance);
  Require(Engine::GameInstance::GetInstance() == secondOwner.get(),
          "The newest GameInstance was not activated.");
  DELETE_MEMORY(firstOwner);
  Require(Engine::GameInstance::GetInstance() == secondOwner.get(),
          "Destroying an inactive GameInstance cleared the active singleton.");
  DELETE_MEMORY(secondOwner);
  Require(Engine::GameInstance::GetInstance() == nullptr,
          "Destroying the active GameInstance did not clear the singleton.");

  ObjectRef<Entity> sharedReference;
  gameInstanceComponentDestructionOrder.clear();
  {
    Engine::GameInstance first;
    Require(first.GetWorld() == first.GetWorld(),
            "GameInstance returned more than one World.");
    ConfigureTestWorld(*first.GetWorld());
    sharedReference = first.GetWorld()->Spawn(TestEntity, "FirstWorldEntity");
    first.GetWorld()->FlushSpawns();
    sharedReference.Resolve()->transform.position.x = 10.0f;
    first.AddComponent<FirstInstanceComponent>();
    first.AddComponent<SecondInstanceComponent>();
    const Engine::GameInstance &constFirst = first;
    Require(constFirst.GetComponent<FirstInstanceComponent>() != nullptr,
            "Const GameInstanceComponent lookup failed.");

    {
      Engine::GameInstance second;
      ConfigureTestWorld(*second.GetWorld());
      const ObjectRef<Entity> secondReference =
          second.GetWorld()->Spawn(TestEntity, "SecondWorldEntity");
      second.GetWorld()->FlushSpawns();
      secondReference.Resolve()->transform.position.x = 20.0f;
      Require(secondReference.GetID() == sharedReference.GetID() &&
                  sharedReference.Resolve()->transform.position.x == 20.0f,
              "ObjectRef did not resolve through the newest active manager.");

      first.Activate();
      Require(sharedReference.Resolve()->transform.position.x == 10.0f,
              "ObjectRef did not follow explicit GameInstance activation.");
    }

    Require(Engine::GameInstance::GetInstance() == &first &&
                sharedReference.Resolve()->transform.position.x == 10.0f,
            "Destroying an inactive instance disturbed the reactivated one.");
  }

  Require(Engine::GameInstance::GetInstance() == nullptr &&
              sharedReference.Resolve() == nullptr,
          "ObjectRef resolved without an active GameInstance.");
  Require(gameInstanceComponentDestructionOrder == std::vector<int>({2, 1}),
          "GameInstanceComponents were not destroyed in reverse order.");
}
} // namespace

int main() {
  try {
    Engine::Vector2 position{2.0f, 3.0f};
    position += Engine::Vector2{4.0f, -1.0f};
    const Engine::Vector2 moved = position + Engine::Vector2{1.0f, 2.0f};
    const Engine::Vector2 scaled = moved * 2.0f;
    Require(std::abs(scaled.x - 14.0f) < 0.001f,
            "Vector2 x arithmetic failed.");
    Require(std::abs(scaled.y - 8.0f) < 0.001f, "Vector2 y arithmetic failed.");

    const Engine::Vector3 origin{};
    Require(origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f,
            "Vector3 defaults failed.");
    const Engine::Color white{};
    Require(white.red == 255 && white.green == 255 && white.blue == 255 &&
                white.alpha == 255,
            "Color defaults failed.");

#if defined(ENGINE_AUTOTESTS)
    Engine::InputSystem input;
    input.EnableAutoTestInput();
    input.SetAutoTestKeyDown(Engine::Key::Right, true);
    input.SetAutoTestKeyPressed(Engine::Key::Q, true);
    Require(input.IsDown(Engine::Key::Right),
            "Injected held key was not visible.");
    Require(input.WasPressed(Engine::Key::Q),
            "Injected pressed key was not visible.");
    input.ClearAutoTestPressedKeys();
    Require(!input.WasPressed(Engine::Key::Q),
            "Pressed keys were not cleared.");

    auto general = NEW_MEMORY(GeneralAllocation, 42);
    GeneralAllocation *firstAddress = general.get();
    const auto firstMemoryID = Engine::Memory::GetAllocationInfo(
        general.get(), sizeof(GeneralAllocation), alignof(GeneralAllocation));
    Require(reinterpret_cast<std::uintptr_t>(general.get()) %
                        alignof(GeneralAllocation) ==
                    0 &&
                general->values[0] == 42,
            "General memory bucket did not honor construction or alignment.");
    DELETE_MEMORY(general);
    auto recycledGeneral = NEW_MEMORY(GeneralAllocation, 7);
    const auto recycledMemoryID = Engine::Memory::GetAllocationInfo(
        recycledGeneral.get(), sizeof(GeneralAllocation),
        alignof(GeneralAllocation));
    Require(
        recycledGeneral.get() == firstAddress &&
            recycledMemoryID.index == firstMemoryID.index &&
            recycledMemoryID.version != firstMemoryID.version,
        "General sparse memory bucket did not recycle its slot generation.");
    const auto generalStats = Engine::Memory::GetBucketStats(
        sizeof(GeneralAllocation), alignof(GeneralAllocation));
    Require(generalStats.liveAllocations == 1 &&
                generalStats.recycledAllocations >= 1,
            "General memory bucket statistics are inconsistent.");
    DELETE_MEMORY(recycledGeneral);
    constexpr TypeID reloadLayoutType = StableTypeID("Tests.HotReloadLayout");
    void *oldLayout = AllocateObjectMemory(reloadLayoutType, 32, 8);
    void *newLayout = AllocateObjectMemory(reloadLayoutType, 96, 16);
    Require(
        oldLayout != nullptr && newLayout != nullptr,
        "Object pools rejected simultaneous hot-reload layout generations.");
    ReleaseObjectMemory(reloadLayoutType, newLayout, 96, 16);
    ReleaseObjectMemory(reloadLayoutType, oldLayout, 32, 8);
    {
      std::vector<int, Engine::Memory::Allocator<int>> pooledVector;
      pooledVector.reserve(64);
      pooledVector.push_back(9);
      Require(pooledVector.front() == 9 &&
                  Engine::Memory::GetBucketStats(sizeof(int) * 64, alignof(int))
                          .liveAllocations == 1,
              "STL allocator did not route vector storage through a general "
              "bucket.");
    }
    Require(Engine::Memory::GetBucketStats(sizeof(int) * 64, alignof(int))
                    .liveAllocations == 0,
            "STL allocator did not return vector storage to its bucket.");

    TestGameInstanceLifecycle();

    Engine::GameInstance worldInstance;
    Require(Engine::GameInstance::GetInstance() == &worldInstance &&
                worldInstance.GetObjectManager() != nullptr &&
                worldInstance.GetWorld() != nullptr,
            "GameInstance did not expose its active ObjectManager and World.");
    auto *instanceComponent =
        worldInstance.AddComponent<TestGameInstanceComponent>(42);
    Require(instanceComponent != nullptr && instanceComponent->value == 42 &&
                worldInstance.GetComponent<TestGameInstanceComponent>() ==
                    instanceComponent,
            "GameInstanceComponent registration or lookup failed.");
    RequireThrows(
        [&] { worldInstance.AddComponent<TestGameInstanceComponent>(7); },
        "Duplicate GameInstanceComponent TypeID was accepted.");
    auto &world = *worldInstance.GetWorld();
    ConfigureTestWorld(world);
    const auto pending = world.Spawn(TestEntity, "Alpha");
    Require(pending.Resolve() == nullptr,
            "Deferred spawn resolved during its request frame.");
    world.FlushSpawns();
    auto *entity = pending.Resolve();
    Require(entity != nullptr && entity->components.size() == 2,
            "EntityFactory did not create the declared component layout.");
    Require(world.Objects().GetDebugName(entity->components[0].GetID()) ==
                "Alpha+Second",
            "Component debug naming convention failed.");
    Require(entity->GetComponent<TestComponent>().IsAssigned(),
            "Entity typed component lookup failed.");
    updateOrder.clear();
    world.Update(0.016f);
    Require(updateOrder == std::vector<int>({1, 2}),
            "Components were not updated in registered type order.");
    const auto oldComponent = entity->components[0];
    constexpr TypeID componentStorage = ObjectStorageTypeID<TestComponent>();
    const auto componentMemoryID = GetObjectAllocationInfo(
        componentStorage,
        world.Objects().GetAs<Component>(entity->components[1].GetID()));
    const auto snapshot = world.Save();
    Engine::GameInstance restoredInstance;
    auto &restored = *restoredInstance.GetWorld();
    ConfigureTestWorld(restored);
    restored.Resume(snapshot);
    Require(
        pending.Resolve() != nullptr,
        "ObjectRef did not resolve through the active restored GameInstance.");
    Require(restored.Save() == snapshot,
            "World snapshot did not round-trip byte-for-byte.");
    Engine::GameInstance replacementInstance;
    auto &replacementWorld = *replacementInstance.GetWorld();
    ConfigureReplacementWorld(replacementWorld);
    replacementWorld.Resume(snapshot);
    Require(replacementWorld.Entities().empty() &&
                replacementWorld.Objects().GetObject(pending.GetID()) ==
                    nullptr,
            "A replaced EntityType was incorrectly restored.");
    const auto freshReplacement = replacementWorld.Spawn(
        StableTypeID("Tests.ReplacementEntity"), "Fresh");
    replacementWorld.FlushSpawns();
    Require(freshReplacement.Resolve() != nullptr &&
                replacementWorld.Objects().GetObject(pending.GetID()) ==
                    nullptr,
            "Rejected ObjectIDs were not generation-invalidated before "
            "replacement spawning.");
    auto truncated = snapshot;
    truncated.pop_back();
    RequireThrows(
        [&] {
          Engine::GameInstance invalidInstance;
          auto &invalid = *invalidInstance.GetWorld();
          ConfigureTestWorld(invalid);
          invalid.Resume(truncated);
        },
        "Truncated snapshot was accepted.");
    replacementInstance.Activate();
    auto badMagic = snapshot;
    badMagic.front() ^= std::byte{0xff};
    RequireThrows(
        [&] {
          Engine::GameInstance invalidInstance;
          auto &invalid = *invalidInstance.GetWorld();
          ConfigureTestWorld(invalid);
          invalid.Resume(badMagic);
        },
        "Unknown snapshot format was accepted.");
    restoredInstance.Activate();
    const auto restoredEntity = ObjectRef<Entity>(pending.GetID()).Resolve();
    Require(restoredEntity != nullptr &&
                restoredEntity->components[0] == oldComponent,
            "ObjectIDs were not preserved on resume.");
    worldInstance.Activate();
    world.Destroy(pending);
    world.FlushSpawns();
    Require(pending.Resolve() == nullptr && oldComponent.Resolve() == nullptr,
            "Destroyed ObjectRefs remained resolvable.");
    const auto replacement = world.Spawn(TestEntity, "Beta");
    world.FlushSpawns();
    Require(replacement.GetID().index == pending.GetID().index &&
                replacement.GetID().version != pending.GetID().version,
            "Sparse ObjectID slot was not reused with a new version.");
    const auto *replacementEntity = replacement.Resolve();
    const auto replacementMemoryID = GetObjectAllocationInfo(
        componentStorage, world.Objects().GetAs<Component>(
                              replacementEntity->components[0].GetID()));
    Require(replacementMemoryID.index == componentMemoryID.index &&
                replacementMemoryID.version != componentMemoryID.version,
            "Dedicated Object pool did not recycle its sparse memory slot "
            "generation.");
    Require(GetObjectPoolStats(componentStorage).recycledAllocations >= 1,
            "Dedicated Object pool reported no recycled allocation.");
#else
    throw std::runtime_error(
        "EngineTests must not run without ENGINE_AUTOTESTS.");
#endif

    std::cout << "Engine pure tests passed.\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Engine pure test failure: " << exception.what() << '\n';
    return 1;
  }
}
