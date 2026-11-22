#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/pass/render_pass.hpp"

namespace componeng::renderer {

/**
 * @brief Copies the finished scene from the off-screen target onto the window
 * through the "fxaa" shader, which smooths the edges on the way.
 *
 * RenderSystem draws every earlier pass into PassContext::sceneTarget; this
 * pass switches back to the window and draws one full-screen triangle that
 * samples it. It runs only when there is a scene target.
 */
class FxaaPass final : public RenderPass {
public:
  api::PassId id() const override {
    return api::PassId::Fxaa;
  }
  bool enabled(const PassContext &ctx) const override {
    return ctx.sceneTarget != nullptr;
  }
  void execute(const PassContext &ctx) override;

private:
  core::HandleID m_shaderID = 0;
};

} // namespace componeng::renderer
