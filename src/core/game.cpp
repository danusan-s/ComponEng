#include "components/plane_component.hpp"
#include "renderer/resource_manager.hpp"

#include "components/bounding_box_component.hpp"
#include "core/debug_ui.hpp"
#include "core/game.hpp"
#include "core/logger.hpp"
#include "core/utils.hpp"
#include "ecs/entity.hpp"
#include "systems/mesh_generator.hpp"
#include "systems/physics_system.hpp"
#include <GLFW/glfw3.h>
#include <random>

#include "components/camera_component.hpp"
#include "components/input_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "core/logger.hpp"
#include "systems/camera_system.hpp"
#include "systems/input_system.hpp"
#include "systems/render_system.hpp"

void Game::Init() {
  window.Init(1280, 720, "ECS Game");
  world.Init();
  DebugUI::Init();

  ResourceManager::LoadShader(
      Utils::GetAssetPath("assets/shaders/diffuse.vert").c_str(),
      Utils::GetAssetPath("assets/shaders/diffuse.frag").c_str(), nullptr,
      "default");
  ResourceManager::LoadShader(
      Utils::GetAssetPath("assets/shaders/water.vert").c_str(),
      Utils::GetAssetPath("assets/shaders/water.frag").c_str(), nullptr,
      "water");

  ResourceManager::LoadTexture(
      Utils::GetAssetPath("assets/textures/white.png").c_str(), false, "white");

  ResourceManager::LoadMesh(
      Utils::GetAssetPath("assets/models/cube.obj").c_str(), "cube");
  ResourceManager::LoadMesh(
      Utils::GetAssetPath("assets/models/sphere_smooth.obj").c_str(), "sphere");

  InitComponents();
  InitObjects();
  InitSystems();
}

void Game::InitComponents() {
  world.RegisterComponents<TransformComponent, MeshComponent, MaterialComponent,
                           CameraComponent, MouseInputComponent,
                           RigidBodyComponent, InputComponent,
                           BoundingBoxComponent, PlaneComponent>();
}

void Game::InitSystems() {
  world.RegisterSystem<MeshGenerator>();
  world.RegisterSystem<InputSystem>();
  world.RegisterSystem<CameraSystem>();
  world.RegisterSystem<PhysicsSystem>();
  world.RegisterSystem<OpenGLRenderSystem>();
}

void Game::InitObjects() {
  EntityID cameraEntity = world.CreateEntity();
  world.AddComponents(cameraEntity,
                      TransformComponent{.position = Vec3(0.0f, 5.0f, 0.0f),
                                         .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                         .scale = Vec3(1.0f)},
                      CameraComponent{.fov = 45.0f,
                                      .aspectRatio = 16.0f / 9.0f,
                                      .nearPlane = 0.1f,
                                      .farPlane = 1000.0f,
                                      .isMainCamera = true},
                      InputComponent{.forward = false,
                                     .backward = false,
                                     .left = false,
                                     .right = false,
                                     .jump = false,
                                     .crouch = false},
                      MouseInputComponent{.deltaX = 0.0f,
                                          .deltaY = 0.0f,
                                          .leftButton = false,
                                          .rightButton = false});

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
                                        .shaderName = "water"});

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
        BoundingBoxComponent{.min = Vec3(-scale / 2.0f),
                             .max = Vec3(scale / 2.0f)},
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

void Game::Run() {
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  while (!window.ShouldClose()) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    world.time += deltaTime;

    DebugUI::BeginFrame();
    world.Update(deltaTime);
    DebugUI::EndFrame();

    window.SwapBuffers();
    window.PollEvents();

    while (GLenum err = glGetError()) {
      LOG_ERROR("OpenGL error: %d", err);
    }
  }
}

void Game::Shutdown() {
  world.Shutdown();
  DebugUI::Shutdown();
  ResourceManager::Clear();
  window.Shutdown();
}
