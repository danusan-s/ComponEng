#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/batching/draw_key.hpp"
#include "componeng/renderer/batching/render_queue.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace componeng::renderer {

/**
 * @brief The GPU side of the RenderQueue's per-instance data.
 *
 * One buffer is kept per (mesh, material) pair and reused across frames
 * instead of being created and destroyed every draw call. A larger buffer
 * replaces it only when a batch needs more room than it ever did before; the
 * old one is retired by the device. Otherwise it is rewritten in place.
 */
class InstanceBufferCache {
public:
  /** Uploads every batch's instance data; call once a frame, before any pass
   *  draws. Procedural batches have none (the shader reads gl_InstanceID).
   *  Returns the bytes uploaded. */
  std::size_t upload(rhi::IDevice &device, const RenderQueue &queue);

  /** The buffer holding `batch`'s instances, as of the last upload(). Not for
   *  procedural batches, nor for one whose shader has no per-instance inputs
   *  (null then). */
  rhi::IBuffer *bufferFor(const RenderBatch &batch) const;

private:
  std::unordered_map<DrawKey, std::unique_ptr<rhi::IBuffer>, DrawKeyHash>
      m_buffers;
};

} // namespace componeng::renderer
