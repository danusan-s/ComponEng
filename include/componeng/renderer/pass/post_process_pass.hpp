#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/draw/fullscreen_draw.hpp"
#include "componeng/renderer/pass/render_pass.hpp"

namespace componeng::renderer {

/**
 * @brief The last pass: copies the finished scene from the off-screen target
 * onto the window through the "post_process" shader. The shader smooths the
 * edges on the way (FXAA, PassContext::fxaa), or, with FXAA off, only scales
 * the scene to the window's size (a render scale other than 1).
 *
 * RenderSystem draws every earlier pass into PassContext::sceneColor; this
 * pass ends that rendering, starts one on the window and draws one full-screen
 * triangle that samples it. It leaves the window rendering open for
 * RenderSystem to end. It runs only when there is a scene image.
 */
class PostProcessPass final : public RenderPass {
public:
  api::PassId id() const override {
    return api::PassId::PostProcess;
  }
  bool enabled(const PassContext &ctx) const override {
    return ctx.sceneColor != nullptr;
  }
  void execute(const PassContext &ctx) override;

private:
  core::HandleID m_shaderID = 0;
  FullscreenDraw m_draw;
};

} // namespace componeng::renderer
