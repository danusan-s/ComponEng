#pragma once

#include "componeng/ecs/system.hpp"
#include <GLFW/glfw3.h>

namespace componeng::input {

/**
 * @brief Initialization-phase system that polls raw GLFW input and converts
 * it into semantic ActionState for simulation systems to read.
 *
 * Runs once per frame before simulation systems so they can read
 * the current input state without depending on GLFW directly.
 * This decoupling allows for input remapping easily.
 */
class InputSystem : public ecs::ISystem {
public:
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::input
