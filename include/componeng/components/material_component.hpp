#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct MaterialComponent {
  static constexpr const char *component_name = "MaterialComponent";

  core::Name materialName;

  // Runtime data (not serialized)
  core::HandleID materialID = 0;
  core::HandleID textureID = 0;
  core::HandleID shaderID = 0;
};

#define MATERIAL_COMPONENT_FIELDS(F, ctx) F(materialName, ctx)

SERIALIZABLE_COMPONENT(MaterialComponent, MATERIAL_COMPONENT_FIELDS)

#undef MATERIAL_COMPONENT_FIELDS

struct ColorComponent {
  static constexpr const char *component_name = "ColorComponent";

  core::Vec4 color;
};

#define COLOR_COMPONENT_FIELDS(F, ctx) F(color, ctx)

SERIALIZABLE_COMPONENT(ColorComponent, COLOR_COMPONENT_FIELDS)

#undef COLOR_COMPONENT_FIELDS

} // namespace componeng::components
