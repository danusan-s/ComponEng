#pragma once
#include "world.hpp"

class Game {
public:
  void Init();
  void Run();

private:
  void InitSystems();
  void InitObjects();
  void InitComponents();
  World world;
};
