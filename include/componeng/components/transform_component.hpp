#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct TransformComponent {
  static constexpr const char* component_name = "TransformComponent";

  core::Vec3 position; // 12 bytes
  core::Vec3 rotation; // 12 bytes (Euler angles in radians)
  core::Vec3 scale;    // 12 bytes (default to (1, 1, 1) for no scaling)
};

#define TRANSFORM_COMPONENT_FIELDS(F, ctx) \
  F(Vec3, position, ctx) \
  F(Vec3, rotation, ctx) \
  F(Vec3, scale, ctx)

SERIALIZABLE_COMPONENT(TransformComponent, TRANSFORM_COMPONENT_FIELDS)

#undef TRANSFORM_COMPONENT_FIELDS

} // namespace componeng::components
