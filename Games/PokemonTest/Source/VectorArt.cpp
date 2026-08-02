#include "Game/VectorArt.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace {
static constexpr std::string_view GroundSvg = R"svg(
<svg viewBox="0 0 100 100">
  <g id="base"><rect x="0" y="0" width="100" height="100" fill="#74CFA0"/><rect x="0" y="0" width="100" height="100" fill="#8BDBA8" opacity="0.38"/></g>
  <g id="moss"><rect x="12" y="18" width="3" height="7" fill="#4EB87D" opacity="0.60"/><rect x="16" y="21" width="3" height="5" fill="#A9E8B8" opacity="0.55"/><rect x="42" y="72" width="4" height="8" fill="#4EB87D" opacity="0.48"/><rect x="78" y="38" width="3" height="7" fill="#A9E8B8" opacity="0.50"/><rect x="66" y="83" width="6" height="3" fill="#5FC98C" opacity="0.45"/></g>
  <g id="veins"><line x1="8" y1="52" x2="22" y2="52" stroke="#5FC98C" stroke-width="1" opacity="0.22"/><line x1="54" y1="18" x2="66" y2="18" stroke="#BDEFC6" stroke-width="1" opacity="0.18"/><line x1="30" y1="38" x2="40" y2="38" stroke="#5FC98C" stroke-width="1" opacity="0.18"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> GroundGroups{"base", "moss",
                                                              "veins"};

static constexpr std::string_view RoadSvg = R"svg(
<svg viewBox="0 0 100 24">
  <g id="pavement"><rect x="0" y="0" width="100" height="24" fill="#8B6F3E"/><rect x="0" y="3" width="100" height="18" fill="#C8A85D"/><rect x="0" y="11" width="100" height="2" fill="#9A7C45" opacity="0.70"/></g>
  <g id="glow"><line x1="0" y1="3" x2="100" y2="3" stroke="#F1D88A" stroke-width="2" opacity="0.58"/><line x1="0" y1="21" x2="100" y2="21" stroke="#5C4A32" stroke-width="2" opacity="0.72"/></g>
  <g id="dash"><line x1="16" y1="4" x2="16" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="36" y1="4" x2="36" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="56" y1="4" x2="56" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><line x1="76" y1="4" x2="76" y2="20" stroke="#7B6137" stroke-width="2" opacity="0.82"/><rect x="8" y="6" width="2" height="2" fill="#FFE9A5"/><rect x="27" y="17" width="2" height="2" fill="#FFE9A5"/><rect x="69" y="7" width="2" height="2" fill="#FFE9A5"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> RoadGroups{"pavement", "glow",
                                                            "dash"};

static constexpr std::string_view HouseSvg = R"svg(
<svg viewBox="0 0 120 120">
  <g id="shadow"><ellipse cx="60" cy="108" rx="50" ry="9" fill="#050816" opacity="0.35"/></g>
  <g id="body"><rect x="14" y="44" width="92" height="64" fill="#F8F1C9"/><rect x="20" y="50" width="80" height="52" fill="#DDBB86" opacity="0.35"/></g>
  <g id="roof"><polygon points="8,48 60,10 112,48" fill="#FF4FD8"/><rect x="20" y="36" width="80" height="18" fill="#8A5CFF"/></g>
  <g id="windows"><rect x="27" y="60" width="19" height="18" fill="#00E5FF"/><rect x="74" y="60" width="19" height="18" fill="#00E5FF"/><line x1="36" y1="60" x2="36" y2="78" stroke="#F8F871" stroke-width="2"/><line x1="83" y1="60" x2="83" y2="78" stroke="#F8F871" stroke-width="2"/></g>
  <g id="door"><rect x="51" y="74" width="20" height="34" fill="#51344D"/><circle cx="66" cy="91" r="2" fill="#F8F871"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 5> HouseGroups{
    "shadow", "body", "roof", "windows", "door"};

static constexpr std::string_view SignSvg = R"svg(
<svg viewBox="0 0 80 60">
  <g id="post"><rect x="37" y="24" width="6" height="34" fill="#51344D"/></g>
  <g id="board"><rect x="6" y="4" width="68" height="28" fill="#F8F1C9"/><line x1="6" y1="4" x2="74" y2="4" stroke="#00E5FF" stroke-width="3"/><line x1="6" y1="32" x2="74" y2="32" stroke="#FF4FD8" stroke-width="3"/></g>
  <g id="spark"><circle cx="67" cy="8" r="3" fill="#F8F871"/><circle cx="13" cy="28" r="2" fill="#F8F871"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> SignGroups{"post", "board",
                                                            "spark"};

