#include "core/game.hpp"

int main() {
  Game &game = Game::Get();
  game.Init();
  game.Run();
  game.Shutdown();
  return 0;
}
