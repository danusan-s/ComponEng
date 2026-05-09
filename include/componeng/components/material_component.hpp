#pragma once
#include "componeng/core/types.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MaterialComponent {
  core::Vec3 color;              // 12 bytes
  renderer::TextureID textureID; // 4 bytes
  renderer::ShaderID shaderID;   // 4 bytes
};

} // namespace componeng::components