static constexpr std::string_view FlowerSvg = R"svg(
<svg viewBox="0 0 32 40">
  <g id="stem"><line x1="16" y1="18" x2="16" y2="38" stroke="#2DD36F" stroke-width="3"/><ellipse cx="10" cy="29" rx="6" ry="3" fill="#39FFB6"/><ellipse cx="22" cy="32" rx="6" ry="3" fill="#39FFB6"/></g>
  <g id="petals"><circle cx="16" cy="10" r="5" fill="#F8F871"/><circle cx="8" cy="14" r="5" fill="#FF4FD8"/><circle cx="24" cy="14" r="5" fill="#8A5CFF"/><circle cx="16" cy="20" r="5" fill="#00E5FF"/></g>
  <g id="heart"><circle cx="16" cy="15" r="4" fill="#F8F1C9"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> FlowerGroups{"stem", "petals",
                                                              "heart"};

static constexpr std::string_view WaterSvg = R"svg(
<svg viewBox="0 0 100 60">
  <g id="pool"><rect x="0" y="0" width="100" height="60" fill="#3F79D1"/><rect x="0" y="0" width="100" height="60" fill="#5B98E1" opacity="0.35"/></g>
  <g id="waves"><polyline points="2,12 10,7 18,12 26,7 34,12 42,7 50,12 58,7 66,12 74,7 82,12 90,7 98,12" stroke="#73B7E8" stroke-width="2" fill="none" opacity="0.65"/><polyline points="0,30 8,25 16,30 24,25 32,30 40,25 48,30 56,25 64,30 72,25 80,30 88,25 96,30" stroke="#2F6FC5" stroke-width="2" fill="none" opacity="0.38"/><polyline points="2,48 10,43 18,48 26,43 34,48 42,43 50,48 58,43 66,48 74,43 82,48 90,43 98,48" stroke="#73B7E8" stroke-width="2" fill="none" opacity="0.55"/></g>
  <g id="shine"><rect x="74" y="10" width="4" height="4" fill="#A8DFFF" opacity="0.75"/><rect x="30" y="42" width="3" height="3" fill="#A8DFFF" opacity="0.55"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 3> WaterGroups{"pool", "waves",
                                                             "shine"};

static constexpr std::string_view TreeSvg = R"svg(
<svg viewBox="0 0 56 76">
  <g id="shadow"><ellipse cx="28" cy="70" rx="17" ry="5" fill="#31583D" opacity="0.30"/></g>
  <g id="trunk"><rect x="22" y="45" width="12" height="21" fill="#5F4631"/><rect x="25" y="45" width="6" height="21" fill="#8B6A42"/></g>
  <g id="crown"><circle cx="28" cy="21" r="20" fill="#2F7F48"/><circle cx="17" cy="33" r="15" fill="#3E9959"/><circle cx="39" cy="33" r="15" fill="#2D7442"/><circle cx="28" cy="39" r="16" fill="#34894E"/><rect x="17" y="12" width="8" height="5" fill="#BDEFC6" opacity="0.70"/><rect x="8" y="30" width="6" height="4" fill="#8EDC95" opacity="0.62"/><rect x="34" y="22" width="8" height="5" fill="#8EDC95" opacity="0.55"/></g>
  <g id="fruit"><rect x="18" y="24" width="4" height="4" fill="#D94F45"/><rect x="38" y="19" width="4" height="4" fill="#F4D35E"/><rect x="30" y="42" width="4" height="4" fill="#D94F45"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 4> TreeGroups{"shadow", "trunk",
                                                            "crown", "fruit"};

