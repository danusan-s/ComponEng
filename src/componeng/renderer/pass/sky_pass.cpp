#include "componeng/renderer/pass/sky_pass.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/component/skybox_component.hpp"
#include "componeng/renderer/draw/render_state_policy.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/resources/fog.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

void SkyPass::execute(const PassContext &ctx) {
  ecs::World &world = ctx.world;
  const SkyboxComponent *sky = nullptr;
  world.query().read<SkyboxComponent>().each([&](const SkyboxComponent &s) {
    if (!sky) {
      sky = &s;
    }
  });
  if (!sky) {
    return;
  }

  const Shader &shader =
      ctx.assets.getShader(ctx.assets.getShaderID(sky->shaderName));
  const auto &cam = world.readComponent<camera::CameraComponent>(ctx.camera);
  const auto &fog = world.getResource<resources::Fog>();

  ctx.profiler.timeShader(&shader);

  // Projection x rotation-only view, inverted: turns a pixel back into the
  // direction the camera sees through it. No camera translation, so the sky
  // stays fixed relative to the camera's orientation.
  ParameterBlock &params = m_draw.params(ctx.device, shader);
  params.set("invSkyViewProj",
             core::inverse(cam.projectionMatrix * cam.viewMatrix));
  params.set("skyColor", sky->color);
  // The fog colour as horizon lets fogged ground fade into the sky.
  params.set("horizonColor",
             sky->horizonFromFog
                 ? core::Vec3(fog.color.x, fog.color.y, fog.color.z)
                 : sky->horizonColor);

  m_draw.draw(ctx, shader,
              renderStateFor(api::PassId::Sky, rhi::RenderState{},
                             /*depthPrepassed=*/false),
              ctx.colorFormat, ctx.depthFormat);

  resources::PassStats &ps = ctx.stats.of(api::PassId::Sky);
  ++ps.drawCalls;
  ++ps.instances;
  ++ps.triangles;
  ++ps.shaderBinds;
}

} // namespace componeng::renderer
