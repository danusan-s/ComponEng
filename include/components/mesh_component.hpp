#pragma once
#include <cstddef>

struct MeshComponent {
  unsigned int VAO, VBO, EBO; // 12 bytes
  size_t indexCount;          // 8 bytes
};
