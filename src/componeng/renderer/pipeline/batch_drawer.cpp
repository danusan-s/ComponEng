#include "componeng/renderer/pipeline/batch_drawer.hpp"

#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset/pbr.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/instance_buffer_cache.hpp"
#include "componeng/renderer/pipeline/material_binder.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/renderer/pipeline/render_state_policy.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

BatchDrawer::BatchDrawer(const PassContext &ctx,
                         const InstanceBufferCache &instances,
                         MaterialBinder &binder, bool depthPrepassed)
    : m_ctx(ctx), m_instances(instances), m_binder(binder),
      m_depthPrepassed(depthPrepassed) {}

void BatchDrawer::forgetBoundState() {
  m_boundProgram = nullptr;
  m_boundAlbedo = core::INVALID_HANDLE;
  m_materialOverrodeUniforms = false;
}

void BatchDrawer::draw(const RenderBatch &batch, api::RenderPass pass) {
  api::IRenderDevice &device = m_ctx.device;
  const AssetManager &assets = m_ctx.assets;
  const bool depthOnly = pass == api::RenderPass::DepthPrepass;

  IMaterial &material = assets.getMaterial(batch.materialID);
  const Mesh &mesh = assets.getMesh(batch.meshID);
  const Shader &colorShader = assets.getShader(material.getShaderID());
  // The pre-pass runs the shader's vertex stage with an empty fragment stage,
  // so nothing the fragment stage reads (albedo, PBR textures) needs setting
  // up. A cutout material's pre-pass keeps the fragment stage's alpha test, so
  // it does read the albedo (and the material's own textures).
  const bool alphaTestedDepth = depthOnly && material.isAlphaTested();
  const bool readsAlbedo = !depthOnly || alphaTestedDepth;
  const Shader &shader =
      !depthOnly         ? colorShader
      : alphaTestedDepth ? colorShader.alphaTestedDepthVariant()
                         : colorShader.depthVariant();

  resources::PassStats &ps = m_ctx.stats.of(pass);

  const bool shaderChanged = &shader != m_boundProgram;
  if (shaderChanged) {
    ++ps.shaderBinds;
    // Time is attributed to the shader the batch belongs to, not its depth
    // variant, so a shader's timing covers all of its draws.
    m_ctx.profiler.timeShader(&colorShader);
    shader.use();
    m_boundProgram = &shader;
  }

  device.setRenderState(
      renderStateFor(pass, material.getRenderState(), m_depthPrepassed));

  if (shaderChanged || m_materialOverrodeUniforms) {
    if (!shader.usesFrameData()) {
      m_ctx.frame.applyLoose(shader);
    }
    if (readsAlbedo) {
      shader.setInteger("albedoMap", 0);
    }
  }

  if (readsAlbedo) {
    const auto *pbr = dynamic_cast<const PbrMaterial *>(&material);
    MaterialBinder::setTextureToggles(shader, pbr != nullptr);

    const Texture2D &albedo = assets.getTexture(material.getTextureID());
    if (material.getTextureID() != m_boundAlbedo) {
      device.setActiveTextureUnit(0);
      albedo.bind();
      m_boundAlbedo = material.getTextureID();
    }
    if (pbr && !depthOnly) {
      MaterialBinder::bindPbrTextures(device, assets, shader, *pbr, albedo);
    }
  }

  m_binder.bindResources(device, assets, shader, material, depthOnly);
  MaterialBinder::setUniforms(shader, material);
  m_materialOverrodeUniforms = !material.getUniforms().empty();

  mesh.getImpl().bind();

  const api::VertexLayout &layout = material.getVertexLayout();
  if (batch.procedural) {
    // No per-instance data: no instance attributes to enable, the shader
    // reads gl_InstanceID.
    device.drawIndexedInstanced(mesh.indexCount(), batch.instanceCount);
  } else {
    device.setupInstanceAttributes(m_instances.bufferFor(batch), layout);
    device.drawIndexedInstanced(mesh.indexCount(), batch.instanceCount);
    device.unbindInstanceAttributes(layout);
  }

  ++ps.drawCalls;
  ps.instances += batch.instanceCount;
  ps.triangles +=
      static_cast<uint64_t>(mesh.indexCount() / 3) * batch.instanceCount;
}

} // namespace componeng::renderer
