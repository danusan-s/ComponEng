#pragma once
#include "componeng/core/types.hpp"

struct RigidBodyComponent {
  enum Type {
    Static,   // Immovable object, not affected by forces
    Dynamic,  // Affected by forces, can move and rotate
    Kinematic // Moves according to user velocities, not affected by forces
  };

  Type type;
  Vec3 velocity;
  float mass;
  float restitution;
};
