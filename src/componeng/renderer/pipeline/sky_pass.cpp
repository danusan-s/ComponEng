#include "componeng/renderer/pipeline/sky_pass.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/component/skybox_component.hpp"
#include "componeng/renderer/pipeline/frame_uniforms.hpp"
#include "componeng/renderer/pipeline/pass_profiler.hpp"
#include "componeng/renderer/pipeline/render_state_policy.hpp"
#include "componeng/resources/fog.hpp"
#include "componeng/resources/render_stats.hpp"

namespace componeng::renderer {

void drawSkyPass(const PassContext &ctx, ecs::World &world,
                 ecs::EntityID camera) {
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
  const auto &cam = world.readComponent<camera::CameraComponent>(camera);
  const auto &fog = world.getResource<resources::Fog>();

  ctx.profiler.timeShader(&shader);
  shader.use();
  if (!shader.usesFrameData()) {
    ctx.frame.applyLoose(shader);
  }

  // Projection x rotation-only view, inverted: turns a pixel back into the
  // direction the camera sees through it. No camera translation, so the sky
  // stays fixed relative to the camera's orientation.
  shader.setMatrix4("invSkyViewProj",
                    core::inverse(cam.projectionMatrix * cam.viewMatrix));
  shader.setVector3f("skyColor", sky->color);
  // The fog colour as horizon lets fogged ground fade into the sky.
  shader.setVector3f("horizonColor",
                     sky->horizonFromFog
                         ? core::Vec3(fog.color.x, fog.color.y, fog.color.z)
                         : sky->horizonColor);

  ctx.device.setRenderState(renderStateFor(
      api::RenderPass::Sky, api::RenderState{}, /*depthPrepassed=*/false));
  ctx.device.drawFullscreenTriangle();

  resources::PassStats &ps = ctx.stats.of(api::RenderPass::Sky);
  ++ps.drawCalls;
  ++ps.instances;
  ++ps.triangles;
  ++ps.shaderBinds;

  ctx.device.setRenderState(api::RenderState{});
  ctx.profiler.timeShader(nullptr);
}

} // namespace componeng::renderer
