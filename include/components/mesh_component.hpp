#pragma once
#include <GL/gl.h>
#include <string>

struct MeshComponent {
  std::string modelName; // 32 bytes (pointer + size)
};
