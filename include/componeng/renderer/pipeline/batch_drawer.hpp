#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/backend/api/render_pass.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/pipeline/pass_context.hpp"

namespace componeng::renderer {

class InstanceBufferCache;
class MaterialBinder;

/**
 * @brief Issues the instanced draw for one RenderBatch in a given pass:
 * shader, pipeline state, material data, mesh, instance attributes, draw call,
 * and the pass's counters.
 *
 * The queue is sorted by shader, then texture, so consecutive batches often
 * share GPU state. The drawer remembers what it left bound and skips the
 * redundant work. That memory is only trusted while the drawer is the only
 * thing touching GL: anything else that binds a shader (the sky pass, ImGui)
 * makes it stale, so call forgetBoundState() after one.
 */
class BatchDrawer {
public:
  /** `depthPrepassed`: a depth pre-pass already ran, so opaque batches are
   *  shaded only where they are the nearest surface (see renderStateFor). */
  BatchDrawer(const PassContext &ctx, const InstanceBufferCache &instances,
              MaterialBinder &binder, bool depthPrepassed);

  /** `pass` is DepthPrepass (the shader's depth-only variant), Opaque or
   *  Transparent. */
  void draw(const RenderBatch &batch, api::RenderPass pass);

  void forgetBoundState();

private:
  PassContext m_ctx;
  const InstanceBufferCache &m_instances;
  MaterialBinder &m_binder;
  const bool m_depthPrepassed;

  core::HandleID m_boundShader = core::INVALID_HANDLE;
  core::HandleID m_boundAlbedo = core::INVALID_HANDLE;
  // A material's own uniforms can overwrite a frame-wide one (e.g. its own
  // fogColor). The shader keeps that value, so after such a material the
  // frame-wide uniforms must be sent again even on the same shader.
  bool m_materialOverrodeUniforms = false;
};

} // namespace componeng::renderer
