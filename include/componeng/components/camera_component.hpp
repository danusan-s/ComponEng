#pragma once
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"

namespace componeng::components {

struct CameraComponent {
  float fov;                       // 4 bytes
  float aspectRatio;               // 4 bytes
  float nearPlane;                 // 4 bytes
  float farPlane;                  // 4 bytes
  core::Mat4 viewProjectionMatrix; // 64 bytes
};

struct MainCameraSingleton {
  ecs::EntityID entity;
};

} // namespace componeng::components
