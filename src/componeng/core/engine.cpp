#include "componeng/core/engine.hpp"

#include "componeng/audio/audio_component.hpp"
#include "componeng/audio/audio_manager.hpp"
#include "componeng/audio/audio_system.hpp"
#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/camera_system.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/input/input_system.hpp"
#include "componeng/physics/collider_component.hpp"
#include "componeng/physics/physics_system.hpp"
#include "componeng/physics/rigidbody_component.hpp"
#include "componeng/renderer/asset/diffuse.hpp"
#include "componeng/renderer/asset/pbr.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/opengl/gl_render_device.hpp"
#include "componeng/renderer/batching/batching_system.hpp"
#include "componeng/renderer/component/light_component.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/component/procedural_instances_component.hpp"
#include "componeng/renderer/component/skybox_component.hpp"
#include "componeng/renderer/culling/culling_system.hpp"
#include "componeng/renderer/lod/lod_component.hpp"
#include "componeng/renderer/lod/lod_system.hpp"
#include "componeng/renderer/render_system.hpp"
#include "componeng/resources/fog.hpp"
#include "componeng/utils/logger.hpp"
#include "componeng/utils/utils.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace componeng::core {
namespace {

/**
 * Averages samples over a window and shows the last completed average.
 *
 * add() accumulates every frame; publish() turns what has accumulated into the
 * displayed value and starts a new window. A number that only changes a couple
 * of times a second is readable at any frame rate, which a per-frame moving
 * average is not (at hundreds of fps it still redraws different digits every
 * frame). Negative samples mean "no data" and are ignored; `value` stays
 * negative until the first window completes.
 */
struct Smoothed {
  double value = -1.0;

  void add(double sample) {
    if (sample < 0.0)
      return;
    m_sum += sample;
    ++m_count;
  }

  void publish() {
    if (m_count > 0)
      value = m_sum / static_cast<double>(m_count);
    m_sum = 0.0;
    m_count = 0;
  }

private:
  double m_sum = 0.0;
  int m_count = 0;
};

/**
 * Per-frame CPU and GPU timings for the debug UI.
 *
 *  - CPU frame: from the start of the frame to just before the buffer swap,
 *    i.e. all systems plus submitting the UI. Includes the time spent issuing
 *    GL calls, but not waiting at the swap.
 *  - Swap + events: the swap and event polling. Large here means the CPU is
 *    waiting on the GPU (or the driver), i.e. the frame is GPU-bound.
 *  - GPU frame: GPU time for the frame's commands, measured with timer
 *    queries (a few frames behind, see IRenderDevice::beginGpuTimer).
 *  - Per system: each system's onUpdate wall time.
 *
 * Each frame's samples are accumulated and the displayed numbers refresh every
 * kWindowSeconds with the average over that window (and once right after the
 * first frame, so the panel is not blank at start-up).
 */
class FrameTimings {
public:
  static constexpr double kWindowSeconds = 0.5;

  void record(const ecs::World &world, const renderer::AssetManager &assets,
              double deltaSeconds, double cpuMs, double swapMs, double gpuMs) {
    m_cpu.add(cpuMs);
    m_swap.add(swapMs);
    m_gpu.add(gpuMs);

    assets.forEachShader([&](Name name, const renderer::Shader &shader) {
      auto it = std::find_if(
          m_shaders.begin(), m_shaders.end(),
          [&](const SystemTime &s) { return s.name == name.c_str(); });
      if (it == m_shaders.end()) {
        m_shaders.push_back({name.c_str(), {}});
        it = m_shaders.end() - 1;
      }
      it->ms.add(shader.lastGpuTimeMs());
    });

    std::size_t i = 0;
    world.forEachSystemTiming([&](const char *name, double ms) {
      if (i >= m_systems.size())
        m_systems.push_back({name, {}});
      m_systems[i++].ms.add(ms);
    });

    m_elapsed += deltaSeconds;
    if (!m_published || m_elapsed >= kWindowSeconds) {
      m_cpu.publish();
      m_swap.publish();
      m_gpu.publish();
      for (SystemTime &s : m_systems)
        s.ms.publish();
      for (SystemTime &s : m_shaders)
        s.ms.publish();
      m_elapsed = 0.0;
      m_published = true;
    }
  }

