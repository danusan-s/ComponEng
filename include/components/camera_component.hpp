#pragma once
#include "types.hpp"

struct CameraComponent {
  float fov;                  // 4 bytes
  float aspectRatio;          // 4 bytes
  float nearPlane;            // 4 bytes
  float farPlane;             // 4 bytes
  bool isMainCamera;          // 1 byte
  Mat4 viewMatrix;            // 64 bytes
  Mat4 projectionMatrix;      // 64 bytes
};
