#pragma once
#include "ecs/system.hpp"

class UISystem : public System {
public:
  void Init(World &world) override;
  void Update(float deltaTime) override;
  void Shutdown() override;
};
