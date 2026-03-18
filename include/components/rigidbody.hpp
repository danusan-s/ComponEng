#pragma once

#include "vec3.hpp"

struct RigidBody {
  Vec3 Velocity;
  Vec3 Acceleration;
  float Mass;
};
