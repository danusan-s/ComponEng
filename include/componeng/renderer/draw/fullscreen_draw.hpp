#pragma once

#include "componeng/renderer/asset/parameter_block.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"

#include <memory>

namespace componeng::renderer {

class Shader;
struct PassContext;

/**
 * @brief One full-screen triangle drawn with a shader that has no vertex
 * input: its parameter block (set by the caller) and at most one sampled
 * image. Used by the sky and post-process passes, which each keep one across
 * frames.
 */
class FullscreenDraw {
public:
  /** The shader's parameter block, created on first use. Set its values,
   *  then call draw(). */
  ParameterBlock &params(rhi::IDevice &device, const Shader &shader);

  /** Draws into the open rendering, whose colour and depth formats are given.
   *  `image` and `sampler` go to the shader's sampler, if it has one. Returns
   *  false and draws nothing if the shader is not valid. */
  bool draw(const PassContext &ctx, const Shader &shader,
            const rhi::RenderState &state, rhi::Format colorFormat,
            rhi::Format depthFormat, rhi::IImage *image = nullptr,
            rhi::ISampler *sampler = nullptr);

private:
  ParameterBlock m_params;
  std::unique_ptr<rhi::IBindGroup> m_group;
  rhi::IBuffer *m_groupBuffer = nullptr;
  rhi::IImage *m_groupImage = nullptr;
  rhi::ISampler *m_groupSampler = nullptr;
};

} // namespace componeng::renderer
