#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/components/transform_component.hpp"

namespace componeng::physics {

enum class ColliderType { Box, Sphere };

struct ColliderComponent {
  static constexpr const char *component_name = "ColliderComponent";

  ColliderType type = ColliderType::Box;
  components::TransformComponent transform;
  bool isTrigger = false;
};

} // namespace componeng::physics

namespace componeng::components {

#define COLLIDER_COMPONENT_FIELDS(F, ctx)                                      \
  F(type, ctx)                                                                 \
  F(transform, ctx)                                                            \
  F(isTrigger, ctx)

SERIALIZABLE_COMPONENT(physics::ColliderComponent, COLLIDER_COMPONENT_FIELDS)

#undef COLLIDER_COMPONENT_FIELDS

} // namespace componeng::components
