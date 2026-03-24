#pragma once
#include "ecs/world.hpp"

class MeshGenerator : public System {
public:
  void Init(World &world) override;
  void Update(float deltaTime) override;
};
