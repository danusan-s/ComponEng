#pragma once
#include "entity.hpp"
#include <set>

class World;

class System {
  friend class SystemManager;

public:
  std::set<EntityID> entities;
  virtual ~System() = default;
  virtual void Update(float deltaTime) = 0;
  virtual void Shutdown() {
    return;
  };

protected:
  World *world = nullptr;
  virtual void Init(World &world) {
    this->world = &world;
  }
};
