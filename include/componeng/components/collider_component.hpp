#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/components/transform_component.hpp"

namespace componeng::components {

enum class ColliderType { Box, Sphere };

struct ColliderComponent {
  static constexpr const char* component_name = "ColliderComponent";

  ColliderType type;
  TransformComponent transform;
};

#define COLLIDER_COMPONENT_FIELDS(F, ctx) \
  F(enum, type, 0, ctx) \
  F(Nested, transform, TransformComponent, ctx)

SERIALIZABLE_COMPONENT(ColliderComponent, COLLIDER_COMPONENT_FIELDS)

#undef COLLIDER_COMPONENT_FIELDS

} // namespace componeng::components