static constexpr std::string_view PlayerSvg = R"svg(
<svg viewBox="0 0 64 80">
  <g id="shadow"><ellipse cx="32" cy="73" rx="15" ry="5" fill="#31583D" opacity="0.32"/></g>
  <g id="left_leg">
    <rect x="24" y="55" width="7" height="13" fill="#26334F"/>
  </g>
  <g id="left_foot">
    <rect x="22" y="67" width="10" height="4" fill="#2D2D34"/>
  </g>
  <g id="right_leg">
    <rect x="34" y="55" width="7" height="13" fill="#26334F"/>
  </g>
  <g id="right_foot">
    <rect x="34" y="67" width="10" height="4" fill="#2D2D34"/>
  </g>
  <g id="body">
    <rect x="20" y="36" width="24" height="20" fill="#2D2D34"/>
    <rect x="22" y="38" width="20" height="16" fill="#5C6FCB"/>
    <rect x="24" y="38" width="16" height="6" fill="#F4F1E8"/>
    <rect x="15" y="39" width="7" height="16" fill="#2D2D34"/>
    <rect x="16" y="41" width="4" height="12" fill="#FFD2A1"/>
    <rect x="42" y="39" width="7" height="16" fill="#2D2D34"/>
    <rect x="44" y="41" width="4" height="12" fill="#FFD2A1"/>
    <rect x="18" y="34" width="7" height="18" fill="#8B4E8E"/>
  </g>
  <g id="head">
    <rect x="22" y="18" width="20" height="18" fill="#2D2D34"/>
    <rect x="24" y="20" width="16" height="14" fill="#FFD2A1"/>
    <rect x="27" y="33" width="10" height="3" fill="#E8A06D"/>
  </g>
  <g id="hat">
    <rect x="20" y="12" width="24" height="8" fill="#C63D32"/>
    <rect x="24" y="8" width="14" height="5" fill="#E65645"/>
    <rect x="38" y="15" width="7" height="4" fill="#F4F1E8"/>
    <rect x="18" y="19" width="14" height="4" fill="#C63D32"/>
  </g>
  <g id="eyes">
    <rect x="27" y="25" width="3" height="3" fill="#2D2D34"/>
    <rect x="35" y="25" width="3" height="3" fill="#2D2D34"/>
    <rect x="30" y="31" width="5" height="2" fill="#B54A42"/>
  </g>
</svg>
)svg";
static constexpr std::array<std::string_view, 9> PlayerGroups{
    "shadow", "left_leg", "left_foot", "right_leg", "right_foot",
    "body",   "head",     "hat",       "eyes"};

static constexpr std::string_view NpcSvg = R"svg(
<svg viewBox="0 0 64 80">
  <g id="shadow"><ellipse cx="32" cy="73" rx="16" ry="5" fill="#050816" opacity="0.26"/></g>
  <g id="legs">
    <rect x="23" y="55" width="7" height="13" fill="#050816"/>
    <rect x="24" y="55" width="5" height="11" fill="#FFD2A1"/>
    <rect x="34" y="55" width="7" height="13" fill="#050816"/>
    <rect x="35" y="55" width="5" height="11" fill="#FFD2A1"/>
    <rect x="20" y="67" width="11" height="4" fill="#050816"/>
    <rect x="33" y="67" width="11" height="4" fill="#050816"/>
  </g>
  <g id="body">
    <rect x="18" y="33" width="28" height="24" fill="#050816"/>
    <rect x="20" y="35" width="24" height="20" fill="#FF6BCB"/>
    <rect x="24" y="39" width="16" height="4" fill="#F8F8F0"/>
    <rect x="15" y="36" width="6" height="17" fill="#050816"/>
    <rect x="16" y="38" width="4" height="13" fill="#FFD2A1"/>
    <rect x="43" y="36" width="6" height="17" fill="#050816"/>
    <rect x="44" y="38" width="4" height="13" fill="#FFD2A1"/>
  </g>
  <g id="head">
    <rect x="21" y="13" width="22" height="21" fill="#050816"/>
    <rect x="24" y="17" width="16" height="15" fill="#FFD2A1"/>
    <rect x="27" y="31" width="10" height="3" fill="#E8A06D"/>
  </g>
  <g id="hair">
    <rect x="19" y="10" width="24" height="9" fill="#050816"/>
    <rect x="17" y="18" width="8" height="12" fill="#050816"/>
    <rect x="39" y="18" width="8" height="12" fill="#050816"/>
    <rect x="28" y="8" width="8" height="4" fill="#050816"/>
  </g>
  <g id="eyes">
    <rect x="27" y="22" width="3" height="3" fill="#050816"/>
    <rect x="35" y="22" width="3" height="3" fill="#050816"/>
    <rect x="30" y="28" width="5" height="2" fill="#D94F45"/>
  </g>
</svg>
)svg";
static constexpr std::array<std::string_view, 6> NpcGroups{
    "shadow", "legs", "body", "head", "hair", "eyes"};

