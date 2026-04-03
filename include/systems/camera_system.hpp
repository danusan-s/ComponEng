#pragma once
#include "ecs/system.hpp"

/**
 * @brief Simulation-phase system that processes camera movement and mouse look.
 *
 * Reads InputComponent and MouseInputComponent to update the camera entity's
 * TransformComponent and computes the view/projection matrix in
 * CameraComponent.
 */
class CameraSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
};
