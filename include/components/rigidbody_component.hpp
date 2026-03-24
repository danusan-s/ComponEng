#pragma once
#include "core/types.hpp"

struct RigidBodyComponent {
  enum Type {
    Static,   // Immovable object, not affected by forces
    Dynamic,  // Affected by forces, can move and rotate
    Kinematic // Moves according to user velocities, not affected by forces
  } type;     // 4 bytes

  Vec3 velocity; // 12 bytes
  float mass;    // 4 bytes
};
