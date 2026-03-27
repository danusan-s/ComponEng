#pragma once
#include "core/game.hpp"
#include "core/window.hpp"
#include "ecs/world.hpp"

#include "components/camera_component.hpp"
#include "components/collider_component.hpp"
#include "components/input_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "physics/physics_system.hpp"
#include "renderer/render_system.hpp"
#include "systems/camera_system.hpp"
#include "systems/input_system.hpp"

class Engine {
public:
  static Engine &Get() {
    static Engine instance;
    return instance;
  }
  void Init();
  void Run(IGame &game);
  void Shutdown();

  World world;
  Window window;

private:
  Engine() = default;
  ~Engine() = default;
  void RegisterSystems();
  void RegisterComponents();
  void InitObjects();
};
