#include "componeng/core/engine.hpp"
#include "componeng/core/game.hpp"

#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/utils/utils.hpp"

#include "aim_system.hpp"
#include "helper.hpp"
#include "player_controller.hpp"

using namespace componeng::core;
using namespace componeng::components;
using namespace componeng::ecs;
using namespace componeng::renderer;
using namespace componeng::resources;
using namespace componeng::utils;

constexpr int kOrbCount = 15;
constexpr float kFloorSize = 2.0f;

constexpr Vec3 kCameraPos = Vec3(0, 2, 0);
constexpr Vec3 kCameraRot = Vec3(0, -90, 0);
constexpr Vec3 kFloorColor = Vec3(0.5f, 0.5f, 0.5f);
constexpr Vec3 kOrbColor = Vec3(0.0f, 1.0f, 1.0f);

class AimTrain : public IGame {
public:
  void init(World &world) override {
    world.registerSystem<aim_train::PlayerController>(SystemGroup::Simulation);
    world.registerSystem<aim_train::AimSystem>(SystemGroup::Simulation);

    auto &assetManager = world.get_resource<AssetManager>();
    assetManager.loadAudio(Utils::getAssetPath("assets/audio/boop.wav").c_str(),
                           "boop");
    assetManager.loadAudio(Utils::getAssetPath("assets/audio/gun.wav").c_str(),
                           "gunshot");

    auto &mainCam = world.get_resource<MainCamera>();
    auto &t = world.getComponent<TransformComponent>(mainCam.entity);
    t.position = kCameraPos;
    t.rotation = kCameraRot;

    // Spawn floor at origin
    EntityID floor = world.createEntity();
    world.addComponents(
        floor, MeshComponent{.meshName = "cube"},
        TransformComponent{.position = Vec3(0, -0.25f, 0),
                           .scale = Vec3(kFloorSize, 0.5f, kFloorSize)},
        MaterialComponent{.color = kFloorColor,
                          .textureName = "white",
                          .shaderName = "default"});

    for (int i = 0; i < kOrbCount; ++i) {
      spawnOrb(world);
    }
  }

  void shutdown(World &world) override {
  }
};

int main() {
  Engine &engine = Engine::get();
  engine.init();

  AimTrain game;
  engine.run(game);

  engine.shutdown();
  return 0;
}
