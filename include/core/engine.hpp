#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "components/camera_component.hpp"
#include "components/collider_component.hpp"
#include "components/input_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "core/game.hpp"
#include "core/window.hpp"
#include "ecs/world.hpp"
#include "physics/physics_system.hpp"
#include "renderer/render_system.hpp"
#include "systems/camera_system.hpp"
#include "systems/input_system.hpp"

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
  void init();
  void run(IGame &game);
  void shutdown();

  World m_world;
  Window m_window;

private:
  Engine() = default;
  ~Engine() = default;
  void registerSystems();
  void registerComponents();
  void initObjects();
};
