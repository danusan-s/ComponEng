#pragma once
#include "ecs/system.hpp"

class CameraSystem : public System {
public:
  void Update(float deltaTime) override;
};
