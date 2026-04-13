#include "core/engine.hpp"
#include "core/game.hpp"
#include "renderer/resource_manager.hpp"
#include <random>

class Game : public IGame {
public:
  void init(World &world) override {
    std::default_random_engine generator;
    std::uniform_real_distribution<float> randPosition(-500.0f, 500.0f);
    std::uniform_real_distribution<float> randScale(1.0f, 5.0f);
    std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);
    std::uniform_real_distribution<float> randColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> randMass(0.5f, 5.0f);
    std::uniform_int_distribution<int> randVelocity(-5.0f, 5.0f);

    const int count = 100;

    for (int i = 0; i < count; ++i) {
      EntityID entity = world.createEntity();

      float scale = randScale(generator);
      world.addComponents(
          entity, MeshComponent{.meshID = ResourceManager::getMeshID("cube")},
          TransformComponent{.position = Vec3(randPosition(generator),
                                              randPosition(generator),
                                              randPosition(generator)),
                             .rotation = Vec3(0.0f),
                             .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .mass = randMass(generator),
                             .restitution = 1.0f},
          MaterialComponent{
              .color = Vec4(randColor(generator), randColor(generator),
                            randColor(generator), 1.0f),
              .textureID = ResourceManager::getTextureID("white"),
              .shaderID = ResourceManager::getShaderID("default")},
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
          entity, MeshComponent{.meshID = ResourceManager::getMeshID("sphere")},
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
          MaterialComponent{
              .color = Vec4(randColor(generator), randColor(generator),
                            randColor(generator), 1.0f),
              .textureID = ResourceManager::getTextureID("white"),
              .shaderID = ResourceManager::getShaderID("default")},
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
        MeshComponent{.meshID = ResourceManager::getMeshID("cube")},
        MaterialComponent{.color = Vec4(0.5f, 0.5f, 0.5f, 1.0f),
                          .textureID = ResourceManager::getTextureID("white"),
                          .shaderID = ResourceManager::getShaderID("default")},
        RigidBodyComponent{.type = RigidBodyComponent::Static,
                           .restitution = 1.0f},
        ColliderComponent{.type = ColliderType::Box,
                          .transform =
                              TransformComponent{.position = Vec3(0.0f),
                                                 .rotation = Vec3(0.0f),
                                                 .scale = Vec3(1.0f)}});
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
