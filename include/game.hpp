#pragma once
#include "world.hpp"

class Game {
public:
  void Init();
  void Run();
  void Shutdown();

private:
  void InitSystems();
  void InitObjects();
  void InitComponents();
  World world;
};
