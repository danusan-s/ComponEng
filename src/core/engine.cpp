#include "renderer/resource_manager.hpp"

#include "core/debug_ui.hpp"
#include "core/engine.hpp"
#include "core/logger.hpp"
#include "core/utils.hpp"
#include "ecs/entity.hpp"
#include <GLFW/glfw3.h>

#include "core/logger.hpp"
#include "systems/render_system.hpp"

void Engine::Init() {
  window.Init(1280, 720, "ECS Game");
  world.Init();
  DebugUI::Init();

  ResourceManager::LoadShader(
      Utils::GetAssetPath("assets/shaders/diffuse.vert").c_str(),
      Utils::GetAssetPath("assets/shaders/diffuse.frag").c_str(), nullptr,
      "default");

  ResourceManager::LoadTexture(
      Utils::GetAssetPath("assets/textures/white.png").c_str(), false, "white");

  ResourceManager::LoadMesh(
      Utils::GetAssetPath("assets/models/cube.obj").c_str(), "cube");
  ResourceManager::LoadMesh(
      Utils::GetAssetPath("assets/models/sphere_smooth.obj").c_str(), "sphere");

  RegisterComponents();
  RegisterSystems();
  InitObjects();
}

void Engine::RegisterComponents() {
  world.RegisterComponents<TransformComponent, MeshComponent, MaterialComponent,
                           CameraComponent, MouseInputComponent,
                           RigidBodyComponent, InputComponent,
                           BoundingBoxComponent>();
  world.RegisterComponents<MainCameraSingleton>();
}

void Engine::RegisterSystems() {
  world.RegisterSystem<InputSystem>(SystemGroup::Initialization);
  world.RegisterSystem<CameraSystem>(SystemGroup::Simulation);
  world.RegisterSystem<PhysicsSystem>(SystemGroup::Simulation);
  world.RegisterSystem<OpenGLRenderSystem>(SystemGroup::Presentation);
}

void Engine::InitObjects() {
  EntityID cameraEntity = world.CreateEntity();
  world.AddComponents(cameraEntity,
                      TransformComponent{.position = Vec3(0.0f, 5.0f, 0.0f),
                                         .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                         .scale = Vec3(1.0f)},
                      CameraComponent{.fov = 45.0f,
                                      .aspectRatio = 16.0f / 9.0f,
                                      .nearPlane = 0.1f,
                                      .farPlane = 1000.0f},
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
  world.SetSingleton<MainCameraSingleton>(
      MainCameraSingleton{.entity = cameraEntity});
}

void Engine::Run(IGame &game) {
  game.Init(world);
  world.CreateSystems();

  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  while (!window.ShouldClose()) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    world.time += deltaTime;

    DebugUI::BeginFrame();
    world.UpdateSystems(deltaTime);
    DebugUI::AddValue("FPS", 1.0f / deltaTime);
    DebugUI::EndFrame();

    window.SwapBuffers();
    window.PollEvents();

    while (GLenum err = glGetError()) {
      LOG_ERROR("OpenGL error: %d", err);
    }
  }

  game.Shutdown(world);
  world.DestroySystems();
}

void Engine::Shutdown() {
  DebugUI::Shutdown();
  ResourceManager::Clear();
  window.Shutdown();
}
