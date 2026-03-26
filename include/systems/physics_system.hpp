#pragma once
#include "ecs/system.hpp"

class PhysicsSystem : public ISystem {
public:
  void onUpdate(const SystemState& state) override;
};
