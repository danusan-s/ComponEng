#include "componeng/renderer/draw/instance_buffer_cache.hpp"

namespace componeng::renderer {

std::size_t InstanceBufferCache::upload(rhi::IDevice &device,
                                        const RenderQueue &queue) {
  std::size_t uploaded = 0;
  for (std::size_t i = 0; i < queue.size(); ++i) {
    const RenderBatch &batch = queue.sorted(i);
    if (batch.procedural) {
      continue;
    }
    const std::size_t requiredBytes = batch.instanceData.size() * sizeof(float);
    if (requiredBytes == 0) {
      continue;
    }
    std::unique_ptr<rhi::IBuffer> &buf =
        m_buffers[DrawKey{batch.meshID, batch.materialID}];
    uploaded += requiredBytes;
    if (!buf || requiredBytes > buf->size()) {
      buf = device.createBuffer({requiredBytes, rhi::BufferUsage::Vertex,
                                 rhi::MemoryKind::Upload, "instances"});
    }
    buf->write(0, batch.instanceData.data(), requiredBytes);
  }
  return uploaded;
}

rhi::IBuffer *InstanceBufferCache::bufferFor(const RenderBatch &batch) const {
  auto it = m_buffers.find(DrawKey{batch.meshID, batch.materialID});
  return it == m_buffers.end() ? nullptr : it->second.get();
}

} // namespace componeng::renderer
