#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/frame_data.hpp"

#include <memory>

namespace componeng::ecs {
class World;
}

namespace componeng::renderer {

/**
 * @brief The values that are constant for a whole frame (camera, light, fog,
 * time): gathered from the world once and uploaded to one uniform buffer. The
 * buffer is set 0 of every pipeline (layout() and group()); a shader that does
 * not declare the FrameData block simply does not read it.
 */
class FrameUniforms {
public:
  /** Gathers this frame's values from `world` (the main camera's matrices,
   *  the directional light, fog, time) and uploads them. Creates the buffer,
   *  layout and group on the first call. */
  void update(ecs::World &world, ecs::EntityID camera);

  const FrameData &data() const {
    return m_data;
  }

  /** Set 0 of every pipeline; valid after the first update(). */
  rhi::IBindGroupLayout *layout() const {
    return m_layout.get();
  }
  rhi::IBindGroup *group() const {
    return m_group.get();
  }

private:
  FrameData m_data{};
  std::unique_ptr<rhi::IBuffer> m_buffer;
  std::unique_ptr<rhi::IBindGroupLayout> m_layout;
  std::unique_ptr<rhi::IBindGroup> m_group;
};

} // namespace componeng::renderer
