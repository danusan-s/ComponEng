#pragma once
#include "core/window.hpp"
#include "ecs/world.hpp"

class Game {
public:
  static Game &Get() {
    static Game instance;
    return instance;
  }
  void Init();
  void Run();
  void Shutdown();

  World world;
  Window window;

private:
  Game() = default;
  ~Game() = default;

  void InitSystems();
  void InitObjects();
  void InitComponents();
};
