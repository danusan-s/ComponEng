#include "componeng/core/engine.hpp"
#include "componeng/core/game.hpp"
#include "componeng/renderer/asset_manager.hpp"

#include "collision_sounds.hpp"
#include "player_controller.hpp"
#include <random>

#include "componeng/components/audio_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/rigidbody_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/utils/utils.hpp"

using namespace componeng::core;
using namespace componeng::components;
using namespace componeng::ecs;
using namespace componeng::renderer;
using namespace componeng::utils;

class Game : public IGame {
public:
  void init(World &world) override {
    auto &assetManager = world.get_resource<AssetManager>();
    assetManager.loadAudio(Utils::getAssetPath("assets/audio/boop.wav").c_str(),
                           "boop");

    std::default_random_engine generator;
    std::uniform_real_distribution<float> randPosition(-500.0f, 500.0f);
    std::uniform_real_distribution<float> randScale(1.0f, 5.0f);
    std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);
    std::uniform_real_distribution<float> randColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> randMass(0.5f, 5.0f);
    std::uniform_int_distribution<int> randVelocity(-5.0f, 5.0f);

    // EntityID soundEntity = world.createEntity();
    // world.addComponents(
    //     soundEntity,
    //     TransformComponent{.position = glm::vec3(0, 0, 0),
    //                        .rotation = glm::vec3(0, 0, 0),
    //                        .scale = glm::vec3(1, 1, 1)},
    //     AudioComponent{.audioID = assetManager.getAudioID("boop"),
    //                    .playOnAwake = true,
    //                    .loop = true,
    //                    .volume = 0.8f,
    //                    .is3D = true,
    //                    .minDistance = 1.0f,
    //                    .maxDistance = 300.0f},
    //     MaterialComponent{.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
    //                       .textureID = assetManager.getTextureID("white"),
    //                       .shaderID = assetManager.getShaderID("default")},
    //     MeshComponent{.meshID = assetManager.getMeshID("cube")});

    const int count = 10;

    for (int i = 0; i < count; ++i) {
      componeng::ecs::EntityID entity = world.createEntity();

      float scale = randScale(generator);
      world.addComponents(
          entity,
          componeng::components::MeshComponent{
              .meshID = assetManager.getMeshID("cube")},
          componeng::components::TransformComponent{
              .position = Vec3(randPosition(generator), randPosition(generator),
                               randPosition(generator)),
              .rotation = Vec3(0.0f),
              .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .mass = randMass(generator),
                             .restitution = 1.0f},
          MaterialComponent{.color =
                                Vec4(randColor(generator), randColor(generator),
                                     randColor(generator), 1.0f),
                            .textureID = assetManager.getTextureID("white"),
                            .shaderID = assetManager.getShaderID("default")},
          ColliderComponent{.type = ColliderType::Box,
                            .transform =
                                TransformComponent{.position = Vec3(0.0f),
                                                   .rotation = Vec3(0.0f),
                                                   .scale = Vec3(1.0f)}});
    }

    for (int i = 0; i < count; ++i) {
      EntityID entity = world.createEntity();

      float scale = randScale(generator);
      world.addComponents(
          entity, MeshComponent{.meshID = assetManager.getMeshID("sphere")},
          TransformComponent{.position = Vec3(randPosition(generator),
                                              randPosition(generator),
                                              randPosition(generator)),
                             .rotation = Vec3(0.0f),
                             .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .velocity =
                                 Vec3(0.0f, randVelocity(generator), 0.0f),
                             .mass = randMass(generator),
                             .restitution = 1.0f},
          MaterialComponent{.color =
                                Vec4(randColor(generator), randColor(generator),
                                     randColor(generator), 1.0f),
                            .textureID = assetManager.getTextureID("white"),
                            .shaderID = assetManager.getShaderID("default")},
          ColliderComponent{.type = ColliderType::Sphere,
                            .transform =
                                TransformComponent{.position = Vec3(0.0f),
                                                   .rotation = Vec3(0.0f),
                                                   .scale = Vec3(1.0f)}});
    }

    EntityID ground = world.createEntity();
    world.addComponents(
        ground,
        TransformComponent{.position = Vec3(0.0f, -1000.0f, 0.0f),
                           .rotation = Vec3(0.0f),
                           .scale = Vec3(1000.0f, 1.0f, 1000.0f)},
        MeshComponent{.meshID = assetManager.getMeshID("cube")},
        MaterialComponent{.color = Vec4(0.5f, 0.5f, 0.5f, 1.0f),
                          .textureID = assetManager.getTextureID("white"),
                          .shaderID = assetManager.getShaderID("default")},
        RigidBodyComponent{.type = RigidBodyComponent::Static,
                           .restitution = 1.0f},
        ColliderComponent{.type = ColliderType::Box,
                          .transform =
                              TransformComponent{.position = Vec3(0.0f),
                                                 .rotation = Vec3(0.0f),
                                                 .scale = Vec3(1.0f)}});

    world.registerSystem<PlayerController>(SystemGroup::Simulation);
    world.registerSystem<CollisionSounds>(SystemGroup::Simulation);
  }

  void shutdown(World &world) override {
  }
};

int main() {
  Engine &engine = Engine::get();
  engine.init();

  Game testScene;
  engine.run(testScene);

  engine.shutdown();
  return 0;
}
