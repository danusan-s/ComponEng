#pragma once
#include "core/types.hpp"
#include "ecs/entity.hpp"

struct CameraComponent {
  float fov;                 // 4 bytes
  float aspectRatio;         // 4 bytes
  float nearPlane;           // 4 bytes
  float farPlane;            // 4 bytes
  Mat4 viewProjectionMatrix; // 64 bytes
};

struct MainCameraSingleton {
  EntityID entity;
};
