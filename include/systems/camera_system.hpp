#pragma once
#include "ecs/system.hpp"

class CameraSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
};
