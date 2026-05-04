#pragma once

#include "componeng/ecs/world.hpp"

/**
 * @brief User-defined game logic interface.
 *
 * Implement this to hook custom initialization and shutdown logic
 * into the engine lifecycle. The World is passed so the game can
 * create entities, add components, and register additional systems.
 */
class IGame {
public:
  virtual ~IGame() = default;
  virtual void init(World &world) = 0;
  virtual void shutdown(World &world) = 0;
};
