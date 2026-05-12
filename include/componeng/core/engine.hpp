#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "componeng/core/game.hpp"
#include "componeng/core/window.hpp"
#include "componeng/ecs/world.hpp"

namespace componeng::core {

/**
 * @brief Singleton entry point for the game engine.
 *
 * Owns the Window, World, and the main game loop. Initializes GLFW, OpenGL,
 * registers all components and systems, loads default assets, and drives
 * the frame loop (delta time, system updates, buffer swaps).
 *
 * Access via Engine::get().
 */
class Engine {
public:
  static Engine &get() {
    static Engine instance;
    return instance;
  }

  // Load resources, run component registration and init world and window
  void init();
  // Load game init, create systems and run main loop and also teardown game
  void run(IGame &game);
  // Teardown world and window
  void shutdown();

  ecs::World m_world;
  Window m_window;

private:
  Engine() = default;
  ~Engine() = default;
  // Helper to register engine components and systems
  void registerComponents();
  void registerSystems();
  // Create some default entities (camera) to populate the world with
  void initObjects();
};

} // namespace componeng::core
