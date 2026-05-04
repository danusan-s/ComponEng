#pragma once

#include "componeng/ecs/system.hpp"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

/**
 * @brief Initialization-phase system that polls raw GLFW input and writes it
 * into InputComponent and MouseInputComponent.
 *
 * Runs once per frame before simulation systems so they can read
 * the current input state without depending on GLFW directly.
 * This decoupling allows for input remapping easily.
 */
class InputSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
};
