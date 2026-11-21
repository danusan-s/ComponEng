#pragma once

#include "componeng/ecs/system.hpp"
#include "componeng/renderer/draw/instance_buffer_cache.hpp"
#include "componeng/renderer/draw/material_binder.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/render_pipeline.hpp"

namespace componeng::renderer {

/**
 * @brief Presentation-phase system that draws the frame from the RenderQueue
 * BatchingSystem built: depth pre-pass, opaque, sky, transparent, in that
 * order.
 *
 * It builds the RenderPipeline once (onCreate), and each frame gathers the
 * per-frame values, uploads the instance data and runs the pipeline. What each
 * piece does lives beside it:
 *  - renderer/pass/:     one RenderPass per stage of the frame.
 *  - renderer/pipeline/: RenderPipeline (runs the passes), PassContext (what a
 *                        pass gets), FrameUniforms (per-frame values),
 *                        PassProfiler (timing).
 *  - renderer/draw/:     what the batch passes share: BatchDrawer (one batch's
 *                        draw), InstanceBufferCache (instance data upload),
 *                        MaterialBinder (material data), renderStateFor
 *                        (pipeline state per pass).
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
  RenderPipeline m_pipeline;
  // These live across frames because they own GPU buffers, or remember what
  // was already reported.
  FrameUniforms m_frame;
  InstanceBufferCache m_instances;
  MaterialBinder m_binder;
};

} // namespace componeng::renderer
