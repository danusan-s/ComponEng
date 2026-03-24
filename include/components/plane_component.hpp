#pragma once
#include "core/types.hpp"
#include <string>

struct PlaneComponent {
  float width;          // 4 bytes
  float height;         // 4 bytes
  int widthSegments;    // 4 bytes
  int heightSegments;   // 4 bytes
  Vec3 normal;          // 12 bytes
  const char *meshName; // 32 bytes (pointer + size)
};

struct WaterComponent {};
