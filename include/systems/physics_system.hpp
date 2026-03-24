#pragma once
#include "ecs/system.hpp"

class PhysicsSystem : public System {
public:
  void Update(float deltaTime) override;
};
