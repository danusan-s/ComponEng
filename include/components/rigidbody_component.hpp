#pragma once
#include "types.hpp"

struct RigidBodyComponent {
  Vec3 velocity;     // 12 bytes
  Vec3 acceleration; // 12 bytes
  float mass;        // 4 bytes
};
