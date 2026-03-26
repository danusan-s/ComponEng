#include "core/engine.hpp"
#include "core/game.hpp"
#include <random>

class Game : public IGame {
public:
  void Init(World &world) override {
    std::default_random_engine generator;
    std::uniform_real_distribution<float> randPosition(-100.0f, 100.0f);
    std::uniform_real_distribution<float> randRotation(0.0f, 3.0f);
    std::uniform_real_distribution<float> randScale(1.0f, 5.0f);
    std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);
    std::uniform_real_distribution<float> randColor(0.0f, 1.0f);

    for (int i = 0; i < 100; ++i) {
      EntityID entity = world.CreateEntity();

      float scale = randScale(generator);
      world.AddComponents(
          entity, MeshComponent{.meshName = "cube"},
          TransformComponent{
              .position = Vec3(randPosition(generator), randPosition(generator),
                               randPosition(generator)),
              .rotation = Vec3(randRotation(generator), randRotation(generator),
                               randRotation(generator)),
              .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .velocity = Vec3(0.0f),
                             .mass = 1.0f},
          MaterialComponent{.color =
                                Vec4(randColor(generator), randColor(generator),
                                     randColor(generator), 1.0f),
                            .textureName = "white",
                            .shaderName = "default"});
    }

    for (int i = 0; i < 100; ++i) {
      EntityID entity = world.CreateEntity();

      float scale = randScale(generator);
      world.AddComponents(
          entity, MeshComponent{.meshName = "sphere"},
          TransformComponent{
              .position = Vec3(randPosition(generator), randPosition(generator),
                               randPosition(generator)),
              .rotation = Vec3(randRotation(generator), randRotation(generator),
                               randRotation(generator)),
              .scale = Vec3(scale)},
          RigidBodyComponent{.type = RigidBodyComponent::Dynamic,
                             .velocity = Vec3(0.0f),
                             .mass = 1.0f},
          BoundingBoxComponent{.min = Vec3(-scale / 2.0f),
                               .max = Vec3(scale / 2.0f)},
          MaterialComponent{.color =
                                Vec4(randColor(generator), randColor(generator),
                                     randColor(generator), 1.0f),
                            .textureName = "white",
                            .shaderName = "default"});
    }
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
