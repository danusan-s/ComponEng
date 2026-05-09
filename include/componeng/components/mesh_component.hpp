#pragma once
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MeshComponent {
  renderer::MeshID meshID; // 4 bytes
  bool visible = true;     // 1 byte
};

} // namespace componeng::components