static constexpr std::string_view DragonSvg = R"svg(
<svg viewBox="0 0 80 64">
  <g id="shadow"><ellipse cx="40" cy="55" rx="20" ry="6" fill="#050816" opacity="0.25"/></g>
  <g id="tail"><path d="M21 37 L7 43 L19 47 L27 42 Z" fill="#2C7A5C"/><path d="M8 43 L2 39 L5 48 Z" fill="#74DE8D"/></g>
  <g id="body"><ellipse cx="38" cy="36" rx="19" ry="13" fill="#37B26C"/><ellipse cx="42" cy="34" rx="13" ry="8" fill="#74DE8D" opacity="0.72"/><rect x="31" y="47" width="6" height="7" fill="#20533F"/><rect x="47" y="46" width="6" height="7" fill="#20533F"/></g>
  <g id="wings"><polygon points="31,30 20,13 18,34" fill="#8A5CFF" opacity="0.88"/><polygon points="46,29 62,12 58,35" fill="#8A5CFF" opacity="0.88"/><line x1="31" y1="30" x2="21" y2="23" stroke="#F8F871" stroke-width="2" opacity="0.65"/><line x1="46" y1="29" x2="58" y2="24" stroke="#F8F871" stroke-width="2" opacity="0.65"/></g>
  <g id="head"><ellipse cx="57" cy="30" rx="13" ry="11" fill="#37B26C"/><rect x="58" y="19" width="5" height="7" fill="#F8F871"/><rect x="66" y="24" width="6" height="4" fill="#2C7A5C"/><circle cx="62" cy="29" r="2" fill="#050816"/><rect x="50" y="22" width="5" height="5" fill="#F8F871"/></g>
  <g id="flame"><polygon points="72,31 79,27 76,33 80,38 72,36" fill="#FF7A2F"/><polygon points="73,32 78,31 75,35" fill="#F8F871"/></g>
</svg>
)svg";
static constexpr std::array<std::string_view, 6> DragonGroups{
    "shadow", "tail", "body", "wings", "head", "flame"};

template <std::size_t Count>
std::array<Engine::VectorShapeGroupID, Count>
LoadShape(Engine::VectorShape &shape, std::string_view svg,
          const std::array<std::string_view, Count> &groups) {
  if (!shape.LoadFromSvg(svg) || !shape.Upload())
    throw std::runtime_error(shape.GetLastError());
  auto ids = shape.ResolveGroups(groups);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (!ids[index].IsValid())
      throw std::runtime_error("Missing SVG group: " +
                               std::string(groups[index]));
  }
  return ids;
}
} // namespace

void VectorArtResources::Initialize() {
  Shutdown();
  groundGroups = LoadShape(ground, GroundSvg, GroundGroups);
  roadGroups = LoadShape(road, RoadSvg, RoadGroups);
  houseGroups = LoadShape(house, HouseSvg, HouseGroups);
  signGroups = LoadShape(sign, SignSvg, SignGroups);
  flowerGroups = LoadShape(flower, FlowerSvg, FlowerGroups);
  waterGroups = LoadShape(water, WaterSvg, WaterGroups);
  treeGroups = LoadShape(tree, TreeSvg, TreeGroups);
  playerGroups = LoadShape(player, PlayerSvg, PlayerGroups);
  npcGroups = LoadShape(npc, NpcSvg, NpcGroups);
  dragonGroups = LoadShape(dragon, DragonSvg, DragonGroups);
  roadPose = std::make_unique<Engine::VectorShapePose>(road);
  housePose = std::make_unique<Engine::VectorShapePose>(house);
  signPose = std::make_unique<Engine::VectorShapePose>(sign);
  flowerPose = std::make_unique<Engine::VectorShapePose>(flower);
  waterPose = std::make_unique<Engine::VectorShapePose>(water);
  treePose = std::make_unique<Engine::VectorShapePose>(tree);
  playerPose = std::make_unique<Engine::VectorShapePose>(player);
  npcPose = std::make_unique<Engine::VectorShapePose>(npc);
  dragonPose = std::make_unique<Engine::VectorShapePose>(dragon);
  ready = true;
}

void VectorArtResources::Shutdown() {
  ready = false;
  dragonPose.reset();
  npcPose.reset();
  playerPose.reset();
  treePose.reset();
  waterPose.reset();
  flowerPose.reset();
  signPose.reset();
  housePose.reset();
  roadPose.reset();
  dragon.Unload();
  npc.Unload();
  player.Unload();
  tree.Unload();
  water.Unload();
  flower.Unload();
  sign.Unload();
  house.Unload();
  road.Unload();
  ground.Unload();
}
