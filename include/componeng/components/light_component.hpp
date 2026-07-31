#pragma once

#include "componeng/core/types.hpp"

namespace componeng::components {

struct DirectionalLightComponent {
  core::Vec3 direction;
  core::Vec3 color;
};

struct PointLightComponent {
  core::Vec3 position;
  core::Vec3 color;
  float intensity;
  float radius;
};

} // namespace componeng::components
