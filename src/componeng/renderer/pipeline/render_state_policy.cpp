#include "componeng/renderer/pipeline/render_state_policy.hpp"

namespace componeng::renderer {

api::RenderState renderStateFor(api::RenderPass pass,
                                const api::RenderState &material,
                                bool depthPrepassed) {
  api::RenderState s;
  switch (pass) {
    case api::RenderPass::DepthPrepass:
      s.cull = material.cull;
      s.depthTest = true;
      s.depthWrite = true;
      s.depthFunc = api::DepthFunc::Less;
      s.blend = api::BlendMode::None;
      s.colorWrite = false;
      return s;

    case api::RenderPass::Opaque:
      s.cull = material.cull;
      s.depthTest = true;
      s.depthWrite = !depthPrepassed;
      s.depthFunc = depthPrepassed ? api::DepthFunc::Equal
                                   : api::DepthFunc::Less;
      s.blend = api::BlendMode::None;
      return s;

    case api::RenderPass::Sky:
      // A full-screen triangle has no winding worth culling. The shader pins
      // depth to exactly 1.0, so the test must accept equal depth, and there
      // is nothing behind the sky to protect, so it does not write depth.
      s.cull = api::CullMode::None;
      s.depthFunc = api::DepthFunc::LessEqual;
      s.depthWrite = false;
      return s;

    case api::RenderPass::Transparent:
      return material;
  }
  return s;
}

} // namespace componeng::renderer
