#pragma once

#include "componeng/core/types.hpp"

namespace componeng::renderer {

struct DirectionalLightComponent {
  constexpr static const char *component_name = "DirectionalLightComponent";

  core::Vec3 direction;
  core::Vec3 color;
};

struct PointLightComponent {
  constexpr static const char *component_name = "PointLightComponent";

  core::Vec3 position;
  core::Vec3 color;
  float intensity;
  float radius;
};

} // namespace componeng::renderer
