#include "componeng/renderer/render_system.hpp"

#include "componeng/camera/main_camera.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/backend/api/render_pass.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/pipeline/batch_drawer.hpp"
#include "componeng/renderer/pipeline/pass_context.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/renderer/pipeline/sky_pass.hpp"
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
  const PassContext ctx{device, world.getResource<AssetManager>(), m_frame,
                        profiler, stats};

  // The queue is cleared by BatchingSystem at the start of next frame, which
  // lets its batch storage be reused instead of freed.
  const auto &queue = world.getResource<RenderQueue>();

  profiler.beginUpload();
  stats.uploadBytes = m_instances.upload(device, queue);
  profiler.endUpload();

  // The queue is sorted opaque first, transparent last.
  const std::size_t opaqueEnd = queue.opaqueCount();
  const bool prepassed = depthPrepassEnabled(world) && opaqueEnd > 0;

  BatchDrawer drawer(ctx, m_instances, m_binder, prepassed);

  // Runs `drawer` over queue[first, last) in `pass`, with the pass bracketed
  // for timing and the drawer's bound state forgotten afterwards.
  auto runBatches = [&](api::RenderPass pass, std::size_t first,
                        std::size_t last) {
    profiler.beginPass(pass);
    for (std::size_t i = first; i < last; ++i) {
      drawer.draw(queue.sorted(i), pass);
    }
    profiler.timeShader(nullptr);
    profiler.endPass(pass);
    drawer.forgetBoundState();
  };

  // Depth pre-pass: the opaque batches again, depth only. The opaque pass then
  // shades just the pixel's nearest surface.
  runBatches(api::RenderPass::DepthPrepass, 0, prepassed ? opaqueEnd : 0);

  runBatches(api::RenderPass::Opaque, 0, opaqueEnd);

  // After the opaque batches and before the transparent ones: anything blended
  // or drawn without depth testing must land on top of the sky.
  profiler.beginPass(api::RenderPass::Sky);
  drawSkyPass(ctx, world, camera);
  profiler.endPass(api::RenderPass::Sky);
  drawer.forgetBoundState();

  runBatches(api::RenderPass::Transparent, opaqueEnd, queue.size());
}

} // namespace componeng::renderer
