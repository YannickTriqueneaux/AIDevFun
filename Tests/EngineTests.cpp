#include "Engine/Application/GameInstance.h"
#include "Engine/Audio/ProceduralAudio.h"
#include "Engine/Core/Memory.h"
#include "Engine/Gameplay/World.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Graphics/VectorShape.h"
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
class TestComponent : public Component {
public:
  TestComponent(ObjectRef<Entity> owner, int marker)
      : Component(owner), marker_(marker) {}
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
  int marker_;
};

class FirstTestComponent final : public TestComponent {
public:
  static constexpr TypeID Type = FirstType;
  explicit FirstTestComponent(ObjectRef<Entity> owner)
      : TestComponent(owner, 1) {}
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
};

class SecondTestComponent final : public TestComponent {
public:
  static constexpr TypeID Type = SecondType;
  explicit SecondTestComponent(ObjectRef<Entity> owner)
      : TestComponent(owner, 2) {}
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
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
  w.RegisterComponent(MakeComponentType<FirstTestComponent>("First"));
  w.RegisterComponent(MakeComponentType<SecondTestComponent>("Second"));
  w.RegisterEntity({TestEntity, "TestEntity", {SecondType, FirstType}});
}

void ConfigureReplacementWorld(World &w) {
  w.RegisterComponent(MakeComponentType<FirstTestComponent>("First"));
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
      updateOrder.clear();
      second.GetWorld()->Update(0.016f);
      Require(updateOrder == std::vector<int>({1, 2}),
              "Active pool update leaked across GameInstance domains.");

      first.Activate();
      Require(sharedReference.Resolve()->transform.position.x == 10.0f,
              "ObjectRef did not follow explicit GameInstance activation.");
      updateOrder.clear();
      first.GetWorld()->Update(0.016f);
      Require(updateOrder == std::vector<int>({1, 2}),
              "Reactivated pool update used another GameInstance domain.");
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

void TestVectorShapes() {
  enum class Group : std::size_t { Body, Weapon, Count };
  static constexpr std::array<std::string_view,
                              static_cast<std::size_t>(Group::Count)>
      groupNames{"body", "weapon"};
  static constexpr std::string_view svg = R"svg(
<svg viewBox="0 0 100 80">
  <g id="body" transform="translate(2 3)">
    <rect x="10" y="10" width="40" height="20" fill="#4ADE9C"/>
  </g>
  <g id="weapon"><path d="M50 10 L80 20 L50 30 Z" fill="white"/></g>
  <circle cx="20" cy="60" r="8" fill="#FF000080" stroke="black" stroke-width="2"/>
</svg>)svg";

  Engine::VectorShape shape;
  Require(shape.LoadFromSvg(svg), "Valid SVG vector shape was rejected.");
  Require(shape.IsValid() && !shape.IsUploaded(),
          "CPU-loaded vector shape unexpectedly required a GPU context.");
  Require(shape.GetViewBoxSize().x == 100.0f &&
              shape.GetViewBoxSize().y == 80.0f,
          "Vector shape viewBox was not preserved.");
  Require(shape.HasGroup("body") && shape.HasGroup("weapon") &&
              shape.GetTriangleCount() > 2,
          "Vector shape groups or tessellated geometry were missing.");
  const auto groups = shape.ResolveGroups(groupNames);
  const auto group = [&groups](Group value) {
    return groups[static_cast<std::size_t>(value)];
  };
  Require(group(Group::Body).IsValid() && group(Group::Weapon).IsValid(),
          "Compile-time vector group table did not bind to the SVG.");

  Engine::VectorShapePose pose(shape);
  Engine::VectorShapeAnimation animation(shape);
  Engine::VectorShapeTransform start;
  Engine::VectorShapeTransform end;
  end.translation = {20.0f, 10.0f};
  end.rotationDegrees = 90.0f;
  end.scale = {2.0f, 0.5f};
  end.opacity = 0.25f;
  Require(animation.AddKeyframe(group(Group::Weapon), 0.0f, start) &&
              animation.AddKeyframe(group(Group::Weapon), 2.0f, end),
          "Resolved vector group ID was rejected by its animation.");
  Require(animation.Sample(1.0f, false, pose),
          "Vector animation could not resolve its named group.");
  Engine::VectorShapeTransform sampled;
  Require(pose.GetGroupTransform(group(Group::Weapon), sampled) &&
              std::abs(sampled.translation.x - 10.0f) < 0.001f &&
              std::abs(sampled.rotationDegrees - 45.0f) < 0.001f &&
              std::abs(sampled.scale.x - 1.5f) < 0.001f &&
              std::abs(sampled.opacity - 0.625f) < 0.001f,
          "Vector animation interpolation was incorrect.");
  Require(!pose.SetGroupTransform({}, start),
          "Invalid vector group ID was silently accepted.");

  Engine::VectorShape invalid;
  Require(!invalid.LoadFromSvg("<svg viewBox=\"0 0 1 1\"><script/></svg>") &&
              !invalid.GetLastError().empty(),
          "Unsafe SVG content was not rejected with an error.");
}

