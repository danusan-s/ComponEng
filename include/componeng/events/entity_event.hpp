#pragma once

#include "componeng/ecs/entity.hpp"

namespace componeng::events {

struct EntityCreateEvent {
  ecs::EntityID entity;
};

struct EntityDestroyEvent {
  ecs::EntityID entity;
};

} // namespace componeng::events
