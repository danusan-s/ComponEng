#pragma once

#include <cstddef>

namespace componeng::renderer::api {

/**
 * @brief The stages RenderSystem draws a frame in, in order. Each is timed on
 * the GPU (IRenderDevice::beginGpuPass) and has its own counters
 * (resources::RenderStats).
 */
enum class RenderPass : int {
  DepthPrepass, // opaque and cutout batches, depth only
  Opaque,       // opaque and cutout batches, shaded
  Sky,          // the full-screen sky triangle
  Transparent,  // blended / depth-less batches
};

constexpr std::size_t kRenderPassCount = 4;

constexpr const char *renderPassName(RenderPass pass) {
  switch (pass) {
    case RenderPass::DepthPrepass:
      return "Depth pre-pass";
    case RenderPass::Opaque:
      return "Opaque";
    case RenderPass::Sky:
      return "Sky";
    case RenderPass::Transparent:
      return "Transparent";
  }
  return "?";
}

} // namespace componeng::renderer::api
