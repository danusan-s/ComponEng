#pragma once
#include "input_state.hpp"
#include "window.hpp"
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

  InputState inputState;
  World world;
  Window window;
};