void TestProceduralAudio() {
  static constexpr std::array pitch{Engine::AudioEnvelopePoint{0.0f, 1.0f},
                                    Engine::AudioEnvelopePoint{0.2f, 0.25f}};
  static constexpr std::array voices{
      Engine::ProceduralSoundVoice{
          .waveform = Engine::AudioWaveform::Square,
          .frequencyHz = 880.0f,
          .volume = 0.65f,
          .pan = 0.35f,
          .dutyCycle = 0.35f,
          .envelope = {.attackSeconds = 0.005f,
                       .decaySeconds = 0.04f,
                       .sustainLevel = 0.55f,
                       .releaseSeconds = 0.05f},
          .frequencyMultiplier = {.points = pitch, .defaultValue = 1.0f},
          .vibratoFrequencyHz = 8.0f,
          .vibratoDepthCents = 14.0f,
          .filter = Engine::AudioFilterType::LowPass,
          .filterCutoffHz = 4'500.0f},
      Engine::ProceduralSoundVoice{.waveform = Engine::AudioWaveform::Noise,
                                   .frequencyHz = 120.0f,
                                   .volume = 0.12f,
                                   .pan = 0.65f,
                                   .envelope = {.attackSeconds = 0.0f,
                                                .decaySeconds = 0.03f,
                                                .sustainLevel = 0.2f,
                                                .releaseSeconds = 0.08f},
                                   .filter = Engine::AudioFilterType::HighPass,
                                   .filterCutoffHz = 700.0f,
                                   .noiseSeed = 42}};
  static constexpr Engine::ProceduralSoundDefinition soundDefinition{
      .durationSeconds = 0.2f,
      .sampleRate = 22'050,
      .masterVolume = 0.8f,
      .voices = voices,
      .effects = {
          .echoDelaySeconds = 0.04f, .echoFeedback = 0.2f, .echoMix = 0.15f}};

  Engine::ProceduralSound first;
  Engine::ProceduralSound second;
  Require(first.Build(soundDefinition) && second.Build(soundDefinition),
          "Procedural sound definition did not build.");
  Require(!first.IsUploaded(),
          "Headless procedural sound unexpectedly touched the audio device.");
  Require(first.GetSampleCount() == 4'410 &&
              std::abs(first.GetDuration() - 0.2f) < 0.001f,
          "Procedural sound duration or sample count was incorrect.");
  Require(first.GetPeakAmplitude() > 0.05f && first.GetPeakAmplitude() <= 1.0f,
          "Procedural sound produced invalid amplitude.");
  Require(first.GetPcmHash() != 0 && first.GetPcmHash() == second.GetPcmHash(),
          "Procedural synthesis was not deterministic.");

  static constexpr std::array notes{
      Engine::ProceduralMusicNote{0.0f, 0.5f, 60, 1.0f},
      Engine::ProceduralMusicNote{0.5f, 0.5f, 64, 0.8f},
      Engine::ProceduralMusicNote{1.0f, 1.0f, 67, 0.9f}};
  static constexpr std::array tracks{
      Engine::ProceduralMusicTrack{.instrument = &soundDefinition,
                                   .notes = notes,
                                   .volume = 0.7f,
                                   .pan = 0.5f}};
  static constexpr Engine::ProceduralMusicDefinition musicDefinition{
      .tempoBeatsPerMinute = 120.0f,
      .lengthBeats = 2.0f,
      .sampleRate = 22'050,
      .masterVolume = 0.7f,
      .tracks = tracks,
      .effects = {
          .reverbSeconds = 0.08f, .reverbDecay = 0.25f, .reverbMix = 0.1f}};
  Engine::ProceduralMusic music;
  Require(music.Build(musicDefinition) && music.GetSampleCount() == 22'050 &&
              std::abs(music.GetDuration() - 1.0f) < 0.001f &&
              music.GetPcmHash() != 0,
          "Procedural music sequence did not render deterministically.");
}
} // namespace

int main() {
  try {
    TestProceduralAudio();
    TestVectorShapes();
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
    const std::size_t lookupsBeforeUpdate =
        world.Objects().GetObjectLookupCount();
    world.Update(0.016f);
    Require(updateOrder == std::vector<int>({1, 2}),
            "Components were not updated in registered type order.");
    Require(world.Objects().GetObjectLookupCount() == lookupsBeforeUpdate,
            "Component pool update performed ObjectManager lookups.");

    for (int index = 0; index < 3; ++index)
      (void)world.Spawn(TestEntity, "Contiguous" + std::to_string(index));
    world.FlushSpawns();
    std::vector<void *> firstComponentAddresses;
    VisitObjectsInActivePool(
        ObjectStorageTypeID<FirstTestComponent>(), sizeof(FirstTestComponent),
        alignof(FirstTestComponent),
        [](void *object, void *context) {
          static_cast<std::vector<void *> *>(context)->push_back(object);
        },
        &firstComponentAddresses);
    Require(firstComponentAddresses.size() == 4,
            "Direct component pool iteration missed live components.");
    const auto firstStride =
        static_cast<std::byte *>(firstComponentAddresses[1]) -
        static_cast<std::byte *>(firstComponentAddresses[0]);
    Require(
        firstStride > 0 &&
            static_cast<std::byte *>(firstComponentAddresses[2]) -
                    static_cast<std::byte *>(firstComponentAddresses[1]) ==
                firstStride &&
            static_cast<std::byte *>(firstComponentAddresses[3]) -
                    static_cast<std::byte *>(firstComponentAddresses[2]) ==
                firstStride,
        "Objects of one type were not laid out contiguously in pool order.");
    updateOrder.clear();
    const std::size_t lookupsBeforeBatchUpdate =
        world.Objects().GetObjectLookupCount();
    world.Update(0.016f);
    Require(updateOrder == std::vector<int>({1, 1, 1, 1, 2, 2, 2, 2}),
            "World did not update complete component pools in type order.");
    Require(world.Objects().GetObjectLookupCount() == lookupsBeforeBatchUpdate,
            "Batch component pool update performed ObjectManager lookups.");
    const auto oldComponent = entity->components[0];
    constexpr TypeID componentStorage =
        ObjectStorageTypeID<FirstTestComponent>();
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
                              replacementEntity->components[1].GetID()));
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
