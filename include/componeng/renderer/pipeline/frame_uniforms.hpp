#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/frame_data.hpp"

#include <memory>

namespace componeng::ecs {
class World;
}

namespace componeng::renderer {

class Shader;

/**
 * @brief The values that are constant for a whole frame (camera, light, fog,
 * time): gathered from the world once, uploaded to one uniform buffer, and
 * bound once, for every shader that declares the FrameData block.
 *
 * Shaders that predate the block get the same values as loose uniforms
 * (applyLoose), from the same struct, so both paths always agree.
 */
class FrameUniforms {
public:
  /** Gathers this frame's values from `world` (the main camera's matrices,
   *  the directional light, fog, time), uploads them and binds the buffer at
   *  kFrameDataBinding. */
  void update(ecs::World &world, ecs::EntityID camera);

  const FrameData &data() const {
    return m_data;
  }

  /** Sends the frame values to `shader` as loose uniforms. Only for shaders
   *  with `!usesFrameData()`. */
  void applyLoose(const Shader &shader) const;

private:
  FrameData m_data{};
  core::Mat4 m_viewProj{1.0f};
  std::unique_ptr<api::IBuffer> m_buffer;
};

} // namespace componeng::renderer
