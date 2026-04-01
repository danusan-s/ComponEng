#pragma once

#include "ecs/world.hpp"

class IGame {
public:
  virtual ~IGame() = default;
  virtual void init(World& world) = 0;
  virtual void shutdown(World& world) = 0;
};
