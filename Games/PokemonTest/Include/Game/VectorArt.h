#pragma once

#include "Engine/Graphics/VectorShape.h"

#include <array>
#include <memory>

enum class PlayerGroup : std::size_t {
  Shadow,
  LeftLeg,
  LeftFoot,
  RightLeg,
  RightFoot,
  Body,
  Head,
  Hat,
  Eyes,
  Count
};

enum class NpcGroup : std::size_t {
  Shadow,
  Legs,
  Body,
  Head,
  Hair,
  Eyes,
  Count
};

enum class DragonGroup : std::size_t {
  Shadow,
  Tail,
  Body,
  Wings,
  Head,
  Flame,
  Count
};

struct VectorArtResources {
  Engine::VectorShape ground;
  Engine::VectorShape road;
  Engine::VectorShape house;
  Engine::VectorShape sign;
  Engine::VectorShape flower;
  Engine::VectorShape water;
  Engine::VectorShape tree;
  Engine::VectorShape player;
  Engine::VectorShape npc;
  Engine::VectorShape dragon;

  std::array<Engine::VectorShapeGroupID, 3> groundGroups{};
  std::array<Engine::VectorShapeGroupID, 3> roadGroups{};
  std::array<Engine::VectorShapeGroupID, 5> houseGroups{};
  std::array<Engine::VectorShapeGroupID, 3> signGroups{};
  std::array<Engine::VectorShapeGroupID, 3> flowerGroups{};
  std::array<Engine::VectorShapeGroupID, 3> waterGroups{};
  std::array<Engine::VectorShapeGroupID, 4> treeGroups{};
  std::array<Engine::VectorShapeGroupID, 9> playerGroups{};
  std::array<Engine::VectorShapeGroupID, 6> npcGroups{};
  std::array<Engine::VectorShapeGroupID, 6> dragonGroups{};

  std::unique_ptr<Engine::VectorShapePose> roadPose;
  std::unique_ptr<Engine::VectorShapePose> housePose;
  std::unique_ptr<Engine::VectorShapePose> signPose;
  std::unique_ptr<Engine::VectorShapePose> flowerPose;
  std::unique_ptr<Engine::VectorShapePose> waterPose;
  std::unique_ptr<Engine::VectorShapePose> treePose;
  std::unique_ptr<Engine::VectorShapePose> playerPose;
  std::unique_ptr<Engine::VectorShapePose> npcPose;
  std::unique_ptr<Engine::VectorShapePose> dragonPose;
  bool ready = false;

  void Initialize();
  void Shutdown();
};
