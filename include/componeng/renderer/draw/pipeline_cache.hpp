#pragma once

#include "componeng/renderer/backend/api/renderer_types.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace componeng::renderer {

/** Everything that makes one graphics pipeline different from another. */
struct PipelineRequest {
  rhi::IShaderModule *vertex = nullptr;
  rhi::IShaderModule *fragment = nullptr;
  rhi::IBindGroupLayout *frameLayout = nullptr;
  rhi::IBindGroupLayout *materialLayout = nullptr;
  rhi::RenderState state;
  rhi::Format colorFormat = rhi::Format::RGBA8;
  rhi::Format depthFormat = rhi::Format::D32F;
  // Vertex slot 0 holds a mesh (position, normal, uv).
  bool meshVertices = false;
  // Vertex slot 1 holds per-instance data in this layout; null for none.
  const api::VertexLayout *instanceLayout = nullptr;
};

/**
 * @brief Creates each distinct graphics pipeline once and hands it out again.
 * The same shader and state drawn for many materials is one pipeline.
 */
class PipelineCache {
public:
  /** The pipeline for `request`, created on first use. Null if the device
   *  could not make it. */
  rhi::IGraphicsPipeline *get(rhi::IDevice &device,
                              const PipelineRequest &request);

  void clear() {
    m_pipelines.clear();
  }

  /** Pipelines created so far, for tests. */
  std::size_t created() const {
    return m_pipelines.size();
  }

private:
  std::unordered_map<std::string, std::unique_ptr<rhi::IGraphicsPipeline>>
      m_pipelines;
};

} // namespace componeng::renderer
