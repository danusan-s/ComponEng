#pragma once
#include "componeng/core/types.hpp"

namespace componeng::components {

struct TransformComponent {
  core::Vec3 position; // 12 bytes
  core::Vec3 rotation; // 12 bytes (Euler angles in radians)
  core::Vec3 scale;    // 12 bytes (default to (1, 1, 1) for no scaling)
};

} // namespace componeng::components
