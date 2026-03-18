#pragma once
#include "quaternion.hpp"
#include "vec3.hpp"

struct Transform {
  Vec3 Position;
  Quaternion rotation;
  Vec3 Scale;
};
