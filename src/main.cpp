#include "core/game.hpp"

int main() {
  Game game;
  game.Init();
  game.Run();
  game.Shutdown();
  return 0;
}
