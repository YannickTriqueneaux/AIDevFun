#include "GameHost/ReloadableGame.h"

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

    RecordingSerializer before;
    game.SerializeAutoTestState(before);
    Require(!before.values.empty(), "Game exposed no entity state.");

    Engine::InputSystem input;
    input.EnableAutoTestInput();
    input.SetAutoTestKeyDown(Engine::Key::Right, true);
    game.Update(input, 0.25f);

    RecordingSerializer afterTick;
    game.SerializeAutoTestState(afterTick);
    Require(afterTick.values != before.values,
            "A headless gameplay tick did not change entity state.");

    game.RequestReload();
    game.ProcessAutoTestReload();
    Require(game.GetReloadStatus().find("Reloaded Game generation 2") !=
                std::string::npos,
            "The second DLL generation was not loaded.");

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

    std::cout << "Headless gameplay and hot reload passed with "
              << afterReload.values.size() << " state fields.\n";
    return 0;
#endif
  } catch (const std::exception &exception) {
    std::cerr << "Hot reload test failure: " << exception.what() << '\n';
    return 1;
  }
}