  void show() const {
    if (!DebugUI::addSection("Frame timing (ms)"))
      return;
    DebugUI::addValue("CPU frame", static_cast<float>(m_cpu.value));
    if (m_gpu.value >= 0.0)
      DebugUI::addValue("GPU frame", static_cast<float>(m_gpu.value));
    else
      DebugUI::addText("GPU frame", "n/a");
    DebugUI::addValue("Swap + events", static_cast<float>(m_swap.value));

    if (DebugUI::addSection("System CPU (ms), slowest first"))
      showSorted(m_systems);
    if (DebugUI::addSection("Shader GPU (ms), slowest first"))
      showSorted(m_shaders);
  }

private:
  struct SystemTime {
    std::string name;
    Smoothed ms;
  };

  static void showSorted(const std::vector<SystemTime> &times) {
    std::vector<const SystemTime *> order;
    order.reserve(times.size());
    for (const SystemTime &s : times) {
      if (s.ms.value >= 0.0) // appeared since the last refresh
        order.push_back(&s);
    }
    std::sort(order.begin(), order.end(),
              [](const SystemTime *a, const SystemTime *b) {
                return a->ms.value > b->ms.value;
              });
    for (const SystemTime *s : order)
      DebugUI::addValue(s->name, static_cast<float>(s->ms.value));
  }

  double m_elapsed = 0.0;
  bool m_published = false;

  Smoothed m_cpu;
  Smoothed m_swap;
  Smoothed m_gpu;
  std::vector<SystemTime> m_systems;
  std::vector<SystemTime> m_shaders;
};

} // namespace

void Engine::init() {
  m_world.init();

  std::unique_ptr<renderer::api::IRenderDevice> renderDevice =
      std::make_unique<renderer::opengl::GLRenderDevice>();
  m_world.setResource<std::unique_ptr<renderer::api::IRenderDevice>>(
      std::move(renderDevice));

  m_window.init(1280, 720, "ECS Game");
  m_world.setWindowHandle(m_window.getHandle());

  LOG_INFO("Initialized render device");

  DebugUI::init();

  m_world.setResource<audio::AudioManager>(audio::AudioManager());
  auto &audioManager = m_world.getResource<audio::AudioManager>();
  audioManager.init();

  LOG_INFO("Initialized audio engine");

  m_world.setResource<renderer::AssetManager>(
      renderer::AssetManager(m_world.getRenderDevice()));
  auto &assetManager = m_world.getResource<renderer::AssetManager>();

  LOG_INFO("Loading built-in assets...");

  assetManager.loadShader(
      utils::Utils::getAssetPath("assets/shaders/diffuse.vert").c_str(),
      utils::Utils::getAssetPath("assets/shaders/diffuse.frag").c_str(),
      nullptr, "default");

  assetManager.loadTexture(
      utils::Utils::getAssetPath("assets/textures/white.png").c_str(), false,
      "white");

  assetManager.registerMaterial<renderer::DiffuseMaterial>("default_diffuse",
                                                           "default", "white");

  // Same shader as default_diffuse, but blended and without depth writes so
  // things behind it show through. The entity's ColorComponent alpha sets the
  // opacity. Culling is off so a thin plane is visible from both sides.
  assetManager.registerMaterial<renderer::DiffuseMaterial>(
      "transparent_diffuse", "default", "white");
  {
    renderer::api::RenderState state;
    state.cull = renderer::api::CullMode::None;
    state.depthWrite = false;
    state.blend = renderer::api::BlendMode::Alpha;
    assetManager.getMaterial(assetManager.getMaterialID("transparent_diffuse"))
        .setRenderState(state);
  }

  assetManager.loadShader(
      utils::Utils::getAssetPath("assets/shaders/pbr.vert").c_str(),
      utils::Utils::getAssetPath("assets/shaders/pbr.frag").c_str(), nullptr,
      "pbr");

  assetManager.loadShader(
      utils::Utils::getAssetPath("assets/shaders/skybox_procedural.vert")
          .c_str(),
      utils::Utils::getAssetPath("assets/shaders/skybox_procedural.frag")
          .c_str(),
      nullptr, "skybox");

  // Set up fog resource
  m_world.setResource<resources::Fog>(resources::Fog{});

  assetManager.loadMesh(
      utils::Utils::getAssetPath("assets/models/cube.obj").c_str(), "cube");
  assetManager.loadMesh(
      utils::Utils::getAssetPath("assets/models/sphere_smooth.obj").c_str(),
      "sphere");

  LOG_INFO("Finished loading built-in assets");

  registerComponents();
  registerSystems();
}

