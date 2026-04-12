#pragma once
#include "ecs/system.hpp"
#include "renderer/api/irender_device.hpp"
#include "renderer/batch_map.hpp"
#include <memory>

/**
 * @brief Presentation-phase system that renders all entities with MeshComponent
 * and MaterialComponent.
 *
 * Performs frustum culling, builds draw batches, and issues draw calls
 * using instanced rendering through the render device abstraction.
 */
class RenderSystem : public ISystem {
public:
  void onCreate(const SystemState &state) override;
  void onUpdate(const SystemState &state) override;
  void onDestroy(const SystemState &state) override;

private:
  std::unique_ptr<IRenderDevice> m_device;
  std::unique_ptr<BatchMap> m_batches;
};
