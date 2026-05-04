#pragma once
#include "componeng/components/transform_component.hpp"

enum class ColliderType { Box, Sphere };

// --- Collider Component ---
struct ColliderComponent {
  ColliderType type;
  TransformComponent transform;
};
