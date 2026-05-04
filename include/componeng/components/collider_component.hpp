#pragma once
#include "componeng/components/transform_component.hpp"

namespace componeng::components {

enum class ColliderType { Box, Sphere };

struct ColliderComponent {
  ColliderType type;
  TransformComponent transform;
};

} // namespace componeng::components
