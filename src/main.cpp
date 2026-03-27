#include "core/engine.hpp"
#include "core/game.hpp"
#include <random>

class Game : public IGame {
public:
  void Init(World &world) override {
    std::default_random_engine generator;
    std::uniform_real_distribution<float> randPosition(-100.0f, 100.0f);
    std::uniform_real_distribution<float> randScale(1.0f, 5.0f);
    std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);
    std::uniform_real_distribution<float> randColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> randMass(0.5f, 5.0f);
    std::uniform_int_distribution<int> randVelocity(-5.0f, 5.0f);

    for (int i = 0; i < 100; ++i) {
      EntityID entity = world.CreateEntity();

      float scale = randScale(generator);
      world.AddComponents(
          entity, MeshComponent{.meshName = "cube"},
          TransformComponent{.position = Vec3(randPosition(generator),
                                              randPosition(generator),
                                              randPosition(generator)),
                             .rotation = Vec3(0.0f),
                             .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .mass = randMass(generator),
                             .restitution = 1.0f},
          MaterialComponent{.color =
                                Vec4(randColor(generator), randColor(generator),
                                     randColor(generator), 1.0f),
                            .textureName = "white",
                            .shaderName = "default"},
          ColliderComponent{.type = ColliderType::AABB,
                            .shape = AABB{.localCenter = Vec3(0.0f),
                                          .halfExtents = Vec3(scale)}});
    }

    for (int i = 0; i < 100; ++i) {
      EntityID entity = world.CreateEntity();

      float scale = randScale(generator);
      world.AddComponents(
          entity, MeshComponent{.meshName = "sphere"},
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
                            .textureName = "white",
                            .shaderName = "default"},
          ColliderComponent{
              .type = ColliderType::Sphere,
              .shape = Sphere{.localCenter = Vec3(0.0f), .radius = scale}});
    }

    // Create a large static plane as the ground
    EntityID ground = world.CreateEntity();
    world.AddComponents(
        ground,
        TransformComponent{.position = Vec3(0.0f, -100.0f, 0.0f),
                           .rotation = Vec3(0.0f),
                           .scale = Vec3(100.0f, 1.0f, 100.0f)},
        MeshComponent{.meshName = "cube"},
        MaterialComponent{.color = Vec4(0.5f, 0.5f, 0.5f, 1.0f),
                          .textureName = "white",
                          .shaderName = "default"},
        RigidBodyComponent{.type = RigidBodyComponent::Static,
                           .restitution = 1.0f},
        ColliderComponent{.type = ColliderType::AABB,
                          .shape =
                              AABB{.localCenter = Vec3(0.0f),
                                   .halfExtents = Vec3(100.0f, 1.0f, 100.0f)}});
  }

  void Shutdown(World &world) override {
  }
};

int main() {
  Engine &engine = Engine::Get();
  engine.Init();

  Game testScene;
  engine.Run(testScene);

  engine.Shutdown();
  return 0;
}
