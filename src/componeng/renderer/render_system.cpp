#include "componeng/renderer/render_system.hpp"

#include "componeng/camera/main_camera.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/pass/depth_prepass.hpp"
#include "componeng/renderer/pass/opaque_pass.hpp"
#include "componeng/renderer/pass/sky_pass.hpp"
#include "componeng/renderer/pass/transparent_pass.hpp"
#include "componeng/renderer/pipeline/pass_context.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/resources/render_settings.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

namespace {

bool depthPrepassEnabled(ecs::World &world) {
  return !world.hasResource<resources::RenderSettings>() ||
         world.getResource<resources::RenderSettings>().depthPrepass;
}

} // namespace

void RenderSystem::onCreate(const ecs::SystemState &state) {
  state.world->setResource(resources::RenderStats());

  // The order is the draw order. The sky goes after the opaque geometry and
  // before the transparent one: anything blended or drawn without depth
  // testing must land on top of it.
  m_pipeline.add(std::make_unique<DepthPrepass>())
      .add(std::make_unique<OpaquePass>())
      .add(std::make_unique<SkyPass>())
      .add(std::make_unique<TransparentPass>());
}

void RenderSystem::onUpdate(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  api::IRenderDevice &device = world.getRenderDevice();
  device.setRenderState(api::RenderState{});
  device.clear(0.0f, 0.0f, 0.0f, 1.0f);

  const ecs::EntityID camera = world.getResource<camera::MainCamera>().entity;

  // Counters and CPU timings for the debug panes; see resources::RenderStats.
  auto &stats = world.getResource<resources::RenderStats>();
  stats = resources::RenderStats();

  m_frame.update(world, camera);

  PassProfiler profiler(device, stats);

  // The queue is cleared by BatchingSystem at the start of next frame, which
  // lets its batch storage be reused instead of freed.
  const auto &queue = world.getResource<RenderQueue>();

  profiler.beginUpload();
  stats.uploadBytes = m_instances.upload(device, queue);
  profiler.endUpload();

  // A pre-pass only pays off when there is opaque geometry to prepass.
  const bool prepassed = depthPrepassEnabled(world) && queue.opaqueCount() > 0;

  const PassContext ctx{
      world,       camera,   device,   world.getResource<AssetManager>(),
      m_frame,     profiler, stats,    queue,
      m_instances, m_binder, prepassed};
  m_pipeline.execute(ctx);
}

} // namespace componeng::renderer
