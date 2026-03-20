#pragma once
#include "system.hpp"

class CameraSystem : public System {
public:
  void Update(float deltaTime) override;
};
