#include "componeng/renderer/pass/post_process_pass.hpp"

#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/draw/render_state_policy.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

void PostProcessPass::execute(const PassContext &ctx) {
  if (m_shaderID == 0) {
    m_shaderID = ctx.assets.getShaderID("post_process");
  }
  const Shader &shader = ctx.assets.getShader(m_shaderID);
  rhi::IImage &scene = *ctx.sceneColor;
  rhi::IDevice &device = ctx.device;

  // The scene's rendering ends here; the copy is the window's own. It clears
  // the whole window, letterbox bars included.
  device.endRendering();
  rhi::RenderingDesc window;
  window.hasDepth = false;
  device.beginRendering(window);
  device.bindGroup(0, *ctx.frame.group());

  ctx.profiler.timeShader(&shader);
  ParameterBlock &params = m_draw.params(device, shader);
  const rhi::Extent2D size = scene.extent();
  params.set("invResolution",
             core::Vec2(1.0f / static_cast<float>(size.width),
                        1.0f / static_cast<float>(size.height)));
  params.set("fxaaOn", ctx.fxaa ? 1.0f : 0.0f);

  const rhi::RenderState state = renderStateFor(
      api::PassId::PostProcess, rhi::RenderState{}, /*depthPrepassed=*/false);
  m_draw.draw(ctx, shader, state, device.backbufferFormat(),
              rhi::Format::Undefined, &scene, ctx.sceneSampler);

  resources::PassStats &ps = ctx.stats.of(api::PassId::PostProcess);
  ++ps.drawCalls;
  ++ps.instances;
  ++ps.triangles;
  ++ps.shaderBinds;
}

} // namespace componeng::renderer
