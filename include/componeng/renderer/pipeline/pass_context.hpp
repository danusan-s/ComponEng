#pragma once

#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"

namespace componeng::ecs {
class World;
}

namespace componeng::resources {
struct RenderStats;
}

namespace componeng::renderer {

class AssetManager;
class FrameUniforms;
class InstanceBufferCache;
class MaterialBindings;
class PassProfiler;
class PipelineCache;
class RenderQueue;

/**
 * @brief What every pass of a frame needs, gathered so a pass takes one
 * argument. Built once per frame by RenderSystem and handed to every pass of
 * the RenderPipeline.
 *
 * `instances`, `materials` and `pipelines` live across frames in
 * RenderSystem, because they own GPU objects. A pass only borrows them.
 */
struct PassContext {
  ecs::World &world;
  ecs::EntityID camera;
  rhi::IDevice &device;
  const AssetManager &assets;
  const FrameUniforms &frame;
  PassProfiler &profiler;
  resources::RenderStats &stats;
  const RenderQueue &queue;
  const InstanceBufferCache &instances;
  MaterialBindings &materials;
  PipelineCache &pipelines;
  /** A depth pre-pass runs this frame, so the opaque pass shades only the
   *  nearest surface of each pixel (see renderStateFor). */
  bool depthPrepassed;
  /** The off-screen image the scene is drawn into this frame, for a pass that
   *  post-processes it onto the window; null when the scene is drawn straight
   *  to the window. Sample it with `sceneSampler`. */
  rhi::IImage *sceneColor;
  rhi::ISampler *sceneSampler;
  /** The formats of the rendering the scene passes draw in, which a pipeline
   *  they create must match. */
  rhi::Format colorFormat;
  rhi::Format depthFormat;
  /** Smooth the edges while copying the scene target to the window. Off, the
   *  copy only scales it (a render scale other than 1). */
  bool fxaa;
};

} // namespace componeng::renderer
