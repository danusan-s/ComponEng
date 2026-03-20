#pragma once
#include "types.hpp"

struct TransformComponent {
  Vec3 position; // 12 bytes
  Vec3 rotation; // 12 bytes (Euler angles in radians)
  Vec3 scale;    // 12 bytes (default to (1, 1, 1) for no scaling)
};
