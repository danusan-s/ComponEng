#pragma once
#include "core/game.hpp"
#include "core/window.hpp"
#include "ecs/world.hpp"

#include "components/bounding_box_component.hpp"
#include "components/camera_component.hpp"
#include "components/input_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "systems/camera_system.hpp"
#include "systems/input_system.hpp"
#include "systems/physics_system.hpp"
#include "systems/render_system.hpp"

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
  void RegisterLaterSystems();
  void RegisterComponents();
  void InitObjects();
};
