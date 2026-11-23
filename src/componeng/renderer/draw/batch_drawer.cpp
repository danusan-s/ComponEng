#include "componeng/renderer/draw/batch_drawer.hpp"

#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/draw/instance_buffer_cache.hpp"
#include "componeng/renderer/draw/material_bindings.hpp"
#include "componeng/renderer/draw/pipeline_cache.hpp"
#include "componeng/renderer/draw/render_state_policy.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

BatchDrawer::BatchDrawer(const PassContext &ctx) : m_ctx(ctx) {
}

void BatchDrawer::draw(const RenderBatch &batch, api::PassId pass) {
  rhi::IDevice &device = m_ctx.device;
  const AssetManager &assets = m_ctx.assets;
  const bool depthOnly = pass == api::PassId::DepthPrepass;

  IMaterial &material = assets.getMaterial(batch.materialID);
  const Mesh &mesh = assets.getMesh(batch.meshID);
  const Shader &colorShader = assets.getShader(material.getShaderID());
  if (!colorShader.valid() || !mesh.vertexBuffer()) {
    return;
  }

  // The pre-pass runs the shader's vertex stage with an empty fragment stage,
  // so nothing the fragment stage reads (albedo, PBR textures) matters. A
  // cutout material's pre-pass keeps the fragment stage's alpha test, so it
  // does read the albedo (and the material's own textures). So does a batch
  // with LOD-fading instances: it discards on the fade pattern, and the
  // pre-pass must cut the same pixels.
  const bool alphaTestedDepth =
      depthOnly && (material.isAlphaTested() || batch.fading);
  const Shader::Variant variant = !depthOnly ? Shader::Variant::Color
                                  : alphaTestedDepth
                                      ? Shader::Variant::AlphaTestedDepth
                                      : Shader::Variant::DepthOnly;
  const Shader::Program program = colorShader.program(variant);
  if (!program.vertex || !program.fragment) {
    return;
  }

  resources::PassStats &ps = m_ctx.stats.of(pass);

  const bool programChanged =
      program.vertex != m_boundVertex || program.fragment != m_boundFragment;
  if (programChanged) {
    ++ps.shaderBinds;
    // Time is attributed to the shader the batch belongs to, not its depth
    // variant, so a shader's timing covers all of its draws.
    m_ctx.profiler.timeShader(&colorShader);
    m_boundVertex = program.vertex;
    m_boundFragment = program.fragment;
  }

  // Procedural batches have no per-instance data: the shader reads
  // INSTANCE_INDEX. A shader with no per-instance inputs has none either.
  const api::VertexLayout &layout = material.getVertexLayout();
  rhi::IBuffer *instances =
      batch.procedural ? nullptr : m_ctx.instances.bufferFor(batch);

  PipelineRequest request;
  request.vertex = program.vertex;
  request.fragment = program.fragment;
  request.frameLayout = m_ctx.frame.layout();
  request.materialLayout = colorShader.materialLayout();
  const bool batchPrepassed =
      pass == api::PassId::Opaque && m_ctx.depthPrepassed && batch.depthPrepass;
  request.state =
      renderStateFor(pass, material.getRenderState(), batchPrepassed);
  request.colorFormat = m_ctx.colorFormat;
  request.depthFormat = m_ctx.depthFormat;
  request.meshVertices = true;
  request.instanceLayout = instances ? &layout : nullptr;
  rhi::IGraphicsPipeline *pipeline = m_ctx.pipelines.get(device, request);
  if (!pipeline) {
    return;
  }
  if (pipeline != m_boundPipeline) {
    device.bindPipeline(*pipeline);
    m_boundPipeline = pipeline;
  }

  rhi::IBindGroup *group =
      m_ctx.materials.groupFor(device, assets, material, colorShader);
  if (!group) {
    return;
  }
  if (group != m_boundGroup) {
    device.bindGroup(1, *group);
    m_boundGroup = group;
  }

  if (batch.meshID != m_boundMesh) {
    device.bindVertexBuffer(0, *mesh.vertexBuffer(), 0);
    device.bindIndexBuffer(*mesh.indexBuffer(), 0, rhi::IndexType::U32);
    m_boundMesh = batch.meshID;
  }
  if (instances) {
    device.bindVertexBuffer(1, *instances, 0);
  }
  device.drawIndexed(static_cast<uint32_t>(mesh.indexCount()),
                     batch.instanceCount, 0, 0, 0);

  ++ps.drawCalls;
  ps.instances += batch.instanceCount;
  ps.triangles +=
      static_cast<uint64_t>(mesh.indexCount() / 3) * batch.instanceCount;
}

void drawBatches(const PassContext &ctx, api::PassId pass, std::size_t first,
                 std::size_t last) {
  BatchDrawer drawer(ctx);
  for (std::size_t i = first; i < last; ++i) {
    drawer.draw(ctx.queue.sorted(i), pass);
  }
}

} // namespace componeng::renderer
