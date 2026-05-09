#pragma once

#include "componeng/ecs/system.hpp"
#include <GLFW/glfw3.h>

namespace componeng::systems {

/**
 * @brief Initialization-phase system that polls raw GLFW input and writes it
 * into InputComponent and MouseInputComponent.
 *
 * Runs once per frame before simulation systems so they can read
 * the current input state without depending on GLFW directly.
 * This decoupling allows for input remapping easily.
 */
class InputSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::systems
