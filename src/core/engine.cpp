#include "core/engine.hpp"
#include "core/debug_ui.hpp"
#include "core/logger.hpp"
#include "core/utils.hpp"
#include "ecs/entity.hpp"
#include "renderer/resource_manager.hpp"

#include "renderer/render_system.hpp"

void Engine::init() {
  m_window.init(1280, 720, "ECS Game");
  m_world.init();
  m_world.setWindowHandle(m_window.getHandle());
  DebugUI::init();

  ResourceManager::loadShader(
      Utils::getAssetPath("assets/shaders/diffuse.vert").c_str(),
      Utils::getAssetPath("assets/shaders/diffuse.frag").c_str(), nullptr,
      "default");

  ResourceManager::loadTexture(
      Utils::getAssetPath("assets/textures/white.png").c_str(), false, "white");

  ResourceManager::loadMesh(
      Utils::getAssetPath("assets/models/cube.obj").c_str(), "cube");
  ResourceManager::loadMesh(
      Utils::getAssetPath("assets/models/sphere_smooth.obj").c_str(), "sphere");

  registerComponents();
  registerSystems();
  initObjects();
}

void Engine::registerComponents() {
  m_world.registerComponents<TransformComponent, MeshComponent,
                             MaterialComponent, CameraComponent,
                             MouseInputComponent, RigidBodyComponent,
                             InputComponent, ColliderComponent>();
  m_world.registerComponents<MainCameraSingleton>();
}

void Engine::registerSystems() {
  m_world.registerSystem<InputSystem>(SystemGroup::Initialization);
  m_world.registerSystem<CameraSystem>(SystemGroup::Simulation);
  m_world.registerSystem<PhysicsSystem>(SystemGroup::Simulation);
  m_world.registerSystem<RenderSystem>(SystemGroup::Presentation);
}

void Engine::initObjects() {
  EntityID cameraEntity = m_world.createEntity();
  m_world.addComponents(cameraEntity,
                        TransformComponent{.position = Vec3(0.0f, 5.0f, 0.0f),
                                           .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                           .scale = Vec3(1.0f)},
                        CameraComponent{.fov = 45.0f,
                                        .aspectRatio = 16.0f / 9.0f,
                                        .nearPlane = 0.1f,
                                        .farPlane = 10000.0f},
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
  m_world.setSingleton<MainCameraSingleton>(
      MainCameraSingleton{.entity = cameraEntity});
}

void Engine::run(IGame &game) {
  game.init(m_world);
  m_world.createSystems();

  double deltaTime = 0.0f;
  double accumulator = 0.0f;
  double lastFrame = 0.0f;
  long long frameCount = 0;
  double avgFPS = 0.0f;

  while (!m_window.shouldClose()) {
    double currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    accumulator += deltaTime;
    lastFrame = currentFrame;
    frameCount++;
    m_world.time += deltaTime;

    if (accumulator >= 1.0f) {
      avgFPS = frameCount / accumulator;
      frameCount = 0;
      accumulator = 0.0f;
    }

    DebugUI::beginFrame();
    m_world.updateSystems(deltaTime);
    DebugUI::addValue("FPS", 1.0f / deltaTime);
    DebugUI::addValue("Avg FPS: %.2f", avgFPS);
    DebugUI::endFrame();

    m_window.swapBuffers();
    m_window.pollEvents();

    while (GLenum err = glGetError()) {
      LOG_ERROR("OpenGL error: %d", err);
    }
  }

  game.shutdown(m_world);
  m_world.destroySystems();
}

void Engine::shutdown() {
  DebugUI::shutdown();
  ResourceManager::clear();
  m_window.shutdown();
}
