#include "components/plane_component.hpp"
#include "core/engine.hpp"
#include "core/game.hpp"
#include "systems/mesh_generator.hpp"
#include <random>

class Game : public IGame {
public:
  void onInit() override {
    World &world = Engine::Get().world;
    world.RegisterComponent<PlaneComponent>();
    world.RegisterSystem<MeshGenerator>();

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

    EntityID floor = world.CreateEntity();
    world.AddComponents(floor,
                        PlaneComponent{.width = 100.0f,
                                       .height = 100.0f,
                                       .widthSegments = 100,
                                       .heightSegments = 100,
                                       .normal = Vec3(0.0f, 1.0f, 0.0f),
                                       .meshName = "generated_plane"},
                        RigidBodyComponent{.type = RigidBodyComponent::Static,
                                           .velocity = Vec3(0.0f),
                                           .mass = 1.0f},
                        MaterialComponent{.color = Vec3(0.6f, 0.6f, 1.0f),
                                          .textureName = "white",
                                          .shaderName = "default"});
  }

  void onUpdate(float deltaTime) override {
    World &world = Engine::Get().world;
    world.query<PlaneComponent>().eachOptional([](PlaneComponent &plane) {
      LOG_INFO("Plane: width=%.2f, height=%.2f, segments=%dx%d", plane.width,
               plane.height, plane.widthSegments, plane.heightSegments);
    });
  }
  void onShutdown() override {
  }
};

int main() {
  Engine &engine = Engine::Get();
  engine.Init();

  Game game;
  engine.Run(game);

  engine.Shutdown();
  return 0;
}
