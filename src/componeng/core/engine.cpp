#include "componeng/core/engine.hpp"

#include "componeng/components/camera_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/rigidbody_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/utils/utils.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/physics/physics_system.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/opengl/gl_render_device.hpp"
#include "componeng/renderer/render_system.hpp"
#include "componeng/resources/input_state.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/systems/camera_system.hpp"
#include "componeng/systems/input_system.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::core {

void Engine::init() {
  renderer::opengl::GLRenderDevice *renderDevice =
      new renderer::opengl::GLRenderDevice;
  m_render_device = renderDevice;
  m_world.init();

  m_window.init(1280, 720, "ECS Game");
  m_world.setWindowHandle(m_window.getHandle());
  m_world.setRenderDevice(m_render_device);
  DebugUI::init();

  renderer::AssetManager::loadShader(
      utils::Utils::getAssetPath("assets/shaders/diffuse.vert").c_str(),
      utils::Utils::getAssetPath("assets/shaders/diffuse.frag").c_str(), nullptr,
      "default");

  renderer::AssetManager::loadTexture(
      utils::Utils::getAssetPath("assets/textures/white.png").c_str(), false, "white");

  renderer::AssetManager::loadMesh(
      utils::Utils::getAssetPath("assets/models/cube.obj").c_str(), "cube");
  renderer::AssetManager::loadMesh(
      utils::Utils::getAssetPath("assets/models/sphere_smooth.obj").c_str(), "sphere");

  registerComponents();
  registerSystems();
  initObjects();
}

void Engine::registerComponents() {
  m_world.registerComponents<
      components::TransformComponent, components::MeshComponent,
      components::MaterialComponent, components::CameraComponent,
      components::RigidBodyComponent, components::ColliderComponent>();
}

void Engine::registerSystems() {
  m_world.registerSystem<systems::InputSystem>(
      ecs::SystemGroup::Initialization);
  m_world.registerSystem<systems::CameraSystem>(ecs::SystemGroup::Simulation);
  m_world.registerSystem<physics::PhysicsSystem>(ecs::SystemGroup::Simulation);
  m_world.registerSystem<renderer::RenderSystem>(
      ecs::SystemGroup::Presentation);
}

void Engine::initObjects() {
  ecs::EntityID cameraEntity = m_world.createEntity();
  m_world.addComponents(
      cameraEntity,
      components::TransformComponent{.position = Vec3(0.0f, 5.0f, 0.0f),
                                     .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                     .scale = Vec3(1.0f)},
      components::CameraComponent{.fov = 45.0f,
                                  .aspectRatio = 16.0f / 9.0f,
                                  .nearPlane = 0.1f,
                                  .farPlane = 10000.0f});

  m_world.set_resource(resources::MainCamera{.entity = cameraEntity});
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
    m_world.swapInputBuffers();
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
  renderer::AssetManager::clear();
  m_window.shutdown();
  delete m_render_device;
}

} // namespace componeng::core
