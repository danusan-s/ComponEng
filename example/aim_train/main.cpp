#include "componeng/core/engine.hpp"
#include "componeng/core/game.hpp"

#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/utils/utils.hpp"

#include "aim_system.hpp"

#include <random>

using namespace componeng::core;
using namespace componeng::components;
using namespace componeng::ecs;
using namespace componeng::renderer;
using namespace componeng::resources;
using namespace componeng::utils;

class AimTrain : public IGame {
public:
    void init(World &world) override {
        world.registerSystem<aim_train::AimSystem>(SystemGroup::Simulation);

        auto &mainCam = world.get_resource<MainCamera>();
        auto &t = world.getComponent<TransformComponent>(mainCam.entity);
        t.position = Vec3(0, 2, 20);
        t.rotation = Vec3(0, -90, 0);

        std::default_random_engine gen;
        std::uniform_real_distribution<float> distScale(0.3f, 0.8f);
        std::uniform_real_distribution<float> distX(-25.0f, 25.0f);
        std::uniform_real_distribution<float> distY(0.5f, 10.0f);
        std::uniform_real_distribution<float> distZ(-15.0f, -60.0f);

        auto spawnWall = [&](Vec3 pos, Vec3 scale) {
            EntityID e = world.createEntity();
            world.addComponents(e,
                MeshComponent{.meshName = "cube"},
                TransformComponent{.position = pos, .scale = scale},
                MaterialComponent{
                    .color = Vec3(0.5f, 0.5f, 0.5f),
                    .textureName = "white",
                    .shaderName = "default"
                }
            );
        };
        spawnWall(Vec3(0, -0.5f, -17.5f), Vec3(50, 0.5f, 85));
        spawnWall(Vec3(0, 5, 25), Vec3(50, 10, 0.5f));
        spawnWall(Vec3(0, 5, -60), Vec3(50, 10, 0.5f));
        spawnWall(Vec3(-25, 5, -17.5f), Vec3(0.5f, 10, 85));
        spawnWall(Vec3(25, 5, -17.5f), Vec3(0.5f, 10, 85));

        for (int i = 0; i < 20; ++i) {
            float s = distScale(gen);
            EntityID e = world.createEntity();
            world.addComponents(e,
                MeshComponent{.meshName = "sphere"},
                TransformComponent{
                    .position = Vec3(distX(gen), distY(gen), distZ(gen)),
                    .scale = Vec3(s)
                },
                MaterialComponent{
                    .color = Vec3(0.0f, 1.0f, 1.0f),
                    .textureName = "white",
                    .shaderName = "default"
                },
                ColliderComponent{
                    .type = ColliderType::Sphere,
                    .transform = TransformComponent{.scale = Vec3(1)}
                }
            );
        }
    }

    void shutdown(World &world) override {}
};

int main() {
    Engine &engine = Engine::get();
    engine.init();

    AimTrain game;
    engine.run(game);

    engine.shutdown();
    return 0;
}
