#pragma once

#include "Engine/Application/GameInterface.h"

#include "Engine/Gameplay/World.h"
#include "Game/GameInput.h"
#include "Game/Player.h"

class ProceduralGame final : public Engine::GameInterface {
public:
  ProceduralGame();
  void Update(const Engine::InputSystem &input, float deltaTime) override;
  [[nodiscard]] Engine::Color GetClearColor() const override;
  void Render(Engine::RenderContext &context) const override;
  [[nodiscard]] std::vector<std::byte> SaveResumeState() const override;
  void ResumeFromState(std::span<const std::byte> state) override;
#if defined(ENGINE_AUTOTESTS)
  void SerializeAutoTestState(Engine::Serializer &serializer) override;
#endif

private:
  GameInput inputBindings_;
  Engine::Gameplay::World world_;
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> playerEntity_;
  Engine::Gameplay::ObjectRef<Player> player_;
};
