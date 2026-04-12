#pragma once
#include "renderer/resource_manager.hpp"

struct MeshComponent {
  MeshID meshID;       // 4 bytes
  bool visible = true; // 1 byte
};
