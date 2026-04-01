#pragma once
#include "ecs/system.hpp"

class OpenGLRenderSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
  void onDestroy(const SystemState &state) override;
};
