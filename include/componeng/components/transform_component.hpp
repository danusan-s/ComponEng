#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct TransformComponent {
  static constexpr const char *component_name = "TransformComponent";

  core::Vec3 position = core::Vec3(0.0f, 0.0f, 0.0f);
  core::Vec3 rotation = core::Vec3(0.0f, 0.0f, 0.0f); // Euler angles in degrees
  core::Vec3 scale = core::Vec3(1.0f, 1.0f, 1.0f);
};

#define TRANSFORM_COMPONENT_FIELDS(F, ctx)                                     \
  F(position, ctx)                                                             \
  F(rotation, ctx)                                                             \
  F(scale, ctx)

SERIALIZABLE_COMPONENT(TransformComponent, TRANSFORM_COMPONENT_FIELDS)

#undef TRANSFORM_COMPONENT_FIELDS

} // namespace componeng::components
