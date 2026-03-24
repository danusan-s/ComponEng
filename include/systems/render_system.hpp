#pragma once
#include "ecs/system.hpp"

class OpenGLRenderSystem : public System {
public:
  void Update(float deltaTime) override;
};
