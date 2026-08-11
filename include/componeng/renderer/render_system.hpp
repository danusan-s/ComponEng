#pragma once

#include "componeng/ecs/system.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/batch_map.hpp"

#include <memory>
#include <unordered_map>

namespace componeng::renderer {

/**
 * @brief Presentation-phase system that renders all entities with MeshComponent
 * and MaterialComponent.
 *
 * Performs builds draw batches, and issues draw calls
 * using instanced rendering through the render device abstraction.
 */
class RenderSystem final : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;

private:
  // One persistent instance-data buffer per (mesh,material) combo, reused
  // across frames via setSubData instead of being recreated every draw call.
  std::unordered_map<DrawKey, std::unique_ptr<api::IBuffer>, DrawKeyHash>
      m_instanceBuffers;
};

} // namespace componeng::renderer
