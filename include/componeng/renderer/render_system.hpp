#pragma once

#include "componeng/ecs/system.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/instance_buffer_cache.hpp"
#include "componeng/renderer/pipeline/material_binder.hpp"

namespace componeng::renderer {

/**
 * @brief Presentation-phase system that draws the frame from the RenderQueue
 * BatchingSystem built: depth pre-pass, opaque, sky, transparent, in that
 * order.
 *
 * It only sequences the passes. What each piece does lives under
 * renderer/pipeline/: FrameUniforms (per-frame values), InstanceBufferCache
 * (instance data upload), BatchDrawer (one batch's draw), MaterialBinder
 * (material data), renderStateFor (pipeline state per pass), drawSkyPass, and
 * PassProfiler (timing).
 *
 * Fills resources::RenderStats every frame: per-pass draw, instance, triangle
 * and shader-bind counts, plus (with the debug UI) per-pass CPU submit time.
 * With the debug UI it also times each pass on the GPU, see
 * IRenderDevice::beginGpuPass.
 */
class RenderSystem final : public ecs::ISystem {
public:
  void onCreate(const ecs::SystemState &state) override;
  void onUpdate(const ecs::SystemState &state) override;

private:
  // These live across frames because they own GPU buffers, or remember what
  // was already reported.
  FrameUniforms m_frame;
  InstanceBufferCache m_instances;
  MaterialBinder m_binder;
};

} // namespace componeng::renderer
