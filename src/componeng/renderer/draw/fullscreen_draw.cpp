#include "componeng/renderer/draw/fullscreen_draw.hpp"

#include "componeng/renderer/asset/shader.hpp"
#include "componeng/renderer/draw/pipeline_cache.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/pass_context.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::renderer {

ParameterBlock &FullscreenDraw::params(rhi::IDevice &device,
                                       const Shader &shader) {
  if (!m_params.valid()) {
    if (const shader::ReflectedResource *block = shader.materialBlock()) {
      m_params.init(device, *block);
    }
  }
  return m_params;
}

bool FullscreenDraw::draw(const PassContext &ctx, const Shader &shader,
                          const rhi::RenderState &state,
                          rhi::Format colorFormat, rhi::Format depthFormat,
                          rhi::IImage *image, rhi::ISampler *sampler) {
  if (!shader.valid()) {
    return false;
  }
  rhi::IDevice &device = ctx.device;
  params(device, shader);
  m_params.flush();

  if (!m_group || m_groupBuffer != m_params.buffer() || m_groupImage != image ||
      m_groupSampler != sampler) {
    rhi::BindGroupDesc desc;
    desc.layout = shader.materialLayout();
    for (const shader::ReflectedResource &r : shader.reflection().resources) {
      if (r.set != 1) {
        continue;
      }
      rhi::BindGroupEntry e;
      e.binding = r.binding;
      if (r.kind == shader::ResourceKind::UniformBlock && r.binding == 0 &&
          m_params.valid()) {
        e.buffer = m_params.buffer();
      } else if (r.kind == shader::ResourceKind::Sampler2D && image) {
        e.image = image;
        e.sampler = sampler;
      } else {
        LOG_ERROR("A full-screen shader declares '%s', which it cannot be "
                  "given",
                  r.name.c_str());
        return false;
      }
      desc.entries.push_back(e);
    }
    m_group = device.createBindGroup(desc);
    m_groupBuffer = m_params.buffer();
    m_groupImage = image;
    m_groupSampler = sampler;
  }

  const Shader::Program program = shader.program(Shader::Variant::Color);
  PipelineRequest request;
  request.vertex = program.vertex;
  request.fragment = program.fragment;
  request.frameLayout = ctx.frame.layout();
  request.materialLayout = shader.materialLayout();
  request.state = state;
  request.colorFormat = colorFormat;
  request.depthFormat = depthFormat;
  rhi::IGraphicsPipeline *pipeline = ctx.pipelines.get(device, request);
  if (!pipeline) {
    return false;
  }
  device.bindPipeline(*pipeline);
  device.bindGroup(0, *ctx.frame.group());
  device.bindGroup(1, *m_group);
  device.draw(3, 1, 0, 0);
  return true;
}

} // namespace componeng::renderer
