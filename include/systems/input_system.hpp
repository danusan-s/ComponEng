#pragma once

#include "glad/glad.h"
#include "ecs/system.hpp"
#include <GLFW/glfw3.h>

class InputSystem : public ISystem {
public:
  void onUpdate(const SystemState& state) override;
};
