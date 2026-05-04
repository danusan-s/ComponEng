#pragma once
#include "componeng/core/types.hpp"
#include "componeng/renderer/resource_manager.hpp"

struct MaterialComponent {
  Vec3 color;          // 12 bytes
  TextureID textureID; // 4 bytes
  ShaderID shaderID;   // 4 bytes
};
