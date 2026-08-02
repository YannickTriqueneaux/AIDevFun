#include "GameHost/ReloadableGame.h"

#include "Engine/Application/GameInstance.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"
#include "Engine/Serialization/Serializer.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
using StateValue = std::variant<bool, int, float, std::string>;

class RecordingSerializer final : public Engine::Serializer {
public:
  void Value(std::string_view name, bool &value) override {
    values[std::string(name)] = value;
  }
  void Value(std::string_view name, int &value) override {
    values[std::string(name)] = value;
  }
  void Value(std::string_view name, float &value) override {
    values[std::string(name)] = value;
  }
  void Value(std::string_view name, std::string &value) override {
    values[std::string(name)] = value;
  }

  std::map<std::string, StateValue> values;
};

void Require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

int ReadInt(const RecordingSerializer &serializer, const char *name) {
  const auto found = serializer.values.find(name);
  if (found == serializer.values.end() ||
      !std::holds_alternative<int>(found->second))
    throw std::runtime_error(std::string("Missing integer state field: ") +
                             name);
  return std::get<int>(found->second);
}

float ReadFloat(const RecordingSerializer &serializer, const char *name) {
  const auto found = serializer.values.find(name);
  if (found == serializer.values.end() ||
      !std::holds_alternative<float>(found->second))
    throw std::runtime_error(std::string("Missing float state field: ") + name);
  return std::get<float>(found->second);
}
} // namespace

int main(int argc, char **argv) {
  try {
#if !defined(ENGINE_AUTOTESTS)
    throw std::runtime_error(
        "HotReloadTests must not run without ENGINE_AUTOTESTS.");
#else
    if (argc != 3) {
      throw std::runtime_error(
          "Usage: HotReloadTests <Game.dll> <shadow-directory>");
    }

    ReloadableGame game(std::filesystem::absolute(argv[1]),
                        std::filesystem::absolute(argv[2]));
    Engine::GameInstance *firstInstance = Engine::GameInstance::GetInstance();
    Require(firstInstance != nullptr,
            "The loaded Game did not install a GameInstance singleton.");
    Engine::Gameplay::ObjectManager *firstManager =
        firstInstance->GetObjectManager();
    Engine::Gameplay::World *firstWorld = firstInstance->GetWorld();
    const auto firstDomain = firstInstance->GetObjectPoolDomain();

    RecordingSerializer before;
    game.SerializeAutoTestState(before);
    Require(!before.values.empty(), "Game exposed no entity state.");

    Engine::InputSystem input;
    input.EnableAutoTestInput();
    input.SetAutoTestKeyDown(Engine::Key::Right, true);
    input.SetAutoTestKeyDown(Engine::Key::W, true);
    for (int tick = 0; tick < 5; ++tick)
      game.Update(input, 0.25f);

    RecordingSerializer afterTick;
    game.SerializeAutoTestState(afterTick);
    Require(afterTick.values != before.values,
            "A headless gameplay tick did not change entity state.");
    Require(ReadInt(afterTick, "world.enemyCount") > 0,
            "The arena director did not spawn an enemy entity.");
    Require(ReadInt(afterTick, "world.playerProjectileCount") > 0,
            "The player weapon did not spawn projectile entities.");
    Require(ReadInt(afterTick, "world.enemyProjectileCount") > 0,
            "Enemy weapons did not spawn projectile entities.");
    const Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> playerRef(
        {static_cast<std::uint32_t>(ReadInt(afterTick, "player.entity.index")),
         static_cast<std::uint32_t>(
             ReadInt(afterTick, "player.entity.version"))});
    Require(playerRef.Resolve() != nullptr,
            "Player ObjectRef did not resolve before hot reload.");

    game.RequestReload();
    game.ProcessAutoTestReload();
    Require(game.GetReloadStatus().find("Reloaded Game generation 2") !=
                std::string::npos,
            "The second DLL generation was not loaded.");
    Require(Engine::GameInstance::GetInstance() != nullptr &&
                Engine::GameInstance::GetInstance() != firstInstance,
            "Hot reload did not renew the active GameInstance singleton.");
    Require(Engine::GameInstance::GetInstance()->GetObjectManager() !=
                    firstManager &&
                Engine::GameInstance::GetInstance()->GetWorld() != firstWorld,
            "Hot reload reused the old ObjectManager or World.");
    Require(!Engine::Gameplay::IsObjectPoolDomainAlive(firstDomain),
            "The old GameInstance Object pool domain survived hot reload.");
    const auto *reloadedInstance = Engine::GameInstance::GetInstance();
    Require(Engine::Gameplay::GetObjectPoolDomainStats(
                reloadedInstance->GetObjectPoolDomain())
                    .liveObjects ==
                reloadedInstance->GetObjectManager()->LiveCount(),
            "The restored domain contains orphaned gameplay objects.");
    Require(playerRef.Resolve() != nullptr &&
                playerRef.Resolve()->transform.position.x ==
                    ReadFloat(afterTick, "player.position.x") &&
                playerRef.Resolve()->transform.position.y ==
                    ReadFloat(afterTick, "player.position.y"),
            "A pre-reload ObjectRef did not resolve restored state in the new "
            "GameInstance.");

    RecordingSerializer afterReload;
    game.SerializeAutoTestState(afterReload);
    Require(!afterReload.values.empty(),
            "Entity state was unavailable after reload.");
    if (afterReload.values != afterTick.values) {
      for (const auto &[name, value] : afterTick.values) {
        const auto found = afterReload.values.find(name);
        if (found == afterReload.values.end() || found->second != value)
          std::cerr << "Changed after reload: " << name << '\n';
      }
    }
    Require(afterReload.values == afterTick.values,
            "Entity/component state or ObjectIDs changed across DLL reload.");

    const auto secondDomain = reloadedInstance->GetObjectPoolDomain();
    game.RequestReload();
    game.ProcessAutoTestReload();
    Require(!Engine::Gameplay::IsObjectPoolDomainAlive(secondDomain),
            "A prior Object pool domain survived a repeated hot reload.");
    const auto *thirdInstance = Engine::GameInstance::GetInstance();
    Require(Engine::Gameplay::GetObjectPoolDomainStats(
                thirdInstance->GetObjectPoolDomain())
                    .liveObjects ==
                thirdInstance->GetObjectManager()->LiveCount(),
            "Repeated hot reload left duplicate component allocations.");

    std::cout << "Headless gameplay and hot reload passed with "
              << afterReload.values.size() << " state fields.\n";
    return 0;
#endif
  } catch (const std::exception &exception) {
    std::cerr << "Hot reload test failure: " << exception.what() << '\n';
    return 1;
  }
}
