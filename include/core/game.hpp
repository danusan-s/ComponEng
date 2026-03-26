#pragma once
#include "ecs/world.hpp"

class IGame {
public:
  virtual void Init(World &world) = 0;
  virtual void Shutdown(World &world) = 0;
};
