#pragma once
#include "core/types.hpp"

struct BoundingBoxComponent {
  Vec3 min; // 12 bytes
  Vec3 max; // 12 bytes
};
