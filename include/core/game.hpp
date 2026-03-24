#pragma once
#include "core/input_state.hpp"
#include "core/window.hpp"
#include "ecs/world.hpp"

class Game {
public:
  void Init();
  void Run();
  void Shutdown();

private:
  void InitSystems();
  void InitObjects();
  void InitComponents();

  InputState inputState;
  World world;
  Window window;
};
