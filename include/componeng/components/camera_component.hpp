#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct CameraComponent {
  static constexpr const char* component_name = "CameraComponent";

  float fov = 60.0f;
  float aspectRatio = 16.0f / 9.0f;
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;
  core::Mat4 viewProjectionMatrix;
};

#define CAMERA_COMPONENT_FIELDS(F, ctx) \
  F(fov, ctx) \
  F(aspectRatio, ctx) \
  F(nearPlane, ctx) \
  F(farPlane, ctx) \
  F(viewProjectionMatrix, ctx)

SERIALIZABLE_COMPONENT(CameraComponent, CAMERA_COMPONENT_FIELDS)

#undef CAMERA_COMPONENT_FIELDS

} // namespace componeng::components
