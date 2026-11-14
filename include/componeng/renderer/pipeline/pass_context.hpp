#pragma once

namespace componeng::resources {
struct RenderStats;
}

namespace componeng::renderer {

class AssetManager;
class FrameUniforms;
class PassProfiler;
namespace api {
class IRenderDevice;
}

/**
 * @brief What every pass of a frame needs, gathered so a pass takes one
 * argument instead of five. Built once per frame by RenderSystem.
 */
struct PassContext {
  api::IRenderDevice &device;
  const AssetManager &assets;
  const FrameUniforms &frame;
  PassProfiler &profiler;
  resources::RenderStats &stats;
};

} // namespace componeng::renderer
