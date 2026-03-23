#include "game.hpp"
#include "components/bounding_box_component.hpp"
#include "entity.hpp"
#include "resource_manager.hpp"
#include "systems/physics_system.hpp"
#include "utils.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <random>

#include "components/camera_component.hpp"
#include "components/input_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "systems/camera_system.hpp"
#include "systems/input_system.hpp"
#include "systems/render_system.hpp"
#include "systems/ui_system.hpp"

void Game::Init() {
  window.Init(1280, 720, "ECS Game", inputState);
  world.Init(&inputState);

  ResourceManager::LoadShader(
      Utils::GetAssetPath("shaders/diffuse.vert").c_str(),
      Utils::GetAssetPath("shaders/diffuse.frag").c_str(), nullptr, "default");

  ResourceManager::LoadTexture(
      Utils::GetAssetPath("textures/white.png").c_str(), false, "white");

  ResourceManager::LoadModel(Utils::GetAssetPath("models/cube.obj").c_str(),
                             "cube");
  ResourceManager::LoadModel(
      Utils::GetAssetPath("models/sphere_smooth.obj").c_str(), "sphere");

  InitComponents();
  InitSystems();
  InitObjects();
}

void Game::InitComponents() {
  world.RegisterComponents<TransformComponent, MeshComponent, MaterialComponent,
                           CameraComponent, MouseInputComponent,
                           RigidBodyComponent, InputComponent,
                           BoundingBoxComponent>();
}

void Game::InitSystems() {
  auto inputSystem = world.RegisterSystem<InputSystem>();

  auto cameraSystem = world.RegisterSystem<CameraSystem>();

  auto physicsSystem = world.RegisterSystem<PhysicsSystem>();

  auto renderSystem = world.RegisterSystem<OpenGLRenderSystem>();

  auto uiSystem = world.RegisterSystem<UISystem>();
}

void Game::InitObjects() {
  EntityID cameraEntity = world.CreateEntity();
  world.AddComponents(cameraEntity,
                      TransformComponent{.position = Vec3(0.0f, 0.0f, 5.0f),
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
        entity, MeshComponent{.modelName = "cube"},
        TransformComponent{
            .position = Vec3(randPosition(generator), randPosition(generator),
                             randPosition(generator)),
            .rotation = Vec3(randRotation(generator), randRotation(generator),
                             randRotation(generator)),
            .scale = Vec3(scale)},
        RigidBodyComponent{.velocity = Vec3(0.0f),
                           .acceleration =
                               Vec3(0.0f, randGravity(generator), 0.0f),
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
        entity, MeshComponent{.modelName = "sphere"},
        TransformComponent{
            .position = Vec3(randPosition(generator), randPosition(generator),
                             randPosition(generator)),
            .rotation = Vec3(randRotation(generator), randRotation(generator),
                             randRotation(generator)),
            .scale = Vec3(scale)},
        RigidBodyComponent{.velocity = Vec3(0.0f),
                           .acceleration =
                               Vec3(0.0f, randGravity(generator), 0.0f),
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

    world.Update(deltaTime);

    window.SwapBuffers();
    window.PollEvents();

    while (GLenum err = glGetError()) {
      std::cerr << "OpenGL error: " << err << std::endl;
    }
  }
}

void Game::Shutdown() {
  world.Shutdown();
  ResourceManager::Clear();
  window.Shutdown();
}
