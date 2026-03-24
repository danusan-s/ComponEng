#pragma once
#include "ecs/system.hpp"
#include <GLFW/glfw3.h>

class InputSystem : public System {
public:
  void Update(float deltaTime) override;
};
