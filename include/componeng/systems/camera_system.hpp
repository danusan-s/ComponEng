#pragma once
#include "componeng/ecs/system.hpp"

namespace componeng::systems {

/**
 * @brief Simulation-phase system that processes camera movement and mouse look.
 *
 * Reads InputComponent and MouseInputComponent to update the camera entity's
 * TransformComponent and computes the view/projection matrix in
 * CameraComponent.
 */
class CameraSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::systems
