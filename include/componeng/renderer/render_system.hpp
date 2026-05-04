#pragma once
#include "componeng/ecs/system.hpp"
#include "componeng/renderer/batch_map.hpp"
#include <memory>

namespace componeng::renderer {

/**
 * @brief Presentation-phase system that renders all entities with MeshComponent
 * and MaterialComponent.
 *
 * Performs frustum culling, builds draw batches, and issues draw calls
 * using instanced rendering through the render device abstraction.
 */
class RenderSystem : public ecs::ISystem {
public:
  void onCreate(const ecs::SystemState &state) override;
  void onUpdate(const ecs::SystemState &state) override;
  void onDestroy(const ecs::SystemState &state) override;

private:
  std::unique_ptr<BatchMap> m_batches;
};

} // namespace componeng::renderer
