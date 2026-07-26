#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct CameraComponent {
  static constexpr const char* component_name = "CameraComponent";

  float fov;                       // 4 bytes
  float aspectRatio;               // 4 bytes
  float nearPlane;                 // 4 bytes
  float farPlane;                  // 4 bytes
  core::Mat4 viewProjectionMatrix; // 64 bytes
};

#define CAMERA_COMPONENT_FIELDS(F, ctx) \
  F(float, fov, 60.0f, ctx) \
  F(float, aspectRatio, 16.0f / 9.0f, ctx) \
  F(float, nearPlane, 0.1f, ctx) \
  F(float, farPlane, 1000.0f, ctx) \
  F(Mat4, viewProjectionMatrix, ctx)

SERIALIZABLE_COMPONENT(CameraComponent, CAMERA_COMPONENT_FIELDS)

#undef CAMERA_COMPONENT_FIELDS

} // namespace componeng::components
