#pragma once
#include "world.hpp"

class Game {
public:
  void Init();
  void Run();

private:
  World world;
};
