#pragma once

#include "componeng/ecs/entity.hpp"
#include "componeng/physics/collision_detection.hpp"

namespace componeng::events {

struct CollisionEvent {
  ecs::EntityID entityA;
  ecs::EntityID entityB;
  physics::CollisionInfo info;
};

} // namespace componeng::events