void Engine::registerComponents() {
  m_world.registerComponents<
      core::TransformComponent, renderer::MeshComponent,
      renderer::MaterialComponent, renderer::ColorComponent,
      camera::CameraComponent, physics::RigidBodyComponent,
      physics::ColliderComponent, audio::AudioComponent,
      renderer::DirectionalLightComponent, renderer::SkyboxComponent,
      renderer::LodComponent, renderer::ProceduralInstancesComponent>();
}

void Engine::registerSystems() {
  m_world.registerSystem<input::InputSystem>(ecs::SystemGroup::Initialization);
  m_world.registerSystem<physics::PhysicsSystem>(ecs::SystemGroup::Simulation);
  m_world.registerSystem<camera::CameraSystem>(ecs::SystemGroup::Simulation);
  m_world.registerSystem<audio::AudioSystem>(ecs::SystemGroup::Simulation);
  // LOD first: culling tests the bounds of whichever mesh LOD selected.
  m_world.registerSystem<renderer::LodSystem>(ecs::SystemGroup::Presentation);
  m_world.registerSystem<renderer::CullingSystem>(
      ecs::SystemGroup::Presentation);
  m_world.addSystemDependencies<renderer::CullingSystem, renderer::LodSystem>();
  m_world.registerSystem<renderer::BatchingSystem>(
      ecs::SystemGroup::Presentation);
  m_world.addSystemDependencies<renderer::BatchingSystem,
                                renderer::CullingSystem>();
  m_world.registerSystem<renderer::RenderSystem>(
      ecs::SystemGroup::Presentation);
  m_world.addSystemDependencies<renderer::RenderSystem,
                                renderer::BatchingSystem>();
}

void Engine::ensureDefaultCamera() {
  if (m_world.hasResource<camera::MainCamera>()) {
    return;
  }

  ecs::EntityID cameraEntity = m_world.createEntity();
  m_world.addComponents(
      cameraEntity,
      core::TransformComponent{.position = Vec3(0.0f, 5.0f, 0.0f),
                               .rotation = Vec3(0.0f, 0.0f, 0.0f),
                               .scale = Vec3(1.0f)},
      camera::CameraComponent{.fov = 45.0f,
                              .aspectRatio = 16.0f / 9.0f,
                              .nearPlane = 0.1f,
                              .farPlane = 10000.0f});

  m_world.setResource(camera::MainCamera{.entity = cameraEntity});
}

void Engine::run(IGame &game) {
  ensureDefaultCamera();
  game.init(m_world);
  m_world.createSystems();

  double deltaTime = 0.0f;
  double accumulator = 0.0f;
  double lastFrame = 0.0f;
  long long frameCount = 0;
  double avgFPS = 0.0f;

  LOG_INFO("Starting main loop");

  using Clock = std::chrono::steady_clock;
  const auto msBetween = [](Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };
  FrameTimings timings;

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

    auto &device = m_world.getRenderDevice();
    const auto cpuStart = Clock::now();

    DebugUI::beginFrame();
    if constexpr (DebugUI::kEnabled)
      device.beginGpuTimer();
    m_world.updateSystems(deltaTime);
    DebugUI::addValue("FPS", 1.0f / deltaTime);
    DebugUI::addValue("Avg FPS", avgFPS);
    timings.show();
    DebugUI::endFrame();
    if constexpr (DebugUI::kEnabled)
      device.endGpuTimer();
    if constexpr (DebugUI::kEnabled)
      m_world.getResource<renderer::AssetManager>().collectShaderTimings();

    const auto cpuEnd = Clock::now();
    m_window.swapBuffers();
    m_world.swapInputBuffers();
    m_window.pollEvents();

    device.checkErrors();
    if constexpr (DebugUI::kEnabled) {
      timings.record(m_world, m_world.getResource<renderer::AssetManager>(),
                     deltaTime, msBetween(cpuStart, cpuEnd),
                     msBetween(cpuEnd, Clock::now()), device.lastGpuTimeMs());
    }
  }

  LOG_INFO("Exiting main loop");

  game.shutdown(m_world);
  m_world.destroySystems();
}

void Engine::shutdown() {
  DebugUI::shutdown();
  auto &assetManager = m_world.getResource<renderer::AssetManager>();
  auto &audioEngine = m_world.getResource<audio::AudioManager>();
  assetManager.clear();
  audioEngine.shutdown();
  m_window.shutdown();
}

} // namespace componeng::core
