#include "game.hpp"
#include "entity.hpp"
#include "resource_manager.hpp"
#include "utils.hpp"
#include <GLFW/glfw3.h>
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
  world.Init();

  ResourceManager::LoadShader(
      Utils::GetAssetPath("shaders/diffuse.vert").c_str(),
      Utils::GetAssetPath("shaders/diffuse.frag").c_str(), nullptr, "default");

  ResourceManager::LoadTexture(
      Utils::GetAssetPath("textures/white.png").c_str(), false, "white");

  ResourceManager::LoadModel(Utils::GetAssetPath("models/cube.obj").c_str(),
                             "cube");

  InitComponents();
  InitSystems();
  InitObjects();
}

void Game::InitComponents() {
  world.RegisterComponent<TransformComponent>();
  world.RegisterComponent<MeshComponent>();
  world.RegisterComponent<MaterialComponent>();
  world.RegisterComponent<CameraComponent>();
  world.RegisterComponent<MouseInputComponent>();
  world.RegisterComponent<RigidBodyComponent>();
  world.RegisterComponent<InputComponent>();
}

void Game::InitSystems() {
  auto inputSystem = world.RegisterSystem<InputSystem>();
  Signature inputSignature;
  inputSignature.set(world.GetComponentType<InputComponent>());
  inputSignature.set(world.GetComponentType<MouseInputComponent>());
  world.SetSystemSignature<InputSystem>(inputSignature);

  auto cameraSystem = world.RegisterSystem<CameraSystem>();
  Signature cameraSignature;
  cameraSignature.set(world.GetComponentType<TransformComponent>());
  cameraSignature.set(world.GetComponentType<CameraComponent>());
  cameraSignature.set(world.GetComponentType<InputComponent>());
  world.SetSystemSignature<CameraSystem>(cameraSignature);

  auto renderSystem = world.RegisterSystem<OpenGLRenderSystem>();
  Signature renderSignature;
  renderSignature.set(world.GetComponentType<TransformComponent>());
  renderSignature.set(world.GetComponentType<MeshComponent>());
  renderSignature.set(world.GetComponentType<MaterialComponent>());
  world.SetSystemSignature<OpenGLRenderSystem>(renderSignature);

  auto uiSystem = world.RegisterSystem<UISystem>();
  Signature uiSignature;
  uiSignature.set(world.GetComponentType<TransformComponent>());
  uiSignature.set(world.GetComponentType<CameraComponent>());
  world.SetSystemSignature<UISystem>(uiSignature);
}

void Game::InitObjects() {
  EntityID cameraEntity = world.CreateEntity();
  world.AddComponent(cameraEntity,
                     TransformComponent{.position = Vec3(0.0f, 0.0f, 5.0f),
                                        .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                        .scale = Vec3(1.0f)});
  world.AddComponent(cameraEntity, CameraComponent{.fov = 45.0f,
                                                   .aspectRatio = 16.0f / 9.0f,
                                                   .nearPlane = 0.1f,
                                                   .farPlane = 1000.0f,
                                                   .isMainCamera = true});
  world.AddComponent(cameraEntity, InputComponent{.forward = false,
                                                  .backward = false,
                                                  .left = false,
                                                  .right = false,
                                                  .jump = false,
                                                  .crouch = false});
  world.AddComponent(cameraEntity, MouseInputComponent{.deltaX = 0.0f,
                                                       .deltaY = 0.0f,
                                                       .leftButton = false,
                                                       .rightButton = false});

  std::default_random_engine generator;
  std::uniform_real_distribution<float> randPosition(-100.0f, 100.0f);
  std::uniform_real_distribution<float> randRotation(0.0f, 3.0f);
  std::uniform_real_distribution<float> randScale(3.0f, 5.0f);
  std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);

  float scale = randScale(generator);

  const Model &cubeModel = ResourceManager::GetModel("cube");
  for (int i = 0; i < 100; ++i) {
    EntityID entity = world.CreateEntity();

    world.AddComponent(entity,
                       MeshComponent{.VAO = cubeModel.VAO,
                                     .VBO = cubeModel.VBO,
                                     .EBO = cubeModel.EBO,
                                     .indexCount = cubeModel.indices.size()});

    world.AddComponent(
        entity,
        TransformComponent{
            .position = Vec3(randPosition(generator), randPosition(generator),
                             randPosition(generator)),
            .rotation = Vec3(randRotation(generator), randRotation(generator),
                             randRotation(generator)),
            .scale = Vec3(scale)});

    world.AddComponent(entity,
                       MaterialComponent{.color = Vec3(0.5f, 0.5f, 0.5f),
                                         .textureName = "white",
                                         .shaderName = "default"});
  }
}

void Game::Run() {
  // deltaTime variables
  // -------------------
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;
  float accumulatedTime = 0.0f;

  //  240 fps simulation
  const float timeStep = 1 / 60.0f;

  auto window = glfwGetCurrentContext();

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    accumulatedTime += deltaTime;

    while (accumulatedTime >= timeStep) {
      // Fixed step loop
      accumulatedTime -= timeStep;
    }

    world.Update(deltaTime);

    glfwSwapBuffers(window);
    glfwPollEvents();

    while (GLenum err = glGetError()) {
      std::cerr << "OpenGL error: " << err << std::endl;
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

void Game::Shutdown() {
  world.Shutdown();
  ResourceManager::Clear();
}
