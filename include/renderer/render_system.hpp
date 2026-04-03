#pragma once
#include "ecs/system.hpp"

/**
 * @brief Presentation-phase system that renders all entities with MeshComponent
 * and MaterialComponent.
 *
 * Performs frustum culling, builds draw batches, and issues OpenGL draw calls
 * using instanced rendering.
 */
class OpenGLRenderSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
  void onDestroy(const SystemState &state) override;
};
