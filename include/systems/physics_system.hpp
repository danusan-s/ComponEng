#pragma once
#include "system.hpp"

class PhysicsSystem : public System {
public:
  void Update(float deltaTime) override;
};
