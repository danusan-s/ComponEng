#pragma once
#include "componeng/core/types.hpp"

namespace componeng::components {

struct RigidBodyComponent {
  enum Type {
    Static,   // Immovable object, not affected by forces
    Dynamic,  // Affected by forces, can move and rotate
    Kinematic // Moves according to user velocities, not affected by forces
  };

  Type type;
  core::Vec3 velocity;
  float mass;
  float restitution;
};

} // namespace componeng::components
