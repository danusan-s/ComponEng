#pragma once
#include "core/types.hpp"

struct MaterialComponent {
  Vec3 color; // 12 bytes
  const char *textureName;
  const char *shaderName;
};
